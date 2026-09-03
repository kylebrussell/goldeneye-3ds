#!/usr/bin/env python3
"""Keep the 3DS submission cache exact, local to each authored draw pass."""

from pathlib import Path
import os
import re
import subprocess
import tempfile


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", source, re.S)
    assert match is not None, f"missing {name}"
    start = source.index("{", match.start())
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated {name}")


repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text(encoding="utf-8")
apply_cached = function_body(source, "renderer_apply_material_cached")
renderer_draw = function_body(source, "renderer_draw")

# The fastest adjacent cache hit must mean the complete decoded material,
# resolved texture, and missing-texture policy are all identical. It returns
# the exact prior compile result without preparing or emitting state.
hit = apply_cached.index("if (cache->valid")
miss = apply_cached.index("renderer_prepare_material_cached(")
assert hit < miss
assert "cache->texture == binding->texture0" in apply_cached[hit:miss]
assert "cache->fallback == binding->missing_texture_fallback" in apply_cached[hit:miss]
assert re.search(
    r"memcmp\(&cache->material,\s*material,\s*"
    r"sizeof\(cache->material\)\)\s*==\s*0",
    apply_cached[hit:miss],
)
assert "*result = cache->result;" in apply_cached[hit:miss]
assert apply_cached.count("renderer_prepare_material_cached(") == 1
assert apply_cached.count("ge_3ds_material_apply_prepared_delta(") == 1
assert "renderer_material_result_gpu_equal" in apply_cached

# World and first-person state cannot leak across sky, fog, or HUD setup. The
# cache changes submission only; it does not sort, reorder, or delete draws.
assert "RuntimeRendererMaterialCache world_material_cache = {0};" in renderer_draw
assert (
    "RuntimeRendererMaterialCache first_person_material_cache = {0};"
    in renderer_draw
)
assert renderer_draw.count("renderer_apply_material_cached(") == 2
assert "qsort(" not in renderer_draw
assert "C3D_DrawArrays(GPU_TRIANGLES" in renderer_draw

# Invisible authored rooms must not perturb projection, texture, or material
# state. The first visibility rejection precedes all three operations in the
# world-batch loop.
world_loop = renderer_draw.index("for (i = 0U; i < draw_batch_count;)")
first_person_loop = renderer_draw.index(
    "for (i = 0U; i < first_person->batch_count;)", world_loop
)
world = renderer_draw[world_loop:first_person_loop]
reject = world.index("!renderer_room_visible(")
projection = world.index("C3D_FVUnifMtx4x4(")
texture_lookup = world.index("ge_3ds_scene_textures_find(")
material_apply = world.index("renderer_apply_material_cached(")
assert reject < projection < texture_lookup < material_apply
world_fast = world[world.index("if (world_material_cache.valid"):texture_lookup]
assert "memcmp(&world_material_cache.material," in world_fast
assert "sizeof(batch->material)) == 0" in world_fast
assert "world_material_cache.result.state.draw_enabled != 0U" in world_fast
assert "fine_profile.world_material_apply_reuses++" in world_fast
assert "renderer_apply_material_cached(" not in world_fast
assert "C3D_" not in world_fast
merge = world[world.index("while (next < draw_batch_count)"):]
# A rejected range may only bridge a draw under the same projection that
# proved it invisible. Also don't spend clip-test work on an invisible room.
assert merge.index("batch->coordinate_space") < merge.index(
    "renderer_world_batch_may_draw(")
assert merge.index("if (!next_room_visible") < merge.index(
    "renderer_world_batch_may_draw(")

# Runtime results expose actual applications, exact cache hits, and texture
# lookups separately for both passes so a replay can measure the real win.
assert "draw_profile_state=%llu,%llu,%llu,%llu,%llu,%llu" in source
assert "draw_profile_prepare=%llu,%llu,%llu,%llu" in source

# Dynamic actor publication owns its exact range upload and scene generation.
# It must never poison the independent Bond camera generation; doing so made
# animated guards rerun camera/portal visibility on every catch-up tick.
stage_overlay = source.index("if (stage_actor_runtime_updated)")
simulation_end = source.index(
    "simulation_elapsed_milliseconds = osGetTime() - simulation_start;",
    stage_overlay,
)
assert "rendered_player_generation = UINT64_MAX" not in source[
    stage_overlay:simulation_end
]
dam_overlay = source.index("if (guard_runtime_updated")
assert "rendered_player_generation = UINT64_MAX" not in source[
    dam_overlay:stage_overlay
]

# Compile the actual preparation/cache bodies, not a parallel implementation.
# GPU submission is outside this test; material compilation is the real port
# implementation. Deliberate collisions must retain exact bindings/results.
material_source = (repo / "platform/3ds/source/ge_3ds_material.c").read_text()
compiler_source = (repo / "port/src/ge_pica_apply.c").read_text()
compiler_fields = set(re.findall(r"\bmaterial->(\w+)", compiler_source
    + function_body(material_source, "ge_3ds_material_prepare")))
key_fields = set(re.findall(r"\bmaterial->(\w+)",
    function_body(source, "renderer_material_key")))
assert compiler_fields == key_fields, (compiler_fields - key_fields,
    key_fields - compiler_fields)
cache_start = source.index("enum {\n    RENDERER_PREPARED_MATERIAL_CACHE_CAPACITY")
cache_end = source.index("static bool renderer_material_result_gpu_equal(", cache_start)
test_source = r'''
#include "ge_3ds_material.h"
#include "ge_dam_room.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static void ge_3ds_material_replace_texture_source(
    GePicaApplyChannel *channel, GePicaApplySource replacement)
''' + function_body(material_source, "ge_3ds_material_replace_texture_source") + r'''
Ge3dsMaterialStatus ge_3ds_material_prepare(const GePicaMaterial *material,
    const Ge3dsMaterialBinding *binding, Ge3dsMaterialResult *result)
''' + function_body(material_source, "ge_3ds_material_prepare") + source[cache_start:cache_end] + r'''
static bool renderer_material_result_gpu_equal(
    const Ge3dsMaterialResult *left, const Ge3dsMaterialResult *right)
''' + function_body(source, "renderer_material_result_gpu_equal") + r'''
static RuntimeRendererPreparedMaterialCache cache;
static uint64_t hits, misses;
static size_t material_set(const GePicaMaterial *material,
    const Ge3dsMaterialBinding *binding)
{
    RuntimeRendererPreparedMaterialKey key;
    renderer_material_key(material, binding, &key);
    return renderer_material_hash(&key);
}
static void check(const GePicaMaterial *material, Ge3dsMaterialBinding binding)
{
    Ge3dsMaterialResult expected = {0}, actual = {0};
    assert(ge_3ds_material_prepare(material, &binding, &expected)
        == GE_3DS_MATERIAL_OK);
    assert(renderer_prepare_material_cached(&cache, material, &binding,
        &actual, &hits, &misses) == GE_3DS_MATERIAL_OK);
    assert(memcmp(&actual.state, &expected.state, sizeof(actual.state)) == 0);
    assert(actual.texture_bound == expected.texture_bound);
}
static void check_gpu_equality(void)
{
    Ge3dsMaterialResult a, b;
    memset(&a, 0xa5, sizeof(a));
    a.texture_bound = 1;
    assert(!renderer_material_result_gpu_equal(NULL, &a));
    assert(!renderer_material_result_gpu_equal(&a, NULL));
    for (size_t byte = 0; byte < sizeof(a.state); ++byte) {
        for (unsigned value = 0; value < 256U; ++value) {
            memcpy(&b, &a, sizeof(b));
            ((uint8_t *)&b.state)[byte] = (uint8_t)value;
            GePicaApplyState left = a.state, right = b.state;
            left.material_fallback_flags = right.material_fallback_flags = 0U;
            left.apply_fallback_flags = right.apply_fallback_flags = 0U;
            const bool expected = memcmp(&left, &right, sizeof(left)) == 0;
            assert(renderer_material_result_gpu_equal(&a, &b) == expected);
            assert(renderer_material_result_gpu_equal(&b, &a) == expected);
        }
    }
    memcpy(&b, &a, sizeof(b));
    b.texture_bound = 0;
    assert(!renderer_material_result_gpu_equal(&a, &b));
    /* Diagnostic-only changes must not mutate either published result. */
    b.texture_bound = a.texture_bound;
    b.state.material_fallback_flags ^= UINT32_MAX;
    b.state.apply_fallback_flags ^= UINT32_MAX;
    Ge3dsMaterialResult before_a, before_b;
    memcpy(&before_a, &a, sizeof(a));
    memcpy(&before_b, &b, sizeof(b));
    assert(renderer_material_result_gpu_equal(&a, &b));
    assert(memcmp(&a, &before_a, sizeof(a)) == 0);
    assert(memcmp(&b, &before_b, sizeof(b)) == 0);
}
static void check_merge(const GePicaMaterial *left, const GePicaMaterial *right)
{
    GeDamRoomDrawBatch a = {0}, b = {0};
    a.material = *left;
    b.material = *right;
    a.vertex_count = b.first_vertex = b.vertex_count = 3;
    a.texture_valid = b.texture_valid = 1;
    a.texture.texture_id = b.texture.texture_id = 65535;
    if (dam_batches_compatible(&a, &b)) {
        C3D_Tex texture = {0};
        for (size_t present = 0; present < 2; ++present) {
            for (size_t fallback = 0; fallback < 2; ++fallback) {
                Ge3dsMaterialBinding binding = {present ? &texture : NULL,
                    (Ge3dsMaterialTextureFallback)fallback};
                Ge3dsMaterialResult l = {0}, r = {0};
                assert(ge_3ds_material_prepare(left, &binding, &l) == GE_3DS_MATERIAL_OK);
                assert(ge_3ds_material_prepare(right, &binding, &r) == GE_3DS_MATERIAL_OK);
                assert(memcmp(&l.state, &r.state, sizeof(l.state)) == 0);
                assert(l.texture_bound == r.texture_bound);
            }
        }
    }
    b.material = a.material;
    assert(dam_batches_compatible(&a, &b));
    ++b.texture.texture_id;
    assert(!dam_batches_compatible(&a, &b));
    b.texture.texture_id = a.texture.texture_id;
    b.coordinate_space = GE_DAM_ROOM_COORDINATE_EYE;
    assert(!dam_batches_compatible(&a, &b));
    b.coordinate_space = a.coordinate_space;
    b.texture_valid = 0;
    assert(!dam_batches_compatible(&a, &b));
    b.texture_valid = a.texture_valid;
    ++b.first_vertex;
    assert(!dam_batches_compatible(&a, &b));
    assert(dam_batch_materials_compatible(&a, &b));
    a.first_vertex = SIZE_MAX - 1;
    b.first_vertex = 1;
    assert(!dam_batches_compatible(&a, &b));
    assert(!dam_batches_compatible(NULL, &b));
}
int main(void)
{
    check_gpu_equality();
    GePicaMaterial material = {0}, colliders[3];
    C3D_Tex textures[2] = {0};
    Ge3dsMaterialBinding binding = {&textures[0], GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE};
    size_t count = 0, set;
    material.color_combine = GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE;
    material.alpha_combine = GE_PICA_ALPHA_PRIMITIVE;
    material.texture_enabled = 1;
    set = material_set(&material, &binding);
    for (uint32_t i = 0; i < 65536U && count < 3U; ++i) {
        material.primitive_color.red = (uint8_t)i;
        material.primitive_color.green = (uint8_t)(i >> 8U);
        if (material_set(&material, &binding) == set)
            colliders[count++] = material;
    }
    assert(count == 3U);
    for (size_t i = 0; i < 1000U; ++i) check(&colliders[i % 2U], binding);
    assert(misses == 2U && hits == 998U);
    check(&colliders[0], binding); /* 1 becomes least-recently used. */
    check(&colliders[2], binding); /* Evicts 1, retains 0. */
    assert(misses == 3U);
    check(&colliders[0], binding);
    assert(misses == 3U);
    check(&colliders[1], binding);
    assert(misses == 4U);
    /* Mutating ANY decoded byte must preserve the uncached result. Bytes
     * outside the compiler's dependency set may reuse exact preparation. */
    for (size_t i = 0; i < sizeof(material); ++i) {
        material = colliders[0];
        ((uint8_t *)&material)[i] ^= 1U;
        check_merge(&colliders[0], &material);
        uint64_t before = misses;
        check(&material, binding);
        assert(misses <= before + 1U);
        before = misses;
        check(&material, binding);
        assert(misses == before);
    }
    memset(&cache, 0, sizeof(cache));
    hits = misses = 0U;
    material = colliders[0];
    binding.missing_texture_fallback = GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE;
    for (size_t i = 0U; i < 65536U; ++i) {
        material.texture_id = (uint16_t)i;
        material.texture_image_address = (uint32_t)i * 64U;
        material.texture_scale_s = (uint16_t)i;
        material.combine_mux0 = (uint32_t)i;
        check_merge(&colliders[0], &material);
        binding.texture0 = &textures[i % 2U];
        check(&material, binding);
    }
    assert(misses == 1U && hits == 65535U);
    binding.texture0 = NULL;
    check(&material, binding);
    assert(misses == 2U); /* Unbound texture has different preparation. */
    binding.missing_texture_fallback = GE_3DS_MATERIAL_TEXTURE_FALLBACK_REPLACE;
    check(&material, binding);
    assert(misses == 3U);
    for (size_t i = 0; i < 4000U; ++i) {
        material = colliders[i % 3U];
        material.fallback_flags = (i % 7U == 0U)
            ? GE_PICA_FALLBACK_MISSING_TEXTURE : 0U;
        binding.texture0 = i % 3U == 0U ? NULL : &textures[i % 2U];
        binding.missing_texture_fallback = (Ge3dsMaterialTextureFallback)(i % 3U);
        check(&material, binding);
    }
    assert(renderer_prepare_material_cached(NULL, &material, &binding,
        NULL, NULL, NULL) == GE_3DS_MATERIAL_INVALID_ARGUMENT);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="ge-material-cache-") as directory:
    test_path = Path(directory) / "cache.c"
    test_path.write_text(test_source)
    executable = Path(directory) / "cache-test"
    # The target uses short enums, which changes both key and result layout.
    for layout_flags in ([], ["-fshort-enums"]):
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", *layout_flags,
            "-I" + str(repo / "platform/3ds/tests/include"),
            "-I" + str(repo / "platform/3ds/include"),
            "-I" + str(repo / "port/include"), str(test_path),
            str(repo / "port/src/ge_pica_apply.c"), "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)

print(
    "3DS renderer submission cache: exact prepared state, pass-local, "
    "authored draw order retained"
)

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
reject = world.index("!dam_visibility_contains_room(")
projection = world.index("C3D_FVUnifMtx4x4(")
texture_lookup = world.index("ge_3ds_scene_textures_find(")
material_apply = world.index("renderer_apply_material_cached(")
assert reject < projection < texture_lookup < material_apply

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
cache_start = source.index("enum {\n    RENDERER_PREPARED_MATERIAL_CACHE_CAPACITY")
cache_end = source.index("static bool renderer_material_result_gpu_equal(", cache_start)
test_source = r'''
#include "ge_3ds_material.h"
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
static RuntimeRendererPreparedMaterialCache cache;
static uint64_t hits, misses;
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
int main(void)
{
    GePicaMaterial material = {0}, colliders[3];
    C3D_Tex textures[2] = {0};
    Ge3dsMaterialBinding binding = {&textures[0], GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE};
    size_t count = 0, set;
    material.color_combine = GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE;
    material.alpha_combine = GE_PICA_ALPHA_PRIMITIVE;
    material.texture_enabled = 1;
    set = renderer_material_hash(&material, &binding);
    for (uint32_t i = 0; i < 65536U && count < 3U; ++i) {
        material.primitive_color.red = (uint8_t)i;
        material.primitive_color.green = (uint8_t)(i >> 8U);
        if (renderer_material_hash(&material, &binding) == set)
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
    /* Every decoded byte, texture identity and fallback remain part of the
     * key. Randomized deterministic churn checks evictions against uncached
     * compilation, including unknown enum values handled by the compiler. */
    for (size_t i = 0; i < sizeof(material); ++i) {
        material = colliders[0];
        ((uint8_t *)&material)[i] ^= 1U;
        uint64_t before = misses;
        check(&material, binding);
        assert(misses == before + 1U);
        check(&material, binding);
        assert(misses == before + 1U);
    }
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
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
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

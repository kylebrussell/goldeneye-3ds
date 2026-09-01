#!/usr/bin/env python3
"""Keep the 3DS submission cache exact, local to each authored draw pass."""

from pathlib import Path
import re


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

print(
    "3DS renderer submission cache: exact prepared state, pass-local, "
    "authored draw order retained"
)

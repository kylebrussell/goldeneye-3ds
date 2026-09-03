#!/usr/bin/env python3
"""Guard the runtime wiring exercised by the C room/texture sanitizer tests.

This is a source contract, not emulator or rendering verification.
"""

from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()
camera = source.split("static bool update_original_dam_camera(", 1)[1].split(
    "static void initialize_original_dam_camera(", 1
)[0]
assert "if (!project_scene && preview->preload_queue.pending_count == 0U)" in camera
assert "ge_3ds_scene_textures_load(" not in camera
assert "ge_3ds_scene_textures_close(&dam_scene_textures)" not in camera
ordered = [
    camera.index("ge_dam_dynamic_scene_prepare_next("),
    camera.index("ge_3ds_scene_textures_reconcile_prepare("),
    camera.index("prepare_stage_guard_texture_residency("),
    camera.index("project_dam_with_original_camera("),
    camera.index("ge_3ds_scene_textures_reconcile_commit_after("),
    camera.index("memcpy(dam_scene_texture_slots"),
    camera.index("candidate_textures.slots = NULL;"),
]
assert ordered == sorted(ordered)
assert "&texture_stats, commit_room_stream_geometry, &commit" in camera
assert "candidate_preview = calloc(1U, sizeof(*candidate_preview))" in camera
assert "ge_3ds_scene_textures_close(&candidate_textures)" in camera
assert "ge_dam_dynamic_scene_abort(" in camera

# Pending visibility work drains even when the player publication is unchanged.
live = source.split("if (!visual_probe_tour.enabled", 1)[1]
assert re.search(
    r"dam_intro\.player\.publication_generation\s*!= rendered_player_generation"
    r"\s*\|\| dam_preview\.preload_queue\.pending_count\s*!= 0U", live
)
assert "false, &stage_ordinary_objects);" in live
assert "true,\n                    &stage_ordinary_objects);" in live
print("room stream source contract: live queue drain, hidden guard dependencies, "
      "geometry-gated texture commit, safe abort and ownership handoff")

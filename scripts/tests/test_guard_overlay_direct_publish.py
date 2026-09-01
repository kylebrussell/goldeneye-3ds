#!/usr/bin/env python3
"""Pin the 3DS animated-guard cache directly to the scene overlay tail."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    source = (REPO / "platform/3ds/source/main.c").read_text()
    start = source.index("static bool refresh_stage_guard_overlay(")
    end = source.index("static const RuntimeStageScenePartRange", start)
    body = source[start:end]
    direct = body.index(
        "storage.vertices = dynamic_scene->overlay_vertices")
    build = body.index(
        "ge_original_stage_guard_runtime_build_scene_cached(", direct)
    commit = body.index(
        "ge_dam_dynamic_scene_commit_overlay_batches(", build)
    assert direct < build < commit
    assert "ge_dam_dynamic_scene_update_overlay_segment(" not in body
    print("3DS guard overlay: model cache writes scene tail directly; batch-only commit retained")


if __name__ == "__main__":
    main()

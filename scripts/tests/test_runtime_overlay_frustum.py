#!/usr/bin/env python3
"""Pin conservative frustum rejection to every 3DS world coordinate space."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    source = (REPO / "platform/3ds/source/main.c").read_text()
    start = source.index("static bool renderer_world_batch_may_draw(")
    end = source.index("static RuntimeGbiModel rareware_body_model", start)
    body = source[start:end]
    for matrix in (
        "preview->authored_world_to_clip",
        "preview->runtime_world_to_clip",
        "preview->eye_to_clip",
    ):
        assert matrix in body
    assert "source_index >= room_batch_count" not in body
    assert source.count("renderer_world_batch_may_draw(") == 3
    assert "ge_draw_batch_world_bounds_classify(" in body
    assert "GE_DRAW_BATCH_BOUNDS_UNCERTAIN" in body
    assert "ge_draw_batch_world_may_intersect_clip_frustum(" in body
    upload_start = source.index("static bool upload_dam_gpu_world_scene_range(",
                                source.index("static bool upload_dam_gpu_world_scene_range(") + 1)
    upload_end = source.index("static bool upload_dam_gpu_world_scene(", upload_start)
    upload = source[upload_start:upload_end]
    assert "ge_draw_batch_world_bounds_build(" in upload
    assert upload.index("bounds->valid = 0;") < upload.index("if (!map_texture_uv)")
    assert "free(dam_preview.gpu_batch_bounds);" in source
    print("3DS frustum culling: authored rooms plus runtime/eye-space overlays")


if __name__ == "__main__":
    main()

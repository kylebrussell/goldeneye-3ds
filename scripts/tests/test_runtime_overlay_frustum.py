#!/usr/bin/env python3
"""Pin conservative frustum rejection to every 3DS world coordinate space."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    source = (REPO / "platform/3ds/source/main.c").read_text()
    start = source.index("static bool renderer_world_batch_may_draw(")
    end = source.index("static RuntimeGbiModel rareware_body_model", start)
    body = source[start:end]
    render_start = source.index("static void renderer_draw(")
    render = source[render_start:source.index("static void renderer_exit(", render_start)]
    world_start = render.index("const bool gpu_world_render")
    world_loop = render.index("for (i = 0U; i < draw_batch_count;)", world_start)
    context_setup = render[world_start:world_loop]
    for coordinate, matrix in (
        ("AUTHORED", "authored_world_to_clip"),
        ("RUNTIME", "runtime_world_to_clip"),
        ("EYE", "eye_to_clip"),
    ):
        assert "GE_DAM_ROOM_COORDINATE_" + coordinate in body
        assert "&clip_contexts[GE_DAM_ROOM_COORDINATE_" + coordinate + "]" in context_setup
        assert "dam_preview->" + matrix in context_setup
    assert context_setup.count("ge_draw_batch_clip_context_init(") == 3
    assert "source_index >= room_batch_count" not in body
    assert source.count("renderer_world_batch_may_draw(") == 2
    assert render.count("renderer_world_batch_profiled(") == 2
    assert "ge_draw_batch_world_visibility_prepared(" in body
    assert "GE_DRAW_BATCH_FIRST_VERTEX_VISIBLE" in body
    assert "GE_DRAW_BATCH_BOUNDS_CULLED" in body
    assert "GE_DRAW_BATCH_VERTICES_CULLED" in body
    assert "source_index >= preview->batch_count" in body
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

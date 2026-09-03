#!/usr/bin/env python3
"""Keep same-weapon layout changes connected to texture/UV publication."""

from pathlib import Path


def main() -> None:
    repo = Path(__file__).resolve().parents[2]
    source = (repo / "platform/3ds/source/main.c").read_text()
    start = source.index("static bool update_first_person_scene(")
    end = source.index("static void close_first_person_scene(", start)
    body = source[start:end]
    before = body.index("topology_publications_before = runtime->cache.topology_publications;")
    build = body.index("ge_original_first_person_scene_build_cached(")
    changed = body.index("runtime->cache.topology_publications != topology_publications_before")
    invalidate = body.index("runtime->uv_ready = false;", changed)
    ensure = body.index("ge_original_model_scene_visit_textures(", changed)
    remap = body.index("ge_3ds_scene_texture_map_uv(", ensure)
    assert before < build < changed < invalidate < ensure < remap
    assert "ensure_first_person_scene_texture" in body[ensure:remap]
    assert "ge_original_first_person_assets_visit_texture_ids(" in body[ensure:remap]
    colors = body.index("if (uv_updated) {", remap)
    assert colors < body.index("source->processed.rgba[0] / 255.0f", colors)
    print("first-person layout publication: texture residency and UV remap follow cached switches")


if __name__ == "__main__":
    main()

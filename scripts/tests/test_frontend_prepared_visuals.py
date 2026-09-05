#!/usr/bin/env python3
"""Keep renderer preparation outside vertex loops and tied to each frame."""
from pathlib import Path
import re

source = (Path(__file__).resolve().parents[2] / "platform/3ds/source/main.c").read_text()


def function(name):
    match = re.search(rf"static\s+(?:bool|void)\s+{name}\s*\([^;]*?\)\s*\{{", source)
    assert match, name
    depth = 0
    for index in range(source.index("{", match.start()), len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if depth == 0:
            return source[match.start():index + 1]
    raise AssertionError(name)


rareware = function("prepare_original_frontend_rareware")
assert rareware.count("ge_original_frontend_lighting_prepare(") == 1
assert rareware.count("ge_original_frontend_rareware_projection_prepare(") == 1
first_loop = rareware.index("for (index = 0U;")
assert rareware.index("ge_original_frontend_lighting_prepare(") < first_loop
assert rareware.index("ge_original_frontend_rareware_projection_prepare(") < first_loop
assert rareware.count("ge_original_frontend_generate_lit_vertex_prepared(") == 2
assert rareware.count("ge_original_frontend_rareware_project_prepared(") == 3
assert "static GeOriginalFrontendLightingContext" not in rareware
assert "static GeOriginalFrontendProjectionContext" not in rareware
assert "ge_original_frontend_generate_lit_vertex(" not in rareware
assert "ge_original_frontend_rareware_project(" not in rareware

pitem = function("prepare_original_frontend_pitem_scene")
prepare = pitem.index("ge_original_frontend_lighting_prepare(")
assert pitem.index("if (!runtime->logo_ready) goto done;") < prepare
assert prepare < pitem.index("for (part_index = 0U; part_index < runtime->logo_vertex_count;")
assert pitem.count("ge_original_frontend_generate_lit_vertex_prepared(") == 1
assert "ge_original_frontend_generate_lit_vertex(" not in pitem
assert "? 0U : presentation->title_light_diffuse" in pitem
assert "? presentation->nintendo_rotation_radians : 0.0f" in pitem

# The original constructors put selection rectangles after their background
# model. The PICA pass must restore its flat text state before those boxes,
# then draw text above them. Otherwise wallet paper erases the selection.
draw = function("draw_original_frontend_list")
wallet = draw.index("batch_index < runtime->wallet_batch_count")
overlay = draw.index(
    "draw_list->box_vertex_count - draw_list->background_vertex_count")
glyphs = draw.index("const size_t normal_glyph_count")
assert wallet < overlay < glyphs
assert draw.count("draw_list->box_vertex_count - draw_list->background_vertex_count") == 1
assert draw.index("configure_original_font_texture_environment(", wallet) < overlay
assert "draw_list->box_vertex_count);" not in draw[:wallet]
frontend = function("run_original_frontend")
assert re.search(
    r"Ge3dsOriginalFrontendLine\s+lines\[GE_ORIGINAL_FRONTEND_MAX_LINES\]\s*=\s*\{0\};",
    frontend,
), "Every frontend frame must initialize optional tab, color and value fields"
print("Frontend renderer: per-frame lighting/projection snapshots precede all logo vertex loops; Nintendo/Rareware/title inputs retained")

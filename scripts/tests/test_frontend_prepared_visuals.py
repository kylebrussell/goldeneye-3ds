#!/usr/bin/env python3
"""Keep renderer preparation outside vertex loops and tied to each frame."""
from pathlib import Path
import re

source = (Path(__file__).resolve().parents[2] / "platform/3ds/source/main.c").read_text()


def function(name):
    match = re.search(rf"static\s+bool\s+{name}\s*\([^;]*?\)\s*\{{", source)
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
print("Frontend renderer: per-frame lighting/projection snapshots precede all logo vertex loops; Nintendo/Rareware/title inputs retained")

#!/usr/bin/env python3
"""Assert sight extraction retains the original bodies and live publication."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / "scripts"))
from extract_dam_mission_hud_slice import extract_function

generated = Path(sys.argv[1]).read_text()
ammo_body = extract_function((repo / "src/game/gunfire.c").read_text(),
    "microcode_generation_ammo_related")
assert "gDPSetTextureFilter(gdl++, G_TF_BILERP);" in ammo_body
assert "gDPSetTextureFilter(end++, G_TF_BILERP);" in generated
for file, name in (
    ("othermodemicrocode.c", "texSetRenderMode"),
    ("bondwalk2.c", "draw_textured_rectangle"),
    ("bondwalk2.c", "display_image_at_position"),
    ("gunfire.c", "gunDrawSight"),
):
    expected = extract_function((repo / "src/game" / file).read_text(), name)
    if name == "gunDrawSight":
        expected = expected.replace("s32 *gdl", "Gfx **gdl")
        expected = expected.replace("s32 sp54;", "Gfx *sp54;")
        expected = expected.replace("&xypos, &halfedxy", "xypos, halfedxy")
    assert extract_function(generated, name) == expected, name

source = (repo / "platform/3ds/source/main.c").read_text()
assert "ge_original_gun_sight_snapshot(&snapshot)" in source
assert "ge_original_gun_sight_build_draw(&snapshot, draw, &visible)" in source
assert "ge_original_gun_sight_texture_source()" in source
assert "ge_3ds_material_apply(&gun_sight_draw.material" in source
assert "build_crosshair_from_gbi" not in source
assert "fallback_crosshair" not in source
print("canonical sight source: four exact bodies, authored texture and live material path")

#!/usr/bin/env python3
"""Verify the typed Fast3D rectangle reaches PICA in exact authored order."""

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
header = (repo / "platform/3ds/include/ge_3ds_material.h").read_text()
source = (repo / "platform/3ds/source/ge_3ds_material.c").read_text()
body = function_body(source, "ge_3ds_texture_rectangle_submit")

assert "GE_3DS_TEXTURE_RECTANGLE_VERTEX_COUNT = 6" in header
ordered = [
    body.index("ge_pica_texture_rectangle_translate_action("),
    body.index("memcpy(vertex_destination"),
    body.index("GSPGPU_FlushDataCache(vertex_destination"),
    body.index("ge_3ds_material_apply("),
    body.index("C3D_DrawArrays(GPU_TRIANGLES"),
]
assert ordered == sorted(ordered)
assert "submission->material.state.draw_enabled != 0U" in body
assert "GE_3DS_TEXTURE_RECTANGLE_VERTEX_COUNT" in body
assert "qsort(" not in body

print(
    "3DS GBI texture rectangle: translate -> publish -> material -> "
    "single authored PICA draw"
)

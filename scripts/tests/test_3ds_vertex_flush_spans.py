#!/usr/bin/env python3
"""Keep per-frame 3DS cache flushes bounded to vertices actually updated."""

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
renderer_init = function_body(source, "renderer_init")
renderer_draw = function_body(source, "renderer_draw")

# The initial upload still publishes every initialized vertex once.
assert "TOTAL_VERTEX_COUNT * sizeof(Vertex)" in renderer_init

# The only vertices rewritten by renderer_draw are the 12 crosshair vertices.
assert "CROSSHAIR_VERTEX_COUNT == 12U" in source
assert re.search(
    r"GSPGPU_FlushDataCache\(\s*vertex_buffer,\s*"
    r"renderer_vertex_flush_bytes\(\s*CROSSHAIR_VERTEX_COUNT,\s*"
    r"CROSSHAIR_VERTEX_COUNT\)\s*\)",
    renderer_draw,
)
assert "TOTAL_VERTEX_COUNT * sizeof(Vertex)" not in renderer_draw

# The sole scene publication flushes only the dirty authored/dynamic range,
# not the 65,536-vertex allocation backing it. The canonical camera-matrix
# publication does not rewrite renderer vertices at all.
camera_flush = re.compile(
    r"GSPGPU_FlushDataCache\(\s*"
    r"\(Vertex \*\)vertex_buffer \+ DAM_ROOM_VERTEX_OFFSET\s*"
    r"\+ dam_preview\.gpu_dirty_vertex_offset,\s*"
    r"renderer_vertex_flush_bytes\(\s*"
    r"dam_preview\.gpu_dirty_vertex_count,\s*"
    r"DAM_ROOM_VERTEX_COUNT\)\s*\)",
    re.S,
)
assert len(camera_flush.findall(source)) == 1

# The helper makes every byte conversion capacity-checked at runtime and the
# allocation itself is checked against size_t at compile time.
helper = function_body(source, "renderer_vertex_flush_bytes")
assert "assert(vertex_count <= vertex_capacity);" in helper
assert "assert(vertex_capacity <= TOTAL_VERTEX_COUNT);" in helper
assert "TOTAL_VERTEX_COUNT <= SIZE_MAX / sizeof(Vertex)" in source

print("3DS cache flush spans: full init, 12-vertex crosshair, exact Dam projection")

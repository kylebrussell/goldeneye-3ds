#!/usr/bin/env python3
"""Keep per-frame 3DS cache flushes bounded to vertices actually updated."""

from pathlib import Path
import re


def statement_body(source: str, pattern: str, name: str) -> str:
    match = re.search(pattern, source, re.S)
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


def function_body(source: str, name: str) -> str:
    return statement_body(source, rf"\b{name}\s*\([^;]*?\)\s*\{{", name)


repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text(encoding="utf-8")
renderer_init = function_body(source, "renderer_init")
renderer_draw = function_body(source, "renderer_draw")

# The initial upload still publishes every initialized vertex once.
assert "TOTAL_VERTEX_COUNT * sizeof(Vertex)" in renderer_init

# The canonical textured sight writes only its six-vertex rectangle.
assert "CROSSHAIR_VERTEX_COUNT == 6U" in source
assert re.search(
    r"GSPGPU_FlushDataCache\(\s*vertex_buffer,\s*"
    r"renderer_vertex_flush_bytes\(\s*CROSSHAIR_VERTEX_COUNT,\s*"
    r"CROSSHAIR_VERTEX_COUNT\)\s*\)",
    renderer_draw,
)
assert "TOTAL_VERTEX_COUNT * sizeof(Vertex)" not in renderer_draw

# Simulation publication and a slider-only visibility refresh each flush
# their dirty authored/dynamic range, after the corresponding upload succeeds.
camera_flush = re.compile(
    r"GSPGPU_FlushDataCache\(\s*"
    r"\(Vertex \*\)vertex_buffer \+ DAM_ROOM_VERTEX_OFFSET\s*"
    r"\+ dam_preview\.gpu_dirty_vertex_offset,\s*"
    r"renderer_vertex_flush_bytes\(\s*"
    r"dam_preview\.gpu_dirty_vertex_count,\s*"
    r"DAM_ROOM_VERTEX_COUNT\)\s*\)",
    re.S,
)
publication_paths = {
    "simulation publication": (
        r"if\s*\(\s*!multiplayer\.running\s*&&\s*dam_intro\.player\.initialized\s*&&\s*"
        r"dam_intro\.player\.publication_generation\s*!=\s*rendered_player_generation\s*\)\s*\{",
        r"if\s*\(\s*camera_updated\s*\)\s*\{"),
    "stereo slider publication": (
        r"if\s*\(\s*!multiplayer\.running\s*&&\s*dam_preview\.original_camera_ready\s*&&\s*"
        r"dam_preview\.renderer_visibility_slider\s*!=\s*stereo_slider\s*\)\s*\{",
        r"if\s*\(\s*stream_dam_renderer_camera\([^{}]*?\)\s*\)\s*\{"),
}


def assert_publication_flushes(source: str) -> None:
    assert len(camera_flush.findall(source)) == len(publication_paths)
    for name, (path_pattern, success_pattern) in publication_paths.items():
        path = statement_body(source, path_pattern, name)
        successful = statement_body(path, success_pattern, f"{name} successful upload")
        dirty = statement_body(successful,
            r"if\s*\(\s*dam_preview\.gpu_dirty_vertex_count\s*!=\s*0U\s*\)\s*\{",
            f"{name} nonempty dirty span")
        flushes = list(camera_flush.finditer(dirty))
        clears = list(re.finditer(r"dam_preview\.gpu_dirty_vertex_count\s*=\s*0U\s*;", dirty))
        assert len(flushes) == len(clears) == 1, name
        assert clears[0].start() > flushes[0].end(), f"{name} must clear after flushing"
        assert len(re.findall(r"GSPGPU_FlushDataCache\s*\(", path)) == 1, name
        assert "TOTAL_VERTEX_COUNT * sizeof(Vertex)" not in path, name


assert_publication_flushes(source)

# The selected multiplayer view owns a separate publication. Failed room or
# overlay uploads must return before its bounded dirty-span flush.
multiplayer_flush = re.compile(camera_flush.pattern.replace(r"dam_preview\.", "preview->"), re.S)


def assert_multiplayer_flush(source: str) -> None:
    capture = function_body(source, "runtime_multiplayer_view_capture")
    dirty = statement_body(capture,
        r"if\s*\(\s*preview->gpu_dirty_vertex_count\s*!=\s*0U\s*\)\s*\{",
        "multiplayer nonempty dirty span")
    flushes = list(multiplayer_flush.finditer(dirty))
    clears = list(re.finditer(r"preview->gpu_dirty_vertex_count\s*=\s*0U\s*;", dirty))
    assert len(flushes) == len(clears) == 1
    assert clears[0].start() > flushes[0].end()
    assert len(re.findall(r"GSPGPU_FlushDataCache\s*\(", dirty)) == 1
    for call in ("stream_dam_renderer_camera", "refresh_stage_live_overlays"):
        rejection = re.search(rf"if\s*\(\s*!{call}\([^;]*?\)\s*\)\s*return 0;", capture, re.S)
        assert rejection is not None and rejection.end() < capture.index(dirty)


assert_multiplayer_flush(source)
capture = function_body(source, "runtime_multiplayer_view_capture")
for original, replacement in (
    ("preview->gpu_dirty_vertex_count != 0U", "1"),
    ("DAM_ROOM_VERTEX_COUNT", "TOTAL_VERTEX_COUNT"),
    ("+ preview->gpu_dirty_vertex_offset", ""),
    ("preview->gpu_dirty_vertex_count = 0U;", "/* omitted clear */"),
):
    changed = capture.replace(original, replacement, 1)
    assert changed != capture
    try:
        assert_multiplayer_flush(source.replace(capture, changed, 1))
    except AssertionError:
        pass
    else:
        raise AssertionError(f"multiplayer regression not detected: {original}")

# Each path is independently required: duplicating one flush cannot substitute
# for the other. These mutations also prove the assertions detect regressions
# in its nonempty guard, bounded conversion, start offset and post-flush clear.
for name, (path_pattern, _) in publication_paths.items():
    path = statement_body(source, path_pattern, name)
    for original, replacement in (
        ("dam_preview.gpu_dirty_vertex_count != 0U", "1"),
        ("DAM_ROOM_VERTEX_COUNT", "TOTAL_VERTEX_COUNT"),
        ("+ dam_preview.gpu_dirty_vertex_offset", ""),
        ("dam_preview.gpu_dirty_vertex_count = 0U;", "/* omitted clear */"),
    ):
        changed_path = path.replace(original, replacement, 1)
        assert changed_path != path, (name, original)
        try:
            assert_publication_flushes(source.replace(path, changed_path, 1))
        except AssertionError:
            pass
        else:
            raise AssertionError(f"{name}: regression not detected: {original}")

# The helper makes every byte conversion capacity-checked at runtime and the
# allocation itself is checked against size_t at compile time.
helper = function_body(source, "renderer_vertex_flush_bytes")
assert "assert(vertex_count <= vertex_capacity);" in helper
assert "assert(vertex_capacity <= TOTAL_VERTEX_COUNT);" in helper
assert "TOTAL_VERTEX_COUNT <= SIZE_MAX / sizeof(Vertex)" in source

print("3DS cache flush spans: full init, 6-vertex sight, independently bounded solo/slider/multiplayer publication")

#!/usr/bin/env python3
"""Check the actual world upload helper against its previous float divisions."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()
vertex = re.search(r"typedef struct Vertex\s*\{.*?\} Vertex;", source, re.S).group(0)
table = re.search(r"static const float renderer_normalized_color\[.*?\};", source, re.S).group(0)
control = re.search(r"#ifndef GE_3DS_EXPERIMENT_COLOR_LOOKUP.*?#endif", source, re.S).group(0)
assert "#define GE_3DS_EXPERIMENT_COLOR_LOOKUP 0" in control
start = source.index("static void renderer_upload_world_vertices(")
end = source.index("static DVLB_s *shader_dvlb;", start)
helper = source[start:end]
upload_start = source.index("static bool upload_dam_gpu_world_scene_range(",
                            source.index("static bool upload_dam_gpu_world_scene_range(") + 1)
upload_end = source.index("static bool upload_dam_gpu_world_scene(", upload_start)
upload = source[upload_start:upload_end]
assert upload.count("renderer_upload_world_vertices(") == 1
assert "preview->source_vertices + vertex_offset" in upload
assert "destination + vertex_offset, vertex_count, map_texture_uv" in upload
assert "255.0f" not in upload
assert "if (!map_texture_uv) continue;" in upload
prefix = '''#include "ge_dam_room.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
''' + vertex + "\n" + control + "\n" + table + "\n" + helper
reference = r'''
static void previous_upload(const GeDamRoomWorldVertex *source,
    Vertex *destination, size_t vertex_count, bool map_texture_uv)
{
    for (size_t i = 0; i < vertex_count; ++i) {
        destination[i] = (Vertex){
            source[i].world[0], source[i].world[1], source[i].world[2],
            map_texture_uv ? source[i].processed.texture[0] : destination[i].u,
            map_texture_uv ? source[i].processed.texture[1] : destination[i].v,
            (float)source[i].processed.rgba[0] / 255.0f,
            (float)source[i].processed.rgba[1] / 255.0f,
            (float)source[i].processed.rgba[2] / 255.0f,
            (float)source[i].processed.rgba[3] / 255.0f,
        };
    }
}
'''
test = prefix + '''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
''' + reference + r'''
int main(void)
{
    size_t reciprocal_mismatches = 0U;
    for (size_t i = 0; i < 256U; ++i) {
        volatile float byte_value = (float)i;
        const float expected = byte_value / 255.0f;
        const float reciprocal = byte_value * (1.0f / 255.0f);
        assert(memcmp(&expected, &renderer_normalized_color[i], sizeof(float)) == 0);
        reciprocal_mismatches += memcmp(&expected, &reciprocal, sizeof(float)) != 0;
    }
    assert(reciprocal_mismatches != 0U);
    enum { COUNT = 2048 };
    GeDamRoomWorldVertex *vertices = calloc(COUNT, sizeof(*vertices));
    Vertex *reference = malloc(COUNT * sizeof(*reference));
    Vertex *actual = malloc(COUNT * sizeof(*actual));
    assert(vertices && reference && actual);
    memset(actual, 0xa5, COUNT * sizeof(*actual));
    memcpy(reference, actual, COUNT * sizeof(*actual));
    for (size_t i = 0; i < COUNT; ++i) {
        vertices[i].world[0] = (float)i - 1024.0f;
        vertices[i].world[1] = -(float)i;
        vertices[i].world[2] = (float)i * 2.0f;
        vertices[i].processed.texture[0] = (float)i / 32.0f;
        vertices[i].processed.texture[1] = -(float)i / 16.0f;
    }
    uint32_t random = 0x8e91382dU;
    size_t published = 0U;
    for (size_t pass = 0; pass < 2048U; ++pass) {
        for (size_t i = 0; i < COUNT; ++i) {
            /* Recolor every frame even when UVs/topology are retained. */
            for (size_t channel = 0; channel < 4U; ++channel)
                vertices[i].processed.rgba[channel] = (uint8_t)(i + pass + channel * 37U);
            vertices[i].world[0] += (pass & 1U) ? 0.25f : -0.25f;
        }
        random = random * 1664525U + 1013904223U;
        const size_t offset = pass < 2U ? 0U : random % COUNT;
        random = random * 1664525U + 1013904223U;
        const size_t count = pass < 2U ? COUNT : random % (COUNT - offset + 1U);
        const bool map = (pass % 3U) == 0U;
        previous_upload(vertices + offset, reference + offset, count, map);
        renderer_upload_world_vertices(vertices + offset, actual + offset, count, map);
        assert(memcmp(actual, reference, COUNT * sizeof(*actual)) == 0);
        published += count;
    }
    renderer_upload_world_vertices(vertices + COUNT, actual + COUNT, 0U, false);
    assert(memcmp(actual, reference, COUNT * sizeof(*actual)) == 0);
    free(actual); free(reference); free(vertices);
    printf("World upload colors: all 256 exact float values, %zu reciprocal mismatches avoided; %zu vertices in 2048 changing-color/range/UV passes match byte-for-byte\n",
        reciprocal_mismatches, published);
    return 0;
}
'''


def run(directory):
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "test.c").write_text(test)
    # Keep the actual helper/table available for the cross-compiler audit.
    (directory / "helper.c").write_text(prefix + '''
const float *color_table_for_test(void) { return renderer_normalized_color; }
void upload_for_test(const GeDamRoomWorldVertex *source, Vertex *destination,
    size_t count, bool map) { renderer_upload_world_vertices(source, destination, count, map); }
''')
    for enabled in (0, 1):
        subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                        f"-DGE_3DS_EXPERIMENT_COLOR_LOOKUP={enabled}",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-I", str(repo / "port/include"), str(directory / "test.c"),
                        "-o", str(directory / "test")], check=True)
        subprocess.run([str(directory / "test")], check=True)


if len(sys.argv) == 2:
    run(Path(sys.argv[1]))
else:
    with tempfile.TemporaryDirectory(prefix="ge-renderer-colors-") as temporary:
        run(Path(temporary))

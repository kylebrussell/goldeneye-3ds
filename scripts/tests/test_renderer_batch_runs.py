#!/usr/bin/env python3
"""Compare the actual retained weapon draw plan with the prior merge walk."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()


def function(name):
    match = re.search(r"static (?:bool|void) " + name + r"\([^;]*?\)\s*\{", source, re.S)
    start = match.start()
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


update = source.split("static bool update_first_person_scene(", 1)[1].split(
    "static void close_first_person_scene(", 1)[0]
assert update.index("runtime->uv_ready = false;") < update.index("renderer_prepare_batch_runs(")
assert update.index("if (!runtime->uv_ready) {") < update.index("renderer_prepare_batch_runs(")
assert "next = first_person->batch_run_ends[i];" in source

test = r'''
#include "ge_dam_room.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FIRST_PERSON_BATCH_CAPACITY 419U
''' + "\n".join(function(name) for name in (
    "dam_batch_materials_compatible", "dam_batches_compatible", "renderer_prepare_batch_runs")) + r'''
int main(void) {
    GeDamRoomDrawBatch batches[FIRST_PERSON_BATCH_CAPACITY] = {0};
    uint16_t ends[FIRST_PERSON_BATCH_CAPACITY] = {0};
    uint32_t random = 0x957221U;
    size_t tested = 0;
    for(size_t frame = 0; frame < 2048; ++frame) {
        const size_t count = frame % (FIRST_PERSON_BATCH_CAPACITY + 1U);
        size_t vertex = 0;
        for(size_t i = 0; i < count; ++i) {
            random = random * 1664525U + 1013904223U;
            if(i) batches[i] = batches[i - 1U];
            else memset(&batches[i], 0, sizeof(batches[i]));
            batches[i].first_vertex = vertex;
            batches[i].vertex_count = (random % 6U) * 3U;
            batches[i].room_id = random % 256U; /* irrelevant to hand merge */
            if(random % 7U == 0) batches[i].material.primitive_color.red ^= 1U;
            if(random % 11U == 0) batches[i].coordinate_space ^= 1U;
            if(random % 13U == 0) batches[i].texture.texture_id ^= 1U;
            if(random % 17U == 0) batches[i].texture_valid ^= 1U;
            if(random % 19U == 0) batches[i].first_vertex += 3U;
            vertex = batches[i].first_vertex + batches[i].vertex_count;
        }
        renderer_prepare_batch_runs(batches, count, ends);
        for(size_t i = 0; i < count; ++i) {
            size_t next = i + 1U, vertices = batches[i].vertex_count;
            while(next < count && dam_batches_compatible(&batches[next - 1U], &batches[next]))
                vertices += batches[next++].vertex_count;
            assert(ends[i] == next);
            assert(vertices == batches[next - 1U].first_vertex
                + batches[next - 1U].vertex_count - batches[i].first_vertex);
            ++tested;
        }
    }
    printf("Retained draw runs: %zu starts across 2048 layouts match prior merge order/ranges\n", tested);
}
'''
with tempfile.TemporaryDirectory(prefix="ge-draw-runs-") as temporary:
    directory = Path(temporary)
    (directory / "test.c").write_text(test)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I", str(repo / "port/include"), str(directory / "test.c"),
                    "-o", str(directory / "test")], check=True)
    subprocess.run([str(directory / "test")], check=True)

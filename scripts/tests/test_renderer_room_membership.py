#!/usr/bin/env python3
"""Verify renderer room membership and matrix-bank owner reuse exactly."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
renderer = (repo / "platform/3ds/source/main.c").read_text()
model = (repo / "port/src/ge_original_model_scene.c").read_text()


def function(source, name):
    match = re.search(rf"static\s+[\w\s*]+\b{name}\s*\([^;]*?\)\s*\{{", source)
    assert match, name
    depth = 0
    for index in range(source.index("{", match.start()), len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if depth == 0:
            return source[match.start():index + 1]
    raise AssertionError(name)


room_type = re.search(
    r"typedef struct RuntimeRendererRoomVisibility\s*\{.*?\} RuntimeRendererRoomVisibility;",
    renderer, re.S).group(0)
world_begin = renderer.index("static void renderer_draw(")
world = renderer[world_begin:]
world_loop = world.index("for (i = 0U; i < draw_batch_count;)")
world_end = world.index("phase_ticks[2] = svcGetSystemTick();", world_loop)
assert world[:world_end].count("renderer_prepare_room_visibility(") == 1
assert world.index("renderer_prepare_room_visibility(") < world_loop
assert "dam_visibility_contains_room(" not in world[world_loop:world_end]
assert world[world_loop:world_end].count("renderer_room_visible(") == 2
assert "qsort(" not in world[world_loop:world_end]

test = r'''
#include "ge_original_bg_visibility.h"
#include "ge_original_model_scene.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
''' + room_type + "\n" + function(renderer, "renderer_prepare_room_visibility") + "\n" + function(renderer, "renderer_room_visible") + "\n" + function(model, "cache_matrix_bank_matches") + r'''
static size_t bank_comparisons;
static int counted_bank_matches(const GeOriginalModelSceneInput *left,
    const GeOriginalModelSceneInput *right)
{
    ++bank_comparisons;
    return cache_matrix_bank_matches(left, right);
}
#define cache_matrix_bank_matches counted_bank_matches
''' + function(model, "cache_find_matrix_bank_owner") + r'''
#undef cache_matrix_bank_matches
static bool reference_visible(const GeOriginalBgVisibilityResult *result,
    bool ready, uint32_t room)
{
    if (!ready) return true;
    for (size_t i = 0; i < result->room_count; ++i)
        if (result->rooms[i].room == room) return true;
    return false;
}
int main(void)
{
    uint32_t random = 0x9326bc01U;
    RuntimeRendererRoomVisibility membership;
    GeOriginalBgVisibilityResult result = {0};
    for (size_t sample = 0; sample < 10000U; ++sample) {
        random = random * 1664525U + 1013904223U;
        result.room_count = sample < 256U ? 1U
            : random % (GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE + 1U);
        for (size_t i = 0; i < result.room_count; ++i) {
            random = random * 1664525U + 1013904223U;
            result.rooms[i].room = sample < 256U ? (uint8_t)sample
                : (uint8_t)(random >> 16U);
        }
        for (size_t ready = 0; ready < 2U; ++ready) {
            memset(&membership, 0xa5, sizeof(membership));
            renderer_prepare_room_visibility(&membership, &result, ready != 0U);
            for (uint32_t room = 0; room < 512U; ++room)
                assert(renderer_room_visible(&membership, room)
                    == reference_visible(&result, ready != 0U, room));
            assert(renderer_room_visible(&membership, UINT32_MAX) == (ready == 0U));
        }
    }
    result.room_count = 2U;
    result.rooms[0].room = result.rooms[1].room = 255U;
    renderer_prepare_room_visibility(&membership, &result, true);
    result.rooms[0].room = result.rooms[1].room = 0U;
    assert(renderer_room_visible(&membership, 255U));
    assert(!renderer_room_visible(&membership, 0U));
    renderer_prepare_room_visibility(&membership, &result, true);
    assert(!renderer_room_visible(&membership, 255U));
    assert(renderer_room_visible(&membership, 0U));
    result.room_count = GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE + 1U;
    renderer_prepare_room_visibility(&membership, &result, true);
    assert(renderer_room_visible(&membership, UINT32_MAX));
    renderer_prepare_room_visibility(&membership, NULL, true);
    assert(renderer_room_visible(&membership, UINT32_MAX));
    puts("Renderer membership: 10,240,000 exact room queries; snapshot/fail-open checks passed");
    GeOriginalModelSceneInput inputs[256] = {{0}};
    float banks[32][2][4][4] = {{{{0}}}};
    uint64_t owners[256];
    for (size_t pass = 0U; pass < 1024U; ++pass) {
        size_t reference_comparisons = 0U;
        bank_comparisons = 0U;
        memset(owners, 0xa5, sizeof(owners));
        for (size_t i = 0U; i < 256U; ++i) {
            random = random * 1664525U + 1013904223U;
            const size_t bank = pass % 3U == 0U ? i / 8U
                : pass % 3U == 1U ? i % 32U : random % 32U;
            inputs[i].segment3_matrices = pass % 4U == 0U ? NULL
                : (const float (*)[4][4])banks[bank];
            inputs[i].segment3_matrix_count = (random >> 24U) % 3U;
            if (pass % 2U == 0U) inputs[i].segment3_matrix_count = 2U;
            size_t expected = 0U;
            for (; expected < i; ++expected) {
                ++reference_comparisons;
                if (inputs[i].segment3_matrices == inputs[expected].segment3_matrices
                        && inputs[i].segment3_matrix_count == inputs[expected].segment3_matrix_count)
                    break;
            }
            assert(cache_find_matrix_bank_owner(inputs, i, owners) == expected);
            owners[i] = expected < i ? (uint64_t)expected : UINT64_MAX;
        }
        if (pass == 6U) {
            assert(reference_comparisons == 31968U && bank_comparisons == 4223U);
            printf("Grouped matrix banks: %zu -> %zu comparisons, identical earliest owners\n",
                reference_comparisons, bank_comparisons);
        }
    }
    puts("Renderer matrix-bank ownership: 262,144 exact first-owner comparisons, consecutive/nonconsecutive/null/count-change cases passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="ge-renderer-room-membership-") as temporary:
    directory = Path(temporary)
    source = directory / "test.c"
    binary = directory / "test"
    source.write_text(test)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I", str(repo / "port/include"), str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

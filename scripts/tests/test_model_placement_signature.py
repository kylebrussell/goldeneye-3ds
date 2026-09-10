#!/usr/bin/env python3
"""Differential placement-prefix and complete publication signatures."""

from pathlib import Path
import subprocess
import tempfile


REPO = Path(__file__).resolve().parents[2]
SOURCE = (REPO / "port/src/ge_original_model_scene.c").read_text()


def function(name):
    start = SOURCE.index(name + "(")
    start = SOURCE.rfind("static ", 0, start)
    brace = SOURCE.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[start:end]


CODE = r'''
#include "ge_original_model_scene.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t hash_bytes;
'''
for name in (
    "add_size", "write_u16_be", "encode_override_vertex",
    "cache_hash_u64", "cache_reserve_matrices", "cache_profile_now",
    "cache_quantize_matrix_element", "cache_hash_float",
    "cache_placement_hash", "cache_matrix_bank_matches",
    "cache_find_matrix_bank_owner", "cache_prepare_publication_matrices",
):
    body = function(name)
    if name == "cache_hash_u64":
        body = body.replace("size_t byte;", "size_t byte; hash_bytes += 8U;")
    CODE += body + "\n"

CODE += r'''
static uint32_t rng = UINT32_C(983124);
static uint32_t random_word(void)
{
    rng = rng * UINT32_C(1664525) + UINT32_C(1013904223);
    return rng;
}

/* Independent byte serialization of the pre-existing FNV signature format:
 * each numeric field occupies eight little-endian bytes, including floats. */
static uint64_t reference_word(uint64_t seed, uint64_t word)
{
    unsigned char bytes[8];
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = (unsigned char)(word & 255U);
        word /= 256U;
    }
    for (size_t i = 0; i < sizeof(bytes); ++i)
        seed = (seed ^ bytes[i]) * UINT64_C(1099511628211);
    return seed;
}

static uint64_t reference_float(uint64_t seed, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return reference_word(seed, bits);
}

static uint64_t reference_placement(const GeOriginalModelSceneInput *input)
{
    uint64_t seed = reference_word(UINT64_C(1469598103934665603), input->room_id);
    for (size_t i = 0; i < 3; ++i)
        seed = reference_float(seed, input->position[i]);
    for (size_t i = 0; i < 4; ++i)
        for (size_t j = 0; j < 4; ++j)
            seed = reference_float(seed, input->matrix[i][j]);
    return seed;
}

static int read_vertex(const void *context, size_t index, GeGbiVertex *vertex)
{
    *vertex = ((const GeGbiVertex *)context)[index];
    return 1;
}

static uint64_t reference_vertex(uint64_t hash, const GeGbiVertex *vertex)
{
    const uint16_t words[] = {(uint16_t)vertex->x, (uint16_t)vertex->y,
        (uint16_t)vertex->z, vertex->flag, (uint16_t)vertex->texture_s,
        (uint16_t)vertex->texture_t};
    unsigned char bytes[16];
    for (size_t i = 0; i < 6; ++i) {
        bytes[i * 2] = (unsigned char)(words[i] / 256U);
        bytes[i * 2 + 1] = (unsigned char)(words[i] % 256U);
    }
    bytes[12] = vertex->red; bytes[13] = vertex->green;
    bytes[14] = vertex->blue; bytes[15] = vertex->alpha;
    for (size_t i = 0; i < 16; ++i)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

int main(void)
{
    GeOriginalModelSceneInput inputs[65] = {{0}};
    uint64_t previous_hash = 0;
    size_t reused = 0;
    size_t rebuilt = 0;
    /* Arbitrary outer-transform bits include NaN payloads and signed zero.
     * Test every single differing byte as well as same/different room IDs. */
    for (size_t run = 0; run < 100000; ++run) {
        inputs[0] = inputs[1];
        if (run % 4 == 0) {
            unsigned char *bytes = (unsigned char *)inputs[1].matrix;
            bytes[run % sizeof(inputs[1].matrix)] ^= 1U;
        } else if (run % 4 == 1) {
            uint32_t bits = random_word();
            memcpy(&inputs[1].position[run % 3], &bits, sizeof(bits));
        } else if (run % 4 == 2) {
            inputs[1].room_id = random_word();
        }
        const int identical = inputs[0].room_id == inputs[1].room_id
            && memcmp(inputs[0].position, inputs[1].position, sizeof(inputs[0].position)) == 0
            && memcmp(inputs[0].matrix, inputs[1].matrix, sizeof(inputs[0].matrix)) == 0;
        const GeOriginalModelSceneInput *prior = run % 31 ? &inputs[0] : NULL;
        hash_bytes = 0;
        const uint64_t hash = cache_placement_hash(&inputs[1], prior, previous_hash);
        assert(hash == reference_placement(&inputs[1]));
        if (prior && identical) {
            assert(hash_bytes == 0); ++reused;
        } else {
            assert(hash_bytes == 160); ++rebuilt;
        }
        previous_hash = hash;
    }
    for (size_t field = 0; field < 19; ++field) {
        memset(inputs, 0, sizeof(inputs));
        float negative_zero = -0.0f;
        if (field < 3) inputs[1].position[field] = negative_zero;
        else inputs[1].matrix[(field - 3) / 4][(field - 3) % 4] = negative_zero;
        hash_bytes = 0;
        assert(cache_placement_hash(&inputs[1], &inputs[0], reference_placement(&inputs[0]))
            == reference_placement(&inputs[1]));
        assert(hash_bytes == 160);
    }
    for (size_t byte = 0; byte < 76; ++byte) {
        memset(inputs, 0, sizeof(inputs));
        unsigned char *changed = byte < 12
            ? (unsigned char *)inputs[1].position + byte
            : (unsigned char *)inputs[1].matrix + byte - 12;
        *changed = 1U;
        hash_bytes = 0;
        assert(cache_placement_hash(&inputs[1], &inputs[0], reference_placement(&inputs[0]))
            == reference_placement(&inputs[1]));
        assert(hash_bytes == 160);
    }
    for (size_t bit = 0; bit < 32; ++bit) {
        memset(inputs, 0, sizeof(inputs));
        inputs[1].room_id = UINT32_C(1) << bit;
        hash_bytes = 0;
        assert(cache_placement_hash(&inputs[1], &inputs[0], reference_placement(&inputs[0]))
            == reference_placement(&inputs[1]));
        assert(hash_bytes == 160);
    }

    GeOriginalModelSceneCache cache = {0};
    size_t offsets[65];
    uint64_t matrix_hashes[65], signatures[65];
    cache.input_quantized_matrix_offsets = offsets;
    cache.input_quantized_matrix_hashes = matrix_hashes;
    cache.input_publication_signatures = signatures;
    size_t publications = 0;
    for (size_t run = 0; run < 3000; ++run) {
        float matrices[5][3][4][4];
        GeGbiVertex vertices[3] = {{0}};
        size_t count = run % 65;
        memset(inputs, 0, sizeof(inputs));
        for (size_t bank = 0; bank < 5; ++bank)
            for (size_t mat = 0; mat < 3; ++mat)
                for (size_t row = 0; row < 4; ++row)
                    for (size_t col = 0; col < 4; ++col)
                        matrices[bank][mat][row][col] = (float)(int32_t)random_word() / 131072.0f;
        for (size_t i = 0; i < 3; ++i) {
            vertices[i].x = (int16_t)random_word();
            vertices[i].texture_t = (int16_t)random_word();
            vertices[i].alpha = (uint8_t)random_word();
        }
        for (size_t i = 0; i < count; ++i) {
            if (i && i % 7) inputs[i] = inputs[i - 1];
            else {
                inputs[i].room_id = random_word();
                for (size_t axis = 0; axis < 4; ++axis) inputs[i].matrix[axis][axis] = 1;
            }
            inputs[i].segment3_matrices = i % 11 ? matrices[i % 5] : NULL;
            inputs[i].segment3_matrix_count = i % 3 + 1;
            if (i % 13 == 0) {
                inputs[i].segment4_vertices = vertices;
                inputs[i].segment4_vertex_count = 3;
                inputs[i].segment4_read_vertex = read_vertex;
            }
        }
        uint64_t signature, ticks;
        const uint64_t topology = random_word();
        assert(cache_prepare_publication_matrices(&cache, inputs, count, topology,
            &signature, &ticks) == GE_ORIGINAL_MODEL_SCENE_OK);
        assert(ticks == 0);
        uint64_t aggregate = reference_word(UINT64_C(1469598103934665603), topology);
        for (size_t i = 0; i < count; ++i) {
            uint64_t bank_hash = reference_word(UINT64_C(1469598103934665603),
                inputs[i].segment3_matrices ? inputs[i].segment3_matrix_count : 0);
            if (inputs[i].segment3_matrices)
                for (size_t mat = 0; mat < inputs[i].segment3_matrix_count; ++mat)
                    for (size_t row = 0; row < 4; ++row)
                        for (size_t col = 0; col < 4; ++col) {
                            const float value = inputs[i].segment3_matrices[mat][row][col];
                            const float quantized = (float)(trunc((double)value * 65536.0) / 65536.0);
                            assert(memcmp(&quantized, &cache.quantized_matrices[offsets[i] + mat][row][col], sizeof(float)) == 0);
                            bank_hash = reference_float(bank_hash, quantized);
                        }
            uint64_t expected = reference_word(reference_placement(&inputs[i]), bank_hash);
            for (size_t v = 0; v < inputs[i].segment4_vertex_count; ++v)
                expected = reference_vertex(expected, &vertices[v]);
            assert(matrix_hashes[i] == bank_hash);
            assert(signatures[i] == expected);
            aggregate = reference_word(aggregate, expected);
            ++publications;
        }
        assert(signature == aggregate);
    }
    free(cache.quantized_matrices);
    printf("Placement signatures: %zu reused, %zu rebuilt prefixes; %zu complete input signatures match scalar bytes\n",
        reused, rebuilt, publications);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="ge-placement-signature-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(CODE)
    for optimization in ("-O2", "-O3"):
        subprocess.run([
            "cc", "-std=c11", optimization, "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I" + str(REPO / "port/include"), str(path / "test.c"),
            "-lm", "-o", str(path / "test"),
        ], check=True)
        subprocess.run([str(path / "test")], check=True)

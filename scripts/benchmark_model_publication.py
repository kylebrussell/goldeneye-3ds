#!/usr/bin/env python3
"""Prepare a standalone, matched host signature-preparation microbenchmark.

Preparation never runs the benchmark. Coordinate a quiet timing window before
running the printed executable command. Synthetic input shapes are explicit;
these measurements cannot certify in-game or ARM frame rate.
"""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import shlex
import subprocess


REPO = Path(__file__).resolve().parents[1]
HELPERS = (
    "add_size", "write_u16_be", "encode_override_vertex", "cache_hash_u64",
    "cache_reserve_matrices", "cache_profile_now", "cache_quantize_matrix_element",
    "cache_hash_float", "cache_placement_hash", "cache_matrix_bank_matches",
    "cache_find_matrix_bank_owner", "cache_prepare_publication_matrices",
)


def extract(source, name):
    marker = name + "("
    if marker not in source:
        return ""
    start = source.rfind("static ", 0, source.index(marker))
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


DRIVER = r'''
#include "ge_original_model_scene.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern GeOriginalModelSceneStatus baseline_prepare(GeOriginalModelSceneCache *,
    const GeOriginalModelSceneInput *, size_t, uint64_t, uint64_t *, uint64_t *);
extern GeOriginalModelSceneStatus candidate_prepare(GeOriginalModelSceneCache *,
    const GeOriginalModelSceneInput *, size_t, uint64_t, uint64_t *, uint64_t *);
typedef GeOriginalModelSceneStatus (*Prepare)(GeOriginalModelSceneCache *,
    const GeOriginalModelSceneInput *, size_t, uint64_t, uint64_t *, uint64_t *);

enum { INPUTS = 192, BANKS = 8, BONES = 20, ITERATIONS = 2000, REPEATS = 7 };
static GeOriginalModelSceneInput inputs[INPUTS];
static float matrices[BANKS][BONES][4][4];
static GeOriginalModelSceneCache caches[2];
static size_t offsets[2][INPUTS];
static uint64_t bank_hashes[2][INPUTS], signatures[2][INPUTS];

static double run(Prepare prepare, unsigned variant, uint64_t *checksum)
{
    uint64_t result = 0, ticks = 0, sum = 0;
    clock_t start = clock();
    for (unsigned iteration = 0; iteration < ITERATIONS; ++iteration) {
        /* The same animated bone updates on both paths. All output signatures
         * participate in the checksum; this cannot collapse to an idle call. */
        for (size_t bank = 0; bank < BANKS; ++bank)
            matrices[bank][iteration % BONES][3][0] = (float)(iteration % 317);
        assert(prepare(&caches[variant], inputs, INPUTS,
            UINT64_C(0x1234567812345678), &result, &ticks) == GE_ORIGINAL_MODEL_SCENE_OK);
        sum ^= result + iteration;
    }
    clock_t end = clock();
    assert(start != (clock_t)-1 && end != (clock_t)-1 && end >= start);
    *checksum = sum;
    return (double)(end - start) * 1e6 / ((double)CLOCKS_PER_SEC * ITERATIONS);
}

static void reset_matrices(void)
{
    memset(matrices, 0, sizeof(matrices));
    for (size_t bank = 0; bank < BANKS; ++bank)
        for (size_t bone = 0; bone < BONES; ++bone)
            for (size_t axis = 0; axis < 4; ++axis)
                matrices[bank][bone][axis][axis] = 1.0f;
}

int main(void)
{
    for (size_t variant = 0; variant < 2; ++variant) {
        caches[variant].input_quantized_matrix_offsets = offsets[variant];
        caches[variant].input_quantized_matrix_hashes = bank_hashes[variant];
        caches[variant].input_publication_signatures = signatures[variant];
    }
    puts("{\"environment\":\"host synthetic signature preparation only\","
        "\"inputs\":192,\"banks\":8,\"bones_per_bank\":20,"
        "\"iterations\":2000,\"units\":\"microseconds per preparation\",\"samples\":[");
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        memset(inputs, 0, sizeof(inputs));
        for (size_t i = 0; i < INPUTS; ++i) {
            inputs[i].room_id = scenario == 0 ? 1U : scenario == 1
                ? (uint32_t)(i / 24 + 1) : (uint32_t)i;
            inputs[i].segment3_matrices = matrices[i / 24];
            inputs[i].segment3_matrix_count = BONES;
            for (size_t axis = 0; axis < 4; ++axis) inputs[i].matrix[axis][axis] = 1.0f;
        }
        for (unsigned repeat = 0; repeat < REPEATS; ++repeat) {
            double us[2]; uint64_t checksums[2];
            for (unsigned order = 0; order < 2; ++order) {
                unsigned variant = (order + repeat) % 2;
                Prepare prepare = variant ? candidate_prepare : baseline_prepare;
                reset_matrices();
                /* Declared warmup: one complete preparation allocates scratch.
                 * Cold allocation/load behavior is outside this microbenchmark. */
                uint64_t result, ticks;
                assert(prepare(&caches[variant], inputs, INPUTS, 0, &result, &ticks)
                    == GE_ORIGINAL_MODEL_SCENE_OK);
                us[variant] = run(prepare, variant, &checksums[variant]);
            }
            assert(checksums[0] == checksums[1]);
            assert(memcmp(signatures[0], signatures[1], sizeof(signatures[0])) == 0);
            printf("%s{\"scenario\":\"%s\",\"repeat\":%u,\"baseline_us\":%.6f,"
                "\"candidate_us\":%.6f,\"checksum\":\"%016llx\"}",
                scenario || repeat ? ",\n" : "", scenario == 0 ? "same_room"
                    : scenario == 1 ? "24_parts_per_room" : "unique_room_control",
                repeat, us[0], us[1], (unsigned long long)checksums[0]);
        }
    }
    puts("\n]}");
    free(caches[0].quantized_matrices); free(caches[1].quantized_matrices);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-ref", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    # Existing timing results must remain tied to their original executable
    # and source receipt; prepare a new directory for each experiment.
    output.mkdir(parents=True, exist_ok=False)
    sources = {
        "baseline": subprocess.check_output([
            "git", "show", args.baseline_ref + ":port/src/ge_original_model_scene.c",
        ], cwd=REPO).decode(),
        "candidate": (REPO / "port/src/ge_original_model_scene.c").read_text(),
    }
    flags = ["-std=c11", "-O3", "-Wall", "-Wextra", "-Werror",
             "-I" + str(REPO / "port/include")]
    objects = []
    for variant, source in sources.items():
        (output / (variant + "-source.c")).write_text(source)
        code = '#include "ge_original_model_scene.h"\n#include <math.h>\n#include <stdlib.h>\n#include <string.h>\n'
        code += "\n".join(extract(source, name) for name in HELPERS)
        code = code.replace("static GeOriginalModelSceneStatus cache_prepare_publication_matrices(",
                            "GeOriginalModelSceneStatus " + variant + "_prepare(")
        path = output / (variant + ".c")
        path.write_text(code)
        obj = output / (variant + ".o")
        subprocess.run(["cc", *flags, "-c", str(path), "-o", str(obj)], check=True)
        objects.append(str(obj))
    (output / "driver.c").write_text(DRIVER)
    executable = output / "benchmark"
    subprocess.run(["cc", *flags, str(output / "driver.c"), *objects,
                    "-lm", "-o", str(executable)], check=True)
    provenance = {
        "baseline_ref": args.baseline_ref,
        "source_sha256": {name: hashlib.sha256(source.encode()).hexdigest()
                          for name, source in sources.items()},
        "driver_sha256": hashlib.sha256(DRIVER.encode()).hexdigest(),
        "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "platform": platform.platform(), "machine": platform.machine(),
        "compiler": subprocess.check_output(["cc", "--version"]).decode(),
        "flags": flags, "warmup": "one preparation per variant per sample",
        "limitation": "Synthetic host signature cost; excludes geometry, GPU, audio, loading and frame pacing.",
    }
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print("Prepared only. After coordinator grants a quiet timing window, run:")
    print(shlex.quote(str(executable)) + " > " + shlex.quote(str(output / "timing.json")))


if __name__ == "__main__":
    main()

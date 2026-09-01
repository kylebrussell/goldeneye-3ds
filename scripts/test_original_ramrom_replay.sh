#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir="${repo_dir}/build/host-tests/original-ramrom-replay"
mkdir -p "${test_dir}"
dead_strip=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

cc -std=c11 -Wall -Wextra -Werror \
    -Wno-logical-not-parentheses -Wno-empty-body \
    -Wno-unused-variable -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DVERSION_US -I"${repo_dir}/port/include" -I"${repo_dir}/src" \
    "${repo_dir}/port/tests/test_ge_original_ramrom_replay.c" \
    "${repo_dir}/port/src/ge_original_ramrom_replay.c" \
    "${repo_dir}/port/src/ge_original_input.c" \
    "${repo_dir}/port/src/ge_libultra.c" \
    "${repo_dir}/src/joy.c" \
    "${dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_ramrom_replay"

"${test_dir}/test_ge_original_ramrom_replay" \
    "${repo_dir}/assets/ramrom"

#!/usr/bin/env bash
set -euo pipefail
repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir="${repo_dir}/build/host-tests/original-gunbarrel"
mkdir -p "${test_dir}"
python3 "${repo_dir}/scripts/extract_original_gunbarrel_contract.py" \
    "${repo_dir}" \
    "${repo_dir}/port/src/ge_original_gunbarrel_contract.inc" --check
cc -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -D_LANGUAGE_C -DVERSION_US \
    -DGE_PORT_GUNBARREL_HOLE_SLICE \
    -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/libultra/gu" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_original_gunbarrel.c" \
    "${repo_dir}/src/game/title3.c" \
    "${repo_dir}/src/libultra/gu/sins.c" \
    "${repo_dir}/port/tests/test_ge_original_gunbarrel.c" \
    -o "${test_dir}/test_ge_original_gunbarrel"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
    "${test_dir}/test_ge_original_gunbarrel"

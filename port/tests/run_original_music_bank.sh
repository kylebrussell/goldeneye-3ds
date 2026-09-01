#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf "${test_dir}"' EXIT
compiler="${CC:-cc}"

"${compiler}" -std=c11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -D_LANGUAGE_C \
    -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/include/PR" -idirafter "${repo_dir}/include" \
    "${repo_dir}/port/src/ge_original_music_bank.c" \
    "${repo_dir}/port/tests/test_original_music_bank.c" \
    -o "${test_dir}/test_original_music_bank"

"${test_dir}/test_original_music_bank" \
    "${repo_dir}/assets/music/instruments.ctl" \
    "${repo_dir}/assets/music/instruments.tbl"

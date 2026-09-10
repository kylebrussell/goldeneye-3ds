#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-probe-difficulty.XXXXXX")
trap 'rm -rf "${test_dir}"' EXIT
cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${repo_dir}/platform/3ds/include" \
    "${repo_dir}/platform/3ds/source/ge_3ds_probe_difficulty.c" \
    "${repo_dir}/platform/3ds/tests/test_ge_3ds_probe_difficulty.c" \
    -o "${test_dir}/test_ge_3ds_probe_difficulty"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "${test_dir}/test_ge_3ds_probe_difficulty" "${test_dir}/difficulty.cfg" "${test_dir}"

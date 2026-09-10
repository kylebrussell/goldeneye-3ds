#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-mp-state.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -O2 -g \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/tests/test_ge_mp_state.c" "${repo_dir}/port/src/ge_mp_state.c" \
    -o "${test_dir}/test_ge_mp_state"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "${test_dir}/test_ge_mp_state" "${test_dir}/canonical.bin"
python3 "${repo_dir}/scripts/tests/test_mp_state_wire.py" "${test_dir}/canonical.bin"

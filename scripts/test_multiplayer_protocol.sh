#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-mp-protocol.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/tests/test_ge_multiplayer_protocol.c" \
    "${repo_dir}/port/src/ge_multiplayer_protocol.c" \
    -o "${test_dir}/test_ge_multiplayer_protocol"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "${test_dir}/test_ge_multiplayer_protocol"

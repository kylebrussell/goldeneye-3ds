#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
flags=(-O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-variable
    -Wno-unused-parameter -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
    -Wno-sign-compare -Wno-return-type -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -D_LANGUAGE_C
    -I"$repo_dir" -I"$repo_dir/port/include" -I"$repo_dir/src/libultra"
    -I"$repo_dir/include/PR" -idirafter "$repo_dir/include")
dead_strip=-Wl,--gc-sections
if [[ $(uname -s) == Darwin ]]; then dead_strip=-Wl,-dead_strip; fi
for mode in native n64; do
    extra=(-UGE_PORT_LIBAUDIO_NATIVE)
    if [[ $mode == native ]]; then extra=(-DGE_PORT_LIBAUDIO_NATIVE); fi
    "${CC:-cc}" "${flags[@]}" "${extra[@]}" \
        "$repo_dir/src/libultra/audio/load.c" \
        "$repo_dir/port/tests/test_original_audio_loader.c" \
        "$dead_strip" -o "$test_dir/$mode"
    "$test_dir/$mode"
done

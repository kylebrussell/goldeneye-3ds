#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-3ds-uv.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
dead_strip=-Wl,--gc-sections
if [[ $(uname -s) == Darwin ]]; then dead_strip=-Wl,-dead_strip; fi
sources=(
    "$repo_dir/platform/3ds/tests/test_ge_3ds_texture_uv.c"
    "$repo_dir/platform/3ds/source/ge_3ds_scene_texture.c"
    "$repo_dir/port/src/ge_texture_uv.c"
)
options=(-std=c11 -Wall -Wextra -Werror -pedantic
    -ffunction-sections -fdata-sections
    -I"$repo_dir/platform/3ds/tests/include"
    -I"$repo_dir/platform/3ds/include" -I"$repo_dir/port/include")
cc "${options[@]}" -O2 -fsanitize=address,undefined -fno-omit-frame-pointer \
    "${sources[@]}" "$dead_strip" -lm -o "$test_dir/uv"
"$test_dir/uv"
if [[ ${GE_TEXTURE_UV_BENCH:-0} == 1 ]]; then
    # LTO lets the mock Tex3DS corner accessors inline, like the real SDK;
    # otherwise separate mock calls artificially inflate the old path.
    cc "${options[@]}" -O3 -flto -fshort-enums -DGE_TEXTURE_UV_BENCH \
        "${sources[@]}" "$dead_strip" -lm -o "$test_dir/bench"
    "$test_dir/bench"
fi

#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-guard-scene-textures.XXXXXX")
trap 'rm -rf "${test_dir}"' EXIT

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then dead_strip=(-Wl,-dead_strip); fi

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_original_model_scene.c" \
    "${repo_dir}/port/tests/test_ge_original_guard_scene_textures.c" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${test_dir}/test_guard_scene_textures"

"${test_dir}/test_guard_scene_textures"
echo "dynamic guard scene texture publication preserves authored batch ids"

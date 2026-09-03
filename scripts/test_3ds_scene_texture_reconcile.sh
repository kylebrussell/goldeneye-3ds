#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-3ds-texture-reconcile.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

dead_strip=-Wl,--gc-sections
if [ "$(uname -s)" = Darwin ]; then dead_strip=-Wl,-dead_strip; fi

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -ffunction-sections -fdata-sections \
    -I"${repo_dir}/platform/3ds/tests/include" \
    -I"${repo_dir}/platform/3ds/include" \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/platform/3ds/tests/test_ge_3ds_scene_texture_reconcile.c" \
    "${repo_dir}/platform/3ds/source/ge_3ds_scene_texture.c" \
    "${repo_dir}/port/src/ge_texture_uv.c" \
    "${repo_dir}/port/src/ge_dam_dynamic_scene.c" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" "$dead_strip" \
    -o "${test_dir}/test_ge_3ds_scene_texture_reconcile"

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "${test_dir}/test_ge_3ds_scene_texture_reconcile"

if [ "${GE_SCENE_TEXTURE_LOOKUP_BENCH:-0}" = 1 ]; then
    cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 \
        -ffunction-sections -fdata-sections \
        -DGE_SCENE_TEXTURE_LOOKUP_BENCH \
        -I"${repo_dir}/platform/3ds/tests/include" \
        -I"${repo_dir}/platform/3ds/include" \
        -I"${repo_dir}/port/include" \
        "${repo_dir}/platform/3ds/tests/test_ge_3ds_scene_texture_reconcile.c" \
        "${repo_dir}/platform/3ds/source/ge_3ds_scene_texture.c" \
        "${repo_dir}/port/src/ge_texture_uv.c" \
        "${repo_dir}/port/src/ge_dam_dynamic_scene.c" \
        "${repo_dir}/port/src/ge_dam_preload_queue.c" "$dead_strip" \
        -o "${test_dir}/test_ge_3ds_scene_texture_lookup_bench"
    "${test_dir}/test_ge_3ds_scene_texture_lookup_bench"
fi

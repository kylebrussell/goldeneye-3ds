#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/multiplayer-regen"
mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_regen_slice.py" "${repo_dir}" \
    "${build_dir}/regen.c" --fixture "${build_dir}/pickup.c"
cc -std=gnu11 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src" -I"${repo_dir}/src/game" -I"${repo_dir}" \
    -I"${repo_dir}/include/PR" -idirafter "${repo_dir}/include" -Wno-everything \
    "${repo_dir}/port/src/ge_original_multiplayer_regen.c" \
    "${repo_dir}/port/tests/test_ge_original_multiplayer_regen.c" \
    "${build_dir}/regen.c" "${build_dir}/pickup.c" -lm -o "${build_dir}/test_regen"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${build_dir}/test_regen"

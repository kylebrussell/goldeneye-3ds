#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/multiplayer-runtime"
mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_runtime_slice.py" "${repo_dir}" "${build_dir}/interaction.c"
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-comment -Wno-missing-braces -Wno-unused-variable \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions \
    -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_original_multiplayer_runtime.c" \
    "${repo_dir}/port/tests/test_ge_original_multiplayer_runtime.c" \
    "${build_dir}/interaction.c" \
    -o "${build_dir}/test_runtime"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test_runtime"

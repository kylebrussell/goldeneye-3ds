#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"${repo_dir}/build/host-tests/mp-fog"}
mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_fog_slice.py" "${repo_dir}" "${build_dir}/slice.c"
python3 "${repo_dir}/scripts/extract_stage_environment_tables.py" "${repo_dir}" "${build_dir}/tables.c"
for abi in native short-enum; do
    abi_flags=(-fno-short-enums)
    if [[ ${abi} == short-enum ]]; then abi_flags=(-fshort-enums); fi
    cc "${abi_flags[@]}" -std=gnu11 -Wall -Wextra -Werror \
        -Wno-comment -Wno-unused-parameter -Wno-pragma-pack -Wno-missing-braces \
        -Wno-incompatible-pointer-types -Wno-sign-compare \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
        -DAIPARSE -DVERSION_US -DPLAYERFLAG=int -DGE_PORT_MS_INHERITS -fms-extensions \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/src" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        "${build_dir}/slice.c" "${build_dir}/tables.c" \
        "${repo_dir}/port/src/ge_original_multiplayer_fog.c" \
        "${repo_dir}/port/src/ge_original_stage_environment.c" \
        "${repo_dir}/port/src/ge_stage_assets.c" \
        "${repo_dir}/port/tests/test_ge_original_multiplayer_fog.c" \
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -lm \
        -o "${build_dir}/test-${abi}"
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test-${abi}"
done

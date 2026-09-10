#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-state-fog.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_multiplayer_fog_slice.py" "${repo_dir}" "${build_dir}/slice.c"
for abi in native short; do
    enum=(-fno-short-enums);if [[ ${abi} == short ]];then enum=(-fshort-enums);fi
    cc "${enum[@]}" -std=gnu11 -O1 -g -Wall -Wextra -Werror \
        -Wno-comment -Wno-unused-parameter -Wno-pragma-pack -Wno-missing-braces \
        -Wno-incompatible-pointer-types -Wno-sign-compare \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
        -DAIPARSE -DVERSION_US -DPLAYERFLAG=int -DGE_PORT_MS_INHERITS -fms-extensions \
        -I"${repo_dir}/port/include" -idirafter "${repo_dir}/src" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        "${build_dir}/slice.c" "${repo_dir}/port/src/ge_original_multiplayer_fog.c" \
        "${repo_dir}/port/src/ge_mp_state.c" "${repo_dir}/port/src/ge_mp_state_fog.c" \
        "${repo_dir}/port/src/ge_stage_assets.c" \
        "${repo_dir}/port/tests/test_ge_mp_state_fog.c" \
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -lm \
        -o "${build_dir}/test-${abi}"
    for count in 2 4;do
        ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
            "${build_dir}/test-${abi}" "${count}" > "${build_dir}/${abi}-${count}.txt"
        cat "${build_dir}/${abi}-${count}.txt"
    done
done
cmp "${build_dir}/native-2.txt" "${build_dir}/short-2.txt"
cmp "${build_dir}/native-4.txt" "${build_dir}/short-4.txt"

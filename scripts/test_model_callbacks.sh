#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-model-callbacks.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_player_gait_model_slice.py" \
    "${repo_dir}/src/game/model.c" "${repo_dir}/src/game/initBondDATAdefaults.c" \
    "${test_dir}/model.c"
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
flags=(-std=gnu11 -O1 -g -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -fno-sanitize-recover=all
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE
    -DGE_PORT_MODEL_HOST_RWDATA_ABI -Wno-everything
    -I"${repo_dir}" -I"${repo_dir}/port/include"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
    -idirafter "${repo_dir}/src")
for abi in native short; do
    extra=(-fno-short-enums); if [[ ${abi} == short ]]; then extra=(-fshort-enums); fi
    cc "${flags[@]}" "${extra[@]}" "${test_dir}/model.c" \
        "${repo_dir}/port/src/ge_original_player_gait.c" \
        "${repo_dir}/port/src/ge_original_model_callbacks.c" \
        "${repo_dir}/port/tests/test_ge_original_model_callbacks.c" \
        "${strip[@]}" -lm -o "${test_dir}/test-${abi}"
    "${test_dir}/test-${abi}"
done

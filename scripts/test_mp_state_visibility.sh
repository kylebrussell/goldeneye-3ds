#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${GE_VISIBILITY_TEST_DIR:-"${repo_dir}/build/host-tests/mp-visibility-state"}
mkdir -p "${build_dir}"
exporter_source=${GE_VISIBILITY_EXPORTER_SOURCE:-"${repo_dir}/port/src/ge_mp_state_visibility.c"}
runtime_source=${GE_VISIBILITY_RUNTIME_SOURCE:-"${repo_dir}/port/src/ge_dam_visibility_runtime.c"}
adapter_source=${GE_VISIBILITY_ADAPTER_SOURCE:-"${repo_dir}/port/src/ge_original_bg_visibility.c"}
python3 "${repo_dir}/scripts/extract_bg_visibility_slice.py" \
    "${repo_dir}/src/game/bg.c" "${build_dir}/ge_original_bg_visibility_slice.c"
for abi in native short-enum; do
    abi_flags=(-fno-short-enums)
    if [[ ${abi} == short-enum ]]; then abi_flags=(-fshort-enums); fi
    cc "${abi_flags[@]}" -std=gnu11 -Wall -Wextra -Werror \
        -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body \
        -Wno-incompatible-pointer-types -Wno-return-type \
        -DVERSION_US -DLEFTOVERDEBUG -DGE_PORT_HOST_ABI \
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
        -I "${repo_dir}/port/include" -I "${repo_dir}/port/src" -I "${build_dir}" \
        "${repo_dir}/port/src/ge_mp_state.c" "${exporter_source}" \
        "${adapter_source}" \
        "${repo_dir}/port/src/ge_renderer_bg_visibility.c" \
        "${runtime_source}" \
        "${repo_dir}/port/src/ge_asset_pack.c" "${repo_dir}/port/src/ge_stage_assets.c" \
        "${build_dir}/ge_original_bg_visibility_slice.c" \
        "${repo_dir}/port/tests/test_ge_mp_state_visibility.c" \
        -lm -o "${build_dir}/test-${abi}"
    "${build_dir}/test-${abi}" "${build_dir}/${abi}.sha256" "$@"
done
cmp "${build_dir}/native.sha256" "${build_dir}/short-enum.sha256"

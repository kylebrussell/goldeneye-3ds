#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-mp-state-bg.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_bg_geometry_slice.py" "${repo_dir}" "${build_dir}/slice.c"
strip=(-Wl,--gc-sections)
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); fi
for abi in native short-enum; do
    abi_flags=(-fno-short-enums)
    if [[ ${abi} == short-enum ]]; then abi_flags=(-fshort-enums); fi
    cc "${abi_flags[@]}" -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
        -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
        -Wno-empty-body -Wno-pragma-pack -ffunction-sections -fdata-sections \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
        -DVERSION_US -DPLAYERFLAG=int -DGE_PORT_MS_INHERITS -fms-extensions \
        -I "${repo_dir}" -I "${repo_dir}/port/include" -I "${repo_dir}/port/src" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
        "${repo_dir}/port/src/ge_mp_state_bg.c" "${repo_dir}/port/src/ge_mp_state.c" \
        "${repo_dir}/port/src/ge_original_bg_geometry.c" "${build_dir}/slice.c" \
        "${repo_dir}/port/tests/test_ge_mp_state_bg.c" \
        "${repo_dir}/port/src/ge_asset_pack.c" "${repo_dir}/port/src/ge_stage_assets.c" \
        "${repo_dir}/port/src/ge_dam_world.c" \
        "${repo_dir}/port/src/ge_gbi_decoder.c" "${repo_dir}/port/src/ge_gbi_matrix.c" \
        "${repo_dir}/port/src/ge_gbi_rsp.c" "${repo_dir}/port/src/ge_gbi_traverse.c" \
        "${repo_dir}/port/src/ge_gbi_state.c" "${repo_dir}/port/src/ge_gbi_vertex.c" \
        "${repo_dir}/port/src/ge_gbi_pipeline.c" "${repo_dir}/port/src/ge_pica_material.c" \
        -fsanitize=address,undefined -fno-sanitize-recover=all "${strip[@]}" -lm \
        -o "${build_dir}/test_bg_state"
    "${build_dir}/test_bg_state" "${build_dir}/${abi}.sha256" "$@"
done
cmp "${build_dir}/native.sha256" "${build_dir}/short-enum.sha256"

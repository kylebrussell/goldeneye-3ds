#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${GE_CAMPAIGN_RENDER_TEST_DIR:-${repo_dir}/build/host-tests/campaign-room-publication}
mkdir -p "${build_dir}"
if [[ $# != 1 ]]; then echo "Usage: $0 complete-original-asset-pack.gepack" >&2; exit 64; fi
python3 "${repo_dir}/scripts/extract_bg_geometry_slice.py" "${repo_dir}" "${build_dir}/slice.c"
strip=(-Wl,--gc-sections)
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); fi
for abi in ${GE_BG_TEST_ABIS:-native short-enum}; do
    if [[ ${abi} != native && ${abi} != short-enum ]]; then exit 64; fi
    printf 'Campaign room publication ABI: %s\n' "${abi}"
    abi_flags=(-fno-short-enums)
    if [[ ${abi} == short-enum ]]; then abi_flags=(-fshort-enums); fi
    cc "${abi_flags[@]}" -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
        -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
        -Wno-empty-body -Wno-pragma-pack -ffunction-sections -fdata-sections \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
        -DVERSION_US -DPLAYERFLAG=int -DGE_PORT_MS_INHERITS -fms-extensions \
        -I "${repo_dir}" -I "${repo_dir}/port/include" -I "${repo_dir}/port/src" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
        "${repo_dir}/port/src/ge_original_bg_geometry.c" "${build_dir}/slice.c" \
        "${repo_dir}/port/tests/test_ge_campaign_room_publication.c" \
        "${repo_dir}/port/src/ge_dam_dynamic_scene.c" \
        "${repo_dir}/port/src/ge_dam_preload_queue.c" \
        "${repo_dir}/port/src/ge_draw_batch_visibility.c" \
        "${repo_dir}/port/src/ge_asset_pack.c" "${repo_dir}/port/src/ge_stage_assets.c" \
        "${repo_dir}/port/src/ge_dam_world.c" "${repo_dir}/port/src/ge_dam_room.c" \
        "${repo_dir}/port/src/ge_gbi_decoder.c" "${repo_dir}/port/src/ge_gbi_matrix.c" \
        "${repo_dir}/port/src/ge_gbi_rsp.c" "${repo_dir}/port/src/ge_gbi_traverse.c" \
        "${repo_dir}/port/src/ge_gbi_state.c" "${repo_dir}/port/src/ge_gbi_vertex.c" \
        "${repo_dir}/port/src/ge_gbi_pipeline.c" "${repo_dir}/port/src/ge_pica_material.c" \
        -fsanitize=address,undefined -fno-sanitize-recover=all "${strip[@]}" -lm \
        -o "${build_dir}/test_campaign_room_publication"
    "${build_dir}/test_campaign_room_publication" "$@"
done

#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=${1:-"${repo_dir}/build/host-tests/scene-overlay-growth"}
mkdir -p "${test_dir}"
cc -std=c11 -Wall -Wextra -Werror -Wno-empty-body \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DGE_PORT_BG_CONNECTIVITY_SLICE \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tests/test_ge_dam_overlay_growth.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_dam_world.c" "${repo_dir}/src/game/bg.c" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" "${repo_dir}/port/src/ge_pica_material.c" \
    "${repo_dir}/port/src/ge_dam_room.c" -lm -o "${test_dir}/test_ge_dam_overlay_growth"
"${test_dir}/test_ge_dam_overlay_growth"

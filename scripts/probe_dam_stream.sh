#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
probe_dir="${repo_dir}/build/visual-probe"
probe="${probe_dir}/ge-dam-stream-probe"

if [[ $# -eq 0 ]]; then
    # Unique room sequence from the default authored player-route manifest.
    # Room 135 is already part of the initial resident component.
    set -- 113 121 117 116 53 103 102 100 98 97 93 91 90 86 84
fi

mkdir -p "${probe_dir}"
cc -std=c11 -Wall -Wextra -Werror -Wno-empty-body \
    -DGE_PORT_BG_CONNECTIVITY_SLICE \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tools/ge_dam_stream_probe.c" \
    "${repo_dir}/port/src/ge_dam_dynamic_scene.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_dam_world.c" \
    "${repo_dir}/src/game/bg.c" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_pica_material.c" \
    "${repo_dir}/port/src/ge_dam_room.c" \
    -lm -o "${probe}"

cd "${repo_dir}"
"${probe}" build/u/assets/obseg/bg/bg_dam_all_p.bin \
    build/3ds-assets/goldeneye.u.gepack "$@"

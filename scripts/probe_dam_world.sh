#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
probe_dir="${repo_dir}/build/visual-probe"
probe_bin="${probe_dir}/ge_dam_world_probe"

mkdir -p "${probe_dir}"

cc -std=c11 -O2 -Wall -Wextra -Werror -Wno-empty-body \
    -DGE_PORT_BG_CONNECTIVITY_SLICE \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tools/ge_dam_world_probe.c" \
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
    -lm -o "${probe_bin}"

cd "${repo_dir}"
exec "${probe_bin}" "$@"

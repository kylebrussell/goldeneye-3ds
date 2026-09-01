#!/usr/bin/env bash

set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
asset_pack=${2:-"${repo_dir}/build/host-tests/stage-assets/stages.gepack"}
test_dir="${repo_dir}/build/host-tests/stage-model-publication"
mkdir -p "${test_dir}"

if [[ ! -f "${asset_pack}" ]]; then
    echo "missing campaign test asset pack: ${asset_pack}" >&2
    echo "run scripts/test_stage_assets.sh first" >&2
    exit 1
fi

python3 "${repo_dir}/scripts/generate_move_model_tables.py" \
    "${repo_dir}" "${test_dir}/ge_original_move_model_tables.c"

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
fi

cc -std=gnu11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_model_publication.c" \
    "${repo_dir}/port/tests/ge_original_pitem_model_test_support.c" \
    "${repo_dir}/port/src/ge_original_stage_model_publication.c" \
    "${repo_dir}/port/src/ge_original_pitem_models.c" \
    "${repo_dir}/port/src/ge_original_model_scene.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_pica_material.c" \
    "${repo_dir}/port/src/ge_dam_room.c" \
    "${repo_dir}/src/game/matrixmath.c" \
    "${test_dir}/ge_original_move_model_tables.c" \
    -lm "${dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_stage_model_publication"

"${test_dir}/test_ge_original_stage_model_publication" "${asset_pack}"

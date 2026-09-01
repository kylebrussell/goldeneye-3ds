#!/usr/bin/env bash
set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
pack=${2:-"${repo_dir}/build/3ds-assets/goldeneye.u.gepack"}
build_dir="${repo_dir}/build/host-tests/frontend-cast-model-live"

mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/extract_player_gait_model_slice.py" \
    "${repo_dir}/src/game/model.c" \
    "${repo_dir}/src/game/initBondDATAdefaults.c" \
    "${build_dir}/model.c"
python3 "${repo_dir}/scripts/generate_move_model_tables.py" \
    "${repo_dir}" "${build_dir}/ge_original_move_model_tables.c"

flags=(
    -std=gnu11 -Wall -Wextra -Werror
    -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body
    -Wno-unused-value -Wno-switch -Wno-incompatible-pointer-types
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-int-conversion
    -Wno-missing-braces -Wno-missing-field-initializers
    -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
    -DGE_PORT_MODEL_HOST_RWDATA_ABI -DGE_PORT_SETUP_DATA
    -DGE_PORT_MS_INHERITS -DVERSION_US -DBUGFIX_R0 -fms-extensions
    -I"${repo_dir}" -I"${repo_dir}/port/include"
    -I"${repo_dir}/src/game" -I"${repo_dir}/src/libultra/gu"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}"
)
sources=(
    "${build_dir}/model.c"
    "${repo_dir}/port/src/ge_original_frontend_cast_model.c"
    "${repo_dir}/port/src/ge_original_frontend_cast.c"
    "${repo_dir}/port/src/ge_3ds_original_frontend_cast.c"
    "${repo_dir}/port/src/ge_asset_pack.c"
    "${repo_dir}/port/src/ge_original_character_models.c"
    "${repo_dir}/port/src/ge_original_dam_guard_model.c"
    "${repo_dir}/port/src/ge_original_pitem_models.c"
    "${repo_dir}/port/src/ge_original_guard_animation_table.c"
    "${repo_dir}/port/src/ge_original_player_gait.c"
    "${repo_dir}/port/src/ge_original_player_gait_data_source.c"
    "${repo_dir}/port/src/ge_original_animation_root.c"
    "${repo_dir}/port/src/ge_original_model_root_source.c"
    "${repo_dir}/port/src/ge_original_model_clock_source.c"
    "${repo_dir}/port/src/ge_original_model_scene.c"
    "${repo_dir}/port/src/ge_gbi_decoder.c"
    "${repo_dir}/port/src/ge_gbi_matrix.c"
    "${repo_dir}/port/src/ge_gbi_rsp.c"
    "${repo_dir}/port/src/ge_gbi_traverse.c"
    "${repo_dir}/port/src/ge_gbi_state.c"
    "${repo_dir}/port/src/ge_gbi_vertex.c"
    "${repo_dir}/port/src/ge_gbi_pipeline.c"
    "${repo_dir}/port/src/ge_pica_material.c"
    "${repo_dir}/src/game/matrixmath.c"
    "${repo_dir}/src/game/quaternion.c"
    "${repo_dir}/src/game/math_floor.c"
    "${repo_dir}/src/game/math_ceil.c"
    "${repo_dir}/src/game/math_unk_05A9E0.c"
    "${repo_dir}/src/game/title3.c"
    "${repo_dir}/src/libultra/gu/sins.c"
    "${build_dir}/ge_original_move_model_tables.c"
    "${repo_dir}/port/tests/ge_original_frontend_cast_model_test_support.c"
    "${repo_dir}/port/tests/ge_original_gunbarrel_bond_test_support.c"
    "${repo_dir}/port/tests/test_ge_original_frontend_cast_model_live.c"
)

objects=()
index=0
for source in "${sources[@]}"; do
    object="${build_dir}/${index}.o"
    case "${source}" in
        *ge_original_animation_root.c|*ge_original_model_root_source.c)
            cc "${flags[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
                -c "${source}" -o "${object}"
            ;;
        *ge_original_model_clock_source.c)
            cc "${flags[@]}" -DGE_PORT_MODEL_ANIMATION_CLOCK_SLICE \
                -c "${source}" -o "${object}"
            ;;
        *)
            cc "${flags[@]}" -c "${source}" -o "${object}"
            ;;
    esac
    objects+=("${object}")
    index=$((index + 1))
done

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then dead_strip=(-Wl,-dead_strip); fi
cc "${objects[@]}" -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${build_dir}/test_ge_original_frontend_cast_model_live"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
    "${build_dir}/test_ge_original_frontend_cast_model_live" "${pack}"

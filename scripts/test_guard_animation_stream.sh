#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "${work_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_player_gait_model_slice.py" \
    "${repo_dir}/src/game/model.c" \
    "${repo_dir}/src/game/initBondDATAdefaults.c" \
    "${work_dir}/model.c"
python3 "${repo_dir}/scripts/generate_move_model_tables.py" \
    "${repo_dir}" "${work_dir}/ge_original_move_model_tables.c"

flags=(
    -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter
    -Wno-unused-variable -Wno-empty-body -Wno-unused-value
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast -Wno-int-conversion
    -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
    -DGE_PORT_MODEL_HOST_RWDATA_ABI
    -DGE_PORT_SETUP_DATA -I"${repo_dir}" -I"${repo_dir}/port/include"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}"
)

sources=(
    "${work_dir}/model.c"
    "${repo_dir}/port/src/ge_original_player_gait.c"
    "${repo_dir}/port/src/ge_original_player_gait_data_source.c"
    "${repo_dir}/port/src/ge_original_guard_animation_table.c"
    "${repo_dir}/port/src/ge_original_character_models.c"
    "${repo_dir}/port/src/ge_asset_pack.c"
    "${repo_dir}/port/src/ge_original_dam_guard_model.c"
    "${repo_dir}/port/src/ge_original_dam_guard_weapon_model.c"
    "${repo_dir}/port/src/ge_original_bug_model.c"
    "${work_dir}/ge_original_move_model_tables.c"
    "${repo_dir}/port/src/ge_original_animation_root.c"
    "${repo_dir}/port/src/ge_original_model_root_source.c"
    "${repo_dir}/port/src/ge_original_model_clock_source.c"
    "${repo_dir}/src/game/matrixmath.c"
    "${repo_dir}/src/game/quaternion.c"
    "${repo_dir}/src/game/math_floor.c"
    "${repo_dir}/src/game/math_ceil.c"
    "${repo_dir}/src/game/math_unk_05A9E0.c"
    "${repo_dir}/port/tests/test_ge_original_guard_animation_stream.c"
)

objects=()
index=0
for source in "${sources[@]}"; do
    object="${work_dir}/${index}.o"
    if [[ "${source}" == *ge_original_animation_root.c \
            || "${source}" == *ge_original_model_root_source.c ]]; then
        cc "${flags[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
            -c "${source}" -o "${object}"
    elif [[ "${source}" == *ge_original_model_clock_source.c ]]; then
        cc "${flags[@]}" -DGE_PORT_MODEL_ANIMATION_CLOCK_SLICE \
            -c "${source}" -o "${object}"
    elif [[ "${source}" == *ge_original_dam_guard_model.c ]]; then
        cc "${flags[@]}" -Wno-missing-braces \
            -Wno-missing-field-initializers \
            -c "${source}" -o "${object}"
    else
        cc "${flags[@]}" -c "${source}" -o "${object}"
    fi
    objects+=("${object}")
    index=$((index + 1))
done

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then dead_strip=(-Wl,-dead_strip); fi
cc "${objects[@]}" -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${work_dir}/test"
"${work_dir}/test" \
    "${repo_dir}/build/3ds-animations/bond/animation_data.bin" \
    "${repo_dir}/build/3ds-animations/bond/animation_entries.bin" \
    "${repo_dir}/build/3ds-assets/goldeneye.u.gepack"

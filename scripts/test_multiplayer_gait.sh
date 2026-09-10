#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-gait.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
if [[ $(uname -s) == Darwin ]]; then port_dead_strip=(-Wl,-dead_strip); else port_dead_strip=(-Wl,--gc-sections); fi
for abi in native short; do
    gait_dir="${test_dir}/player-gait-${abi}"
    mkdir -p "${gait_dir}"
    python3 "${repo_dir}/scripts/extract_player_gait_model_slice.py" \
        "${repo_dir}/src/game/model.c" \
        "${repo_dir}/src/game/initBondDATAdefaults.c" \
        "${gait_dir}/ge_original_player_gait_model_slice.c"
    gait_common=(
        -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter
        -Wno-unused-variable -Wno-empty-body -Wno-unused-value -Wno-switch
        -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast
        -Wno-int-to-pointer-cast -Wno-int-conversion -Wno-missing-braces
        -Wno-missing-field-initializers -ffunction-sections -fdata-sections
        -fsanitize=address,undefined -fno-omit-frame-pointer
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
        -DGE_PORT_SETUP_DATA -I "${repo_dir}" -I "${repo_dir}/port/include"
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}"
    )
    if [[ ${abi} == short ]]; then gait_common+=(-fshort-enums); else gait_common+=(-fno-short-enums); fi
    cc "${gait_common[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
        -c "${repo_dir}/port/src/ge_original_model_root_source.c" \
        -o "${gait_dir}/model_root.o"
    cc "${gait_common[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
        -c "${repo_dir}/port/src/ge_original_animation_root.c" \
        -o "${gait_dir}/animation_root.o"
    cc "${gait_common[@]}" -DGE_PORT_MODEL_ANIMATION_CLOCK_SLICE \
        -c "${repo_dir}/port/src/ge_original_model_clock_source.c" \
        -o "${gait_dir}/model_clock.o"
    cc "${gait_common[@]}" -DGE_PORT_BOND_INTRO_SPAWN_SLICE \
        -c "${repo_dir}/src/game/bondview_r.c" \
        -o "${gait_dir}/spawn_player.o"
    cc "${gait_common[@]}" -DGE_PORT_BOND_HEAD_MOTION_SLICE \
        -DGE_PORT_BOND_HEAD_ANIMATION_SLICE \
        -c "${repo_dir}/port/src/ge_original_bond_head_source.c" \
        -o "${gait_dir}/bond_head_animation.o"
    gait_sources=(
        "${repo_dir}/port/src/ge_original_player_gait_data_source.c"
        "${gait_dir}/ge_original_player_gait_model_slice.c"
        "${repo_dir}/port/src/ge_original_player_gait.c"
        "${repo_dir}/port/src/ge_original_guard_animation_table.c"
        "${repo_dir}/src/game/matrixmath.c"
        "${repo_dir}/src/game/quaternion.c"
        "${repo_dir}/src/game/math_floor.c"
        "${repo_dir}/src/game/math_ceil.c"
        "${repo_dir}/src/game/math_unk_05A9E0.c"
        "${repo_dir}/port/tests/test_ge_original_player_gait.c"
    )
    gait_index=0
    # This directory may be reused. Later movement-chain objects must not
    # leak into the standalone gait link on a subsequent full-suite run.
    gait_objects=(
        "${gait_dir}/model_root.o" "${gait_dir}/animation_root.o"
        "${gait_dir}/model_clock.o" "${gait_dir}/spawn_player.o"
        "${gait_dir}/bond_head_animation.o"
    )
    for gait_source in "${gait_sources[@]}"; do
        cc "${gait_common[@]}" -c "${gait_source}" \
            -o "${gait_dir}/source_${gait_index}.o"
        gait_objects+=("${gait_dir}/source_${gait_index}.o")
        gait_index=$((gait_index + 1))
    done
    cc "${gait_objects[@]}" -lm -fsanitize=address,undefined \
        "${port_dead_strip[@]}" -o "${gait_dir}/test_ge_original_player_gait"
    "${gait_dir}/test_ge_original_player_gait" \
        "${repo_dir}/build/3ds-animations/bond/animation_data.bin" \
        "${repo_dir}/build/3ds-animations/bond/bond_eye_walk.entry.bin" \
        "${repo_dir}/build/3ds-animations/bond/sprinting.entry.bin" \
        "${repo_dir}/build/3ds-animations/bond/idle.entry.bin" \
        "${repo_dir}/build/3ds-animations/bond/animation_entries.bin"
done

#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
collision_path=${1:-"${repo_dir}/build/3ds-levels/dam/collision/collision.gestan"}
guard_model_path="${repo_dir}/build/3ds-models/greatguard2/model.bin"
guard_weapon_model_path="${repo_dir}/build/u/assets/obseg/prop/PchrkalashZ.bin"
first_person_model_dir="${repo_dir}/build/3ds-models/first-person-pp7"
animation_path="${repo_dir}/build/3ds-animations/bond/animation_data.bin"
animation_entries_path="${repo_dir}/build/3ds-animations/bond/animation_entries.bin"
work_dir=$(mktemp -d)
build_log="${work_dir}/build.log"
damage_only=${GE_TEST_GUARD_DAMAGE_ONLY:-0}
death_tick_only=${GE_TEST_GUARD_DEATH_TICK_ONLY:-0}
if [[ "${death_tick_only}" == 1 ]]; then damage_only=1; fi
trap 'rm -rf "${work_dir}"' EXIT

if [[ ! -f "${collision_path}" ]]; then
    echo "missing authored Dam collision asset: ${collision_path}" >&2
    exit 1
fi
if [[ ! -f "${guard_model_path}" ]]; then
    echo "missing authored greatguard2 model asset: ${guard_model_path}" >&2
    exit 1
fi
if [[ ! -f "${guard_weapon_model_path}" ]]; then
    echo "missing authored PchrkalashZ model asset: ${guard_weapon_model_path}" >&2
    exit 1
fi
if [[ ! -f "${first_person_model_dir}/GwppksilZ.bin" ]]; then
    echo "missing authored first-person PP7 model assets" >&2
    exit 1
fi
python3 "${repo_dir}/scripts/pack_3ds_assets.py" \
    --assets "${repo_dir}/port/tests/fixtures" \
    --source-sha1 abe01e4aeb033b6c0836819f549c791b26cfde83 \
    --extra-dir \
      "converted/models/first-person-pp7=${first_person_model_dir}" \
    --extra \
      "converted/models/pitem/Palarm1Z.bin=${repo_dir}/build/u/assets/obseg/prop/Palarm1Z.bin" \
    --output "${work_dir}/exact-gun.gepack" >/dev/null

python3 "${repo_dir}/scripts/extract_bond_input_gun_data_slice.py" \
    "${repo_dir}" "${work_dir}/gun-data.c"
python3 "${repo_dir}/scripts/extract_player_thrown_object_slice.py" \
    "${repo_dir}" "${work_dir}/player-thrown-object.c"
python3 "${repo_dir}/scripts/extract_gun_update_and_fire_slice.py" \
    "${repo_dir}" "${work_dir}/gun-update-and-fire.c"
python3 "${repo_dir}/scripts/extract_gun_pose_helpers_slice.py" \
    "${repo_dir}" "${work_dir}/gun-pose-helpers.c"
python3 "${repo_dir}/scripts/extract_gun_casing_slice.py" \
    "${repo_dir}" "${work_dir}/gun-casing.c"
python3 "${repo_dir}/scripts/extract_gun_secondary_sinks.py" \
    "${repo_dir}" "${work_dir}/gun-secondary-sinks.c"
python3 "${repo_dir}/scripts/extract_gun_secondary_dependencies.py" \
    "${repo_dir}" "${work_dir}/gun-secondary-dependencies.c"
python3 "${repo_dir}/scripts/extract_player_gait_model_slice.py" \
    "${repo_dir}/src/game/model.c" \
    "${repo_dir}/src/game/initBondDATAdefaults.c" \
    "${work_dir}/model-slice.c"
python3 "${repo_dir}/scripts/extract_guard_bullet_hit_slice.py" \
    "${repo_dir}" "${work_dir}/guard-hit-slice.c"
python3 "${repo_dir}/scripts/extract_object_bullet_hit_slice.py" \
    "${repo_dir}" "${work_dir}/object-bullet-hit-slice.c"
python3 "${repo_dir}/scripts/extract_guard_damage_slice.py" \
    "${repo_dir}" "${work_dir}/guard-damage-slice.c"
python3 "${repo_dir}/scripts/extract_guard_damage_support_slice.py" \
    "${repo_dir}" "${work_dir}/guard-damage-support-slice.c"
python3 "${repo_dir}/scripts/extract_guard_animation_init_slice.py" \
    "${repo_dir}" "${work_dir}/guard-animation-init-slice.c"
if [[ "${death_tick_only}" == 1 ]]; then
    if [[ ! -f "${animation_entries_path}" ]]; then
        echo "missing authored guard animation entries: ${animation_entries_path}" >&2
        exit 1
    fi
    python3 "${repo_dir}/scripts/extract_dam_guard_death_tick_slice.py" \
        "${repo_dir}" "${work_dir}/guard-death-tick-slice.c"
fi
python3 "${repo_dir}/scripts/extract_bond_move_collision_slice.py" \
    "${repo_dir}" "${work_dir}/bond-move-collision-slice.c"
python3 "${repo_dir}/scripts/extract_bond_move_explosion_slice.py" \
    "${repo_dir}" "${work_dir}/bond-move-explosion-slice.c"
python3 "${repo_dir}/scripts/extract_bond_move_state_slice.py" \
    "${repo_dir}/src/game/bondview.c" \
    "${repo_dir}/src/game/bondview2.c" \
    "${repo_dir}/src/game/player.c" "${repo_dir}/src/game/lv.c" \
    "${repo_dir}/src/game/debugmenu_handler.c" \
    "${repo_dir}/src/game/model.c" "${repo_dir}/src/game/stan.c" \
    "${work_dir}/bond-move-state-slice.c"
python3 "${repo_dir}/scripts/extract_bond_input_live_state_slice.py" \
    "${repo_dir}" "${work_dir}/bond-input-live-state-slice.c"
python3 "${repo_dir}/scripts/generate_animation_table_abi.py" \
    "${repo_dir}/assets/animationtable_data.c" \
    "${work_dir}/animation-table-abi.c"
python3 "${repo_dir}/scripts/generate_move_model_tables.py" \
    "${repo_dir}" "${work_dir}/move-model-tables.c"
python3 "${repo_dir}/scripts/extract_door_collision_slice.py" \
    "${repo_dir}" "${work_dir}/door-collision-slice.c"
python3 "${repo_dir}/scripts/extract_door_character_collision_slice.py" \
    "${repo_dir}" "${work_dir}/door-character-collision-slice.c"

port_flags=(
    -std=gnu11 -Wall -Wextra -Wno-error
    -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body
    -Wno-incompatible-pointer-types
    -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0
    -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}"
)

port_sources=(
    port/src/ge_original_prop_state_source.c
    port/src/ge_original_prop_state.c
    port/src/ge_original_objinit_source.c
    port/src/ge_original_default_object_source.c
    port/src/ge_original_default_object.c
    port/src/ge_dam_setup_world_materializer.c
    assets/obseg/setup/UsetupdamZ.c
    port/src/ge_original_bug_model.c
    port/src/ge_original_covert_modem_object.c
    port/src/ge_original_covert_modem_projectile.c
    port/src/ge_original_covert_modem_fire.c
    port/src/ge_original_pp7_fire.c
    port/src/ge_original_dam_guard_model.c
    port/src/ge_original_dam_guard_weapon_model.c
    port/src/ge_original_guard_grenade_model.c
    port/src/ge_original_guard_grenade_object.c
    port/src/ge_original_dam_guards.c
    port/src/ge_original_dam_guard_scene.c
    port/src/ge_original_model_scene.c
    port/src/ge_original_guard_bullet_hit.c
    port/src/ge_original_player_gait.c
    port/src/ge_original_animation_root.c
    port/src/ge_original_guard_animation_table.c
    port/src/ge_original_door_collision.c
    port/src/ge_original_dam_setup.c
    port/src/ge_original_gameplay_services.c
    port/src/ge_original_gun_frame_arena.c
    port/src/ge_original_gun_live.c
    port/src/ge_asset_pack.c
    port/src/ge_original_first_person_assets.c
    port/src/ge_original_first_person_item_model.c
    port/src/ge_original_first_person_scene.c
    port/src/ge_original_pitem_models.c
    port/src/ge_dam_camera.c
    port/src/ge_original_player_spawn.c
    port/src/ge_original_sfx_bank.c
    port/src/ge_original_bond_input_provider.c
    port/src/ge_stan_collision.c
    port/src/ge_stan_native.c
    port/src/ge_gbi_decoder.c
    port/src/ge_gbi_matrix.c
    port/src/ge_gbi_pipeline.c
    port/src/ge_gbi_rsp.c
    port/src/ge_gbi_state.c
    port/src/ge_gbi_traverse.c
    port/src/ge_gbi_vertex.c
    port/src/ge_pica_material.c
    port/src/random_port.c
    src/game/matrixmath.c
    src/game/quaternion.c
    src/game/math_unk_05A9E0.c
    src/libultra/gu/align.c
    src/libultra/gu/normalize.c
    src/libultra/gu/rotate.c
    src/libultra/gu/lookatref.c
    src/libultra/gu/perspective.c
)

set +e
(
    set -e
    for source in "${port_sources[@]}"; do
        object_name=${source//\//_}
        if [[ "${source}" == port/src/ge_original_dam_guards.c \
                || "${source}" == port/src/ge_original_guard_grenade_object.c ]]; then
            cc "${port_flags[@]}" -fms-extensions \
                -DGE_PORT_MS_INHERITS \
                -c "${repo_dir}/${source}" \
                -o "${work_dir}/${object_name}.o"
        else
            cc "${port_flags[@]}" -c "${repo_dir}/${source}" \
                -o "${work_dir}/${object_name}.o"
        fi
    done
    cc "${port_flags[@]}" -c "${work_dir}/gun-data.c" \
        -o "${work_dir}/gun-data.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -c "${repo_dir}/port/tests/ge_object_bullet_hit_host_services.c" \
        -o "${work_dir}/object-bullet-hit-host-services.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -Wno-pointer-to-int-cast -Wno-int-conversion \
        -c "${work_dir}/player-thrown-object.c" \
        -o "${work_dir}/player-thrown-object.o"
    for gun_source in gun-update-and-fire gun-pose-helpers gun-casing \
            gun-secondary-sinks gun-secondary-dependencies; do
        cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
            -DGE_PORT_GUN_HOST_MODEL_ABI \
            -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
            -Wno-int-conversion -Wno-array-bounds \
            -Wno-maybe-uninitialized -Wno-parentheses \
            -c "${work_dir}/${gun_source}.c" \
            -o "${work_dir}/${gun_source}.o"
    done
    cc "${port_flags[@]}" -DGE_PORT_MODEL_HOST_RWDATA_ABI \
        -c "${work_dir}/model-slice.c" \
        -o "${work_dir}/model-slice.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -DGE_PORT_MODEL_HIT_NATIVE_ABI \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
        -Wno-int-conversion -c "${work_dir}/guard-hit-slice.c" \
        -o "${work_dir}/guard-hit-slice.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
        -Wno-int-conversion -c "${work_dir}/object-bullet-hit-slice.c" \
        -o "${work_dir}/object-bullet-hit-slice.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -DGE_PORT_DAMAGE_HOST_ANIMATION_OFFSETS \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
        -Wno-int-conversion -c "${work_dir}/guard-damage-slice.c" \
        -o "${work_dir}/guard-damage-slice.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
        -Wno-int-conversion -c "${work_dir}/guard-damage-support-slice.c" \
        -o "${work_dir}/guard-damage-support-slice.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -DGE_PORT_ANIMATION_INIT_HOST_POINTER_ABI \
        -DGE_PORT_ANIMATION_INIT_OFFSETS \
        -DGE_PORT_ANIMATION_INIT_NATIVE_ABI \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
        -Wno-int-conversion -Wno-missing-braces \
        -c "${work_dir}/guard-animation-init-slice.c" \
        -o "${work_dir}/guard-animation-init-slice.o"
    if [[ "${death_tick_only}" == 1 ]]; then
        cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
            -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
            -Wno-int-conversion -c "${work_dir}/guard-death-tick-slice.c" \
            -o "${work_dir}/guard-death-tick-slice.o"
    fi
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
        -Wno-int-conversion -c "${work_dir}/bond-move-collision-slice.c" \
        -o "${work_dir}/bond-move-collision-slice.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -DGE_PORT_VTXSTORE_HOST_POINTER_ABI \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
        -Wno-int-conversion -Wno-missing-braces \
        -Wno-parentheses-equality -Wno-unused-value \
        -Wno-pointer-bool-conversion -Wno-missing-field-initializers \
        -Wno-excess-initializers -Wno-deprecated-declarations \
        -c "${work_dir}/bond-move-explosion-slice.c" \
        -o "${work_dir}/bond-move-explosion-slice.o"
    cc "${port_flags[@]}" -Wno-comment -Wno-missing-braces \
        -c "${work_dir}/bond-move-state-slice.c" \
        -o "${work_dir}/bond-move-state-slice.o"
    cc "${port_flags[@]}" -DGE_PORT_BOND_INPUT_HOST_STATE \
        -Wno-comment -Wno-int-conversion \
        -Wno-pointer-to-int-cast -Wno-unused-but-set-variable \
        -Wno-return-type -Wno-switch -Wno-int-to-pointer-cast \
        -Wno-unused-value -Wno-sometimes-uninitialized \
        -Wno-pointer-sign -Wno-missing-braces -Wno-overflow \
        -c "${work_dir}/bond-input-live-state-slice.c" \
        -o "${work_dir}/bond-input-live-state-slice.o"
    cc "${port_flags[@]}" -DGE_PORT_ANIMATION_TABLE_HOST_TEST \
        -fno-data-sections \
        -c "${work_dir}/animation-table-abi.c" \
        -o "${work_dir}/animation-table-abi.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -Wno-missing-braces -c "${work_dir}/move-model-tables.c" \
        -o "${work_dir}/move-model-tables.o"
    cc "${port_flags[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
        -Wno-unused-value -c \
        "${repo_dir}/port/src/ge_original_model_root_source.c" \
        -o "${work_dir}/model-root-source.o"
    cc "${port_flags[@]}" -DGE_PORT_MODEL_ANIMATION_CLOCK_SLICE \
        -Wno-int-conversion -Wno-unused-value -c \
        "${repo_dir}/port/src/ge_original_model_clock_source.c" \
        -o "${work_dir}/model-clock-source.o"
    cc "${port_flags[@]}" -DGE_PORT_BOND_INTRO_SPAWN_SLICE \
        -c "${repo_dir}/src/game/bondview_r.c" \
        -o "${work_dir}/bondview-r-spawn.o"
    cc "${port_flags[@]}" -DGE_PORT_BOND_CAMERA_SLICE \
        -c "${repo_dir}/port/src/ge_original_bond_camera.c" \
        -o "${work_dir}/bond-camera.o"
    cc "${port_flags[@]}" -DGE_PORT_BOND_CAMERA_SLICE \
        -DGE_PORT_BOND_PLAYER_SPAWN_SLICE \
        -c "${repo_dir}/src/game/bondview2.c" \
        -o "${work_dir}/bond-camera-source.o"
    cc "${port_flags[@]}" -fno-sanitize=shift \
        -c "${repo_dir}/src/libultra/gu/mtxutil.c" \
        -o "${work_dir}/gu-mtxutil.o"
    cc "${port_flags[@]}" \
        -c "${repo_dir}/port/src/ge_original_chr_obj_random.c" \
        -o "${work_dir}/chr-obj-random.o"
    cc "${port_flags[@]}" -c "${repo_dir}/src/game/math_floor.c" \
        -o "${work_dir}/math-floor.o"
    cc "${port_flags[@]}" -c "${repo_dir}/src/game/math_ceil.c" \
        -o "${work_dir}/math-ceil.o"
    cc "${port_flags[@]}" -c "${work_dir}/door-collision-slice.c" \
        -o "${work_dir}/door-collision-slice.o"
    cc "${port_flags[@]}" -c "${work_dir}/door-character-collision-slice.c" \
        -o "${work_dir}/door-character-collision-slice.o"
    cc "${port_flags[@]}" -DGE_PORT_PROP_SETUP_PAD_SLICE \
        -c "${repo_dir}/src/game/prop.c" -o "${work_dir}/setup-pad.o"
    cc "${port_flags[@]}" -DGE_PORT_STAN_GEOMETRY_SLICE \
        -DGE_PORT_BOND_MOVEMENT_SLICE -c "${repo_dir}/src/game/stan.c" \
        -o "${work_dir}/stan.o"
    cc "${port_flags[@]}" -DGE_PORT_STAN_GEOMETRY_SLICE \
        -DGE_PORT_BOND_MOVEMENT_SLICE \
        -c "${repo_dir}/src/game/stanintersection.c" \
        -o "${work_dir}/stanintersection.o"
    cc "${port_flags[@]}" -DGE_PORT_BG_CONNECTIVITY_SLICE \
        -c "${repo_dir}/src/game/bg.c" -o "${work_dir}/bg.o"
    cc "${port_flags[@]}" \
        -c "${repo_dir}/port/tests/test_ge_original_covert_modem_fire.c" \
        -o "${work_dir}/fire-test.o"
    cc "${port_flags[@]}" -DGE_TEST_EXACT_GUN_BOTH_HANDS \
        -c "${repo_dir}/port/tests/test_ge_original_covert_modem_fire.c" \
        -o "${work_dir}/exact-gun-fire-test.o"
    cc "${port_flags[@]}" \
        -c "${repo_dir}/port/tests/test_ge_original_covert_modem_projectile.c" \
        -o "${work_dir}/projectile-test.o"
    cc "${port_flags[@]}" \
        -c "${repo_dir}/port/tests/test_ge_original_pp7_fire.c" \
        -o "${work_dir}/pp7-fire-test.o"
    cc "${port_flags[@]}" -fms-extensions -DGE_PORT_MS_INHERITS \
        -c "${repo_dir}/port/tests/test_ge_original_dam_guards.c" \
        -o "${work_dir}/dam-guards-test.o"
    cc "${port_flags[@]}" \
        -c "${repo_dir}/port/tests/test_ge_original_guard_bullet_hit.c" \
        -o "${work_dir}/guard-hit-test.o"
    if [[ "${damage_only}" == 1 ]]; then
        damage_test_defines=(-DGE_TEST_GUARD_DAMAGE_CONSEQUENCE)
        if [[ "${death_tick_only}" == 1 ]]; then
            damage_test_defines+=(-DGE_TEST_GUARD_DEATH_TICK)
        fi
        cc "${port_flags[@]}" "${damage_test_defines[@]}" \
            -c "${repo_dir}/port/tests/test_ge_original_guard_bullet_hit.c" \
            -o "${work_dir}/guard-damage-test.o"
    fi
) >"${build_log}" 2>&1
build_status=$?
set -e
if [[ ${build_status} -ne 0 ]]; then
    tail -n 120 "${build_log}" >&2
    exit "${build_status}"
fi

for symbol in chrSetHiddenToRandom get_distance_actor_to_position \
    chrlvAttackAnimationRelated7F026F30 triggered_on_shot_hit \
    handles_shot_actors chrHandleBulletHit gunItemGetDestructionAmount \
    gunSetTracerTarget inc_curplayer_hitcount_with_weapon \
    chrlvGetGuard007ArghRating get_007_reaction_speed \
    lvlGetSelectedDifficulty get_hat_model bondwalkItemGetForceOfImpact \
    chrlvPathingCollisionRelated7F0264B0 set_cur_player \
    recall_joy2_hits_edit_detail_edit_flag chrCreateHitPuffs \
    chrCreateBloodStain bullet_spark_create; do
    if ! nm -g "${work_dir}/guard-damage-slice.o" \
            | grep -Eq " _?${symbol}$"; then
        echo "missing canonical guard-damage body: ${symbol}" >&2
        exit 1
    fi
done
for symbol in initResolveAnimTable initResolveAnimGroupTable \
        initResolveAnimGroups initWeaponAnimGroups; do
    if ! nm -g "${work_dir}/guard-animation-init-slice.o" \
            | grep -Eq " _?${symbol}$"; then
        echo "missing canonical guard-animation startup body: ${symbol}" >&2
        exit 1
    fi
done

common_objects=()
while IFS= read -r object; do
    common_objects+=("${object}")
done < <(find "${work_dir}" -name '*.o' \
    ! -name 'player-thrown-object.o' \
    ! -name 'gun-update-and-fire.o' \
    ! -name 'gun-pose-helpers.o' ! -name 'gun-casing.o' \
    ! -name 'gun-secondary-sinks.o' \
    ! -name 'gun-secondary-dependencies.o' \
    ! -name 'guard-damage-slice.o' \
    ! -name 'guard-damage-support-slice.o' \
    ! -name 'guard-animation-init-slice.o' \
    ! -name 'guard-death-tick-slice.o' \
    ! -name 'bond-move-collision-slice.o' \
    ! -name 'bond-move-explosion-slice.o' \
    ! -name 'bond-move-state-slice.o' \
    ! -name 'bond-input-live-state-slice.o' \
    ! -name 'animation-table-abi.o' \
    ! -name 'move-model-tables.o' \
    ! -name 'model-root-source.o' \
    ! -name 'model-clock-source.o' \
    ! -name 'bondview-r-spawn.o' \
    ! -name 'bond-camera.o' \
    ! -name 'bond-camera-source.o' \
    ! -name 'gu-mtxutil.o' \
    ! -name 'chr-obj-random.o' \
    ! -name 'math-floor.o' ! -name 'math-ceil.o' \
    ! -name 'port_src_ge_original_guard_bullet_hit.c.o' \
    ! -name 'fire-test.o' ! -name 'exact-gun-fire-test.o' \
    ! -name 'projectile-test.o' \
    ! -name 'pp7-fire-test.o' ! -name 'dam-guards-test.o' \
    ! -name 'guard-hit-test.o' ! -name 'guard-damage-test.o' | sort)

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == "Darwin" ]]; then
    dead_strip=(-Wl,-dead_strip)
fi

cc "${common_objects[@]}" \
    "${work_dir}/port_src_ge_original_guard_bullet_hit.c.o" \
    "${work_dir}/projectile-test.o" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${work_dir}/projectile-test"
if [[ "${damage_only}" != 1 ]]; then
cc "${common_objects[@]}" "${work_dir}/player-thrown-object.o" \
    "${work_dir}/port_src_ge_original_guard_bullet_hit.c.o" \
    "${work_dir}/fire-test.o" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${work_dir}/fire-test"
cc "${common_objects[@]}" "${work_dir}/player-thrown-object.o" \
    "${work_dir}/gun-update-and-fire.o" \
    "${work_dir}/gun-pose-helpers.o" "${work_dir}/gun-casing.o" \
    "${work_dir}/gun-secondary-sinks.o" \
    "${work_dir}/gun-secondary-dependencies.o" \
    "${work_dir}/bond-move-collision-slice.o" \
    "${work_dir}/bond-move-explosion-slice.o" \
    "${work_dir}/bond-input-live-state-slice.o" \
    "${work_dir}/bondview-r-spawn.o" \
    "${work_dir}/bond-camera.o" \
    "${work_dir}/bond-camera-source.o" \
    "${work_dir}/gu-mtxutil.o" \
    "${work_dir}/port_src_ge_original_guard_bullet_hit.c.o" \
    "${work_dir}/exact-gun-fire-test.o" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${work_dir}/exact-gun-fire-test"
fi
if [[ "${GE_TEST_MODEM_ONLY:-0}" == 1 ]]; then
    "${work_dir}/projectile-test" "${collision_path}"
    "${work_dir}/fire-test" "${collision_path}"
    "${work_dir}/exact-gun-fire-test" "${collision_path}" \
        "${work_dir}/exact-gun.gepack"
    exit 0
fi
cc "${common_objects[@]}" \
    "${work_dir}/port_src_ge_original_guard_bullet_hit.c.o" \
    "${work_dir}/guard-damage-slice.o" \
    "${work_dir}/guard-damage-support-slice.o" \
    "${work_dir}/guard-animation-init-slice.o" \
    "${work_dir}/bond-move-collision-slice.o" \
    "${work_dir}/bond-move-explosion-slice.o" \
    "${work_dir}/bond-move-state-slice.o" \
    "${work_dir}/bond-input-live-state-slice.o" \
    "${work_dir}/animation-table-abi.o" \
    "${work_dir}/move-model-tables.o" \
    "${work_dir}/model-root-source.o" \
    "${work_dir}/model-clock-source.o" \
    "${work_dir}/chr-obj-random.o" \
    "${work_dir}/math-floor.o" "${work_dir}/math-ceil.o" \
    "${work_dir}/player-thrown-object.o" \
    "${work_dir}/pp7-fire-test.o" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${work_dir}/pp7-fire-test"
cc "${common_objects[@]}" "${work_dir}/dam-guards-test.o" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${work_dir}/dam-guards-test"
cc "${common_objects[@]}" \
    "${work_dir}/port_src_ge_original_guard_bullet_hit.c.o" \
    "${work_dir}/guard-hit-test.o" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${work_dir}/guard-hit-test"
if [[ "${damage_only}" == 1 ]]; then
damage_objects=(
    "${common_objects[@]}"
    "${work_dir}/port_src_ge_original_guard_bullet_hit.c.o"
    "${work_dir}/guard-damage-slice.o"
    "${work_dir}/guard-damage-support-slice.o"
    "${work_dir}/guard-animation-init-slice.o"
)
if [[ "${death_tick_only}" == 1 ]]; then
    damage_objects+=("${work_dir}/guard-death-tick-slice.o")
fi
damage_objects+=(
    "${work_dir}/bond-move-collision-slice.o"
    "${work_dir}/bond-move-explosion-slice.o"
    "${work_dir}/bond-move-state-slice.o"
    "${work_dir}/bond-input-live-state-slice.o"
    "${work_dir}/animation-table-abi.o"
    "${work_dir}/move-model-tables.o"
    "${work_dir}/model-root-source.o"
    "${work_dir}/model-clock-source.o"
    "${work_dir}/chr-obj-random.o"
    "${work_dir}/math-floor.o" "${work_dir}/math-ceil.o"
    "${work_dir}/player-thrown-object.o"
    "${work_dir}/guard-damage-test.o"
)
cc "${damage_objects[@]}" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${work_dir}/guard-damage-test"
if [[ "${death_tick_only}" == 1 ]]; then
    "${work_dir}/guard-damage-test" "${collision_path}" \
        "${animation_path}" "${animation_entries_path}"
else
    "${work_dir}/guard-damage-test" "${collision_path}" \
        "${animation_path}"
fi
    exit 0
fi

if [[ "${GE_GUARD_SCENE_BENCH:-0}" == 1 ]]; then
    "${work_dir}/dam-guards-test" "${collision_path}" \
        "${guard_model_path}" "${guard_weapon_model_path}"
    exit 0
fi

"${work_dir}/projectile-test" "${collision_path}"
"${work_dir}/fire-test" "${collision_path}"
"${work_dir}/exact-gun-fire-test" "${collision_path}" \
    "${work_dir}/exact-gun.gepack"
"${work_dir}/pp7-fire-test" "${collision_path}" \
    "${work_dir}/exact-gun.gepack"
"${work_dir}/dam-guards-test" "${collision_path}" \
    "${guard_model_path}" "${guard_weapon_model_path}"
"${work_dir}/guard-hit-test" "${collision_path}"

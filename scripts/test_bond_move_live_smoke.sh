#!/usr/bin/env bash

set -euo pipefail

repo_dir=${1:?repository path required}
test_dir=${2:?aggregate test directory required}
smoke_dir="${test_dir}/bond-move-live-smoke"
mkdir -p "${smoke_dir}"

dead_strip=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections -no-pie)
fi

common=(
    -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter
    -Wno-unused-variable -Wno-empty-body -Wno-unused-value -Wno-switch
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast -Wno-int-conversion -Wno-missing-braces
    -Wno-missing-field-initializers -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0
    -DGE_PORT_MS_INHERITS -fms-extensions
    -I "${repo_dir}" -I "${repo_dir}/port/include"
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include"
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src"
    -iquote "${repo_dir}"
)

python3 "${repo_dir}/scripts/extract_bond_move_runtime_slice.py" \
    "${repo_dir}/src/game/bondview2.c" \
    "${repo_dir}/src/game/bondview.c" \
    "${repo_dir}/src/game/stan.c" \
    "${smoke_dir}/runtime.c"
cc "${common[@]}" -DGE_PORT_EXACT_MOVEBOND_FRONTIER \
    -c "${smoke_dir}/runtime.c" -o "${smoke_dir}/runtime.o"
cc "${common[@]}" -c "${repo_dir}/port/src/ge_original_bond_live.c" \
    -o "${smoke_dir}/live.o"
cc "${common[@]}" -c "${repo_dir}/port/src/ge_original_input.c" \
    -o "${smoke_dir}/input.o"
cc "${common[@]}" -DGE_PORT_BOND_CAMERA_SLICE \
    -DGE_PORT_BOND_MOVEMENT_SLICE \
    -c "${repo_dir}/port/src/ge_original_bond_movement.c" \
    -o "${smoke_dir}/movement.o"
cc "${common[@]}" -DGE_PORT_VEC3_LERP_SLICE \
    -c "${repo_dir}/src/game/matrixmath_misc.c" \
    -o "${smoke_dir}/vec3lerp.o"
cc "${common[@]}" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_move_live_smoke.c" \
    -o "${smoke_dir}/test.o"
cc -c "${repo_dir}/port/tests/ge_original_animation_table_host_alias.S" \
    -o "${smoke_dir}/animation_alias.o"

compile_support() {
    local source=$1
    local output=$2
    shift 2
    cc "${common[@]}" "$@" -c "${repo_dir}/${source}" \
        -o "${smoke_dir}/${output}"
}

compile_support src/boss.c boss.o -DGE_PORT_BOSS_STAGE_SLICE
compile_support src/game/lv.c lv.o -DGE_PORT_LV_STAGE_TICK_SLICE
compile_support port/src/ge_original_chr_obj_random.c chr_obj_random.o
compile_support port/src/ge_original_prop_state_source.c prop_state_source.o
compile_support port/src/ge_original_prop_state.c prop_state.o
compile_support port/src/ge_original_objinit_source.c objinit_source.o
compile_support port/src/ge_original_gameplay_services.c gameplay_services.o
compile_support port/src/ge_original_effect_buffers.c effect_buffers.o
compile_support port/src/ge_original_door_collision.c door_collision.o
compile_support port/src/ge_original_dam_guard_model.c dam_guard_model.o
compile_support port/src/ge_original_guard_grenade_model.c \
    guard_grenade_model.o
compile_support port/src/ge_original_guard_grenade_object.c \
    guard_grenade_object.o
compile_support port/src/ge_original_bug_model.c bug_model.o
compile_support port/src/ge_original_music_source.c music.o
compile_support port/src/ge_original_dam_mission_stage_storage.c stage_storage.o
compile_support port/src/ge_original_first_person_assets.c first_person_assets.o
compile_support port/src/ge_original_covert_modem_projectile.c projectile.o
compile_support port/src/random_port.c random.o
compile_support port/src/ge_original_rom_copy.c rom_copy.o
compile_support port/src/ge_original_decompress_source.c decompress.o
compile_support port/src/ge_original_zlib_source.c zlib.o
compile_support port/src/ge_asset_pack.c asset_pack.o
compile_support port/src/ge_dam_setup_world_materializer.c world_materializer.o
compile_support port/src/ge_original_covert_modem_fire.c modem_fire.o
compile_support port/src/ge_original_covert_modem_object.c modem_object.o
compile_support port/src/ge_original_pp7_fire.c pp7_fire.o
compile_support port/src/ge_original_sfx_bank.c sfx_bank.o
compile_support port/src/ge_original_default_object.c default_object.o
compile_support src/game/bg.c bg_connectivity.o -DGE_PORT_BG_CONNECTIVITY_SLICE
language_objects=()
for language_bank in LdamE LarkE LrunE LsevxE LsevE LsiloE LdestE \
        LsevxbE LsevbE LstatE LarchE LpeteE LdepoE LtraE LjunE LarecE \
        LcaveE LcradE LaztE LcrypE LgunE LmiscE LoptionsE LpropobjE LtitleE; do
    compile_support "assets/obseg/text/${language_bank}.c" \
        "${language_bank}.o"
    language_objects+=("${smoke_dir}/${language_bank}.o")
done
compile_support port/src/ge_original_guard_bullet_hit.c guard_bullet_hit.o
compile_support port/src/ge_original_dam_guards.c dam_guards.o

for audio_source in cspgetstate cseq cspsetseq cspsetvol cspplay cspstop \
        event copy sl; do
    compile_support "src/libultra/audio/${audio_source}.c" \
        "${audio_source}.o"
done
cc -std=c11 -Wall -Wextra -Wno-error -include stdbool.h \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "${repo_dir}/port/include" \
    -c "${repo_dir}/port/src/ge_libultra_scheduler.c" \
    -o "${smoke_dir}/scheduler.o"

# The absolute host alias must equal the generated authored-table byte delta.
python3 - "${test_dir}/animation-table/animation_table.o" \
    "${smoke_dir}/animation_alias.o" <<'PY'
import subprocess
import sys

def symbols(path):
    values = {}
    for line in subprocess.check_output(["nm", "-g", path], text=True).splitlines():
        fields = line.split()
        if len(fields) >= 3:
            values[fields[-1].lstrip("_")] = int(fields[0], 16)
    return values

table = symbols(sys.argv[1])
alias = symbols(sys.argv[2])
actual = (table["ge_port_storage_ANIM_DATA_bond_watch"]
          - table["ge_port_storage_ANIM_DATA_empty"])
assert actual == alias["ANIM_DATA_bond_watch"] == 0x42C8
PY

objects=(
    "${smoke_dir}/runtime.o"
    "${smoke_dir}/live.o"
    "${smoke_dir}/input.o"
    "${smoke_dir}/movement.o"
    "${smoke_dir}/test.o"
    "${test_dir}/bond-move-state/state.o"
    "${test_dir}/bond-move-state/adapter.o"
    "${test_dir}/bond-move-collision/collision.o"
    "${test_dir}/bond-move-non-tank/non_tank.o"
    "${test_dir}/bond-move-explosion/explosion.o"
    "${test_dir}/move-model-tables/model_tables.o"
    "${test_dir}/animation-table/animation_table.o"
    "${smoke_dir}/animation_alias.o"
    "${test_dir}/original-bond-input/provider-bound.o"
    "${test_dir}/player-gait/model_root.o"
    "${test_dir}/player-gait/animation_root.o"
    "${test_dir}/player-gait/model_clock.o"
    "${test_dir}/player-gait/spawn_player.o"
    "${test_dir}/player-gait/source_0.o"
    "${test_dir}/player-gait/source_1.o"
    "${test_dir}/player-gait/source_2.o"
    "${test_dir}/player-gait/source_3.o"
    "${test_dir}/player-gait/source_4.o"
    "${test_dir}/player-gait/source_5.o"
    "${test_dir}/player-gait/source_6.o"
    "${test_dir}/player-gait/source_7.o"
    "${test_dir}/player-gait/source_8.o"
    "${test_dir}/player-gait/chain_0.o"
    "${test_dir}/player-gait/chain_1.o"
    "${test_dir}/player-gait/chain_2.o"
    "${test_dir}/bond-head-update/head.o"
    "${test_dir}/bond-head-update/adapter.o"
    "${smoke_dir}/vec3lerp.o"
    "${test_dir}/bond_movement_source.o"
    "${test_dir}/stanintersection_geometry_slice.o"
    "${test_dir}/bond_player_position_for_movement.o"
    "${smoke_dir}/boss.o"
    "${smoke_dir}/lv.o"
    "${smoke_dir}/chr_obj_random.o"
    "${smoke_dir}/prop_state_source.o"
    "${smoke_dir}/prop_state.o"
    "${smoke_dir}/objinit_source.o"
    "${smoke_dir}/gameplay_services.o"
    "${smoke_dir}/effect_buffers.o"
    "${smoke_dir}/door_collision.o"
    "${smoke_dir}/dam_guard_model.o"
    "${smoke_dir}/guard_grenade_model.o"
    "${smoke_dir}/guard_grenade_object.o"
    "${smoke_dir}/bug_model.o"
    "${smoke_dir}/music.o"
    "${smoke_dir}/stage_storage.o"
    "${smoke_dir}/first_person_assets.o"
    "${smoke_dir}/projectile.o"
    "${smoke_dir}/random.o"
    "${smoke_dir}/rom_copy.o"
    "${smoke_dir}/decompress.o"
    "${smoke_dir}/zlib.o"
    "${smoke_dir}/asset_pack.o"
    "${smoke_dir}/world_materializer.o"
    "${smoke_dir}/modem_fire.o"
    "${smoke_dir}/modem_object.o"
    "${smoke_dir}/pp7_fire.o"
    "${smoke_dir}/sfx_bank.o"
    "${smoke_dir}/default_object.o"
    "${smoke_dir}/bg_connectivity.o"
    "${language_objects[@]}"
    "${smoke_dir}/cspgetstate.o"
    "${smoke_dir}/cseq.o"
    "${smoke_dir}/cspsetseq.o"
    "${smoke_dir}/cspsetvol.o"
    "${smoke_dir}/cspplay.o"
    "${smoke_dir}/cspstop.o"
    "${smoke_dir}/event.o"
    "${smoke_dir}/copy.o"
    "${smoke_dir}/sl.o"
    "${smoke_dir}/guard_bullet_hit.o"
    "${smoke_dir}/dam_guards.o"
    "${smoke_dir}/scheduler.o"
    "${test_dir}/original-dam-setup/dam_mission_hud.o"
    "${test_dir}/original-dam-setup/dam_mission_object_state.o"
)

cc "${objects[@]}" -fsanitize=address,undefined \
    "${dead_strip[@]}" -lm \
    -o "${smoke_dir}/test_ge_original_bond_move_live_smoke"
"${smoke_dir}/test_ge_original_bond_move_live_smoke" \
    "${repo_dir}/build/3ds-levels/dam/collision/collision.gestan" \
    "${repo_dir}/build/3ds-animations/bond/animation_data.bin" \
    "${repo_dir}/build/3ds-animations/bond/bond_eye_walk.entry.bin" \
    "${repo_dir}/build/3ds-animations/bond/sprinting.entry.bin" \
    "${repo_dir}/build/3ds-animations/bond/idle.entry.bin"

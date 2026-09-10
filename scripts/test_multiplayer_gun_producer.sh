#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
gameplay=${GE_TEST_GUN_GAMEPLAY:-0}
build_dir="${repo_dir}/build/host-tests/multiplayer-gun-producer"
if [[ ${gameplay} == 1 ]]; then build_dir="${repo_dir}/build/host-tests/multiplayer-gun-gameplay"; fi
pack=${1:-"${repo_dir}/build/host-tests/multiplayer-gun-producer/guns.gepack"}
mkdir -p "${build_dir}/empty-assets"
if [[ $# == 0 && ! -f ${pack} ]]; then
    rom=${GE_TEST_ROM:-"${repo_dir}/baserom.u.z64"}
    if [[ ! -f ${rom} ]]; then
        echo "Supply a packed first-person model asset file, or set GE_TEST_ROM to the source ROM." >&2
        exit 2
    fi
    python3 "${repo_dir}/scripts/pack_3ds_assets.py" --assets "${build_dir}/empty-assets" \
        --rom "${rom}" \
        --extra-dir "converted/models/first-person-pp7=${repo_dir}/build/3ds-models/first-person-pp7" \
        --output "${pack}"
fi
for name in gun_update_and_fire gun_pose_helpers gun_casing multiplayer_gun_fixture; do
    script="${repo_dir}/scripts/extract_${name}_slice.py"
    if [[ ${name} == multiplayer_gun_fixture ]]; then script="${repo_dir}/scripts/extract_${name}.py"; fi
    if [[ ${gameplay} == 1 && ${name} == multiplayer_gun_fixture ]]; then
        python3 "${script}" "${repo_dir}" "${build_dir}/${name}.c" --gameplay
    else
        python3 "${script}" "${repo_dir}" "${build_dir}/${name}.c"
    fi
done
python3 "${repo_dir}/scripts/extract_player_gait_model_slice.py" \
    "${repo_dir}/src/game/model.c" "${repo_dir}/src/game/initBondDATAdefaults.c" "${build_dir}/model.c"
flags=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
    -ffunction-sections -fdata-sections -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES
    -DGE_PORT_SETUP_DATA -DAIPARSE -DGE_PORT_MS_INHERITS -DGE_PORT_GUN_HOST_MODEL_ABI
    -DGE_PORT_MODEL_HOST_RWDATA_ABI
    -DVERSION_US -DBUGFIX_R0 -fms-extensions -Wno-everything
    -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
sources=(port/tests/test_ge_original_multiplayer_gun_producer.c
    port/src/ge_original_gun_live.c port/src/ge_original_gun_frame_arena.c
    port/src/ge_original_bond_input_provider.c port/src/ge_original_first_person_assets.c
    port/src/ge_original_first_person_item_model.c
    port/src/ge_asset_pack.c port/src/ge_original_player_gait.c port/src/random_port.c
    src/game/matrixmath.c src/libultra/gu/normalize.c src/libultra/gu/rotate.c
    src/libultra/gu/align.c src/libultra/gu/mtxutil.c)
generated=(gun_update_and_fire gun_pose_helpers gun_casing multiplayer_gun_fixture model)
if [[ ${gameplay} == 1 ]]; then
    if [[ $# != 2 ]]; then echo "Gun gameplay needs first-person.gepack and multiplayer-arena.gepack." >&2; exit 2; fi
    flags+=(-DGE_TEST_GUN_GAMEPLAY -DGE_PORT_BOND_INPUT_HOST_STATE)
    python3 "${repo_dir}/scripts/extract_bond_input_live_state_slice.py" "${repo_dir}" "${build_dir}/live_state.c"
    generated+=(live_state)
    sources+=(port/tests/ge_original_multiplayer_gun_gameplay_resources.c
        port/src/ge_original_gameplay_services.c port/src/ge_original_sfx_bank.c
        port/src/ge_original_covert_modem_fire.c port/src/ge_original_pp7_fire.c
        port/src/ge_original_covert_modem_projectile.c
        port/src/ge_original_guard_grenade_object.c port/src/ge_original_covert_modem_object.c
        port/src/ge_original_guard_bullet_hit.c port/src/ge_original_dam_guards.c
        port/src/ge_original_guard_grenade_model.c port/src/ge_original_bug_model.c
        port/src/ge_original_prop_state_source.c port/src/ge_original_objinit_source.c
        port/src/ge_original_default_object.c
        port/src/ge_dam_setup_world_materializer.c
        src/game/quaternion.c port/src/ge_stage_assets.c
        port/src/ge_stan_collision.c port/src/ge_stan_native.c)
fi
objects=()
for i in "${!sources[@]}"; do
    cc "${flags[@]}" -c "${repo_dir}/${sources[i]}" -o "${build_dir}/source_${i}.o"
    objects+=("${build_dir}/source_${i}.o")
done
for name in "${generated[@]}"; do
    cc "${flags[@]}" -c "${build_dir}/${name}.c" -o "${build_dir}/${name}.o"
    objects+=("${build_dir}/${name}.o")
done
cc -fsanitize=address,undefined "${objects[@]}" "${strip[@]}" -lm -o "${build_dir}/test_producer"
python3 - "${gameplay}" "${objects[@]}" <<'PY'
from pathlib import Path
import subprocess
import sys

owners = {}
for obj in map(Path,sys.argv[2:]):
    for line in subprocess.check_output(["nm", "-g", str(obj)], text=True).splitlines():
        fields = line.split()
        if len(fields) >= 3 and fields[-2] in ("T", "D", "B", "S"):
            owners.setdefault(fields[-1].lstrip("_"), []).append(obj.name)
required = {
    "ge_original_gun_update_and_fire_exact": "gun_update_and_fire.o",
    "ge_original_gun_update_and_fire_both_hands_exact": "gun_update_and_fire.o",
    "gunCreateBeamForHand": "gun_update_and_fire.o",
    "CapBeamLengthAndDecideIfRendered": "gun_update_and_fire.o",
    "sub_GAME_7F068508": "gun_casing.o",
    "modelInit": "model.o",
    "wppk_stats": "multiplayer_gun_fixture.o",
    "wppksil_stats": "multiplayer_gun_fixture.o",
    "D_8003FDA0": "multiplayer_gun_fixture.o",
}
if sys.argv[1] == '1':
    required.update(gunTickGameplay='live_state.o', gunTickHandState='live_state.o',
                    gunTickNoise='live_state.o', joyRumblePakStart='multiplayer_gun_fixture.o')
for symbol, owner in required.items():
    assert owners.get(symbol) == [owner], (symbol, owners.get(symbol), owner)
print("link audit: actual source-owned gun, beam, casing, model and authored hand/stat producers")
PY
args=("${pack}")
if [[ ${gameplay} == 1 ]]; then args+=("$2"); fi
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${build_dir}/test_producer" "${args[@]}"

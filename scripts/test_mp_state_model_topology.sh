#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-state-topology.XXXXXX)
model_pack=${1:-"${repo_dir}/build/host-tests/multiplayer-bodies/stages.gepack"}
trap 'rm -rf "${test_dir}"' EXIT
test -f "${model_pack}" || { echo "Missing staged read-only model pack: ${model_pack}" >&2; exit 1; }
python3 "${repo_dir}/scripts/generate_move_model_tables.py" "${repo_dir}" "${test_dir}/tables.c"
python3 "${repo_dir}/scripts/extract_player_gait_model_slice.py" \
    "${repo_dir}/src/game/model.c" "${repo_dir}/src/game/initBondDATAdefaults.c" "${test_dir}/model.c"
python3 - "${repo_dir}" "${test_dir}/physical.c" <<'PY'
import sys
from pathlib import Path
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_player_gait_model_slice import find_function
src=(root/'src/game/model.c').read_text();front=(root/'src/game/front.c').read_text()
Path(sys.argv[2]).write_text('#include <ultra64.h>\n#include <bondtypes.h>\n#include \"game/model.h\"\n'+
    '\n'.join(find_function(src,n) for n in ('modelAttachPart','modelAttachHead'))+'\n'+
    '\n'.join(find_function(front,n) for n in ('disable_all_switches','set_item_visibility_in_objinstance')))
PY
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
flags=(-std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fno-omit-frame-pointer
    -fsanitize=address,undefined -fno-sanitize-recover=all
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE -DVERSION_US
    -DGE_PORT_MODEL_HOST_RWDATA_ABI -DGE_PORT_MS_INHERITS -fms-extensions -Wno-everything
    -I"${repo_dir}" -I"${repo_dir}/port/include"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
for abi in native short; do
    extra=(-fno-short-enums); if [[ ${abi} == short ]]; then extra=(-fshort-enums); fi
    cc "${flags[@]}" "${extra[@]}" -DGE_PORT_MODEL_ANIMATION_CLOCK_SLICE \
        -c "${repo_dir}/port/src/ge_original_model_clock_source.c" -o "${test_dir}/clock.o"
    cc "${flags[@]}" "${extra[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
        -c "${repo_dir}/port/src/ge_original_model_root_source.c" -o "${test_dir}/root.o"
    cc "${flags[@]}" "${extra[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
        -c "${repo_dir}/port/src/ge_original_animation_root.c" -o "${test_dir}/animation.o"
    cc "${flags[@]}" "${extra[@]}" -DPLAYERFLAG=int \
        -c "${repo_dir}/port/src/ge_mp_state_props.c" -o "${test_dir}/props.o"
    cc "${flags[@]}" "${extra[@]}" -DGE_PORT_BOND_INTRO_SPAWN_SLICE \
        -c "${repo_dir}/src/game/bondview_r.c" -o "${test_dir}/spawn.o"
    cc "${flags[@]}" "${extra[@]}" "${test_dir}/model.c" "${test_dir}/physical.c" \
        "${test_dir}/clock.o" "${test_dir}/root.o" "${test_dir}/animation.o" "${test_dir}/spawn.o" \
        "${repo_dir}/port/src/ge_original_player_gait.c" \
        "${repo_dir}/port/src/ge_original_guard_animation_table.c" \
        "${repo_dir}/port/src/ge_original_player_gait_data_source.c" \
        "${repo_dir}/port/src/ge_original_model_callbacks.c" \
        "${repo_dir}/port/src/ge_mp_state.c" "${test_dir}/props.o" \
        "${repo_dir}/port/src/ge_mp_state_model_topology.c" \
        "${repo_dir}/port/src/ge_original_prop_state.c" "${repo_dir}/port/src/ge_original_prop_state_source.c" \
        "${repo_dir}/port/src/ge_original_pitem_models.c" "${repo_dir}/port/src/ge_original_character_models.c" \
        "${repo_dir}/port/src/ge_original_dam_guard_model.c" "${repo_dir}/port/src/ge_original_dam_guard_weapon_model.c" \
        "${repo_dir}/port/src/ge_original_bug_model.c" \
        "${repo_dir}/port/src/ge_asset_pack.c" "${test_dir}/tables.c" "${repo_dir}/src/game/matrixmath.c" \
        "${repo_dir}/src/game/math_floor.c" "${repo_dir}/src/game/math_ceil.c" \
        "${repo_dir}/port/tests/test_ge_mp_state_model_topology.c" \
        "${strip[@]}" -lm -o "${test_dir}/test-${abi}"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${test_dir}/test-${abi}" "${model_pack}"
done

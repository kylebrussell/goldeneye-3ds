#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-port.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
python3 - "${repo_dir}" "${test_dir}/player.c" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_state_slice import extract_function
source=(root/'src/game/player.c').read_text()
text=['#include <ultra64.h>','#include <bondconstants.h>','#include <bondtypes.h>',
      '#include "game/player.h"','extern s32 player_num;',
      'extern struct player_data *g_playerPerm;',
      'extern struct player_data g_playerPlayerData[4];']
text += [extract_function(source,name) for name in ('set_cur_player','get_cur_playernum')]
Path(sys.argv[2]).write_text('\n'.join(text))
PY
for enum in -fno-short-enums -fshort-enums; do
cc "$enum" -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-missing-braces \
-Wno-logical-not-parentheses -Wno-empty-body -Wno-unused-variable \
-Wno-unused-parameter -fsanitize=address,undefined -fno-omit-frame-pointer \
-ffunction-sections -fdata-sections \
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
-DGE_PORT_MS_INHERITS -DAIPARSE -DPLAYERFLAG=int -fms-extensions \
-DGE_PORT_BOSS_STAGE_SLICE -DGE_PORT_LV_STAGE_TICK_SLICE -DVERSION_US \
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
-idirafter "${repo_dir}/src" \
"${test_dir}/player.c" \
"${repo_dir}/port/src/ge_port.c" "${repo_dir}/port/src/ge_libultra.c" \
"${repo_dir}/port/src/ge_original_boss.c" \
"${repo_dir}/port/src/ge_original_input.c" \
"${repo_dir}/port/src/ge_original_ramrom_replay.c" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${repo_dir}/port/src/ge_original_level.c" "${repo_dir}/port/src/random_port.c" \
"${repo_dir}/src/joy.c" "${repo_dir}/src/boss.c" \
"${repo_dir}/src/game/lv.c" "${repo_dir}/src/game/frametiming.c" \
"${repo_dir}/src/game/quaternion.c" \
"${repo_dir}/port/tests/test_ge_port_multiplayer_tick.c" "${strip[@]}" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"
done

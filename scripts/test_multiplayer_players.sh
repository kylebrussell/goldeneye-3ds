#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-players.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_multiplayer_players_slice.py" "${repo_dir}" "${test_dir}/players.c"
python3 - "${repo_dir}" "${test_dir}/support.c" <<'PY'
import sys
from pathlib import Path
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_state_slice import extract_function,extract_variable
p=(root/'src/game/player.c').read_text()
s=['#include <ultra64.h>','#include <bondconstants.h>','#include <bondtypes.h>','#include "game/player.h"', 's32 g_VisibleToGuardsFlag;']
for name in ('g_playerPointers','g_CurrentPlayer','g_playerPlayerData','g_playerPerm','player_num'):
    s.append(extract_variable(p,name))
for name in ('set_cur_player','get_cur_playernum','getPlayerCount','default_player_perspective_and_height'):
    s.append(extract_function(p,name))
Path(sys.argv[2]).write_text('\n'.join(s))
PY
flags=(-std=gnu11 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter
-Wno-missing-braces -Wno-empty-body -Wno-logical-not-parentheses
-fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
sources=("${test_dir}/players.c" "${test_dir}/support.c"
"${repo_dir}/port/src/ge_original_multiplayer_players.c"
"${repo_dir}/port/src/ge_original_bond_input_provider.c"
"${repo_dir}/port/src/ge_original_input.c"
"${repo_dir}/port/src/ge_libultra.c" "${repo_dir}/port/src/ge_services.c"
"${repo_dir}/port/src/random_port.c" "${repo_dir}/src/joy.c"
"${repo_dir}/port/src/ge_original_chr_obj_random.c"
"${repo_dir}/port/tests/test_ge_original_multiplayer_players.c")
cc "${flags[@]}" -UPLAYERFLAG -Wno-switch -DGE_PORT_BOND_INTRO_SPAWN_SLICE -c "${repo_dir}/src/game/bondview_r.c" -o "${test_dir}/spawn.o"
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" "${sources[@]}" "${test_dir}/spawn.o" "${strip[@]}" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"

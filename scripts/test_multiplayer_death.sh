#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-death.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_multiplayer_death_slice.py" "${repo_dir}" "${test_dir}/death.c"
python3 - "${repo_dir}" "${test_dir}/support.c" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_state_slice import extract_function
text=['#include <ultra64.h>','#include <bondconstants.h>','#include <bondtypes.h>',
      '#include <macro.h>','#include "game/player.h"','#include "game/model.h"',
      'extern u8 *g_GfxMemPos;', 'extern s32 player_num;',
      'extern struct player_data *g_playerPerm;',
      'extern struct player_data g_playerPlayerData[4];']
for file,names in [('player.c',('getPlayerCount','get_cur_playernum','set_cur_player')),
                   ('model.c',('modelGetAnimFrame','modelGetAnimEndFrame')),
                   ('dyn.c',('dynAllocate',))]:
 source=(root/'src/game'/file).read_text()
 text += [extract_function(source,name) for name in names]
Path(sys.argv[2]).write_text('\n'.join(text))
PY
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pointer-sign \
-Wno-incompatible-pointer-types -Wno-int-conversion -Wno-pointer-to-int-cast \
-Wno-unused-but-set-variable -Wno-unused-variable -Wno-missing-braces \
-fsanitize=address,undefined -fno-omit-frame-pointer \
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
-DGE_PORT_BLOOD_DECODE_SLICE -DGE_PORT_MS_INHERITS -DAIPARSE \
-DVERSION_US -DPLAYERFLAG=int -fms-extensions \
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
-idirafter "${repo_dir}/src" \
"${test_dir}/death.c" "${test_dir}/support.c" \
"${repo_dir}/port/src/ge_original_multiplayer_death.c" \
"${repo_dir}/port/src/ge_original_gun_frame_arena.c" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${repo_dir}/src/game/blood_animation.c" \
"${repo_dir}/src/game/blood_decrypt.c" \
"${repo_dir}/port/tests/test_ge_original_multiplayer_death.c" \
-o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"

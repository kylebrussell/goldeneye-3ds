#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
if [[ -n ${GE_MP_GUN_EVIDENCE_DIR:-} ]]; then
    mkdir -p "${GE_MP_GUN_EVIDENCE_DIR}"
    test_dir=$(cd "${GE_MP_GUN_EVIDENCE_DIR}" && pwd)
else
    test_dir=$(mktemp -d /tmp/ge-mp-state-guns.XXXXXX)
    trap 'rm -rf "${test_dir}"' EXIT
fi
# Run both complete ABI builds by default; no object is shared between them.
if [[ -z ${GE_MP_GUN_SHORT_ENUMS+x} ]]; then
    GE_MP_GUN_SHORT_ENUMS=0 GE_MP_GUN_EVIDENCE_DIR="${test_dir}/native" "${BASH_SOURCE[0]}"
    GE_MP_GUN_SHORT_ENUMS=1 GE_MP_GUN_EVIDENCE_DIR="${test_dir}/short-enum" "${BASH_SOURCE[0]}"
    for players in 2 4; do cmp "${test_dir}/native/${players}p-a.bin" "${test_dir}/short-enum/${players}p-a.bin"; done
    echo "gun diagnostics: native and short-enum canonical bytes also match"
    exit 0
fi
python3 "${repo_dir}/scripts/extract_multiplayer_players_slice.py" "${repo_dir}" "${test_dir}/players.c"
python3 - "${repo_dir}" "${test_dir}" <<'PY'
import sys,hashlib,json
from pathlib import Path
root,out=map(Path,sys.argv[1:]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_state_slice import extract_function,extract_variable
p=(root/'src/game/player.c').read_text()
s=['#include <ultra64.h>','#include <bondconstants.h>','#include <bondtypes.h>','#include "game/player.h"', 's32 g_VisibleToGuardsFlag;']
for name in ('g_playerPointers','g_CurrentPlayer','g_playerPlayerData','g_playerPerm','player_num'):
    s.append(extract_variable(p,name))
for name in ('set_cur_player','get_cur_playernum','getPlayerCount','default_player_perspective_and_height'):
    s.append(extract_function(p,name))
(out/'support.c').write_text('\n'.join(s))
gun=(root/'src/game/gun.c').read_text();model=(root/'src/game/model.c').read_text();dyn=(root/'src/game/dyn.c').read_text()
s=['#include <ultra64.h>','#include <macro.h>','#include <bondconstants.h>','#include <bondtypes.h>','#include "game/player.h"','#include "game/gun.h"','#include "game/model.h"','extern u8 *g_GfxMemPos;']
receipt={}
for source,names in [(gun,('getCurrentPlayerWeaponId','get_next_weapon_in_cycle_for_hand','gunRequestHandWeaponChange','draw_item_in_hand','sub_GAME_7F05E808')),(model,('modelGetNodeRwData','modelApplyReorderRelationsByArg','modelApplyReorderRelations','modelInitRwData','modelInit')),(dyn,('dynAllocate',))]:
    for name in names:
        body=extract_function(source,name);receipt[name]=hashlib.sha256(body.encode()).hexdigest();s.append(body)
(out/'native.c').write_text('\n'.join(s));(out/'extracted-functions.json').write_text(json.dumps(receipt,indent=2)+'\n')
PY
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter
-Wno-missing-braces -Wno-empty-body -Wno-logical-not-parentheses -Wno-comment
-Wno-incompatible-pointer-types
-fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
if [[ ${GE_MP_GUN_SHORT_ENUMS:-0} == 1 ]]; then flags+=(-fshort-enums); fi
sources=("${test_dir}/players.c" "${test_dir}/support.c" "${test_dir}/native.c"
"${repo_dir}/port/src/ge_original_multiplayer_players.c"
"${repo_dir}/port/src/ge_original_bond_input_provider.c"
"${repo_dir}/port/src/ge_original_gun_frame_arena.c"
"${repo_dir}/port/src/ge_original_input.c"
"${repo_dir}/port/src/ge_libultra.c" "${repo_dir}/port/src/ge_services.c"
"${repo_dir}/port/src/random_port.c" "${repo_dir}/src/joy.c"
"${repo_dir}/port/src/ge_original_chr_obj_random.c"
"${repo_dir}/port/src/ge_mp_state.c" "${repo_dir}/port/src/ge_mp_state_guns.c"
"${repo_dir}/port/tests/test_ge_mp_state_guns.c")
cc "${flags[@]}" -UPLAYERFLAG -Wno-switch -DGE_PORT_BOND_INTRO_SPAWN_SLICE -c "${repo_dir}/src/game/bondview_r.c" -o "${test_dir}/spawn.o"
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" "${sources[@]}" "${test_dir}/spawn.o" "${strip[@]}" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test" | tee "${test_dir}/tests.log"
for players in 2 4; do
    for repeat in a b; do
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test" "${players}" "${test_dir}/${players}p-${repeat}.bin"
    done
    cmp "${test_dir}/${players}p-a.bin" "${test_dir}/${players}p-b.bin"
done
python3 - "${test_dir}" <<'PY'
from pathlib import Path
import hashlib,json,sys
root=Path(sys.argv[1]);result={}
for count in (2,4):
    b=(root/f'{count}p-a.bin').read_bytes();at=0;n=0
    while at<len(b):
        size=int.from_bytes(b[at:at+4],'big');at+=4
        assert hashlib.sha256(b[at:at+size]).digest()==b[at+size:at+size+32]
        at+=size+32;n+=1
    assert at==len(b) and n==32
    result[f'{count}p']={'records':n,'sha256':hashlib.sha256(b).hexdigest()}
(root/'transcript-receipt.json').write_text(json.dumps(result,indent=2)+'\n')
print('gun diagnostics: independent fresh processes match for 2/4 canonical player owners, 32 weapon-change steps; Python verifies every SHA-256; not a committed gameplay/transcript convergence claim')
PY

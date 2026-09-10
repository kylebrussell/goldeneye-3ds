#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-stage-reset.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 - "${repo_dir}" "${test_dir}/collision.c" <<'PY'
from pathlib import Path
import sys
repo=Path(sys.argv[1]);sys.path.insert(0,str(repo/'scripts'))
from extract_guard_bullet_hit_slice import find_function
body=find_function((repo/'src/game/bondview2.c').read_text(),'bondviewUpdatePlayerCollisionPositionFields')
preamble='''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "game/bondview.h"
#include "game/stan.h"
extern s32 g_ClockTimer, stanlinelog_flag;
extern enum CAMERAMODE g_CameraMode;
extern f32 bondviewYPositionRelated(StandTile *, f32, f32);
'''
move=find_function((repo/'src/game/bondview2.c').read_text(),'MoveBond')
force=move[move.index('    if (g_bondviewForceDisarm > 0)'):move.index('    currentPlayerSetCameraMode(0);')]
life=find_function((repo/'src/game/bondview.c').read_text(),'bondviewPlayerBeginLife')
timer=find_function((repo/'src/game/bondview2.c').read_text(),'getMissiontimer')
preamble += "extern s32 g_bondviewForceDisarm, mission_timer;\nextern ITEM_IDS starting_weapon[2];\nextern void solo_char_load(void);\n#include \"game/gun.h\"\n#include \"game/bondinv.h\"\n"
Path(sys.argv[2]).write_text(preamble+body+'\n'+timer+'\n'+life+'\nvoid fixture_tick_original_force_disarm(void) {\n'+force+'}\n')
from extract_multiplayer_stage_reset_slice import generate,declarations,PREAMBLE,statements
Path(sys.argv[2]).with_name('reset.c').write_text(generate(repo))
existing=('g_CameraMode','g_bondviewForceDisarm','mission_timer','watch_time_0','starting_weapon')
Path(sys.argv[2]).with_name('globals.c').write_text(PREAMBLE+declarations(repo,False,existing))
Path(sys.argv[2]).with_name('reset_globals.h').write_text(PREAMBLE+declarations(repo))
combat_existing=('g_PlayerIsInTank','g_isBondKIA','g_ExplodeTankOnDeathFlag','g_PlayerInvincible','g_PlayerTankProp')
Path(sys.argv[2]).with_name('combat_globals.c').write_text(PREAMBLE+declarations(repo,False,combat_existing))
pickup=find_function((repo/'src/game/chrprop.c').read_text(),'propsTickPlayer')
Path(sys.argv[2]).with_name('pickup.c').write_text(PREAMBLE+"\nextern PropRecord *chrpropGetActiveTail(void);\nextern TICKOP objTickPlayer(PropRecord *);\nextern TICKOP weaponTickPlayer(PropRecord *);\nextern void propExecuteTickOperation(PropRecord *, TICKOP);\n"+pickup)
main=find_function((repo/'platform/3ds/source/main.c').read_text(),'runtime_multiplayer_place_players')
assert main.index('ge_original_multiplayer_configure(') < main.index('ge_original_multiplayer_stage_intro_shared_reset_exact();') < main.index('ge_original_multiplayer_spawn_initial(')
assert main.index('ge_original_stage_multiplayer_setup_prepare(') < main.index('ge_original_multiplayer_stage_intro_shared_reset_exact();')
spawn=(repo/'port/src/ge_original_multiplayer_spawn.c').read_text()
assert 'ge_original_multiplayer_publish_spawns(' in find_function(spawn,'publish_setup')
assert find_function(spawn,'ge_original_multiplayer_spawn_initial').index('publish_setup(providers)') < find_function(spawn,'ge_original_multiplayer_spawn_initial').index('bondviewGetRandomSpawnPadIndex()')

body=statements(repo)
assert body.index('startpadcount = 0;') < body.index('starting_weapon[GUNLEFT] = ITEM_UNARMED;')
PY
flags=(-std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack -Wno-unused-variable
-Wno-unused-parameter -Wno-missing-braces -Wno-sign-compare
-fsanitize=address,undefined -fno-omit-frame-pointer
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DBUGFIX_R0 -fms-extensions
-I"${test_dir}" -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
cc "${flags[@]}" "${test_dir}/collision.c" "${test_dir}/reset.c" "${test_dir}/globals.c" \
"${repo_dir}/port/src/ge_original_multiplayer_stage_reset.c" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${repo_dir}/port/tests/test_ge_original_multiplayer_stage_reset.c" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"

python3 "${repo_dir}/scripts/extract_multiplayer_combat_fixture.py" "${repo_dir}" "${test_dir}/combat.c"
flags+=(-Wno-unused-but-set-variable -Wno-incompatible-pointer-types -Wno-empty-body
-Wno-uninitialized -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
-Wno-int-conversion -Wno-format-security -Wno-pointer-bool-conversion
-Wno-deprecated-declarations -Wno-pointer-sign -ffunction-sections -fdata-sections)
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" -c "${repo_dir}/port/src/ge_original_guard_bullet_hit.c" -o "${test_dir}/hit.o"
cc "${flags[@]}" -DPLAYERFLAG=int "${test_dir}/combat.c" "${test_dir}/reset.c" \
"${test_dir}/combat_globals.c" "${test_dir}/pickup.c" "${test_dir}/hit.o" \
"${repo_dir}/port/src/ge_original_multiplayer_stage_reset.c" \
"${repo_dir}/src/game/matrixmath.c" \
"${repo_dir}/port/tests/test_ge_original_multiplayer_stage_combat_reset.c" \
"${strip[@]}" -lm -o "${test_dir}/combat"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/combat"

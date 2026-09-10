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
source=find_function((repo/'src/game/bondview_r.c').read_text(),'bondviewLoadSetupIntroSection')
statements=('g_bondviewForceDisarm = 0;', 'g_CameraMode = 0;', 'mission_timer = 0;', 'watch_time_0 = 0;')
positions=[source.index(s) for s in statements]
assert positions == sorted(positions) and positions[-1] < source.index('if (intro_record != NULL)')
adapter=(repo/'port/src/ge_original_multiplayer_stage_reset.c').read_text()
positions=[adapter.index(s) for s in statements]
assert positions == sorted(positions)
PY
flags=(-std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack -Wno-unused-variable
-Wno-unused-parameter -Wno-missing-braces -Wno-sign-compare
-fsanitize=address,undefined -fno-omit-frame-pointer
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DBUGFIX_R0 -fms-extensions
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
cc "${flags[@]}" "${test_dir}/collision.c" \
"${repo_dir}/port/src/ge_original_multiplayer_stage_reset.c" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${repo_dir}/port/tests/test_ge_original_multiplayer_stage_reset.c" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"

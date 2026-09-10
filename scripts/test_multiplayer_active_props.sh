#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-active.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 - "${repo_dir}" "${test_dir}/canonical.c" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_state_slice import extract_function
player=(root/'src/game/player.c').read_text()
props=(root/'src/game/chrprop.c').read_text()
text=['#include <ultra64.h>','#include <bondconstants.h>','#include <bondtypes.h>',
      '#include "game/player.h"','PLAYER_ID array_PLAYER_IDs[4];',
      'extern PropRecord *g_ActivePropsHead, *g_ActivePropsTail;']
text += [extract_function(player,'get_nth_player_from_shuffled')]
text += [extract_function(props,name) for name in ('chrpropDelist','chrpropActivateThisFrame')]
Path(sys.argv[2]).write_text('\n'.join(text))
PY
cc -std=gnu11 -Wall -Wextra -Werror -Wno-missing-braces -Wno-comment \
-fsanitize=address,undefined -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
-DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int \
-fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
-I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
-idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
"${repo_dir}/port/src/ge_original_stage_active_props.c" \
"${test_dir}/canonical.c" \
"${repo_dir}/port/tests/test_ge_original_multiplayer_active_props.c" \
-o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"

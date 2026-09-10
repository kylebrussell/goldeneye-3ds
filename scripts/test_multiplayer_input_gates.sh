#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-input-gates.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_multiplayer_input_gates_fixture.py" "${repo_dir}" "${test_dir}/support.c"
flags=(-std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack -Wno-unused-variable
-Wno-unused-parameter -Wno-unused-but-set-variable -Wno-incompatible-pointer-types
-Wno-missing-braces -Wno-empty-body -Wno-sign-compare -Wno-uninitialized
-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-int-conversion
-fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DBUGFIX_R0 -fms-extensions
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" -DGE_PORT_BOND_INPUT_FULL_SLICE -c \
"${repo_dir}/port/tests/ge_original_bond_input_source.c" -o "${test_dir}/input.o"
cc "${flags[@]}" "${test_dir}/input.o" "${test_dir}/support.c" \
"${repo_dir}/port/src/ge_original_bond_live.c" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${repo_dir}/port/tests/test_ge_original_multiplayer_input_gates.c" \
"${strip[@]}" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"
cc -std=c11 -Wall -Wextra -Werror -pedantic -I"${repo_dir}/port/include" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${repo_dir}/port/tests/test_ge_original_bond_input_provider.c" -o "${test_dir}/solo"
"${test_dir}/solo"
python3 - "${repo_dir}" <<'PY'
from pathlib import Path
import sys
repo=Path(sys.argv[1]);sys.path.insert(0,str(repo/'scripts'))
from extract_guard_bullet_hit_slice import find_function
original=find_function((repo/'src/game/mpmenu.c').read_text(),'disablePlayerActionsWhenPausedOrInMpMenu')
native=find_function((repo/'port/src/ge_original_bond_live.c').read_text(),'ge_original_bond_live_actions_enabled_exact')
assert 'static '+original.replace('disablePlayerActionsWhenPausedOrInMpMenu','ge_original_bond_live_actions_enabled_exact')==native
print('source audit: native action query matches mpmenu.c exactly')
PY

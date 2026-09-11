#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT
python3 - "${repo_dir}" "${test_dir}/switch_exact.inc" <<'PY'
import sys
from pathlib import Path
root = Path(sys.argv[1])
sys.path.insert(0, str(root / 'scripts'))
from extract_door_character_collision_slice import extract_function
prop = (root / 'src/game/propobj.c').read_text()
chrprop = (root / 'src/game/chrprop.c').read_text()
names = ('chrobjApplySpeed', 'updateDoorDisplacement', 'doorSetOpenState',
         'doorActivate', 'doorActivateWrapper')
parts = [extract_function(prop, name) for name in names]
parts.append(extract_function(chrprop, 'sub_GAME_7F03E6A0'))
# Retain the exact propTick interlock waiting block before displacement.
start = prop.index('if (door->openstate == DOORSTATE_WAITING)', prop.index('previousOpenPosition = door->openPosition;'))
brace = prop.index('{', start)
depth = 0
for end in range(brace, len(prop)):
    depth += (prop[end] == '{') - (prop[end] == '}')
    if not depth:
        break
parts.append('static void tick_waiting(DoorRecord *door) { DoorRecord *pad1AC; s32 var_v1_5;\n' + prop[start:end+1] + '\n}')
Path(sys.argv[2]).write_text('\n\n'.join(parts))
PY
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -Wno-microsoft-anon-tag -Wno-incompatible-pointer-types \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US -fms-extensions \
    -I"${test_dir}" -I"${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/src/ge_original_stage_switch_runtime.c" \
    "${repo_dir}/port/tests/test_ge_original_stage_switch_runtime.c" \
    -lm -o "${test_dir}/test_switch"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test_switch"

#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/multiplayer-runtime"
mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_runtime_slice.py" "${repo_dir}" "${build_dir}/interaction.c"
python3 - "${repo_dir}" "${build_dir}/timers.c" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_state_slice import extract_function
source=(root/'src/game/bondview2.c').read_text()
mission='s32 mission_timer;'
assert mission in source
camera=(root/'src/game/bondview.c').read_text()
declarations=['enum CAMERAMODE g_CameraMode = CAMERAMODE_NONE;', 's32 is_timer_active = 1;']
assert all(d in camera for d in declarations)
Path(sys.argv[2]).write_text('#include "game/bondview.h"\n'+mission+'\n'+'\n'.join(declarations)+'\n'+extract_function(source,'getMissiontimer')+'\n')
PY
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-comment -Wno-missing-braces -Wno-unused-variable \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions \
    -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_original_multiplayer_runtime.c" \
    "${repo_dir}/port/tests/test_ge_original_multiplayer_runtime.c" \
    "${repo_dir}/src/game/frametiming.c" \
    "${build_dir}/interaction.c" "${build_dir}/timers.c" \
    -o "${build_dir}/test_runtime"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test_runtime"

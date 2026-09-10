#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-guns.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 - "${repo_dir}" "${test_dir}/dyn.c" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_state_slice import extract_function
source=(root/'src/game/dyn.c').read_text()
Path(sys.argv[2]).write_text('#include <ultra64.h>\n#include <macro.h>\nextern u8 *g_GfxMemPos;\n'+extract_function(source,'dynAllocate'))
PY
cc -std=gnu11 -Wall -Wextra -Werror -Wno-missing-braces -Wno-comment \
-fsanitize=address,undefined -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
-DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
-fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
-I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
-idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
"${repo_dir}/port/src/ge_original_gun_live.c" \
"${repo_dir}/port/src/ge_original_gun_frame_arena.c" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${test_dir}/dyn.c" \
"${repo_dir}/port/tests/test_ge_original_multiplayer_gun_ownership.c" \
-o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"

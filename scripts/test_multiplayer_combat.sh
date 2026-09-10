#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-combat.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_multiplayer_combat_fixture.py" "${repo_dir}" "${test_dir}/combat.c"
flags=(-std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack
-Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-incompatible-pointer-types
-Wno-missing-braces -Wno-empty-body -Wno-sign-compare -Wno-uninitialized
-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-format-security
-Wno-int-conversion -Wno-pointer-bool-conversion -Wno-deprecated-declarations
-Wno-pointer-sign
-fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int -fms-extensions
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" -UPLAYERFLAG -c "${repo_dir}/port/src/ge_original_guard_bullet_hit.c" -o "${test_dir}/adapter.o"
cc "${flags[@]}" "${test_dir}/combat.c" "${test_dir}/adapter.o" \
"${repo_dir}/src/game/matrixmath.c" \
"${repo_dir}/port/tests/test_ge_original_multiplayer_combat.c" \
"${strip[@]}" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"
python3 - "${repo_dir}" "${test_dir}/combat.c" <<'PY'
from pathlib import Path
import sys
repo = Path(sys.argv[1]); generated = Path(sys.argv[2]).read_text()
sys.path.insert(0, str(repo / 'scripts'))
from extract_multiplayer_combat_fixture import FUNCTIONS
from extract_guard_bullet_hit_slice import find_function
for path, names in FUNCTIONS.items():
    source = (repo / 'src/game' / path).read_text()
    for name in names:
        body = find_function(source, name).replace(
            's16 mrs[3] = metal_ricochet_SFX;',
            's16 mrs[3] = {HIT_BULLET_METAL_A3_SFX, HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_B_SFX};')
        assert body in generated, (path, name)
print('source audit: canonical ray, model BBOX, hit dispatch, player damage and score bodies retained')
PY

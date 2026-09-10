#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-held-items.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_player_held_items_slice.py" "${repo_dir}" "${test_dir}/held.c"
python3 "${repo_dir}/scripts/extract_multiplayer_combat_fixture.py" "${repo_dir}" "${test_dir}/combat.c"
python3 "${repo_dir}/scripts/extract_player_held_items_fixture.py" "${repo_dir}" "${test_dir}/support.c"
flags=(-std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack
-Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-incompatible-pointer-types
-Wno-missing-braces -Wno-empty-body -Wno-sign-compare -Wno-uninitialized
-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-format-security
-Wno-int-conversion -Wno-pointer-bool-conversion -Wno-deprecated-declarations -Wno-pointer-sign
-fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int -fms-extensions
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" -UPLAYERFLAG -c "${repo_dir}/port/src/ge_original_guard_bullet_hit.c" -o "${test_dir}/hit.o"
cc "${flags[@]}" "${test_dir}/held.c" "${test_dir}/combat.c" "${test_dir}/support.c" "${test_dir}/hit.o" \
"${repo_dir}/port/src/ge_original_player_held_items.c" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${repo_dir}/port/src/ge_original_stage_objective_photo_exact.c" \
"${repo_dir}/port/src/random_port.c" "${repo_dir}/src/game/matrixmath.c" \
"${repo_dir}/port/tests/test_ge_original_player_held_items.c" \
"${strip[@]}" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test"
cc "${flags[@]}" -UPLAYERFLAG \
"${repo_dir}/port/src/ge_original_gameplay_services.c" \
"${repo_dir}/port/src/ge_audio_output.c" \
"${repo_dir}/port/tests/test_ge_original_held_item_service_ownership.c" \
"${strip[@]}" -lm -o "${test_dir}/services"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/services"
python3 - "${repo_dir}" "${test_dir}/held.c" <<'PY'
from pathlib import Path
import sys
repo=Path(sys.argv[1]); generated=Path(sys.argv[2]).read_text()
sys.path.insert(0, str(repo/'scripts'))
from extract_player_held_items_slice import FUNCTIONS, native_body
from extract_guard_bullet_hit_slice import find_function
source=(repo/'src/game/chrprop.c').read_text()
for name in FUNCTIONS:
    assert native_body(find_function(source, name), name) in generated, name
lv=find_function((repo/'src/game/lv.c').read_text(), 'lvlRender')
order=('chrpropUpdateAutoaimTarget();', 'chraiCheckUseHeldItems();',
       'bond_pressed_reload_activate()', 'propsTickPlayer();')
positions=[lv.index(item) for item in order]
assert positions == sorted(positions)
viewport=find_function((repo/'src/game/bondview2.c').read_text(), 'bondviewMovePlayerUpdateViewport')
start=viewport.index('    gunSetSightVisible(')
end=viewport.index('\n    );', start)+len('\n    );')
assert viewport[start:end] in generated
print('source audit: exact held-item dispatch, hitscan/melee bodies and sight call; explicit native ABI/frontier edits only')
PY

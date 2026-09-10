#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-main-state-lifecycle.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 - "${repo_dir}" "${test_dir}" <<'PY'
from pathlib import Path
import sys
root, out = map(Path, sys.argv[1:])
sys.path.insert(0, str(root / 'scripts'))
from extract_stage_pickup_slice import function_text
from extract_mp_state_inventory_fixture import render
from extract_bond_move_explosion_slice import generate
main = (root / 'platform/3ds/source/main.c').read_text()
start = main.index('/* Identities are scoped to one canonical match session.')
end = main.index('static stagesetup *dam_intro_load_setup', start)
(out / 'main_state_lifecycle.c').write_text(main[start:end])
(out / 'main_object_bindings.c').write_text('\n'.join(function_text(main,name) for name in (
    'runtime_multiplayer_object_key_compare','runtime_multiplayer_state_begin_objects')))
(out / 'main_model_bindings.c').write_text(function_text(main,'runtime_multiplayer_state_begin_models'))
source = (root / 'src/game/chrprop.c').read_text()
parts = [render(root), '#include "ge_original_prop_lifetime.h"',
         'extern PropRecord *g_FreeProps,*g_ActivePropsHead,*g_ActivePropsTail;']
parts.append(function_text(generate(root), 'chrpropFree'))
parts.extend(function_text(source, name) for name in ('chrpropDelist', 'chrpropDisable'))
parts.append(function_text(source, 'chrpropFree').replace('chrpropFree(', 'fixture_unobserved_free(', 1))
(out / 'support.c').write_text('\n'.join(parts))
PY
python3 "${repo_dir}/scripts/extract_multiplayer_players_slice.py" "${repo_dir}" "${test_dir}/players.c"
flags=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
    -DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions -Wno-everything
    -I"${test_dir}" -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
sources=(ge_mp_state ge_mp_state_inventory ge_mp_state_props ge_mp_state_players ge_mp_state_objects ge_mp_state_models
    ge_original_prop_state ge_original_prop_state_source ge_original_multiplayer_players
    ge_original_multiplayer_rules ge_original_bond_input_provider ge_original_input
    ge_libultra ge_services random_port ge_original_chr_obj_random)
for i in "${!sources[@]}"; do sources[i]="${repo_dir}/port/src/${sources[i]}.c"; done
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
for abi in native short; do
    enum=(-fno-short-enums); if [[ $abi == short ]]; then enum=(-fshort-enums); fi
    cc "${flags[@]}" "${enum[@]}" -UPLAYERFLAG -DGE_PORT_BOND_INTRO_SPAWN_SLICE \
        -c "${repo_dir}/src/game/bondview_r.c" -o "${test_dir}/spawn-${abi}.o"
    cc "${flags[@]}" "${enum[@]}" "${sources[@]}" "${repo_dir}/src/joy.c" \
        "${repo_dir}/port/tests/test_ge_multiplayer_main_state_lifecycle.c" \
        "${test_dir}/players.c" "${test_dir}/support.c" "${test_dir}/spawn-${abi}.o" \
        "${strip[@]}" -lm -o "${test_dir}/test-${abi}"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "${test_dir}/test-${abi}"
done

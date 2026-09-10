#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/mp-state-props"
mkdir -p "${build_dir}"
python3 - "${repo_dir}" "${build_dir}/support.c" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_explosion_slice import generate
from extract_stage_pickup_slice import function_text
source=(root/'src/game/chrprop.c').read_text()
runtime=generate(root)
s=['#include <ultra64.h>','#include <bondconstants.h>','#include <bondtypes.h>',
   '#include "ge_original_prop_lifetime.h"',
   'extern PropRecord *g_FreeProps,*g_ActivePropsHead,*g_ActivePropsTail;']
free=function_text(runtime,'chrpropFree')
assert free.index('ge_original_prop_observe_free(prop)') < free.index('prop->prev = g_FreeProps')
s.append(free)
s.extend(function_text(source,n) for n in ('chrpropDelist','chrpropDisable','chrpropReparent','chrpropDetach'))
Path(sys.argv[2]).write_text('\n'.join(s))
for stem,internal in (('guard_grenade_object','ge_grenade_prop_free'),('covert_modem_object','ge_original_covert_modem_prop_free')):
    Path(sys.argv[2]).with_name(stem+'.c').write_text(
        '#include "'+str(root/'port/src'/('ge_original_'+stem+'.c'))+'"\n'
        +'void fixture_'+stem+'_rollback(void *prop) { '+internal+'(prop); }\n')
PY
flags=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions -Wno-everything
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" "${repo_dir}/port/src/ge_mp_state.c" "${repo_dir}/port/src/ge_mp_state_props.c" \
    "${repo_dir}/port/src/ge_original_prop_state.c" "${repo_dir}/port/src/ge_original_prop_state_source.c" \
    "${repo_dir}/port/tests/test_ge_mp_state_props.c" "${build_dir}/support.c" \
    "${build_dir}/guard_grenade_object.c" "${build_dir}/covert_modem_object.c" \
    "${strip[@]}" -lm -o "${build_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${build_dir}/test"

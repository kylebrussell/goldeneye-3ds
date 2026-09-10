#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-prop-stage-end.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 - "${repo_dir}" "${build_dir}/support.c" <<'PY'
from pathlib import Path
import sys
repo=Path(sys.argv[1]);sys.path.insert(0,str(repo/'scripts'))
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import generate
source=(repo/'src/game/chrprop.c').read_text()
parts=['#include <ultra64.h>','#include <bondconstants.h>','#include <bondtypes.h>',
       '#include "ge_original_prop_lifetime.h"',
       'extern PropRecord *g_FreeProps,*g_ActivePropsHead,*g_ActivePropsTail;']
parts.append(function_text(generate(repo),'chrpropFree'))
parts.extend(function_text(source,n) for n in ('chrpropDelist','chrpropDisable'))
# Deliberate missing-observer negative controls retain the actual operations.
parts.append(function_text(source,'chrpropFree').replace('chrpropFree(', 'fixture_unobserved_free(',1))
parts.append(function_text(source,'chrpropAllocate').replace('chrpropAllocate(', 'fixture_unobserved_allocate(',1))
Path(sys.argv[2]).write_text('\n'.join(parts))
PY
flags=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions -Wno-everything
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" "${repo_dir}/port/src/ge_mp_state.c" "${repo_dir}/port/src/ge_mp_state_props.c" \
    "${repo_dir}/port/src/ge_original_prop_state.c" "${repo_dir}/port/src/ge_original_prop_state_source.c" \
    "${repo_dir}/port/tests/test_ge_original_prop_stage_end.c" "${build_dir}/support.c" \
    "${strip[@]}" -lm -o "${build_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${build_dir}/test"

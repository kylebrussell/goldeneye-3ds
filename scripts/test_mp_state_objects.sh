#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-state-objects.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_mp_state_objects_fixture.py" "${repo_dir}" "${build_dir}/support.c"
flags=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS
-DVERSION_US -DPLAYERFLAG=int -fms-extensions -Wno-everything -I"${repo_dir}" -I"${repo_dir}/port/include"
-I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
if (( $# ));then flags+=("$@");fi
if [[ $(uname -s) == Darwin ]];then strip=(-Wl,-dead_strip);else strip=(-Wl,--gc-sections);fi
cc "${flags[@]}" "${repo_dir}/port/src/ge_mp_state.c" "${repo_dir}/port/src/ge_mp_state_props.c" \
    "${repo_dir}/port/src/ge_mp_state_objects.c" "${repo_dir}/port/src/ge_original_prop_state.c" \
    "${repo_dir}/port/src/ge_original_prop_state_source.c" "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${build_dir}/support.c" "${repo_dir}/port/tests/test_ge_mp_state_objects.c" \
    "${strip[@]}" -lm -o "${build_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${build_dir}/test"

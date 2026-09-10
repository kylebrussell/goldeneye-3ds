#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/multiplayer-rules"
mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_rules_slice.py" "${repo_dir}" "${build_dir}/rules.c"
python3 "${repo_dir}/scripts/extract_multiplayer_rules_fixture.py" "${repo_dir}" "${build_dir}/support.c"
cc -std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -D_LANGUAGE_C -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA -DVERSION_US -DPLAYERFLAG=int \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src" -I"${repo_dir}/src/game" -I"${repo_dir}" \
    -I"${repo_dir}/include/PR" -idirafter "${repo_dir}/include" -Wno-everything \
    "${build_dir}/rules.c" "${build_dir}/support.c" \
    "${repo_dir}/port/src/ge_original_multiplayer_rules.c" \
    "${repo_dir}/src/game/mp_weapon.c" \
    "${repo_dir}/port/tests/test_ge_original_multiplayer_rules.c" \
    -lm -o "${build_dir}/test_multiplayer_rules"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "${build_dir}/test_multiplayer_rules"

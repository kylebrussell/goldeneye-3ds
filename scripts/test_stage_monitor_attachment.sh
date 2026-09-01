#!/usr/bin/env bash

set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir="${repo_dir}/build/host-tests/stage-monitor-attachment"
mkdir -p "${test_dir}"

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
fi

cc -std=gnu11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_monitor_attachment.c" \
    "${repo_dir}/port/src/ge_original_stage_monitor.c" \
    "${repo_dir}/src/game/matrixmath.c" \
    -lm "${dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_stage_monitor_attachment"

"${test_dir}/test_ge_original_stage_monitor_attachment"

#!/usr/bin/env bash
set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-dam-objective-status.XXXXXX")
trap 'rm -rf "${test_dir}"' EXIT

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_dam_objective_status.c" \
    "${repo_dir}/port/src/ge_original_dam_objective_status.c" \
    "${repo_dir}/port/src/ge_original_stage_objective_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_objective_live.c" \
    "${repo_dir}/assets/obseg/text/LmiscE.c" \
    "${dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_dam_objective_status"

"${test_dir}/test_ge_original_dam_objective_status"
nm -g "${test_dir}/test_ge_original_dam_objective_status" | \
    rg -q '_?ge_original_dam_objective_status_present$'

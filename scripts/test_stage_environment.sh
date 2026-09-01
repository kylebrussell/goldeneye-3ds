#!/usr/bin/env bash
set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir=${2:-"${repo_dir}/build/host-tests/stage-environment"}
mkdir -p "${test_dir}"

python3 "${repo_dir}/scripts/extract_stage_environment_tables.py" \
    "${repo_dir}" "${test_dir}/ge_original_stage_environment_tables.c"

cc -std=c11 -Wall -Wextra -Werror -Wno-missing-braces \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src" \
    "${repo_dir}/port/tests/test_ge_original_stage_environment.c" \
    "${repo_dir}/port/src/ge_original_stage_environment.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${test_dir}/ge_original_stage_environment_tables.c" \
    -o "${test_dir}/test_ge_original_stage_environment"

"${test_dir}/test_ge_original_stage_environment"

#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-guard-visibility.XXXXXX")
trap 'rm -rf "${test_dir}"' EXIT

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA \
    -I"${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/src/ge_original_prop_state.c" \
    "${repo_dir}/port/tests/test_ge_original_guard_visibility_ownership.c" \
    -lm -fsanitize=address,undefined "${dead_strip[@]}" \
    -o "${test_dir}/test_guard_visibility_ownership"

"${test_dir}/test_guard_visibility_ownership"
echo "guard renderer observation preserves canonical chrTick visibility"

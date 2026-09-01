#!/usr/bin/env bash
set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir="${repo_dir}/build/host-tests/draw-batch-visibility"
mkdir -p "${test_dir}"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -Wsign-conversion -Wshadow -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_draw_batch_visibility.c" \
    "${repo_dir}/port/tests/test_ge_draw_batch_visibility.c" \
    -lm -o "${test_dir}/test_ge_draw_batch_visibility"

"${test_dir}/test_ge_draw_batch_visibility"

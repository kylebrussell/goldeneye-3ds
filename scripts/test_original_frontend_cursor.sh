#!/usr/bin/env bash
set -euo pipefail
repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir="${repo_dir}/build/host-tests/original-frontend-cursor"
mkdir -p "${test_dir}"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_original_frontend_cursor.c" \
    "${repo_dir}/port/tests/test_ge_original_frontend_cursor.c" -lm \
    -o "${test_dir}/test_ge_original_frontend_cursor"
"${test_dir}/test_ge_original_frontend_cursor"

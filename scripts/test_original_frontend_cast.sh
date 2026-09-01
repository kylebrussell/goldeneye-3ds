#!/usr/bin/env bash
set -euo pipefail
repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir="${repo_dir}/build/host-tests/original-frontend-cast"
mkdir -p "${test_dir}"
python3 "${repo_dir}/scripts/extract_original_frontend_cast_contract.py" \
    "${repo_dir}" \
    "${repo_dir}/port/src/ge_original_frontend_cast_contract.inc" --check
cc -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_original_frontend_cast.c" \
    "${repo_dir}/port/tests/test_ge_original_frontend_cast.c" -lm \
    -o "${test_dir}/test_ge_original_frontend_cast"
"${test_dir}/test_ge_original_frontend_cast"

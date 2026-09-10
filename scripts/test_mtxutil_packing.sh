#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mtx-packing.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
cc -std=gnu11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-sanitize-recover=all -D_LANGUAGE_C \
    -I"${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    "${repo_dir}/src/libultra/gu/mtxutil.c" \
    "${repo_dir}/port/tests/test_ge_original_mtxutil_packing.c" -o "${test_dir}/test"
"${test_dir}/test"

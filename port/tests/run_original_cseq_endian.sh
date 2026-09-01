#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf "${test_dir}"' EXIT

compiler="${CC:-cc}"
flags=(
    -std=c11 -Wall -Wextra -Werror
    -fsanitize=address,undefined
    -D_LANGUAGE_C -DGE_PORT_CSEQ_BIG_ENDIAN
    -I"${repo_dir}" -I"${repo_dir}/include/PR"
    -idirafter "${repo_dir}/include"
)

"${compiler}" "${flags[@]}" \
    "${repo_dir}/src/libultra/audio/cseq.c" \
    "${repo_dir}/port/tests/test_original_cseq_endian.c" \
    -lm -o "${test_dir}/test_original_cseq_endian"

"${test_dir}/test_original_cseq_endian" \
    "${repo_dir}/assets/music/Mdam.bin"

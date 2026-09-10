#!/usr/bin/env bash
set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir=${2:-"${repo_dir}/build/integration/malformed-assets"}
if [[ $# -eq 3 || $# -gt 4 ]]; then
    echo "usage: $0 [REPO [TEST_DIR [CATALOG PACK]]]" >&2
    exit 2
fi
mkdir -p "${test_dir}"

flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -pedantic
    -Wconversion -Wsign-conversion -Wshadow
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -I"${repo_dir}/port/include")
"${CC:-cc}" "${flags[@]}" -Dmalloc=test_asset_malloc -Dcalloc=test_asset_calloc \
    -c "${repo_dir}/port/src/ge_asset_pack.c" -o "${test_dir}/ge_asset_pack_faults.o"
"${CC:-cc}" "${flags[@]}" "${test_dir}/ge_asset_pack_faults.o" \
    "${repo_dir}/port/src/ge_texture_catalog.c" \
    "${repo_dir}/port/tests/test_ge_asset_malformed.c" \
    -o "${test_dir}/test_ge_asset_malformed"
"${test_dir}/test_ge_asset_malformed" "${test_dir}/malformed.gepack"

"${CC:-cc}" "${flags[@]}" "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_texture_catalog.c" \
    "${repo_dir}/port/tests/test_ge_texture_catalog.c" \
    -o "${test_dir}/test_ge_texture_catalog"
"${test_dir}/test_ge_texture_catalog"

if [[ $# -eq 4 ]]; then
    "${test_dir}/test_ge_texture_catalog" "$3" "$4"
    "${CC:-cc}" "${flags[@]}" "${repo_dir}/port/src/ge_asset_pack.c" \
        "${repo_dir}/port/src/ge_texture_catalog.c" \
        "${repo_dir}/port/src/ge_texture_cache.c" \
        "${repo_dir}/port/tests/test_ge_texture_cache.c" \
        -o "${test_dir}/test_ge_texture_cache"
    "${test_dir}/test_ge_texture_cache" "$3" "$4"
fi

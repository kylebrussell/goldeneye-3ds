#!/usr/bin/env bash
set -euo pipefail
repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir="${repo_dir}/build/host-tests/original-frontend-visuals"
mkdir -p "${test_dir}"
cc -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_original_frontend_visuals.c" \
    "${repo_dir}/port/src/ge_original_rareware_logo.c" \
    "${repo_dir}/port/tests/test_ge_original_frontend_visuals.c" \
    -lm -o "${test_dir}/test_ge_original_frontend_visuals"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
    "${test_dir}/test_ge_original_frontend_visuals" \
    "${repo_dir}/build/3ds-runtime/segments/rarewarelogo.bin"
python3 -m unittest \
    "${repo_dir}/scripts/tests/test_convert_rareware_logo_3ds.py"

#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-gun-sight.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_gun_sight_slice.py" "${repo_dir}" "${build_dir}/sight.c"
python3 "${repo_dir}/scripts/tests/test_gun_sight_source.py" "${build_dir}/sight.c"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-unused-parameter \
    -Wno-pragma-pack -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DGE_PORT_SETUP_DATA -DAIPARSE -DVERSION_US -DPLAYERFLAG=int \
    -DGE_PORT_MS_INHERITS -fms-extensions \
    -I "${repo_dir}/port/include" -idirafter "${repo_dir}/src" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    "${build_dir}/sight.c" \
    "${repo_dir}/port/src/ge_original_gun_sight.c" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_pica_material.c" \
    "${repo_dir}/port/src/ge_pica_apply.c" \
    "${repo_dir}/port/tests/test_ge_original_gun_sight.c" \
    -fsanitize=address,undefined -fno-omit-frame-pointer -lm \
    -o "${build_dir}/test_gun_sight"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test_gun_sight"

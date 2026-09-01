#!/usr/bin/env bash
set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
pack=${2:-"${repo_dir}/build/3ds-assets/goldeneye.u.gepack"}
build_dir="${repo_dir}/build/host-tests/gunbarrel-model-assets"

mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/generate_move_model_tables.py" \
    "${repo_dir}" "${build_dir}/ge_original_move_model_tables.c"

cc -std=gnu11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-missing-braces \
    -Wno-pointer-to-int-cast -fsanitize=address,undefined \
    -fno-omit-frame-pointer -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_original_character_models.c" \
    "${repo_dir}/port/src/ge_original_pitem_models.c" \
    "${build_dir}/ge_original_move_model_tables.c" \
    "${repo_dir}/port/tests/ge_original_pitem_model_test_support.c" \
    "${repo_dir}/port/tests/test_ge_original_gunbarrel_model_assets.c" \
    -lm -o "${build_dir}/test_ge_original_gunbarrel_model_assets"

ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
    "${build_dir}/test_ge_original_gunbarrel_model_assets" "${pack}"

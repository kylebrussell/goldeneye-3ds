#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-player-body.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
cc -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -DPLAYERFLAG=int -fms-extensions -I"${repo_dir}" \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/src/ge_original_player_body.c" \
    "${repo_dir}/port/tests/test_ge_original_player_body.c" \
    -lm -o "${build_dir}/test_ge_original_player_body"
"${build_dir}/test_ge_original_player_body"
nm -g "${build_dir}/test_ge_original_player_body" \
    | grep -E '_?solo_char_load$' >/dev/null

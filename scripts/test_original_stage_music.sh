#!/usr/bin/env bash
set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir=${2:-"${repo_dir}/build/host-tests/original-stage-music"}
pack=${3:-"${repo_dir}/build/3ds-assets/goldeneye.u.gepack"}
mkdir -p "${test_dir}"

python3 "${repo_dir}/scripts/generate_original_stage_music.py" \
    "${repo_dir}" "${test_dir}/ge_original_stage_music_data.inc"
cmp "${repo_dir}/port/include/ge_original_stage_music_data.inc" \
    "${test_dir}/ge_original_stage_music_data.inc"

test -f "${pack}"
cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -Wconversion -Wsign-conversion -Wshadow \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_original_stage_music.c" \
    "${repo_dir}/port/tests/test_ge_original_stage_music.c" \
    -o "${test_dir}/test_ge_original_stage_music"

"${test_dir}/test_ge_original_stage_music" "${pack}"

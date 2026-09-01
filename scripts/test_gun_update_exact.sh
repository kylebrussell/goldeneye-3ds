#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_gun_update_and_fire_slice.py" \
  "${repo_dir}" "${test_dir}/ge_original_gun_update_and_fire_slice.c"
python3 "${repo_dir}/scripts/extract_gun_pose_helpers_slice.py" \
  "${repo_dir}" "${test_dir}/ge_original_gun_pose_helpers_slice.c"

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == "Darwin" ]]; then
  dead_strip=(-Wl,-dead_strip)
fi

cc -std=gnu11 -include stddef.h -ffunction-sections -fdata-sections \
  -fms-extensions -fsanitize=address,undefined -fno-omit-frame-pointer \
  -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
  -DAIPARSE -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS \
  -I "${repo_dir}/port/include" -I "${repo_dir}/src/game" \
  -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
  -idirafter "${repo_dir}/src" -iquote "${repo_dir}" -Wno-everything \
  "${test_dir}/ge_original_gun_pose_helpers_slice.c" \
  "${repo_dir}/port/src/ge_original_gun_frame_arena.c" \
  "${repo_dir}/port/tests/test_ge_original_gun_frame_arena.c" \
  "${dead_strip[@]}" -lm -o "${test_dir}/test_gun_frame_arena"

ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  "${test_dir}/test_gun_frame_arena"

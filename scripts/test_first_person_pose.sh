#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-missing-braces \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
  -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
  -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
  -iquote "${repo_dir}" \
  "${repo_dir}/port/src/ge_original_bond_camera_live.c" \
  "${repo_dir}/port/src/ge_original_first_person_pose.c" \
  "${repo_dir}/port/tests/test_ge_original_first_person_pose.c" -lm \
  -o "${test_dir}/test_first_person_pose"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  "${test_dir}/test_first_person_pose"

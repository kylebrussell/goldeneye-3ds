#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_bond_input_gun_data_slice.py" \
  "${repo_dir}" "${test_dir}/gun-data.c" >/dev/null
python3 "${repo_dir}/scripts/pack_3ds_assets.py" \
  --assets "${repo_dir}/port/tests/fixtures" \
  --source-sha1 abe01e4aeb033b6c0836819f549c791b26cfde83 \
  --extra-dir \
    "converted/models/first-person-pp7=${repo_dir}/build/3ds-models/first-person-pp7" \
  --output "${test_dir}/first-person.gepack" >/dev/null

common_flags=(
  -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-missing-braces
  -Wno-missing-declarations -Wno-visibility
  -fsanitize=address,undefined -fno-omit-frame-pointer
  -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
  -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0
  -I "${repo_dir}" -I "${repo_dir}/port/include"
  -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include"
  -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src"
  -iquote "${repo_dir}"
)
cc "${common_flags[@]}" \
  "${test_dir}/gun-data.c" \
  "${repo_dir}/port/src/ge_asset_pack.c" \
  "${repo_dir}/port/src/ge_original_first_person_assets.c" \
  "${repo_dir}/port/src/ge_original_first_person_item_model.c" \
  "${repo_dir}/port/tests/test_ge_original_first_person_item_model.c" \
  -lm -o "${test_dir}/test_first_person_item_model"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  "${test_dir}/test_first_person_item_model" "${test_dir}/first-person.gepack"

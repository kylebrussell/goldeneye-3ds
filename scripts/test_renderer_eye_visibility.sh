#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir="${repo_dir}/build/host-tests/renderer-eye-visibility"
mkdir -p "${test_dir}"
python3 "${repo_dir}/scripts/extract_bg_visibility_slice.py" \
    "${repo_dir}/src/game/bg.c" "${test_dir}/ge_original_bg_visibility_slice.c"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
    -Wno-return-type -DVERSION_US -DLEFTOVERDEBUG -DGE_PORT_HOST_ABI \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${repo_dir}/port/include" -I"${test_dir}" \
    "${repo_dir}/port/tests/test_ge_dam_renderer_visibility.c" \
    "${repo_dir}/port/src/ge_renderer_bg_visibility.c" \
    "${repo_dir}/port/src/ge_dam_renderer_visibility.c" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" \
    "${test_dir}/ge_original_bg_visibility_slice.c" \
    -lm -o "${test_dir}/test_renderer_eye_visibility"
"${test_dir}/test_renderer_eye_visibility"

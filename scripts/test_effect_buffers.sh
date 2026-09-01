#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=${GE_EFFECT_BUFFER_TEST_DIR:-$(mktemp -d)}
mkdir -p "${test_dir}"

cc -std=gnu11 -Wall -Wextra -Wno-error \
    -Wno-unused-parameter -Wno-unused-variable -Wno-missing-declarations \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
    -DAIPARSE -DVERSION_US -DGE_PORT_MS_INHERITS -fms-extensions \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_original_effect_buffers.c" \
    "${repo_dir}/port/tests/test_ge_original_effect_buffers.c" \
    -o "${test_dir}/test_ge_original_effect_buffers"

"${test_dir}/test_ge_original_effect_buffers"

#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT
if [[ $(uname -s) == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi
python3 "${repo_dir}/scripts/extract_bg_visibility_slice.py" \
    "${repo_dir}/src/game/bg.c" "${test_dir}/ge_original_bg_visibility_slice.c"
for abi in native short-enum; do
    abi_flags=(-fno-short-enums)
    if [[ ${abi} == short-enum ]]; then abi_flags=(-fshort-enums); fi
    cc "${abi_flags[@]}" -std=gnu11 -Wall -Wextra -Werror \
        -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body \
        -Wno-incompatible-pointer-types -Wno-return-type \
        -DVERSION_US -DLEFTOVERDEBUG -DGE_PORT_HOST_ABI \
        -DGE_TEST_MULTIPLAYER_VISIBILITY_RUNTIME \
        -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
        -I "${repo_dir}/port/include" \
        "${repo_dir}/port/tests/test_ge_original_bg_visibility.c" \
        "${repo_dir}/port/src/ge_original_bg_visibility.c" \
        "${repo_dir}/port/src/ge_dam_visibility_runtime.c" \
        "${repo_dir}/port/src/ge_dam_preload_queue.c" \
        "${test_dir}/ge_original_bg_visibility_slice.c" \
        -lm "${dead_strip[@]}" -o "${test_dir}/multiplayer-visibility"
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/multiplayer-visibility"
done

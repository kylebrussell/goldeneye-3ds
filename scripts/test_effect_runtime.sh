#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-effects-runtime.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_effect_runtime_slice.py" "${repo_dir}" "${build_dir}/runtime.c"
python3 "${repo_dir}/scripts/extract_effect_runtime_fixture.py" "${repo_dir}" "${build_dir}"
python3 "${repo_dir}/scripts/extract_bg_visibility_slice.py" "${repo_dir}/src/game/bg.c" "${build_dir}/bg.c"
flags=(-std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fno-omit-frame-pointer -fsanitize=address,undefined
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS
-DVERSION_US -DBUGFIX_R0 -fms-extensions -Wno-everything -I"${repo_dir}" -I"${repo_dir}/port/include"
-I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
cc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    -I"${repo_dir}/port/include" -c "${repo_dir}/port/src/ge_original_bg_visibility.c" -o "${build_dir}/visibility.o"
cc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    -I"${repo_dir}/port/include" -c "${build_dir}/visibility.c" -o "${build_dir}/room.o"
cc "${flags[@]}" -DGE_PORT_HOST_ABI -DLEFTOVERDEBUG -c "${build_dir}/bg.c" -o "${build_dir}/bg.o"
cc "${flags[@]}" -c "${build_dir}/prefix_reference.c" -o "${build_dir}/prefix_reference.o"
if [[ $(uname -s) == Darwin ]];then strip=(-Wl,-dead_strip);else strip=(-Wl,--gc-sections);fi
cc "${flags[@]}" "${build_dir}/runtime.c" "${build_dir}/support.c" \
    "${repo_dir}/port/src/ge_original_effect_runtime.c" "${repo_dir}/port/src/ge_original_effect_buffers.c" \
    "${repo_dir}/port/src/random_port.c" "${repo_dir}/src/game/matrixmath.c" \
    "${repo_dir}/port/tests/test_ge_original_effect_runtime.c" "${build_dir}/visibility.o" "${build_dir}/room.o" "${build_dir}/bg.o" "${build_dir}/prefix_reference.o" \
    "${strip[@]}" -lm -o "${build_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test"

#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "${work_dir}"' EXIT

cc -std=gnu11 -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
    -DAIPARSE -DVERSION_US \
    -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -c "${repo_dir}/port/src/ge_original_gameplay_services.c" \
    -o "${work_dir}/gameplay-services.o"
cc -std=gnu11 -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
    -DAIPARSE -DVERSION_US \
    -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -c "${repo_dir}/port/src/ge_original_sfx_bank.c" \
    -o "${work_dir}/sfx-bank.o"
cc -std=gnu11 -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${repo_dir}/port/include" \
    -c "${repo_dir}/port/src/ge_audio_output.c" \
    -o "${work_dir}/audio-output.o"
cc -std=gnu11 -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
    -DAIPARSE -DVERSION_US \
    -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -c "${repo_dir}/port/tests/test_ge_original_gameplay_audio_lifecycle.c" \
    -o "${work_dir}/test.o"

if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi
cc -fsanitize=address,undefined "${dead_strip[@]}" \
    "${work_dir}/gameplay-services.o" \
    "${work_dir}/sfx-bank.o" \
    "${work_dir}/audio-output.o" \
    "${work_dir}/test.o" -lm -o "${work_dir}/test"

"${work_dir}/test" \
    "${repo_dir}/assets/music/sfx.ctl" \
    "${repo_dir}/assets/music/sfx.tbl"

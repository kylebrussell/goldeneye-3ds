#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf "${test_dir}"' EXIT

compiler="${CC:-cc}"
common_flags=(
    -std=c11
    -Wall
    -Wextra
    -Werror
    -Wno-pointer-to-int-cast
    -ffunction-sections
    -fdata-sections
    -fsanitize=address,undefined
    -D_LANGUAGE_C
    -I"${repo_dir}"
    -I"${repo_dir}/port/include"
    -I"${repo_dir}/include/PR"
    -idirafter "${repo_dir}/include"
)

sources=(
    src/libultra/audio/mainbus.c
    src/libultra/audio/auxbus.c
    src/libultra/audio/save.c
    port/src/ge_audio_abi.c
    port/src/ge_audio_output.c
    port/tests/test_original_audio_producer.c
)
objects=()

for source in "${sources[@]}"; do
    object="${test_dir}/$(basename "${source%.c}").o"
    "${compiler}" "${common_flags[@]}" -c "${repo_dir}/${source}" -o "${object}"
    objects+=("${object}")
done

if [[ "$(uname -s)" == "Darwin" ]]; then
    dead_code_flag=(-Wl,-dead_strip)
else
    dead_code_flag=(-Wl,--gc-sections)
fi

"${compiler}" -fsanitize=address,undefined "${dead_code_flag[@]}" \
    "${objects[@]}" -o "${test_dir}/test_original_audio_producer"
"${test_dir}/test_original_audio_producer"

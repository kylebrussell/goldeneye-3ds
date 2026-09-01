#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
smoke_dir="$(mktemp -d)"
trap 'rm -rf "${smoke_dir}"' EXIT

compiler="${CC:-cc}"
common_flags=(
    -std=c11
    -Wall
    -Wextra
    -Werror
    -ffunction-sections
    -fdata-sections
    -fsanitize=address,undefined
    -I"${repo_dir}/port/audio_smoke/include"
    -I"${repo_dir}/port/smoke/include"
    -I"${repo_dir}/port/include"
    -idirafter "${repo_dir}/include/PR"
)

for source in \
    src/libultra/audio/event.c \
    src/libultra/audio/sl.c \
    src/libultra/audio/copy.c \
    src/libultra/audio/cents2ratio.c \
    port/src/ge_services.c \
    port/src/ge_libultra_services.c \
    port/src/ge_libultra_scheduler.c \
    port/tests/test_original_audio_event.c; do
    object_name="$(basename "${source%.c}").o"
    "${compiler}" "${common_flags[@]}" -c "${repo_dir}/${source}" \
        -o "${smoke_dir}/${object_name}"
done

if [[ "$(uname -s)" == "Darwin" ]]; then
    dead_code_flag=(-Wl,-dead_strip)
else
    dead_code_flag=(-Wl,--gc-sections)
fi

"${compiler}" -fsanitize=address,undefined "${dead_code_flag[@]}" \
    "${smoke_dir}/event.o" \
    "${smoke_dir}/sl.o" \
    "${smoke_dir}/copy.o" \
    "${smoke_dir}/cents2ratio.o" \
    "${smoke_dir}/ge_services.o" \
    "${smoke_dir}/ge_libultra_services.o" \
    "${smoke_dir}/ge_libultra_scheduler.o" \
    "${smoke_dir}/test_original_audio_event.o" \
    -o "${smoke_dir}/test_original_audio_event"

"${smoke_dir}/test_original_audio_event"

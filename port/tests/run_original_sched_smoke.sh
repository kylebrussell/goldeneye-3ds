#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
smoke_dir="$(mktemp -d)"
trap 'rm -rf "${smoke_dir}"' EXIT

compiler="${CC:-cc}"
common_flags=(
    -std=c11
    -ffunction-sections
    -fdata-sections
    -fsanitize=address,undefined
    -I"${repo_dir}/port/smoke/include"
    -I"${repo_dir}/port/include"
    -I"${repo_dir}/src"
    -idirafter "${repo_dir}/include"
    -DMAXCONTROLLERS=4
)
strict_flags=(-Wall -Wextra -Werror -pedantic)
original_flags=(
    -Wall
    -Wextra
    -Werror
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
    -Wno-incompatible-pointer-types
    -Wno-parentheses
    -Wno-unused-but-set-variable
    -Wno-unused-parameter
    -include ge_original_sched_decls.h
)

"${compiler}" "${common_flags[@]}" "${original_flags[@]}" \
    -c "${repo_dir}/src/sched.c" -o "${smoke_dir}/sched.o"

for source in \
    ge_services \
    ge_libultra \
    ge_libultra_services \
    ge_libultra_scheduler; do
    "${compiler}" "${common_flags[@]}" "${strict_flags[@]}" \
        -c "${repo_dir}/port/src/${source}.c" \
        -o "${smoke_dir}/${source}.o"
done

"${compiler}" "${common_flags[@]}" "${strict_flags[@]}" \
    -c "${repo_dir}/port/tests/test_original_sched.c" \
    -o "${smoke_dir}/test_original_sched.o"

if [[ "$(uname -s)" == "Darwin" ]]; then
    dead_code_flag=(-Wl,-dead_strip)
else
    dead_code_flag=(-Wl,--gc-sections)
fi

"${compiler}" -fsanitize=address,undefined "${dead_code_flag[@]}" \
    "${smoke_dir}/sched.o" \
    "${smoke_dir}/ge_services.o" \
    "${smoke_dir}/ge_libultra.o" \
    "${smoke_dir}/ge_libultra_services.o" \
    "${smoke_dir}/ge_libultra_scheduler.o" \
    "${smoke_dir}/test_original_sched.o" \
    -o "${smoke_dir}/test_original_sched"

"${smoke_dir}/test_original_sched"

#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT
flags=(-O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-variable
    -Wno-unused-parameter -Wno-pointer-to-int-cast
    -ffunction-sections -fdata-sections -fsanitize=address,undefined
    -D_LANGUAGE_C -DGE_PORT_LIBAUDIO_NATIVE
    -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/libultra"
    -I"${repo_dir}/include/PR" -idirafter "${repo_dir}/include")
for source in src/libultrare/audio/drvrNew.c src/libultrare/audio/reverb.c \
        port/src/ge_audio_abi.c port/src/ge_audio_output.c \
        port/tests/test_original_reverb_filter.c; do
    cc "${flags[@]}" -c "${repo_dir}/${source}" \
        -o "${test_dir}/$(basename "${source%.c}").o"
done
dead_strip=-Wl,--gc-sections
if [[ $(uname -s) == Darwin ]]; then dead_strip=-Wl,-dead_strip; fi
cc -fsanitize=address,undefined "${test_dir}"/*.o "$dead_strip" \
    -o "${test_dir}/test_original_reverb_filter"
"${test_dir}/test_original_reverb_filter" "$@"

# Compile the same canonical producer without the native-port macro, checking
# the original N64 arithmetic/representation path against the numeric vectors.
# Expose canonical PR headers before the port shim, without putting the
# repository's legacy stddef.h ahead of the host's standard library.
mkdir "${test_dir}/n64-include"
ln -s "${repo_dir}/include/PR" "${test_dir}/n64-include/PR"
cc -I"${test_dir}/n64-include" "${flags[@]}" -UGE_PORT_LIBAUDIO_NATIVE \
    -c "${repo_dir}/src/libultrare/audio/drvrNew.c" -o "${test_dir}/n64-driver.o"
cc -I"${test_dir}/n64-include" "${flags[@]}" -UGE_PORT_LIBAUDIO_NATIVE \
    -c "${repo_dir}/port/tests/test_original_reverb_filter.c" -o "${test_dir}/n64-test.o"
cc -fsanitize=address,undefined "${test_dir}/n64-driver.o" "${test_dir}/n64-test.o" \
    "$dead_strip" -o "${test_dir}/test_n64_reverb_filter"
"${test_dir}/test_n64_reverb_filter"

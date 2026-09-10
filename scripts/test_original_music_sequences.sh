#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=${GE_MUSIC_TEST_BUILD:-$(mktemp -d)}
if [[ -z ${GE_MUSIC_TEST_BUILD:-} ]]; then trap 'rm -rf "$test_dir"' EXIT; fi
mkdir -p "$test_dir"
compiler=${CC:-cc}
flags=(-O2 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-variable
    -Wno-unused-parameter -Wno-pointer-to-int-cast -Wno-builtin-declaration-mismatch
    -Wno-sign-compare -Wno-return-type -Wno-type-limits -Wno-unused-but-set-variable
    -Wno-implicit-fallthrough -Wno-maybe-uninitialized
    -ffunction-sections -fdata-sections -fsanitize=address,undefined
    -D_LANGUAGE_C -DGE_PORT_LIBAUDIO_NATIVE -DGE_PORT_CSEQ_BIG_ENDIAN
    -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/libultra"
    -I"${repo_dir}/include/PR" -idirafter "${repo_dir}/include")
sources=(mainbus auxbus save cspgetstate cseq cspsetseq cspsetvol cspplay cspstop
    event copy sl cents2ratio csplayer filter heapalloc heapinit load resample
    seqplayer seqpsetbank synaddplayer synallocfx synallocvoice syndelete
    synfreevoice synsetfxmix synsetpan synsetpitch synsetpriority synsetvol
    synstartvoice synstartvoiceparam synstopvoice synthesizer)
objects=()
for name in "${sources[@]}"; do
    extra=(-UNDEBUG)
    if [[ $name == synthesizer ]]; then extra=(-DNDEBUG); fi
    "$compiler" "${flags[@]}" "${extra[@]}" -c "$repo_dir/src/libultra/audio/$name.c" -o "$test_dir/$name.o"
    objects+=("$test_dir/$name.o")
done
for source in src/libultrare/audio/drvrNew.c src/libultrare/audio/env.c src/libultrare/audio/reverb.c \
        port/src/ge_original_music_runtime.c port/src/ge_original_music_bank.c \
        port/src/ge_audio_abi.c port/src/ge_audio_output.c port/src/ge_asset_pack.c \
        port/tests/test_original_music_sequences.c; do
    object="$test_dir/$(basename "${source%.c}").o"
    "$compiler" "${flags[@]}" -c "$repo_dir/$source" -o "$object"
    objects+=("$object")
done
"$compiler" -fsanitize=address,undefined "${objects[@]}" -Wl,--gc-sections -lm -o "$test_dir/test_original_music_sequences"
if [[ $# -gt 0 ]]; then
    read -r -a runner <<< "${GE_MUSIC_TEST_RUNNER:-}"
    if [[ -n ${GE_MUSIC_TEST_RUNNER:-} ]]; then
        UBSAN_OPTIONS=halt_on_error=1 "${runner[@]}" "$test_dir/test_original_music_sequences" "$@"
    else
        UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_original_music_sequences" "$@"
    fi
fi

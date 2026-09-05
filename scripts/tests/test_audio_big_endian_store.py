#!/usr/bin/env python3
"""Check sample reads/writes, odd addresses and ordered overlapping buffers."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'port/src/ge_audio_abi.c').read_text()
start = source.index('static uint16_t ge_audio_abi_load_u16(')
end = source.index('\nstatic uint32_t ge_audio_abi_load_u32(', start)
test = r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
''' + source[start:end] + r'''
static void reference(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value;
}
int main(void) {
    uint8_t actual[16], expected[16];
    for (uint32_t value=0; value<65536; ++value) {
        for (unsigned offset=0; offset<15; ++offset) {
            memset(actual, 0xa5, sizeof(actual));
            memcpy(expected, actual, sizeof(actual));
            ge_audio_abi_store_u16(actual+offset, (uint16_t)value);
            reference(expected+offset, (uint16_t)value);
            assert(memcmp(actual, expected, sizeof(actual))==0);
            assert(ge_audio_abi_load_u16(actual+offset)==value);
            assert(ge_audio_abi_load_s16(actual+offset)==(int16_t)value);
        }
    }
    for (uint32_t value=0; value<65536; ++value) {
        for (unsigned offset=0; offset<15; ++offset) {
            memset(actual, 0xa5, sizeof(actual));
            reference(actual+offset, (uint16_t)value);
            memcpy(expected, actual, sizeof(actual));
            assert(ge_audio_abi_load_u16(actual+offset)==value);
            assert(ge_audio_abi_load_s16(actual+offset)==(int16_t)value);
            assert(memcmp(actual, expected, sizeof(actual))==0);
        }
    }
    uint32_t random=7139;
    memset(actual, 0x5a, sizeof(actual));
    memcpy(expected, actual, sizeof(actual));
    for (unsigned i=0; i<1000000; ++i) {
        random=random*1664525U+1013904223U;
        unsigned dst=random%15U, src=(random>>16)%15U;
        uint16_t a=ge_audio_abi_load_u16(actual+src);
        uint16_t b=(uint16_t)((expected[src]<<8)|expected[src+1]);
        ge_audio_abi_store_u16(actual+dst, (uint16_t)(a+random));
        reference(expected+dst, (uint16_t)(b+random));
        assert(memcmp(actual, expected, sizeof(actual))==0);
    }
    puts("Audio sample loads/stores: 983040 exhaustive value/address roundtrips and 1000000 ordered overlapping writes exact");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-audio-store-') as temporary:
    directory = Path(temporary)
    (directory / 'store.c').write_text(test)
    for mode, extra in [('native', []), ('portable', ['-U__BYTE_ORDER__'])]:
        binary = directory / mode
        subprocess.run(['cc', '-std=c11', '-O3', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', *extra,
                        str(directory / 'store.c'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

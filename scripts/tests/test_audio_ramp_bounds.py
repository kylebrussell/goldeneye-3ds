#!/usr/bin/env python3
"""Compare the actual bounded ramp step with its previous 64-bit semantics."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'port/src/ge_audio_abi.c').read_text()
declaration = re.search(r'typedef struct GeAudioAbiRamp \{.*?\} GeAudioAbiRamp;', source, re.S).group()
start = source.index('static int16_t ge_audio_abi_ramp_step(')
begin = source.index('{', start)
depth, end = 1, begin + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
test = '#include <stdint.h>\n#include <limits.h>\n#include <assert.h>\n#include <stdio.h>\n' + declaration + source[start:end] + r'''
static void check(int32_t value, int32_t target, int32_t step) {
    GeAudioAbiRamp actual = {value, step, target};
    int64_t expected_value = value, expected_step = step;
    for (unsigned repeat = 0; repeat < 16; ++repeat) {
        expected_value += expected_step;
        if ((expected_step <= 0 && expected_value <= target)
                || (expected_step > 0 && expected_value >= target)) {
            expected_value = target; expected_step = 0;
        }
        assert(ge_audio_abi_ramp_step(&actual) == (int16_t)(expected_value >> 16));
        assert(actual.value == expected_value && actual.step == expected_step);
        assert(actual.target == target);
    }
}
int main(void) {
    const int32_t edge[] = {INT32_MIN, INT32_MIN+1, -65536, -1, 0, 1,
        65535, 65536, INT32_MAX-1, INT32_MAX};
    for (unsigned i=0;i<10;++i) for (unsigned j=0;j<10;++j)
        for (unsigned k=0;k<10;++k) check(edge[i],edge[j],edge[k]);
    uint32_t random = 7381;
    for (unsigned i=0;i<1000000;++i) {
        random=random*1664525U+1013904223U; int32_t value=(int32_t)random;
        random=random*1664525U+1013904223U; int32_t target=(int32_t)random;
        random=random*1664525U+1013904223U; int32_t step=(int32_t)random;
        check(value,target,step);
    }
    puts("Audio ramp: 16016000 exact steps, signed extrema and zero/crossing/wrong-direction steps");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-ramp-') as temporary:
    directory = Path(temporary)
    (directory / 'ramp.c').write_text(test)
    subprocess.run(['cc', '-std=c11', '-O3', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(directory / 'ramp.c'),
                    '-o', str(directory / 'ramp')], check=True)
    subprocess.run([str(directory / 'ramp')], check=True)

#!/usr/bin/env python3
"""Exercise the actual native envelope volume helper over every signed slope."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "src/libultrare/audio/env.c").read_text()
start = source.index("static f32 _getVol(f32 ivol, s32 samples, s16 ratem, u16 ratel)\n{")
end = source.index("\nstatic s16 _getRate(", start)
program = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef float f32;
typedef int32_t s32;
typedef int16_t s16;
typedef uint16_t u16;
''' + source[start:end] + r'''
int main(void) {
    const u16 low[] = {0, 1, 32767, 32768, 65534, 65535};
    const s32 samples[] = {0, 1, 8, 184, 736, 22050};
    for (s32 high = -32768; high <= 32767; ++high) {
        for (unsigned lo = 0; lo < 6; ++lo) {
            for (unsigned count = 0; count < 6; ++count) {
                /* Construct the original signed Q16 high word without a
                 * signed shift, retaining its float conversion order. */
                s32 word = (s32)((int64_t)high * INT64_C(65536));
                f32 rate = ((f32)word + (f32)low[lo]) / 65536.0;
                f32 expected = 16383.0f + (rate * (f32)samples[count]) * 0.125;
                assert(_getVol(16383.0f, samples[count], (s16)high, low[lo]) == expected);
            }
        }
    }
    puts("Original native envelope: 2359296 signed slope/fraction/sample cases exact");
}
'''
with tempfile.TemporaryDirectory(prefix="ge-native-volume-") as tmp:
    directory = Path(tmp)
    (directory / "volume.c").write_text(program)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-DGE_PORT_LIBAUDIO_NATIVE", "-fsanitize=address,undefined",
                    str(directory / "volume.c"), "-o", str(directory / "test")], check=True)
    subprocess.run([str(directory / "test")], check=True)

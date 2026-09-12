#!/usr/bin/env python3
"""Exercise the real bounded command scanner and completed-pass lifecycle."""
from pathlib import Path
import re
import subprocess
import tempfile
repo = Path(__file__).resolve().parents[2]
s = (repo/'platform/3ds/source/ge_3ds_draw_arrays.c').read_text()
s = s[:s.index('static inline void ge_3ds_encode_draw')]
s = re.sub(r'^#include .*\n', '', s, flags=re.M)
header = (repo/'platform/3ds/include/ge_3ds_draw_trace.h').read_text()
prefix = r'''
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t prefix[8], flags, padding[54];
 struct { uint32_t depthTest; } effect; } C3D_Context;
static uint32_t *gpuCmdBuf, gpuCmdBufOffset, gpuCmdBufSize;
'''
test = r'''
int main(void) {
 Ge3dsDrawTraceRegisters r = {0};
 uint32_t words[] = {
  0x12345678, 0x000f0107,
  0x00ab0000, 0x00040107,
  0x11111111, 0x803f0104, 0x22222222, 0x33333333, 0x00001f61, 0,
  1, 0x801f0114, 3, 0,
  0x0000f000, 0x00120107, 0x0000aa00, 0
 };
 assert(ge_3ds_draw_trace_scan(&r,words,18,0,2));
 assert(r.value[0]==0x12345678 && r.known[0]==15 && r.emitted[0]==15);
 assert(ge_3ds_draw_trace_scan(&r,words,18,2,4));
 assert(r.value[0]==0x12ab5678 && r.emitted[0]==4);
 assert(ge_3ds_draw_trace_scan(&r,words,18,4,10));
 assert(r.value[0]==0x1f61 && r.value[4]==0x11111111);
 assert(ge_3ds_draw_trace_scan(&r,words,18,10,14));
 assert(r.value[1]==1 && r.value[2]==3);
 assert(ge_3ds_draw_trace_scan(&r,words,18,14,18));
 assert(r.value[0]==0xaa61 && r.emitted[0]==2);
 assert(!ge_3ds_draw_trace_scan(&r,words,18,1,2));
 assert(!ge_3ds_draw_trace_scan(&r,words,18,2,1));
 assert(!ge_3ds_draw_trace_scan(&r,words,18,0,20));
 assert(!ge_3ds_draw_trace_scan(&r,words,18,4,8));
 assert(!ge_3ds_draw_trace_scan(&r,NULL,18,0,0));
 assert(!ge_3ds_draw_trace_scan(NULL,words,18,0,0));
 assert(ge_3ds_draw_trace_scan(&r,words,18,18,18));
 assert(r.emitted[0]==0);
 gpuCmdBuf=words;gpuCmdBufSize=18;gpuCmdBufOffset=2;
 ge_3ds_draw_trace_reset(); ge_3ds_draw_trace_begin();
 for(int i=0;i<40;++i)ge_3ds_draw_trace_record(i*3,3,0x1f61,16);
 assert(trace_work.count==32 && !trace_completed.complete);
 ge_3ds_draw_trace_end();
 assert(trace_completed.complete && trace_completed.count==32);
 assert(trace_completed.records[0].registers.value[0]==0x12345678);
 assert(trace_completed.records[31].first==93 && trace_completed.records[31].valid);
 ge_3ds_draw_trace_begin();
 assert(trace_completed.count==32 && trace_work.count==0);
 gpuCmdBufOffset=4;
 ge_3ds_draw_trace_record(12,6,0xf61,16);
 assert(trace_completed.records[0].first==0);
 ge_3ds_draw_trace_end();
 assert(trace_completed.count==1 && trace_completed.records[0].first==12);
 ge_3ds_draw_trace_begin();gpuCmdBuf=NULL;
 ge_3ds_draw_trace_record(0,3,0,0);ge_3ds_draw_trace_end();
 assert(!trace_completed.records[0].valid);
 ge_3ds_draw_trace_reset();
 assert(!trace_completed.complete && trace_completed.count==0 && !trace_active);
 puts("Draw trace: masked/consecutive/repeated registers, bounds, cap, completed-pass/reset passed");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-draw-trace-') as d:
 p=Path(d); (p/'test.c').write_text(prefix+header+s+test)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
main=(repo/'platform/3ds/source/main.c').read_text()
assert 'if (eye_pass == 0U) ge_3ds_draw_trace_begin();' in main
assert 'if (eye_pass == 0U) ge_3ds_draw_trace_end();' in main
assert 'ge_3ds_draw_trace_write(stream);' in main

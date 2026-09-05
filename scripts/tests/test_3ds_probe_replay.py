#!/usr/bin/env python3
"""Exercise the actual replay loader and noninterfering allocator observers."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'scripts'))
from extract_dam_guard_chr_scheduler_slice import function_text
from make_3ds_retrace_replay import replay_text

source = (root/'platform/3ds/source/main.c').read_text()
loader = function_text(source, 'input_probe_load_retraces')
first_person = function_text(source, 'update_first_person_scene')
assert '&runtime->cache, deep_profile ? runtime_profile_clock : NULL, NULL)' in first_person
main = source[source.index('int main('):]
guard_clock = main.index('ge_original_model_scene_cache_bind_profile_clock(')
assert main.index("input_probe.deep_profile = fgetc(detail) == '1';") < guard_clock
assert 'input_probe.deep_profile ? runtime_profile_clock : NULL, NULL)' in main[guard_clock:guard_clock+250]
assert 'FIRST_PERSON_VERTEX_OFFSET,\n                    input_probe.deep_profile)' in main
assert 'memset(&input_probe, 0, sizeof(input_probe));' in main

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ge_3ds_alloc_profile.h"
#define INPUT_RETRACE_PATH "replay.cfg"
typedef struct { unsigned target_frames; uint8_t *replay_retraces; } RuntimeInputProbe;
void *__real_malloc(size_t n) { return n == 13 ? NULL : malloc(n); }
void *__real_calloc(size_t n,size_t s) { return calloc(n,s); }
void *__real_realloc(void *p,size_t n) { return realloc(p,n); }
void __real_free(void *p) { free(p); }
void *__wrap_malloc(size_t); void *__wrap_calloc(size_t,size_t);
void *__wrap_realloc(void *,size_t); void __wrap_free(void *);
'''
suffix = r'''
static void check(const char *text, bool expected)
{
    RuntimeInputProbe r={3,NULL}; FILE *f=fopen(INPUT_RETRACE_PATH,"w");
    assert(f); fputs(text,f); fclose(f);
    assert(input_probe_load_retraces(&r)==expected);
    if (expected) assert(r.replay_retraces[0]==1 && r.replay_retraces[1]==2 && r.replay_retraces[2]==1);
    free(r.replay_retraces);
}
int main(void)
{
    RuntimeInputProbe absent={3,NULL};
    assert(input_probe_load_retraces(&absent) && !absent.replay_retraces);
    check("GE_RETRACE_REPLAY 1\nframes 3\n1 2 1\n",true);
    check("GE_RETRACE_REPLAY 2\nframes 3\n1 2 1\n",false);
    check("GE_RETRACE_REPLAY 1\nframes 4\n1 2 1 1\n",false);
    check("GE_RETRACE_REPLAY 1\nframes 3\n1 2\n",false);
    check("GE_RETRACE_REPLAY 1\nframes 3\n1 0 1\n",false);
    check("GE_RETRACE_REPLAY 1\nframes 3\n1 256 1\n",false);
    check("GE_RETRACE_REPLAY 1\nframes 3\n1 2 1 extra\n",false);
    uint32_t c[6];
    void *p=__wrap_malloc(8); assert(p); __wrap_free(p);
    ge_3ds_alloc_profile_snapshot(c);
    for(unsigned i=0;i<6;++i) assert(c[i]==0);
    ge_3ds_alloc_profile_enable(1);
    p=__wrap_calloc(4,4); assert(p);
    for(unsigned i=0;i<16;++i) assert(((char*)p)[i]==0);
    ((char*)p)[0]=42; p=__wrap_realloc(p,32); assert(p && ((char*)p)[0]==42);
    __wrap_free(p); assert(__wrap_malloc(13)==NULL);
    ge_3ds_alloc_profile_snapshot(c);
    assert(c[0]==1 && c[1]==1 && c[2]==1 && c[3]==1 && c[4]==61 && c[5]==1);
    puts("retrace validation and allocator-result preservation passed");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-replay-') as temporary:
    work = Path(temporary)
    capture = work/'capture.result'
    header = 'status=complete\nframe_timing_version=2\nframe_timing_count=3\n'
    rows = ''.join('frame_timing='+','.join(map(str,[i]+[0]*17+[n,0]))+'\n'
                   for i,n in enumerate([1,2,1],1))
    capture.write_text(header+rows)
    assert replay_text(capture,4) == 'GE_RETRACE_REPLAY 1\nframes 4\n1\n2\n1\n1\n'
    for bad in (header+rows.replace(',2,0',',0,0'), header+rows.splitlines()[0]+'\n',
                (header+rows).replace('version=2','version=1')):
        capture.write_text(bad)
        try:
            replay_text(capture)
        except ValueError:
            pass
        else:
            raise AssertionError('bad cadence capture accepted')
    (work/'test.c').write_text(prefix+loader+suffix)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-I',str(root/'platform/3ds/include'),str(work/'test.c'),
                    str(root/'platform/3ds/source/ge_3ds_alloc_profile.c'),
                    '-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],cwd=work,check=True)

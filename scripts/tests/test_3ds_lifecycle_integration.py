#!/usr/bin/env python3
"""Execute the actual main-loop resume helper against ordered service fixtures."""
from pathlib import Path
import re
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
source = (ROOT/'platform/3ds/source/main.c').read_text()
start = source.index('static bool handle_runtime_resume(')
end = source.index('\ntypedef struct RuntimeFrontendEmbeddedTexture', start)
helper = source[start:end]
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;
typedef int GeOriginalMusicRuntime;
#define KEY_TOUCH 0x100U
static int controls;
static struct { bool enabled,result_written,failed,finished; char failure[128]; } frontend_probe;
static int pending, order, finished, waited, drained, scanned, resumed;
static bool sink_active;
static u32 keys;
static bool ge_3ds_lifecycle_take_resume(void) { int p=pending; pending=0; return p!=0; }
static int ge_original_music_runtime_finish(GeOriginalMusicRuntime *m) { assert(m); assert(order==0); order=1; ++finished; return 0; }
static void ge_3ds_music_worker_wait(void) { assert(order==0 || order==1); order=2; ++waited; }
static void ge_3ds_audio_resume(void) { assert(order==2); order=3; ++drained; }
static bool ge_3ds_audio_is_active(void) { assert(order==3); return sink_active; }
static void hidScanInput(void) { assert(order==3); order=4; ++scanned; }
static u32 hidKeysHeld(void) { assert(order==4); return keys; }
static u32 map_actions(u32 k) { return k & 0xff; }
static void ge_3ds_controls_resume(int *c, u32 actions, bool touch) {
assert(c==&controls && order==4 && actions==(keys&0xff)); assert(touch==((keys&KEY_TOUCH)!=0)); ++resumed; }
'''
suffix = r'''
int main(void) {
GeOriginalMusicRuntime music=1; bool active=true;
assert(!handle_runtime_resume(&music,&active)); assert(!finished&&!waited&&!drained&&!scanned&&!resumed);
pending=1; keys=KEY_TOUCH|3; sink_active=true; frontend_probe.enabled=true;
assert(handle_runtime_resume(&music,&active)); assert(active);
assert(finished==1&&waited==1&&drained==1&&scanned==1&&resumed==1);
assert(frontend_probe.finished && frontend_probe.failed && strstr(frontend_probe.failure,"interrupted"));
assert(!handle_runtime_resume(&music,&active)); assert(resumed==1);
order=0; pending=1; sink_active=false; frontend_probe.result_written=true; frontend_probe.failed=false;
assert(handle_runtime_resume(NULL,&active)); assert(!active && finished==1 && waited==2 && drained==2);
assert(!frontend_probe.failed); return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    c=Path(temp)/'resume.c'; binary=Path(temp)/'resume'
    c.write_text(prefix+helper+suffix)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
# Check the surrounding clock path because this is what prevents sleep time
# becoming thousands of authored simulation ticks. No slow-frame clamp exists.
loop = source[source.index('if (handle_runtime_resume(original_music, &audio_active))'):]
loop = loop[:loop.index('GePortInput input = read_input')]
assert 'previous_time = osGetTime();' in loop
assert 'last_submit_vblank = C3D_FrameCounter(0);' in loop
assert 'port.accumulator_seconds = 0.0;' in loop
assert '++input_probe.suspend_events' in loop
assert 'runtime->suspend_events == 0U' in source
assert 'if (renderer_ready) renderer_exit();' in source
print('Main resume: joined producers, drained audio, held-input release gate, rebase and explicit capture invalidation passed')

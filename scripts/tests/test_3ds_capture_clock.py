#!/usr/bin/env python3
"""SELECT's blocking SD write must not become simulation catch-up time."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'platform/3ds/source/main.c').read_text()
start = source.index('            if (frame_began && world_diagnostic_pending) {')
end = source.index('\n            if (input_probe.enabled && frame_began)', start)
branch = source[start:end]
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef uint64_t u64;
typedef int Vertex;
#define DAM_ROOM_VERTEX_OFFSET 3
static u64 clock_ms, io_ms;
static unsigned captures;
static u64 osGetTime(void) { return clock_ms; }
static void capture_world_diagnostic(void *a, void *b, void *c,
        const Vertex *vertices, void *target) {
    (void)a; (void)b; (void)c; (void)vertices; (void)target;
    ++captures;
    clock_ms += io_ms;
}
static u64 exercise(bool frame_began, bool pending, u64 delay) {
    bool world_diagnostic_pending = pending, probe_capture_io = false;
    int dam_preview = 0, dam_intro = 0, stage_ordinary_objects = 0;
    Vertex vertices[8] = {0}; void *vertex_buffer = vertices, *top_target = 0;
    u64 previous_time = 10000;
    /* Ordinary pre-capture work has already elapsed this frame. */
    clock_ms = previous_time + 7;
    io_ms = delay;
    unsigned before = captures;
''' + branch + r'''
    bool captured = frame_began && pending;
    assert(captures == before + captured);
    assert(probe_capture_io == captured);
    assert(world_diagnostic_pending == (pending && !frame_began));
    clock_ms += 10; /* Ordinary post-capture/render/audio work. */
    return clock_ms - previous_time;
}
int main(void) {
    const u64 delays[] = {0, 1, 25, 11500, 60000};
    for (unsigned i = 0; i < sizeof(delays)/sizeof(delays[0]); ++i) {
        assert(exercise(true, true, delays[i]) == 17);
        assert(exercise(true, false, delays[i]) == 17);
        assert(exercise(false, true, delays[i]) == 17);
    }
    puts("SELECT clock excludes SD stalls and retains normal frame time/pending capture");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-capture-clock-') as temp:
    path = Path(temp)
    (path/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined',str(path/'test.c'),
                    '-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
    old = unit.replace('previous_time += osGetTime() - capture_started_ms;',
                       '(void)capture_started_ms;')
    assert old != unit
    (path/'old.c').write_text(old)
    subprocess.run(['cc','-std=c11',str(path/'old.c'),'-o',str(path/'old')],check=True)
    result = subprocess.run([str(path/'old')],capture_output=True)
    assert result.returncode != 0, 'old capture catch-up must fail the regression'

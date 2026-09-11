#!/usr/bin/env python3
"""Camera-up, raster depth and invalid-coordinate checks on the live projector."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
unit = r'''
#include <assert.h>
#include <math.h>
#include "ge_3ds_original_frontend_cast.h"
int main(void) {
    float center[3], head[3], foot[3], near[3], far[3], cast[3];
    assert(ge_3ds_original_frontend_bond_project((float[]){0,0,-100}, center));
    assert(center[0] == 200 && center[1] == 120);
    assert(ge_3ds_original_frontend_bond_project((float[]){0,20,-100}, head));
    assert(ge_3ds_original_frontend_bond_project((float[]){0,-20,-100}, foot));
    /* Framebuffer is top-down; the ortho vertex input is bottom-up. */
    assert(240-head[1] < 240-center[1] && 240-center[1] < 240-foot[1]);
    assert(ge_3ds_original_frontend_cast_project((float[]){0,20,-100}, cast));
    assert(fabsf(cast[0]-head[0]) < .0001f && fabsf(cast[1]-head[1]) < .0001f);
    assert(ge_3ds_original_frontend_bond_project((float[]){0,0,-100}, near));
    assert(ge_3ds_original_frontend_bond_project((float[]){0,0,-1000}, far));
    /* Simulate GPU_GEQUAL against a cleared zero depth buffer in both orders. */
    float depth=0; int visible=0;
    if (near[2]>=depth) {depth=near[2];visible=1;}
    if (far[2]>=depth) {depth=far[2];visible=2;}
    assert(visible==1);
    depth=0;visible=0;
    if (far[2]>=depth) {depth=far[2];visible=2;}
    if (near[2]>=depth) {depth=near[2];visible=1;}
    assert(visible==1);
    assert(!ge_3ds_original_frontend_bond_project((float[]){0,0,0}, head));
    assert(!ge_3ds_original_frontend_bond_project((float[]){NAN,0,-100}, head));
    assert(!ge_3ds_original_frontend_bond_project((float[]){0,INFINITY,-100}, head));
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-I'+str(root/'port/include'),
                    str(p/'test.c'),str(root/'port/src/ge_3ds_original_frontend_cast.c'),
                    '-lm','-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
source=(root/'platform/3ds/source/main.c').read_text()
body=source.split('static bool prepare_original_frontend_gunbarrel_bond(',1)[1]
body=body.split('static bool prepare_original_frontend_cast(',1)[0]
assert 'ge_3ds_original_frontend_bond_project(source->world, projected)' in body
print('Gun-barrel projection: upright screen position, near-surface occlusion and invalid inputs passed')

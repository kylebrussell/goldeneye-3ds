#!/usr/bin/env python3
"""Run the real GPU state publisher against controlled depth/alpha fragments."""
from pathlib import Path
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / 'scripts'))
from extract_dam_guard_chr_scheduler_slice import function_text
source = (repo / 'platform/3ds/source/ge_3ds_material.c').read_text()
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ge_pica_apply.h"
typedef struct { unsigned maxLevel; } C3D_Tex;
#include "ge_3ds_material.h"
typedef int GPU_WRITEMASK;
enum { GPU_WRITE_COLOR=1, GPU_WRITE_DEPTH=2, GPU_GREATER=3, GPU_EQUAL=4,
       GPU_ALWAYS=5, GPU_LINEAR=6, GPU_BLEND_ADD=7, GPU_SRC_ALPHA=8,
       GPU_ONE_MINUS_SRC_ALPHA=9, GPU_ONE=10, GPU_ZERO=11 };
static bool depth_on, alpha_on, blend_on;
static int depth_func, depth_mask, alpha_threshold;
static unsigned depth_calls;
static int ge_3ds_material_wrap(int x) { return x; }
static int ge_3ds_material_filter(int x) { return x; }
static int ge_3ds_material_cull(int x) { return x; }
static void ge_3ds_material_apply_texenv(const GePicaApplyState *s) { (void)s; }
static void C3D_TexSetWrap(C3D_Tex *t,int a,int b) {(void)t;(void)a;(void)b;}
static void C3D_TexSetFilter(C3D_Tex *t,int a,int b) {(void)t;(void)a;(void)b;}
static void C3D_TexSetFilterMipmap(C3D_Tex *t,int a) {(void)t;(void)a;}
static void C3D_TexBind(int n,C3D_Tex *t) {(void)n;(void)t;}
static void C3D_CullFace(int x) {(void)x;}
static void C3D_DepthTest(bool enabled,int function,int mask) {
    depth_on=enabled; depth_func=function; depth_mask=mask; depth_calls++;
}
static void C3D_AlphaTest(bool enabled,int function,int threshold) {
    assert(function==GPU_GREATER); alpha_on=enabled; alpha_threshold=threshold;
}
static void C3D_AlphaBlend(int a,int b,int src,int dst,int asrc,int adst) {
    assert(a==GPU_BLEND_ADD && b==GPU_BLEND_ADD && asrc==GPU_ONE && adst==GPU_ZERO);
    assert((src==GPU_SRC_ALPHA && dst==GPU_ONE_MINUS_SRC_ALPHA) || (src==GPU_ONE && dst==GPU_ZERO));
    blend_on=src==GPU_SRC_ALPHA;
}
'''
suffix = r'''
static double zbuffer, framebuffer;
static void fragment(double z, unsigned alpha, double color) {
    if (alpha_on && alpha <= (unsigned)alpha_threshold) return;
    if (depth_on && depth_func==GPU_GREATER && !(z>zbuffer)) return;
    if (depth_on && depth_func==GPU_EQUAL && z!=zbuffer) return;
    if (depth_mask & GPU_WRITE_COLOR)
        framebuffer=blend_on ? color*alpha/255.0+framebuffer*(255-alpha)/255.0 : color;
    if (depth_mask & GPU_WRITE_DEPTH) zbuffer=z;
}
static Ge3dsMaterialResult apply(GePicaMaterial *m, const Ge3dsMaterialResult *previous) {
    Ge3dsMaterialResult result={0};
    assert(ge_pica_apply_compile(m,&result.state)==GE_PICA_APPLY_OK);
    assert(ge_3ds_material_apply_prepared_delta(&result,NULL,previous,NULL)==GE_3DS_MATERIAL_OK);
    return result;
}
int main(void) {
    GePicaMaterial m={0}; m.depth_test_enabled=1; m.depth_write_enabled=1;
    Ge3dsMaterialResult opaque=apply(&m,NULL);
    fragment(.5,255,20); assert(zbuffer==.5 && framebuffer==20);
    m.alpha_test=GE_PICA_ALPHA_TEST_THRESHOLD; m.alpha_threshold=0;
    Ge3dsMaterialResult cutout=apply(&m,&opaque);
    fragment(.9,0,200); assert(zbuffer==.5 && framebuffer==20);
    m.alpha_test=GE_PICA_ALPHA_TEST_DISABLED;
    opaque=apply(&m,&cutout); fragment(.75,0,80); assert(zbuffer==.75 && framebuffer==80);
    /* Real decal tolerance needs scene validation: strict equality erased the
     * Facility room 66 exit sign. Do not model that approximation as parity. */
    m.depth_write_enabled=0;
    unsigned before=depth_calls; m.depth_mode=GE_PICA_DEPTH_TRANSLUCENT; m.blend_enabled=1;
    Ge3dsMaterialResult glass=apply(&m,&opaque); assert(depth_calls==before+1);
    fragment(.9,128,200); assert(zbuffer==.75 && framebuffer>140 && framebuffer<141);
    double old=framebuffer; fragment(.5,255,0); assert(framebuffer==old);
    m.depth_write_enabled=1; m.blend_enabled=0; m.depth_mode=GE_PICA_DEPTH_OPAQUE;
    opaque=apply(&m,&glass); fragment(.8,128,33); assert(framebuffer==33 && zbuffer==.8);
    m.depth_test_enabled=0; m.depth_write_enabled=0; m.alpha_test=GE_PICA_ALPHA_TEST_THRESHOLD;
    for (unsigned threshold=0; threshold<256; ++threshold) {
        m.alpha_threshold=threshold; apply(&m,NULL);
        for (unsigned alpha=0; alpha<256; ++alpha) {
            framebuffer=0; fragment(0,alpha,1);
            assert(framebuffer==(alpha>threshold ? 1 : 0));
        }
    }
    puts("captured GPU state: cutout color/depth rejection, glass occlusion/blending/reset, 65536 threshold pairs passed");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-fragments-') as temporary:
    path = Path(temporary)
    (path / 'citro3d.h').write_text('/* API declarations supplied by the captured command sink. */\n')
    (path / 'test.c').write_text(prefix + function_text(source, 'ge_3ds_material_apply_prepared_delta') + suffix)
    for flags in ([], ['-fshort-enums']):
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-fsanitize=address,undefined', '-fno-omit-frame-pointer', *flags,
            '-I', str(path), '-I', str(repo / 'port/include'),
            '-I', str(repo / 'platform/3ds/include'), str(path / 'test.c'),
            str(repo / 'port/src/ge_pica_apply.c'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)

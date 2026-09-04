#!/usr/bin/env python3
"""Exercise the real PICA glass setup with a captured TexEnv command sink."""
from pathlib import Path
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / 'scripts'))
from extract_dam_guard_chr_scheduler_slice import function_text

source = (repo / 'platform/3ds/source/ge_3ds_material.c').read_text()
test = r'''
#include "ge_pica_apply.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef enum { GPU_TEXTURE0, GPU_CONSTANT, GPU_PRIMARY_COLOR } GPU_TEVSRC;
typedef enum { GPU_REPLACE, GPU_MODULATE, GPU_MULTIPLY_ADD } GPU_COMBINEFUNC;
enum { C3D_RGB, C3D_Alpha };
typedef struct { GPU_TEVSRC src[2][3]; GPU_COMBINEFUNC op[2]; uint32_t color; } C3D_TexEnv;
static C3D_TexEnv env;
static C3D_TexEnv *C3D_GetTexEnv(unsigned n) { assert(n == 0); return &env; }
static void C3D_TexEnvInit(C3D_TexEnv *e) { memset(e, 0, sizeof(*e)); }
static void C3D_TexEnvSrc(C3D_TexEnv *e, unsigned c, GPU_TEVSRC a, GPU_TEVSRC b, GPU_TEVSRC d) {
    e->src[c][0] = a; e->src[c][1] = b; e->src[c][2] = d;
}
static void C3D_TexEnvFunc(C3D_TexEnv *e, unsigned c, GPU_COMBINEFUNC op) { e->op[c] = op; }
static void C3D_TexEnvColor(C3D_TexEnv *e, uint32_t color) { e->color = color; }
''' + '\n'.join(function_text(source, name) for name in (
    'ge_3ds_material_source', 'ge_3ds_material_combine',
    'ge_3ds_material_color', 'ge_3ds_material_apply_texenv')) + r'''
int main(void) {
    GePicaMaterial material = {0};
    GePicaApplyState state;
    material.color_combine = GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE;
    material.alpha_combine = GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE_ADD_PRIMITIVE;
    for (unsigned opacity = 0; opacity < 256; ++opacity) {
        material.primitive_color.alpha = (uint8_t)opacity;
        assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
        ge_3ds_material_apply_texenv(&state);
        assert(env.op[C3D_RGB] == GPU_MODULATE);
        assert(env.op[C3D_Alpha] == GPU_MULTIPLY_ADD);
        assert(env.src[C3D_Alpha][0] == GPU_TEXTURE0);
        assert(env.src[C3D_Alpha][1] == GPU_PRIMARY_COLOR);
        assert(env.src[C3D_Alpha][2] == GPU_CONSTANT);
        assert((env.color >> 24) == opacity);
    }
    /* An ordinary material after glass must restore the non-additive mode. */
    material.alpha_combine = GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE;
    assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
    ge_3ds_material_apply_texenv(&state);
    assert(env.op[C3D_Alpha] == GPU_MODULATE);
    assert(env.src[C3D_Alpha][2] == GPU_PRIMARY_COLOR);
    puts("glass GPU setup: all 256 authored opacity bytes and subsequent state reset passed");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-glass-gpu-') as temporary:
    path = Path(temporary)
    (path / 'test.c').write_text(test)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(repo / 'port/include'), str(path / 'test.c'),
                    str(repo / 'port/src/ge_pica_apply.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)

#!/usr/bin/env python3
"""Check original model alpha commands through decoding and real GPU publication.

This is command/arithmetic evidence, not an N64 or PICA pixel comparison.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / "scripts"))
from extract_dam_guard_chr_scheduler_slice import function_text

model = (repo / "src/game/model.c").read_text()
material = (repo / "platform/3ds/source/ge_3ds_material.c").read_text()
constants = (repo / "src/bondconstants.h").read_text()
prop_type = re.search(r"typedef enum PROP_TYPE\s*\{.*?\} PROP_TYPE;", constants, re.S)
assert prop_type
test = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <PR/mbi.h>
#include "ge_pica_apply.h"
/* Preserve SDK command bits without its signed bit-31 shift. */
#undef GBL_c1
#define GBL_c1(a,b,c,d) ((uint32_t)(a)<<30 | (uint32_t)(b)<<26 | (uint32_t)(c)<<22 | (uint32_t)(d)<<18)
typedef struct {
    Gfx *gdl;
    int PropType, zbufferenabled;
    struct { uint32_t word; } fogcolour, envcolour;
} ModelRenderData;
''' + prop_type.group() + "\n" + "\n".join(
    function_text(model, name) for name in (
        "modelApplyRenderModeType3", "modelApplyRenderModeType4")) + r'''
typedef enum { GPU_TEXTURE0, GPU_CONSTANT, GPU_PRIMARY_COLOR } GPU_TEVSRC;
typedef enum { GPU_REPLACE, GPU_MODULATE, GPU_MULTIPLY_ADD, GPU_ADD } GPU_COMBINEFUNC;
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
''' + "\n".join(function_text(material, name) for name in (
    "ge_3ds_material_source", "ge_3ds_material_combine",
    "ge_3ds_material_color", "ge_3ds_material_apply_texenv")) + r'''
static GePicaMaterial authored_material(unsigned type, unsigned prop,
    unsigned opacity, unsigned secondary, unsigned texture_enabled) {
    Gfx commands[32];
    ModelRenderData render = {0};
    GeGbiRenderState state;
    GePicaMaterial material;
    render.gdl = commands;
    render.PropType = (int)prop;
    render.envcolour.word = opacity;
    render.zbufferenabled = 1;
    if (type == 3) modelApplyRenderModeType3(&render, !secondary);
    else modelApplyRenderModeType4(&render, !secondary);
    ge_gbi_state_init(&state);
    state.texture.enabled = (uint8_t)texture_enabled;
    state.active_texture_binding = GE_GBI_TEXTURE_BINDING_IMAGE;
    state.texture_image_valid = 1;
    for (Gfx *g = commands; g != render.gdl; ++g) {
        uint8_t bytes[8];
        GeGbiCommand command;
        GeGbiStateAction action;
        uint32_t words[2] = {(uint32_t)g->words.w0, (uint32_t)g->words.w1};
        for (unsigned word = 0; word < 2; ++word)
            for (unsigned byte = 0; byte < 4; ++byte)
                bytes[4*word+byte] = (uint8_t)(words[word] >> (24-8*byte));
        assert(ge_gbi_decode_command(bytes, sizeof(bytes), GE_GBI_BYTE_ORDER_BIG_ENDIAN,
            0, &command) == GE_GBI_STATUS_OK);
        assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    }
    assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
    return material;
}
static unsigned source_alpha(GPU_TEVSRC source, unsigned shade, unsigned texture) {
    switch (source) {
    case GPU_TEXTURE0: return texture;
    case GPU_PRIMARY_COLOR: return shade;
    case GPU_CONSTANT: return env.color >> 24;
    }
    assert(0); return 0;
}
static unsigned alpha_numerator(unsigned shade, unsigned texture) {
    unsigned a = source_alpha(env.src[C3D_Alpha][0], shade, texture);
    unsigned b = source_alpha(env.src[C3D_Alpha][1], shade, texture);
    switch (env.op[C3D_Alpha]) {
    case GPU_REPLACE: return a*255;
    case GPU_MODULATE: return a*b;
    case GPU_ADD: return (a+b > 255 ? 255 : a+b)*255;
    case GPU_MULTIPLY_ADD: {
        unsigned c = source_alpha(env.src[C3D_Alpha][2], shade, texture);
        return a*b+c*255 > 65025 ? 65025 : a*b+c*255;
    }
    }
    assert(0); return 0;
}
int main(void) {
    for (unsigned type = 3; type <= 4; ++type) {
        /* Canonical branches use a<255 for fading and a>0 for edge alpha. */
        for (unsigned opacity = 0; opacity < 255; ++opacity) {
            GePicaMaterial m = authored_material(type, PROP_TYPE_WEAPON+1, opacity, 0, 1);
            GePicaApplyState state;
            assert(m.alpha_combine == GE_PICA_ALPHA_SHADE_MODULATE_ENVIRONMENT);
            assert(ge_pica_apply_compile(&m, &state) == GE_PICA_APPLY_OK);
            ge_3ds_material_apply_texenv(&state);
            for (unsigned shade = 0; shade <= 255; ++shade) {
                assert(alpha_numerator(shade, 0) == opacity*shade);
                assert(alpha_numerator(shade, 255) == opacity*shade);
            }
            /* Texture enable only governs sampled channels. It cannot erase
             * an authored shade/environment-only alpha expression. */
            m = authored_material(type, PROP_TYPE_WEAPON+1, opacity, 0, 0);
            assert(m.alpha_combine == GE_PICA_ALPHA_SHADE_MODULATE_ENVIRONMENT);
        }
        for (unsigned opacity = 1; opacity <= 255; ++opacity) {
            GePicaMaterial m = authored_material(type, PROP_TYPE_SMOKE+1, opacity, 0, 1);
            GePicaApplyState state;
            assert(m.alpha_combine == GE_PICA_ALPHA_SHADE_ADD_ENVIRONMENT);
            assert(ge_pica_apply_compile(&m, &state) == GE_PICA_APPLY_OK);
            ge_3ds_material_apply_texenv(&state);
            for (unsigned shade = 0; shade <= 255; ++shade) {
                unsigned expected = (shade+opacity > 255 ? 255 : shade+opacity)*255;
                assert(alpha_numerator(shade, 0) == expected);
                assert(alpha_numerator(shade, 255) == expected);
            }
        }
    }
    /* State after the additive branch must restore ordinary texture alpha. */
    GePicaMaterial m = authored_material(4, PROP_TYPE_WEAPON+1, 255, 0, 1);
    GePicaApplyState state;
    assert(m.alpha_combine == GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE);
    assert(ge_pica_apply_compile(&m, &state) == GE_PICA_APPLY_OK);
    ge_3ds_material_apply_texenv(&state);
    assert(alpha_numerator(128, 0) == 0 && alpha_numerator(128, 255) == 32640);
    puts("original type3/type4 commands -> GPU alpha: fade/edge domains, texture independence and reset passed");
}
'''
with tempfile.TemporaryDirectory(prefix="ge-model-env-alpha-") as temporary:
    path = Path(temporary)
    (path / "test.c").write_text(test)
    for flags in ([], ["-fshort-enums"]):
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-D_LANGUAGE_C",
            *flags, "-I", str(repo / "port/include"), "-idirafter", str(repo / "include"),
            str(path / "test.c"), *(str(repo / "port/src" / name) for name in (
                "ge_gbi_decoder.c", "ge_gbi_matrix.c", "ge_gbi_state.c",
                "ge_pica_material.c", "ge_pica_apply.c")), "-o", str(path / "test")], check=True)
        subprocess.run([str(path / "test")], check=True)

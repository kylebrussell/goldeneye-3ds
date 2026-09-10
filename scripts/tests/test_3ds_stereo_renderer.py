#!/usr/bin/env python3
"""Execute actual stereo renderer helpers with a fake GPU and real culling."""

from pathlib import Path
import os
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()


def function(name):
    match = re.search(rf"static\s+[\w\s*]+\b{name}\s*\([^;]*?\)\s*\{{", source)
    assert match, name
    depth = 0
    for index in range(source.index("{", match.start()), len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if depth == 0:
            return source[match.start():index + 1]
    raise AssertionError(name)


draw = function("renderer_draw")
assert "fmaxf(1.0f / 160.0f," in draw
assert "widened[2], cull_margin" in draw
world = draw[:draw.index("phase_ticks[2] = svcGetSystemTick();")]
assert "GE_3DS_STEREO_LAYER_FIRST_PERSON" not in world
assert "renderer_set_stereo_layer(GE_3DS_STEREO_LAYER_FIRST_PERSON);" in draw
flash = draw[draw.index("if(guard_muzzle_flash_vertex_count"):
             draw.index("if (autogun_beam_vertex_count")]
flash_layer_call = re.search(r"renderer_set_stereo_layer\([^;]+;", flash).group()
shader = (repo / "platform/3ds/source/vshader.v.pica").read_text()
for instruction in ("mul r2, stereo[0], r1.wwww", "add r1, r1, r2",
                    "add outpos, stereo[1], r1"):
    assert instruction in shader
init = function("renderer_init")
assert init.index('shaderInstanceGetUniformLocation(shader_program.vertexShader, "stereo")') < init.index("stereo_uniform_cache_valid = false;")

harness = r'''
#include "ge_3ds_stereo.h"
#include "ge_draw_batch_visibility.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#define GPU_VERTEX_SHADER 0
static int stereo_uniform = 4;
static bool stereo_uniform_cache_valid;
static Ge3dsStereoConfig stereo_config;
static float stereo_slider;
static int stereo_current_eye;
static float stereo_screen_x_column[2] = {0.0f, -0.005f};
static unsigned uploads;
static float registers[16][4];
static void C3D_FVUnifSet(int shader, int index, float x, float y, float z, float w)
{
    assert(shader == GPU_VERTEX_SHADER && index >= 0 && index < 16);
    registers[index][0] = x; registers[index][1] = y;
    registers[index][2] = z; registers[index][3] = w;
    ++uploads;
}
''' + function("renderer_set_stereo_layer") + "\n" + function("renderer_stereo_widen_clip") + '''
static void install_guard_flash_layer(void) {
''' + flash_layer_call + r'''
}
static void check_guard_flash_depth(void)
{
    const float depths[]={15,45,180,1000};
    ge_3ds_stereo_config_defaults(&stereo_config);
    for(int eye=-1;eye<=1;eye+=2) for(unsigned step=0;step<=2;++step) {
        stereo_current_eye=eye;stereo_slider=0.5f*(float)step;
        renderer_set_stereo_layer(GE_3DS_STEREO_LAYER_WORLD);
        float weapon_registers[2][4];
        memcpy(weapon_registers,registers+stereo_uniform,sizeof(weapon_registers));
        install_guard_flash_layer();
        for(unsigned depth=0;depth<sizeof(depths)/sizeof(depths[0]);++depth)
            for(unsigned axis=0;axis<2;++axis) {
                /* Actual shader: clip += coefficient*W + constant, then
                 * perspective division. The shared muzzle point must stay
                 * attached in both eyes at every slider/depth sample. */
                const float w=depths[depth],base=7.0f+(float)axis;
                const float weapon=(base+weapon_registers[0][axis]*w
                    +weapon_registers[1][axis])/w;
                const float flash=(base+registers[stereo_uniform][axis]*w
                    +registers[stereo_uniform+1][axis])/w;
                assert(flash==weapon);
            }
    }
}
static void check_layer(Ge3dsStereoLayer layer, unsigned expected_uploads)
{
    Ge3dsStereoUniforms expected;
    renderer_set_stereo_layer(layer);
    assert(uploads == expected_uploads);
    ge_3ds_stereo_uniforms(&stereo_config, stereo_slider, stereo_current_eye,
        layer, stereo_screen_x_column, &expected);
    assert(memcmp(registers[stereo_uniform], expected.coefficient, 2*sizeof(float)) == 0);
    assert(memcmp(registers[stereo_uniform+1], expected.constant, 2*sizeof(float)) == 0);
}
int main(void)
{
    ge_3ds_stereo_config_defaults(&stereo_config);
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 2);
    for(int layer=0;layer<GE_3DS_STEREO_LAYER_COUNT;++layer)
        check_layer((Ge3dsStereoLayer)layer, 2);
    stereo_slider=1.0f; stereo_current_eye=-1;
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 4);
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 4);
    check_layer(GE_3DS_STEREO_LAYER_FIRST_PERSON, 6);
    stereo_current_eye=1;
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 8);
    stereo_slider=0.5f;
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 10);
    stereo_config.enabled=false;
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 12);
    check_layer(GE_3DS_STEREO_LAYER_HUD, 12);
    stereo_uniform=8;
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 14);
    /* Reinitializing a shader at the same index cannot reuse old registers. */
    stereo_uniform_cache_valid=false;
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 16);
    check_layer(GE_3DS_STEREO_LAYER_WORLD, 16);

    /* A small triangle outside the old capped mono margin enters the
     * right-eye image at depth15: 1.25 - 88/160 = 0.7 NDC. */
    GeDamRoomWorldVertex vertices[3] = {0};
    GeDamRoomDrawBatch batch = {0};
    batch.vertex_count=3;
    for(size_t i=0;i<3;++i) {
        vertices[i].world[0]=18.75f+(float)i*0.75f;
        vertices[i].world[1]=(float)i*0.25f;
        vertices[i].world[2]=15.0f;
    }
    const float matrix[4][4]={{1,0,0,0},{0,1,0,0},{0,0,0,1},{0,0,0,0}};
    float widened[4][4];
    renderer_stereo_widen_clip(matrix,widened,16.0f/160.0f);
    assert(!ge_draw_batch_world_may_intersect_clip_frustum(vertices,3,&batch,widened));
    ge_3ds_stereo_config_defaults(&stereo_config);
    float margin=ge_3ds_stereo_max_world_shift_px(&stereo_config,1.0f,15.0f)/160.0f;
    renderer_stereo_widen_clip(matrix,widened,margin);
    assert(ge_draw_batch_world_may_intersect_clip_frustum(vertices,3,&batch,widened));
    for(size_t row=0;row<4;++row)
        for(size_t col=1;col<4;++col)assert(widened[row][col]==matrix[row][col]);
    renderer_stereo_widen_clip(matrix,widened,0.0f);
    assert(memcmp(matrix,widened,sizeof(matrix))==0);
    renderer_stereo_widen_clip(matrix,widened,INFINITY);
    assert(ge_draw_batch_world_may_intersect_clip_frustum(vertices,3,&batch,widened));
    check_guard_flash_depth();
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="ge-stereo-renderer-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(harness)
    subprocess.run([
        "cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-I" + str(repo / "port/include"),
        "-I" + str(repo / "platform/3ds/include"),
        str(path / "test.c"), str(repo / "port/src/ge_draw_batch_visibility.c"),
        str(repo / "platform/3ds/source/ge_3ds_stereo.c"),
        "-lm", "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True, env={
        **os.environ, "ASAN_OPTIONS": "detect_leaks=0", "UBSAN_OPTIONS": "halt_on_error=1",
    })
print("3DS stereo renderer: nearby eye-visible geometry retained; guard flash depth matches world weapon; redundant uniform uploads skipped")

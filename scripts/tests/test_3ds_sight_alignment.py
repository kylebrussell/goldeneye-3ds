#!/usr/bin/env python3
"""Check the real sight submission and canonical aim signs without a GPU."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
def function(path, marker):
    source = (repo / path).read_text()
    start = source.index(marker)
    depth = 0
    for end in range(source.index('{', start), len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if depth == 0:
            return source[start:end + 1]

builder = function('platform/3ds/source/main.c', 'static size_t build_original_sight_vertices(')
conversion = function('port/src/ge_3ds_original_hud.c', 'float ge_3ds_original_hud_screen_y(')
canonical = function('src/game/gunfire.c', 'void caclulate_gun_crosshair_position_rotation(')
harness = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ge_3ds_controls.h"
#include "ge_original_gun_sight.h"
typedef struct {float x,y,z,u,v,r,g,b,a;} Vertex;
typedef struct { GeOriginalGunSightSnapshot sight;
 GePicaTextureRectangleDraw sight_draw; uint8_t sight_visible; } GeOriginalMultiplayerHudSnapshot;
static unsigned gun_sight_frames,gun_sight_failures,gun_sight_visible_frames,gun_sight_suppression;
static bool gun_sight_texture_loaded=true;
static int gun_sight_subtexture;
#define CROSSHAIR_VERTEX_COUNT 6U
#define GE_HUD_SCREEN_HEIGHT 240U
static GeOriginalMultiplayerHudSnapshot fixture;
int ge_original_gun_sight_snapshot(GeOriginalGunSightSnapshot *snapshot) {
 *snapshot=fixture.sight; return 1;
}
int ge_original_gun_sight_build_draw(const GeOriginalGunSightSnapshot *snapshot,
 GePicaTextureRectangleDraw *draw,uint8_t *visible) {
 (void)snapshot; *draw=fixture.sight_draw; *visible=fixture.sight_visible; return 1;
}
static void Tex3DS_SubTextureTopLeft(const int *s,float *u,float *v) {(void)s;*u=0;*v=0;}
static void Tex3DS_SubTextureTopRight(const int *s,float *u,float *v) {(void)s;*u=1;*v=0;}
static void Tex3DS_SubTextureBottomLeft(const int *s,float *u,float *v) {(void)s;*u=0;*v=1;}
static void Tex3DS_SubTextureBottomRight(const int *s,float *u,float *v) {(void)s;*u=1;*v=1;}
typedef float f32;
typedef int s32;
typedef union {float f[3]; struct {float x,y,z;};} coord3d;
static struct {float guncrossdamp,gunaimdamp,crosshair_x_pos,crosshair_y_pos,
 gun_azimuth_angle,gun_azimuth_turning; coord3d crosshair_angle,field_FFC;} player;
#define g_CurrentPlayer (&player)
static int g_ClockTimer=1;
static float getPlayer_c_screenwidth(void) {return 320;}
static float getPlayer_c_screenheight(void) {return 240;}
static float getPlayer_c_screenleft(void) {return 0;}
static float getPlayer_c_screentop(void) {return 0;}
static void transformAndNormalizeByLength2Dto3D(const coord3d *screen,coord3d *world,float depth) {
 *world=*screen; (void)depth;
}
static void sub_GAME_7F067AB4(coord3d *coords) {(void)coords;}
'''
tests = r'''
static float submit(float native_y, bool captured) {
 Vertex vertices[6]; GePicaTextureRectangleDraw draw;
 memset(&fixture,0,sizeof(fixture)); fixture.sight_visible=1;
 for(unsigned i=0;i<6;i++) {
  fixture.sight_draw.vertices[i].x=160;
  fixture.sight_draw.vertices[i].y=native_y;
  fixture.sight_draw.vertices[i].texture_v=(float)(i%2);
 }
 assert(build_original_sight_vertices(vertices,&draw,captured?&fixture:NULL)==6);
 for(unsigned i=0;i<6;i++) {
  assert(vertices[i].x==160);
  assert(vertices[i].y==240-native_y);
  assert(vertices[i].v==(float)(i%2));
 }
 return vertices[0].y;
}
int main(void) {
 for(unsigned captured=0;captured<2;captured++) {
  assert(submit(40,captured)>120); /* native upper remains upper on PICA */
  assert(submit(200,captured)<120);
  assert(submit(120,captured)==120);
 }
 for(unsigned inverted=0;inverted<2;inverted++) {
  Ge3dsControls controls; ge_3ds_controls_init(&controls);
  controls.settings.invert_look_y=inverted;
  Ge3dsControlSample sample={0}; sample.look_y=156; sample.held=GE_PORT_ACTION_AIM;
  GePortInput input=ge_3ds_controls_sample(&controls,&sample,true);
  assert((input.look_y<0)==(inverted!=0));
  memset(&player,0,sizeof(player));
  /* The same signed native turn enters the gun and cursor smoothing. */
  caclulate_gun_crosshair_position_rotation(0,input.look_y*0.65f,0.8f,0.9f);
  float cursor=player.crosshair_angle.y-120;
  float gun=player.field_FFC.y-120;
  assert(cursor*gun>0);
  assert((cursor<0)==(inverted!=0));
  float rendered=submit(player.crosshair_angle.y,false)-120;
  assert(rendered*gun<0); /* top-down native gun projection, bottom-up overlay */
 }
 puts("Sight alignment: upper/lower/center, live/captured, normal/inverted gun and cursor signs passed");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-sight-alignment-') as directory:
    root=Path(directory)
    (root/'test.c').write_text(harness+conversion+canonical+builder+tests)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(repo/'port/include'),'-I'+str(repo/'platform/3ds/include'),
        str(root/'test.c'),str(repo/'platform/3ds/source/ge_3ds_controls.c'),
        '-lm','-o',str(root/'test')],check=True)
    subprocess.run([str(root/'test')],check=True)

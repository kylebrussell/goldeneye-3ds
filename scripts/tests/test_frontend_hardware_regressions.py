#!/usr/bin/env python3
"""Exercise live wallet projection and barrel stencil state; guard solo startup order."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'platform/3ds/source/main.c').read_text()
wallet = source.split('/* frontSetupMenuBackground raises BOTH', 1)[1].split('*/', 1)[1].split('\n            }', 1)[0]
draw = source.split('static void draw_original_frontend_list(', 1)[1]
draw = draw.split('} else if (menu == MENU_EYE_INTRO && runtime != NULL)', 1)[1]
draw = draw.split('} else if (menu != MENU_LEGAL_SCREEN', 1)[0]
hole = draw.index('if (runtime->gunbarrel_hole_vertex_count != 0U)')
bond = draw.index('if ((frame->layers & GE_ORIGINAL_GUNBARREL_LAYER_BOND)')
blood = draw.index('if ((frame->layers & GE_ORIGINAL_GUNBARREL_LAYER_BLOOD_IMAGE)')
def stencil_commands(region):
    return '\n'.join(re.findall(r'C3D_Stencil(?:Test|Op)\([^;]+;', region))
write = stencil_commands(draw[:hole])
read = stencil_commands(draw[hole:bond])
reset = stencil_commands(draw[bond:blood])
assert write and read and reset
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#define GPU_ALWAYS 0
#define GPU_EQUAL 1
#define GPU_STENCIL_KEEP 0
#define GPU_STENCIL_REPLACE 1
static bool enabled;
static int function, reference, input_mask, write_mask, operation;
static void C3D_StencilTest(bool e,int f,int r,int i,int w) {
    enabled=e;function=f;reference=r;input_mask=i;write_mask=w;
}
static void C3D_StencilOp(int s,int d,int p) {
    assert(s==GPU_STENCIL_KEEP && d==GPU_STENCIL_KEEP);operation=p;
}
static bool fragment(int *stencil) {
    if(!enabled) return true;
    if(function==GPU_EQUAL && (*stencil&input_mask)!=(reference&input_mask)) return false;
    if(operation==GPU_STENCIL_REPLACE) *stencil=(*stencil&~write_mask)|(reference&write_mask);
    return true;
}
static void begin_hole(void) { WRITE }
static void begin_bond(void) { READ }
static void end_bond(void) { RESET }
typedef struct {float world[3];} World;
static void camera(const World *source,float out[3]) {
    float camera_x,camera_y,camera_z;
    WALLET
    out[0]=camera_x;out[1]=camera_y;out[2]=camera_z;
}
int main(void) {
    /* Sample an opening pixel and a surrounding barrel pixel: Bond may only
     * replace the opening; the later blood/fade passes must reach both. */
    int inside=0,outside=0;
    begin_hole();assert(fragment(&inside));
    begin_bond();assert(fragment(&inside));assert(!fragment(&outside));
    assert(inside==1 && outside==0);
    end_bond();assert(fragment(&inside) && fragment(&outside));
    for(int x=-1000;x<=1000;x+=1000)
    for(int y=-1000;y<=1000;y+=500)
    for(int z=-200;z<=200;z+=200) {
        World v={{x,y,z}};float actual[3];camera(&v,actual);
        /* front.c eye=(folder.x,folder.y+190,700), target has the same XY.
         * look-at is translation only; folder translation cancels exactly. */
        float folder_x=-900,folder_y=800;
        float expected[]={folder_x+x*.25f-folder_x,
            folder_y+y*.25f-(folder_y+190),z*.25f-700};
        for(int i=0;i<3;++i) assert(fabsf(actual[i]-expected[i])<.0001f);
    }
    return 0;
}
'''.replace('WRITE', write).replace('READ', read).replace('RESET', reset).replace('WALLET', wallet)
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined',str(p/'test.c'),'-lm',
                    '-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
startup=source.split('run_application(void)',1)[1]
assert startup.index('ge_original_bond_live_initialize(') < startup.index('"Initializing solo fog"')
# Multiplayer must retain its earlier fog publication before state effects.
assert startup.index('runtime_multiplayer_state_begin_fog()') < startup.index('runtime_multiplayer_state_begin_effects()')
print('Hardware regressions: canonical wallet camera, barrel coverage, solo/MP fog startup order passed')

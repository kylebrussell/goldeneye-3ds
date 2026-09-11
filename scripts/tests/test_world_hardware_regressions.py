#!/usr/bin/env python3
"""Execute live demand-loading and sampler publication code at missing boundaries."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
main=(root/'platform/3ds/source/main.c').read_text()
body=main.split('static void update_dam_renderer_visibility(RuntimeDamPreview *preview)',1)[1]
body=body.split('static bool stream_dam_renderer_camera(',1)[0]
requests=body[body.index('    if (preview->renderer_visibility_ready && preview->preload_status == GE_DAM_PRELOAD_OK)'):]
requests=requests[:requests.rfind('}')]
material=(root/'platform/3ds/source/ge_3ds_material.c').read_text()
sampler=material.split('    texture_parameters_changed =',1)[1].split('    texenv_changed =',1)[0]
sampler='    texture_parameters_changed ='+sampler
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "ge_dam_preload_queue.h"
typedef struct {
    bool renderer_visibility_ready;
    GeDamPreloadStatus preload_status;
    struct {size_t room_count;} visibility_runtime;
    struct {bool stereo;uint8_t membership[256],preload[256];} renderer_visibility;
    GeDamPreloadQueue preload_queue;
} RuntimeDamPreview;
static void demand(RuntimeDamPreview *preview) { REQUESTS }
#define GPU_LINEAR 1
static unsigned binds;
typedef struct {int wrap_s,wrap_t,mag_filter,min_filter;unsigned maxLevel;} Tex;
static Tex gpu;
static int ge_3ds_material_wrap(int x) {return x;}
static int ge_3ds_material_filter(int x) {return x;}
static void C3D_TexSetWrap(Tex*t,int s,int v) {t->wrap_s=s;t->wrap_t=v;}
static void C3D_TexSetFilter(Tex*t,int m,int n) {t->mag_filter=m;t->min_filter=n;}
static void C3D_TexSetFilterMipmap(Tex*t,int f) {(void)t;(void)f;}
/* Model Citro3D's dirty-register publication: descriptor mutation alone
 * cannot affect the sampled GPU state; TexBind is the publication boundary. */
static void C3D_TexBind(int unit,Tex*t) {assert(unit==0);gpu=*t;++binds;}
typedef struct {int texture_bound;struct {int wrap_s,wrap_t,mag_filter,min_filter;} state;} Prepared;
static void apply(const Prepared *prepared,const Prepared *previous,Tex *texture,Tex *previous_texture) {
    bool texture_parameters_changed;
    SAMPLER
}
int main(void) {
    RuntimeDamPreview p={.renderer_visibility_ready=true,.preload_status=GE_DAM_PRELOAD_OK};
    p.visibility_runtime.room_count=137;
    uint8_t resident[]={1};
    assert(ge_dam_preload_queue_init(&p.preload_queue,137,137,resident,1)==GE_DAM_PRELOAD_OK);
    /* Mono portal-visible room 113 has no VISOP_PRELOAD. The old path never
     * queued it. Authored advance request 121 must also remain queued. */
    p.renderer_visibility.membership[1]=1;p.renderer_visibility.membership[113]=1;
    p.renderer_visibility.preload[121]=1;
    demand(&p);
    assert(ge_dam_preload_queue_room_state(&p.preload_queue,113)==GE_DAM_PRELOAD_ROOM_QUEUED);
    assert(ge_dam_preload_queue_room_state(&p.preload_queue,121)==GE_DAM_PRELOAD_ROOM_QUEUED);
    assert(p.preload_queue.pending_count==2);
    demand(&p);assert(p.preload_queue.pending_count==2);
    assert(ge_dam_preload_queue_room_state(&p.preload_queue,117)==GE_DAM_PRELOAD_ROOM_UNLOADED);
    p.renderer_visibility.stereo=true;p.renderer_visibility.membership[117]=2;
    demand(&p);assert(p.preload_queue.pending_count==3);
    Prepared previous={.texture_bound=1,.state={0,0,0,0}};
    Tex texture={0};apply(&previous,0,&texture,0);assert(binds==1);
    for(unsigned field=0;field<4;++field) {
        Prepared next=previous;
        switch(field){case 0:next.state.wrap_s=1;break;case 1:next.state.wrap_t=2;break;
            case 2:next.state.mag_filter=1;break;default:next.state.min_filter=1;break;}
        unsigned before=binds;apply(&next,&previous,&texture,&texture);
        assert(binds==before+1);
        assert(gpu.wrap_s==next.state.wrap_s && gpu.wrap_t==next.state.wrap_t);
        assert(gpu.mag_filter==next.state.mag_filter && gpu.min_filter==next.state.min_filter);
        previous=next;apply(&next,&previous,&texture,&texture);assert(binds==before+1);
    }
    return 0;
}
'''.replace('REQUESTS',requests).replace('SAMPLER',sampler)
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(root/'port/include'),str(p/'test.c'),str(root/'port/src/ge_dam_preload_queue.c'),
        '-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('World regressions: mono/stereo visible-room demand, deduplication, same-image wrap/filter GPU publication passed')

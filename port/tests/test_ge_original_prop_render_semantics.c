/* Native source CPU render closure. Models, hit records and display nodes are
 * resource-boundary fixtures; fog, RNG, model traversal, player storage and
 * hit-list retirement are real production owners. This is not a match test. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/objecthandler.h"
#include "game/propobj.h"
#include "random.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_multiplayer_fog.h"

extern ModelHitEntry *g_ModelHitFreeList;
extern u64 g_randomSeed;
extern int ge_original_prop_render_semantic_exact(void *,void *,int,
    void (*)(void *,Model *,ModelNode *,u32,u32));
static Model *seen[16];static u32 scales[16],angles[16];static unsigned samples;
static void sample(void *ctx,Model *model,ModelNode *node,u32 scale,u32 angle)
{
    assert(ctx==seen&&node&&(node->Opcode&0xff)==MODELNODE_OPCODE_GUNFIRE);
    assert(samples<16);seen[samples]=model;scales[samples]=scale;angles[samples++]=angle;
}

static void run(unsigned count,unsigned viewer)
{
    assert(ge_original_multiplayer_players_create(count,viewer,1234,5678));
    assert(ge_original_multiplayer_fog_load(LEVELID_TEMPLE,0,count,0.1f));
    Model models[3]={0};ModelFileHeader headers[3]={0};
    ModelNode nodes[5]={0};union ModelRoData data[5]={0};
    uintptr_t rw[3][8]={{0}};
    ModelHitEntry entries[3]={0},spare={0};
    ChrRecord chr={0};PropRecord prop={0};
    for(unsigned i=0;i<3;++i){
        models[i].obj=&headers[i];models[i].scale=1;
        models[i].datas=(void *)rw[i];models[i].rwdatalen=8;
        headers[i].RootNode=&nodes[i];headers[i].isLoaded=1;
        nodes[i].Data=&data[i];nodes[i].Opcode=MODELNODE_OPCODE_GUNFIRE;
        ((union ModelRwData *)(void *)rw[i])->Gunfire.visible=1;
    }
    /* Deliberately different from allocation/hand order: textured model1,
     * body subtree, untextured model0. Nested gunfire is excluded from the
     * body entry and would need its own sorted hit entry in the source. */
    data[1].Gunfire.Image=(void *)(uintptr_t)1;
    nodes[2].Opcode=MODELNODE_OPCODE_HEADER;nodes[2].Child=&nodes[3];
    nodes[3].Parent=&nodes[2];nodes[3].Data=&data[3];
    nodes[3].Opcode=MODELNODE_OPCODE_GUNFIRE;nodes[3].Next=&nodes[4];
    nodes[4].Parent=&nodes[2];nodes[4].Data=&data[4];
    nodes[4].Opcode=MODELNODE_OPCODE_DLPRIMARY;
    data[4].DisplayListPrimary.Primary=(void *)(uintptr_t)1;
    data[4].DisplayListPrimary.numVertices=2;
    entries[0].model=&models[1];entries[0].rootnode=&nodes[1];
    entries[1].model=&models[2];entries[1].rootnode=&nodes[2];
    entries[2].model=&models[0];entries[2].rootnode=&nodes[0];
    for(unsigned i=0;i<2;++i){entries[i].next=&entries[i+1];entries[i+1].prev=&entries[i];}
    prop.type=PROP_TYPE_VIEWER;prop.chr=&chr;prop.zDepth=100;
    chr.model=&models[2];chr.fadealpha=255;chr.chrflags=CHRFLAG_04000000;
    chr.field_20=entries;g_ModelHitFreeList=&spare;
    samples=0;g_randomSeed=UINT64_C(0x987654321);
    u64 expected=g_randomSeed;u32 expected_scale=randomGetNextFrom(&expected);
    u32 expected_angle=randomGetNextFrom(&expected);
    /* dorottex: three source samples per authored quad, between flashes. */
    for(unsigned i=0;i<6;++i)(void)randomGetNextFrom(&expected);
    u32 expected_last=randomGetNextFrom(&expected);
    int before=g_playerPerm->time_other_players_on_screen;
    u64 seed=g_randomSeed;
    assert(ge_original_prop_render_semantic_exact(seen,&prop,0,sample));
    assert(!samples&&g_randomSeed==seed&&chr.field_20==entries);
    assert(g_playerPerm->time_other_players_on_screen==before+1);
    assert(ge_original_prop_render_semantic_exact(seen,&prop,1,sample));
    assert(samples==2&&seen[0]==&models[1]&&seen[1]==&models[0]);
    assert(scales[0]==expected_scale&&angles[0]==expected_angle);
    assert(scales[1]==expected_last&&angles[1]==0&&g_randomSeed==expected);
    assert(g_playerPerm->time_other_players_on_screen==before+2);
    assert(chr.field_20==NULL&&g_ModelHitFreeList==entries);
    assert(entries[2].next==&spare&&spare.prev==&entries[2]);
    /* Fade zero must still retire the hit list on alpha, without RNG/stats. */
    entries[2].next=NULL;spare.prev=NULL;g_ModelHitFreeList=&spare;
    chr.field_20=entries;chr.fadealpha=0;seed=g_randomSeed;
    assert(ge_original_prop_render_semantic_exact(seen,&prop,1,sample));
    assert(g_randomSeed==seed&&chr.field_20==NULL);
    assert(g_playerPerm->time_other_players_on_screen==before+2);
    /* Dropped world objects use the original recursive child order too. */
    ObjectRecord objects[2]={0};PropRecord world[2]={0};
    for(unsigned i=0;i<2;++i){
        objects[i].model=&models[i];objects[i].prop=&world[i];
        objects[i].type=PROPDEF_COLLECTABLE;world[i].obj=&objects[i];
        world[i].type=PROP_TYPE_WEAPON;world[i].flags=PROPFLAG_ONSCREEN;
        world[i].zDepth=100;
    }
    world[0].child=&world[1];world[1].parent=&world[0];
    world[0].timetoregen=CHROBJ_TIMETOREGEN/2;
    samples=0;seed=g_randomSeed;
    assert(ge_original_prop_render_semantic_exact(seen,&world[0],0,sample));
    assert(!samples&&g_randomSeed==seed);
    expected=seed;expected_scale=randomGetNextFrom(&expected);
    expected_last=randomGetNextFrom(&expected);expected_angle=randomGetNextFrom(&expected);
    assert(ge_original_prop_render_semantic_exact(seen,&world[0],1,sample));
    assert(samples==2&&seen[0]==&models[0]&&seen[1]==&models[1]);
    assert(scales[0]==expected_scale&&angles[0]==0);
    assert(scales[1]==expected_last&&angles[1]==expected_angle&&g_randomSeed==expected);
    /* Native monitor controller runs in its opaque object pass. A zero
     * random-jump threshold consumes RNG then advances to the authored yield. */
    MonitorObjRecord monitor={0};PropRecord screen_prop={0};
    u32 commands[]={TVCMD_STOPSCROLL,TVCMD_RANDSETCMDLIST,0,0,TVCMD_YIELD};
    ModelNode *switches[]={&nodes[0]};
    nodes[0].Opcode=MODELNODE_OPCODE_DLCOLLISION;
    headers[0].Switches=switches;headers[0].numSwitches=1;
    monitor.type=PROPDEF_MONITOR;monitor.model=&models[0];monitor.prop=&screen_prop;
    monitor.Monitor.cmdlist=commands;monitor.Monitor.xmidinc=2;monitor.Monitor.ymidinc=3;
    screen_prop.type=PROP_TYPE_OBJ;screen_prop.obj=(void *)&monitor;
    screen_prop.flags=PROPFLAG_ONSCREEN;screen_prop.zDepth=100;
    expected=g_randomSeed;(void)randomGetNextFrom(&expected);
    assert(ge_original_prop_render_semantic_exact(seen,&screen_prop,0,sample));
    assert(monitor.Monitor.offset==4&&monitor.Monitor.xmidinc==0&&monitor.Monitor.ymidinc==0);
    assert(g_randomSeed==expected);
    assert(ge_original_prop_render_semantic_exact(seen,&screen_prop,1,sample));
    assert(g_randomSeed==expected&&monitor.Monitor.offset==4);
    g_ModelHitFreeList=NULL;
    ge_original_multiplayer_fog_close();
    ge_original_multiplayer_players_destroy();
}

int main(void)
{
    for(unsigned count=2;count<=4;count+=2)
        for(unsigned local=0;local<count;++local)run(count,local);
    puts("native render semantics: 2/4 player owners, sorted hit-list/node RNG, image gate, primary-node samples, alpha/fade statistics, actual list retirement, dropped child order and monitor controller; model resources are fixtures");
    return 0;
}

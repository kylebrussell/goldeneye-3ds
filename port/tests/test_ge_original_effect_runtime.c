#include "ge_original_effect_runtime.h"
#include "ge_original_effect_buffers.h"
#include "ge_original_bg_visibility.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/glass.h"
#include "game/explosion.h"
#include "game/gun.h"
#include "game/player.h"
#include "snd.h"
#include "random.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

s32 g_ClockTimer=1;f32 g_GlobalTimerDelta=1;
static struct player player;
struct player *g_CurrentPlayer=&player;
ALBank *g_musicSfxBufferPtr;
static unsigned audio_calls;
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank,s16 sound,ALSoundState *handle)
{ (void)bank;(void)sound;(void)handle;++audio_calls;return NULL; }
extern s_bullet_spark g_BulletSparkArray[20];
extern s_moving_bullet_spark g_MovingBulletSparkArray[50];
extern s_explosiontype g_ExplosionTypes[];
extern s_impacttype g_ImpactTypes[];
extern s32 D_800483C0;
extern ALSoundState *g_CasingSfxState;
PropRecord *g_OnScreenPropList[601],**g_LastOnScreenProp=g_OnScreenPropList;
s32 bossGetStageNum(void){return LEVELID_LIBRARY;}
static unsigned room_visits;
void chraiGetPropRoomIds(PropRecord *prop,s32 *rooms)
{++room_visits;rooms[0]=prop->rooms[0];rooms[1]=-1;}
void fixture_rooms(void);
void explosionInitFlyingParticles(coord3d *,f32,f32,f32,f32);

static void reset_and_update(unsigned count)
{
    coord3d pos={1,10,3};
    g_CasingSfxState=(void *)(uintptr_t)1;
    assert(ge_original_effect_runtime_reset(count,LEVELID_LIBRARY));
    assert(!g_CasingSfxState && D_800483C0==1);
    assert(ptr_shattered_window_pieces && SHATTERED_WINDOW_PIECES_BUFFER_LEN==200/(s32)count);
    glassCreateShard(&pos,0.2f,3);
    assert(g_NextShardNum==1 && ptr_shattered_window_pieces[0].active==1);
    s_bullet_spark *spark=bullet_spark_create(&pos,1,2,1);assert(spark);
    explosionInitFlyingParticles(&pos,1,2,3,1);
    assert(g_NumParticleEntries==1 && g_FlyingParticlesBuffer[0].unk00==1);
    g_MovingBulletSparkArray[0].unk00=*spark;
    g_MovingBulletSparkArray[0].velocity=(coord3d){1,2,3};
    g_MovingBulletSparkArray[0].unk38=-100;
    g_Casings[0].header=(void *)(uintptr_t)1;g_Casings[0].floor_y_pos=-100;
    g_Casings[0].pos=pos;g_Casings[0].vel=(coord3d){1,2,3};
    for(unsigned i=0;i<4;++i)g_Casings[0].rot_mtx.m[i][i]=g_Casings[0].rot_velocity_mtx.m[i][i]=1;
    f32 shard_x=ptr_shattered_window_pieces[0].pos.x, vx=ptr_shattered_window_pieces[0].velocity.x;
    assert(ge_original_effect_runtime_pre_world());
    assert(ptr_shattered_window_pieces[0].active==2 && ptr_shattered_window_pieces[0].pos.x==shard_x+vx);
    assert(spark->age==1 && g_FlyingParticlesBuffer[0].unk00==2);
    assert(g_MovingBulletSparkArray[0].unk00.unk10==pos.x+1);
    assert(g_Casings[0].pos.x==pos.x+1 && g_Casings[0].vel.y<2);
    assert(!audio_calls);
    assert(ge_original_effect_runtime_reset(count,LEVELID_DEPOT));
    assert(SHATTERED_WINDOW_PIECES_BUFFER_LEN==(200/(s32)count)/2);
    assert(!ptr_shattered_window_pieces[0].active && !g_BulletSparkArray[0].lifetime);
    assert(!g_MovingBulletSparkArray[0].unk00.lifetime && !g_Casings[0].header);
    assert(!ge_original_effect_runtime_reset(0,LEVELID_LIBRARY));
}
static unsigned sequence[32],sequence_count;
static int record(void *context,void *opaque,int alpha)
{
    PropRecord *p=opaque;(void)context;
    sequence[sequence_count++]=p->rooms[0]*10+(unsigned)alpha;
    return 1;
}
static void retirement(void)
{
    PropRecord props[4]={{0}};
    assert(ge_original_effect_runtime_reset(2,LEVELID_LIBRARY));
    fixture_rooms();
    GeOriginalBgRenderRoom rooms[3];size_t count;
    assert(ge_original_bg_visibility_render_rooms(rooms,3,&count)&&count==3);
    assert(rooms[0].depth==257 && rooms[1].depth==-2 && rooms[2].depth==257);
    for(unsigned i=0;i<4;++i){props[i].rooms[0]=(u8)(i+1);g_OnScreenPropList[i]=&props[i];}
    g_LastOnScreenProp=g_OnScreenPropList+4;
    props[0].type=PROP_TYPE_EXPLOSION;props[0].explosion=&g_ExplosionBuffer[0];
    g_ExplosionBuffer[0].prop=&props[0];g_ExplosionBuffer[0].explosion_type=1;
    s16 threshold=(s16)(g_ExplosionTypes[1].flareanimspeed*15.0f);
    g_ExplosionBuffer[0].parts[0].frame=threshold+1;
    g_ExplosionBuffer[0].parts[1].frame=threshold;
    props[1].type=PROP_TYPE_SMOKE;props[1].smoke=&g_SmokeBuffer[0];
    g_SmokeBuffer[0].prop=&props[1];g_SmokeBuffer[0].parts[0].size=-1;
    props[2].flags=PROPFLAG_RENDERPOSTBG;
    props[3].type=PROP_TYPE_EXPLOSION;props[3].explosion=&g_ExplosionBuffer[1];
    g_ExplosionBuffer[1].prop=&props[3];g_ExplosionBuffer[1].explosion_type=1;
    g_ExplosionBuffer[1].parts[0].frame=threshold+1;
    assert(ge_original_effect_runtime_render_view(NULL,record));
    unsigned expected[]={20,10,30,11,31,21};
    assert(sequence_count==6 && !memcmp(sequence,expected,sizeof(expected)));
    assert(!g_ExplosionBuffer[0].parts[0].frame && g_ExplosionBuffer[0].parts[1].frame==threshold);
    assert(g_SmokeBuffer[0].parts[0].size==0 && !signbit(g_SmokeBuffer[0].parts[0].size));
    assert(g_ExplosionBuffer[1].parts[0].frame==threshold+1); /* not rendered */
    D_800483C0=0;sequence_count=0;room_visits=0;
    assert(ge_original_effect_runtime_render_view(NULL,record));
    assert(!sequence_count && !room_visits);
    struct Scorch object_alias={0};PropRecord object={0};object.scorch=&object_alias;
    object_alias.unk02=3;
    ge_original_effect_impact_semantic_exact(&object,1);assert(object_alias.unk02==1);
    ge_original_effect_impact_semantic_exact(&object,0);assert(object_alias.unk02==0);
}
void fixture_render_rooms(int index);
void fixture_original_render(unsigned *sequence,size_t *count);
int fixture_arena_index(unsigned slot);
static void dam_prefix(void)
{
    _Static_assert(LEVEL_INDEX_DAM==13,"native Dam table index");
    PropRecord props[2]={{0}};
    unsigned reference[32];size_t reference_count,count;
    GeOriginalBgRenderRoom rooms[2];
    props[0].rooms[0]=35;props[1].rooms[0]=1;
    g_OnScreenPropList[0]=&props[0];g_OnScreenPropList[1]=&props[1];
    g_LastOnScreenProp=g_OnScreenPropList+2;
    assert(ge_original_effect_runtime_reset(2,LEVELID_DAM));
    for(unsigned test=0;test<12;++test) {
        int index=test==0?LEVEL_INDEX_DAM:fixture_arena_index(test-1);
        assert(index>=0 && (test==0 || index!=LEVEL_INDEX_DAM));
        fixture_render_rooms(index);
        fixture_original_render(reference,&reference_count);
        assert(reference_count==4);
        const unsigned expected_dam[]={350,10,11,351};
        const unsigned expected_arena[]={10,350,351,11};
        assert(!memcmp(reference,test==0?expected_dam:expected_arena,4*sizeof(unsigned)));
        fixture_render_rooms(index);sequence_count=0;
        assert(ge_original_effect_runtime_render_view(NULL,record));
        assert(sequence_count==reference_count && !memcmp(sequence,reference,4*sizeof(unsigned)));
        assert(ge_original_bg_visibility_render_rooms(rooms,2,&count) && count==2);
        assert(rooms[0].room==35 && rooms[0].depth==(test==0?0:5));
        assert(rooms[1].room==1 && rooms[1].depth==1);
        /* Depth remains changed in native state; a second view is idempotent. */
        sequence_count=0;assert(ge_original_effect_runtime_render_view(NULL,record));
        assert(!memcmp(sequence,reference,4*sizeof(unsigned)));
    }
    fixture_render_rooms(LEVEL_INDEX_DAM);D_800483C0=0;sequence_count=0;
    assert(ge_original_effect_runtime_render_view(NULL,record) && !sequence_count);
    assert(ge_original_bg_visibility_render_rooms(rooms,2,&count) && rooms[0].depth==0);
    D_800483C0=1;g_LastOnScreenProp=g_OnScreenPropList;
}
int main(void)
{
    randomSetSeed(17);reset_and_update(2);reset_and_update(4);retirement();dam_prefix();
    puts("native effects: real shard/spark/particle construction, 2/4 resets and updates, signed BG pass ordering, original Dam prefix/all11 arenas and eligible alpha/impact retirement passed");
    return 0;
}

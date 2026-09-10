#include "ge_mp_state_fog.h"
#include "ge_original_multiplayer_fog.h"
#include "ge_stage_assets.h"
#include <ultra64.h>
#include <bondconstants.h>
#include "game/bgfog.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern EnvironmentRecord fog_tables[], *g_EnvironmentFoundp, *g_EnvironmentMainp, *g_EnvironmentAltp;
extern EnvironmentFoglessRecord fog_tables2[];
extern NearFogRecord *g_NearFogValuesP;
extern CurrentEnvironmentRecord g_CurrentEnvironment;
extern f32 g_FarFogIntensity,g_DifferenceFromFarFogIntensity;
extern f32 g_ScaledFarFogIntensity,g_ScaledDifferenceFromFarFogIntensity;
struct FogDetails { f32 g_CurFogDetails,scaled_far_fog_dist;u32 unk08,unk0c;
    f32 far_fog_dist_scaled,near_fog_dist_scaled; };
extern struct FogDetails g_CurFogDetails;
static s32 players;
static float clip[2];
static unsigned writes,mutations;
static coord3d position;
static Mtxf matrix;
s32 getPlayerCount(void) { return players; }
void viSetZRange(f32 near,f32 far) { clip[0]=near;clip[1]=far;++writes; }
void viGetZRange(f32 *out) { memcpy(out,clip,sizeof(clip)); }
f32 getPlayer_c_lodscalez(void) { return 1; }
coord3d *bondviewGetCurrentPlayersPosition(void) { return &position; }
Mtxf *camGetWorldToScreenMtxf(void) { return &matrix; }

static GeMpRegistry registry;
static GeMpRegistryEntry entry;
static GeMpStateFogOwner owner;
static GeMpStateFogSnapshot snapshot;
static uint8_t baseline[32];

static void digest(uint8_t hash[32])
{
    unsigned before=writes;
    assert(ge_mp_state_fog_capture(&owner,&snapshot)==GE_MP_STATE_OK);
    assert(before==writes && snapshot.record_count==1);
    uint32_t global=1;
    GeMpFieldSchema gs={256,GE_MP_STATE_U32,0,1,1};
    GeMpStateField gf={256,GE_MP_STATE_U32,1,&global};
    GeMpRecordSchema schemas[]={{0,GE_MP_SYSTEM_SESSION,&gs,1},*ge_mp_state_fog_schema()};
    /* This test attests only its isolated declared fog/session fixture. */
    GeMpStateSchema schema={1,GE_MP_SYSTEM_SESSION|GE_MP_SYSTEM_OBJECTS,1,schemas,2};
    GeMpStateRecord records[]={{0,0,&gf,1},snapshot.record};
    GeMpCanonicalSnapshot canonical={.session=21,.tick=0,.enabled_systems=schema.registered_systems,
        .schema=&schema,.registry=&registry,.records=records,.record_count=2,
        .coverage={schema.registered_systems,0,0}};
    memset(canonical.build_sha256,1,32);memset(canonical.content_sha256,2,32);
    memset(canonical.rules_sha256,3,32);
    assert(ge_mp_state_sha256(&canonical,hash,NULL)==GE_MP_STATE_OK);
}

static void start(unsigned count)
{
    players=(s32)count;
    assert(ge_original_multiplayer_fog_load(LEVELID_DAM,1,count,0.2f));
    assert(ge_mp_registry_init(&registry,&entry,1)==GE_MP_STATE_OK);
    assert(ge_mp_state_fog_begin(&owner,&registry)==GE_MP_STATE_OK);
    assert((uint32_t)owner.actor_id==1);
    digest(baseline);
}
static void finish(void)
{
    assert(ge_mp_state_fog_end(&owner)==GE_MP_STATE_OK);
    ge_original_multiplayer_fog_close();
}
static void changed(uint16_t field,uint32_t index,const void *value,size_t size)
{
    uint8_t hash[32];digest(hash);
    assert(memcmp(hash,baseline,32)!=0);
    const GeMpStateField *f=&snapshot.fields[field-256];
    assert(f->id==field && index<f->count);
    assert(memcmp((const uint8_t *)f->values+index*size,value,size)==0);
    ++mutations;
}
#define CHANGE(type,value,field,index) do { type saved=(value); \
    (value)=saved==0?1:0; changed(field,index,&(value),sizeof(value)); (value)=saved; } while(0)
#define SKY(source,u8field,i16field,floatfield,row,step,start) do { \
    CHANGE(u8,(source).Red,u8field,(row)*5); CHANGE(u8,(source).Green,u8field,(row)*5+1); \
    CHANGE(u8,(source).Blue,u8field,(row)*5+2); CHANGE(u8,(source).Clouds,u8field,(row)*5+3); \
    CHANGE(u8,(source).IsWater,u8field,(row)*5+4); \
    CHANGE(s16,(source).SkyImageId,i16field,(row)*2); \
    CHANGE(s16,(source).WaterImageId,i16field,(row)*2+1); \
    CHANGE(f32,(source).CloudRepeat,floatfield,(row)*(step)+(start)); \
    CHANGE(f32,(source).CloudRed,floatfield,(row)*(step)+(start)+1); \
    CHANGE(f32,(source).CloudGreen,floatfield,(row)*(step)+(start)+2); \
    CHANGE(f32,(source).CloudBlue,floatfield,(row)*(step)+(start)+3); \
    CHANGE(f32,(source).WaterRepeat,floatfield,(row)*(step)+(start)+4); \
    CHANGE(f32,(source).WaterRed,floatfield,(row)*(step)+(start)+5); \
    CHANGE(f32,(source).WaterGreen,floatfield,(row)*(step)+(start)+6); \
    CHANGE(f32,(source).WaterBlue,floatfield,(row)*(step)+(start)+7); \
    CHANGE(f32,(source).WaterConcavity,floatfield,(row)*(step)+(start)+8); \
} while(0)

static void reject_malformed_owner(void)
{
    GeMpRegistry registry;
    GeMpRegistryEntry entries[2];
    GeMpStateFogOwner owner={0};
    players=2;
    assert(ge_original_multiplayer_fog_load(LEVELID_DAM,1,2,0.2f));
    assert(ge_mp_registry_init(&registry,entries,2)==GE_MP_STATE_OK);
    assert(ge_mp_state_fog_begin(&owner,&registry)==GE_MP_STATE_OK);
    const uint32_t other_key=ge_mp_registry_key(0xe7,255,0);
    uint64_t other_id;
    assert(ge_mp_registry_define(&registry,other_key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,other_key,&other_id)==GE_MP_STATE_OK);
    GeMpStateFogOwner malformed=owner;
    malformed.actor_id=other_id;
    GeMpStateFogOwner before=malformed;
    assert(ge_mp_state_fog_end(&malformed)==GE_MP_STATE_INVALID);
    assert(!memcmp(&malformed,&before,sizeof(before)));
    assert(ge_mp_registry_resolve(&registry,other_id,NULL)==GE_MP_STATE_OK);
    assert(ge_mp_registry_resolve(&registry,owner.actor_id,NULL)==GE_MP_STATE_OK);
    malformed=owner;malformed.lifetime=0;before=malformed;
    assert(ge_mp_state_fog_end(&malformed)==GE_MP_STATE_STALE);
    assert(!memcmp(&malformed,&before,sizeof(before)));
    assert(ge_mp_registry_resolve(&registry,owner.actor_id,NULL)==GE_MP_STATE_OK);
    assert(ge_mp_registry_release(&registry,other_id)==GE_MP_STATE_OK);
    ge_original_multiplayer_fog_close();
    /* Structural identity remains valid after native expiry and must retire. */
    assert(ge_mp_state_fog_end(&owner)==GE_MP_STATE_STALE);
    for (unsigned i=0;i<2;++i) assert(!entries[i].live);
}

int main(int argc,char **argv)
{
    reject_malformed_owner();
    assert(argc==2);unsigned count=(unsigned)atoi(argv[1]);assert(count==2 || count==4);
    start(count);
    assert(snapshot.fields[24].count==0);
    uint32_t rows=snapshot.values.environment_count, fogless=snapshot.values.fogless_count;
    CHANGE(s32,g_FogSkyIsEnabled,260,0);
    CHANGE(f32,clip[0],261,0);CHANGE(f32,clip[1],261,1);
    CHANGE(s32,g_CurrentEnvironment.DifferenceFromFarIntensity,262,0);
    CHANGE(s32,g_CurrentEnvironment.FarIntensity,262,1);
    SKY(g_CurrentEnvironment,263,264,265,0,9,0);
    CHANGE(f32,g_CurFogDetails.g_CurFogDetails,266,0);
    CHANGE(f32,g_CurFogDetails.scaled_far_fog_dist,266,1);
    CHANGE(f32,g_CurFogDetails.far_fog_dist_scaled,266,2);
    CHANGE(f32,g_CurFogDetails.near_fog_dist_scaled,266,3);
    CHANGE(f32,g_FarFogIntensity,267,0);CHANGE(f32,g_DifferenceFromFarFogIntensity,267,1);
    CHANGE(f32,g_ScaledFarFogIntensity,267,2);CHANGE(f32,g_ScaledDifferenceFromFarFogIntensity,267,3);
    for (uint32_t i=0;i<rows;++i) {
        EnvironmentRecord *r=&fog_tables[i];
        if(i+1<rows) CHANGE(u32,r->Id,271,i*2);
        CHANGE(u32,r->Visibility.Intensity,271,i*2+1);
        CHANGE(s32,r->Fog.DifferenceFromFarIntensity,272,i*2);
        CHANGE(s32,r->Fog.FarIntensity,272,i*2+1);
        CHANGE(f32,r->Visibility.BlendMultiplier,275,i*15);
        CHANGE(f32,r->Visibility.FarFog,275,i*15+1);
        CHANGE(f32,r->Visibility.Nfd.NearFog,275,i*15+2);
        CHANGE(f32,r->Visibility.Nfd.MaxVisRange,275,i*15+3);
        CHANGE(f32,r->Visibility.Nfd.MaxObfuscationRange,275,i*15+4);
        CHANGE(f32,r->Visibility.MinVisrange,275,i*15+5);
        SKY(r->Sky,273,274,275,i,15,6);
    }
    for (uint32_t i=0;i<fogless;++i) {
        if(i+1<fogless)CHANGE(u32,fog_tables2[i].Id,276,i);
        SKY(fog_tables2[i],277,278,279,i,9,0);
    }
    EnvironmentRecord **bindings[]={&g_EnvironmentFoundp,&g_EnvironmentMainp,&g_EnvironmentAltp};
    for (unsigned i=0;i<3;++i) {
        EnvironmentRecord *saved=*bindings[i];*bindings[i]=&fog_tables[0];
        int32_t expected=0;changed(268,i,&expected,sizeof(expected));
        *bindings[i]=(EnvironmentRecord *)(uintptr_t)1;
        assert(ge_mp_state_fog_capture(&owner,&snapshot)==GE_MP_STATE_INCOMPLETE);
        assert(snapshot.record_count==0);*bindings[i]=saved;
    }
    NearFogRecord *near=g_NearFogValuesP;
    g_NearFogValuesP=(NearFogRecord *)(uintptr_t)3;
    assert(ge_mp_state_fog_capture(&owner,&snapshot)==GE_MP_STATE_INCOMPLETE);
    g_NearFogValuesP=NULL;int32_t null_binding=-1;changed(268,3,&null_binding,4);
    g_NearFogValuesP=near;
    float saved=g_CurrentEnvironment.CloudRepeat;g_CurrentEnvironment.CloudRepeat=0.0f;
    digest(baseline);g_CurrentEnvironment.CloudRepeat=-0.0f;changed(265,0,&g_CurrentEnvironment.CloudRepeat,4);
    g_CurrentEnvironment.CloudRepeat=NAN;
    assert(ge_mp_state_fog_capture(&owner,&snapshot)==GE_MP_STATE_INCOMPLETE);
    assert(snapshot.record_count==0);g_CurrentEnvironment.CloudRepeat=saved;
    fogSwitchToSolosky2(0.5f);digest(baseline);
    assert(snapshot.values.bindings[3]==-2 && snapshot.fields[24].count==3);
    CHANGE(f32,g_NearFogValuesP->NearFog,280,0);
    CHANGE(f32,g_NearFogValuesP->MaxVisRange,280,1);
    CHANGE(f32,g_NearFogValuesP->MaxObfuscationRange,280,2);
    finish();
    /* Closed transition storage is not reachable, and the next switch copies
     * its entire source before use: prior payload must not become wire state. */
    start(count);uint8_t initial[32];memcpy(initial,baseline,32);finish();
    players=count==2?4:2;
    assert(ge_original_multiplayer_fog_load(LEVELID_CRADLE,0,(uint32_t)players,0.5f));
    fogSwitchToSolosky2(0.75f);ge_original_multiplayer_fog_close();
    start(count);assert(!memcmp(initial,baseline,32));
    GeMpStateFogOwner stale=owner;uint64_t id=owner.actor_id;
    assert(ge_mp_state_fog_end(&owner)==GE_MP_STATE_OK);ge_original_multiplayer_fog_close();
    assert(ge_original_multiplayer_fog_load(LEVELID_DAM,1,count,0.2f));
    assert(ge_mp_state_fog_begin(&owner,&registry)==GE_MP_STATE_OK);
    assert((uint32_t)owner.actor_id==(uint32_t)id+1);
    assert(ge_mp_state_fog_capture(&stale,&snapshot)==GE_MP_STATE_STALE);
    ge_original_multiplayer_fog_close();
    assert(ge_mp_state_fog_end(&owner)==GE_MP_STATE_STALE);
    assert(!entry.live);
    /* Every authored selector and in-stage reload stays in the same native
     * lifetime; inspect exact table references after each selection. */
    start(count);const uint64_t lifetime=owner.lifetime;
    uint8_t stage_hashes[(GE_STAGE_COUNT+GE_MULTIPLAYER_STAGE_COUNT)*2*32];
    unsigned stage_hash_count=0;
    for (unsigned stage=0;stage<GE_STAGE_COUNT+GE_MULTIPLAYER_STAGE_COUNT;++stage) {
        const GeStageAssetDescriptor *d=stage<GE_STAGE_COUNT
            ?ge_stage_asset_descriptor((GeStageId)stage)
            :ge_stage_asset_multiplayer(stage-GE_STAGE_COUNT);
        assert(d);
        for (int cinema=0;cinema<2;++cinema) {
            assert(ge_original_multiplayer_fog_load(d->level_id,cinema,count,d->visibility_scale));
            assert(owner.lifetime==lifetime);
            digest(stage_hashes+32*stage_hash_count++);
            assert(snapshot.values.level_id==d->level_id && snapshot.values.cinematic==cinema);
        }
    }
    uint8_t stage_digest[32];
    assert(ge_mp_sha256(stage_hashes,sizeof(stage_hashes),stage_digest)==GE_MP_STATE_OK);
    players=1;
    assert(ge_mp_state_fog_capture(&owner,&snapshot)==GE_MP_STATE_INCOMPLETE);
    players=(s32)count;finish();
    /* Native fogless load retains previous near/main/alternate bindings and
     * coefficients. The selected pointer alone becomes null. */
    start(1);fogSwitchToSolosky2(0.5f);digest(baseline);
    int32_t main_binding=snapshot.values.bindings[1],alt_binding=snapshot.values.bindings[2];
    float previous_details[4];memcpy(previous_details,snapshot.values.details,sizeof(previous_details));
    assert(ge_original_multiplayer_fog_load(LEVELID_BUNKER1,0,1,0.2f));
    digest(baseline);
    assert(snapshot.values.enabled==0 && snapshot.values.bindings[0]==-1);
    assert(snapshot.values.bindings[1]==main_binding && snapshot.values.bindings[2]==alt_binding);
    assert(snapshot.values.bindings[3]==-2 && snapshot.fields[24].count==3);
    assert(!memcmp(previous_details,snapshot.values.details,sizeof(previous_details)));
    finish();
    printf("fog count%u: %u native mutations, references, signedzero, transitions and lifecycle passed\n",count,mutations);
    for (unsigned i=0;i<32;++i)printf("%02x",initial[i]);puts("");
    printf("%u authored selections ",stage_hash_count);
    for (unsigned i=0;i<32;++i)printf("%02x",stage_digest[i]);puts("");
    return 0;
}

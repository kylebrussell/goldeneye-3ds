/* Native owners are allocated/initialized by the real multiplayer player
 * provider. Model/RW storage is the retained per-player host owner; original
 * modelInit/RW traversal, weapon change and dynAllocate bodies are extracted
 * verbatim by the runner. Reference maps/content are explicit test fixtures,
 * not a production asset/lifetime registration implementation. */
#include "ge_mp_state_guns.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_gun_frame_arena.h"
#include "ge_original_input.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/gun.h"
#include "game/model.h"
#include "game/dyn.h"

void gunRequestHandWeaponChange(GUNHAND hand,s32 weapon,s32 direction);
void sub_GAME_7F05E808(GUNHAND hand);

static GeMpRegistry registry;
static GeMpRegistryEntry entries[80];
static uint64_t ids[4][2];
static GeMpGunsState captures[4];
static Model *models[4][2];
static union ModelRwData *rws[4][2];
static GeMpGunRwView rwviews[4][2][32];
static GeMpGunModelView views[4][2];
static ModelFileHeader headers[4][2];
static ModelNode nodes[4][2];
static union ModelRoData rodata[4][2];
static unsigned char frame_memory[65536] __attribute__((aligned(16)));
static unsigned char reference_tokens[64];
static const void *reference_pointers[64];
static uint64_t reference_ids[64];
static unsigned reference_count, player_count, capture_count, mutations;
static int fail_model, fail_actor, fail_content;
static GeMpGunResolvers resolvers;
static uint8_t wire_bytes[262144], previous_bytes[262144];

static uint64_t add_actor(const void *pointer)
{
    uint32_t key=ge_mp_registry_key(0xf0,255,(uint16_t)reference_count);
    assert(reference_count<64);
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,key,&reference_ids[reference_count])==GE_MP_STATE_OK);
    reference_pointers[reference_count]=pointer;
    return reference_ids[reference_count++];
}
static GeMpStateStatus actor(void *ctx,GeMpGunRefKind kind,const void *pointer,uint64_t *id)
{
    unsigned i;(void)ctx;(void)kind;
    if(fail_actor)return GE_MP_STATE_STALE;
    for(i=0;i<reference_count;++i)if(reference_pointers[i]==pointer){*id=reference_ids[i];return GE_MP_STATE_OK;}
    return GE_MP_STATE_INCOMPLETE;
}
static GeMpStateStatus content(void *ctx,GeMpGunRefKind kind,const void *pointer,uint64_t *id)
{
    unsigned p,h,i;(void)ctx;
    if(fail_content)return GE_MP_STATE_INCOMPLETE;
    for(p=0;p<player_count;++p)for(h=0;h<2;++h)if(pointer==&headers[p][h]){
        *id=UINT64_C(0x7144000000000000)+(uint64_t)kind*256+p*2+h+1;return GE_MP_STATE_OK;
    }
    for(i=0;i<sizeof(reference_tokens);++i)if(pointer==&reference_tokens[i]){
        *id=UINT64_C(0x8234000000000000)+(uint64_t)kind*256+i+1;return GE_MP_STATE_OK;
    }
    return GE_MP_STATE_INCOMPLETE;
}
static GeMpStateStatus model_view(void *ctx,const struct player *player,uint32_t slot,uint32_t hand,GeMpGunModelView *v)
{
    (void)ctx;
    GeOriginalMultiplayerInventoryStorage storage;
    assert(ge_original_multiplayer_inventory_storage(slot,&storage));
    assert(player==storage.player);
    if(!storage.active_player)return GE_MP_STATE_INCOMPLETE;
    if(fail_model)return GE_MP_STATE_STALE;
    *v=views[slot][hand];return GE_MP_STATE_OK;
}
static void capture(void)
{
    unsigned p;
    struct player *selected=g_CurrentPlayer;
    for(p=0;p<capture_count;++p){
        GeOriginalMultiplayerInventoryStorage storage;
        assert(ge_original_multiplayer_inventory_storage(p,&storage));
        assert(ge_mp_state_guns_capture(&captures[p],storage.player,p,ids[p],&resolvers)==GE_MP_STATE_INCOMPLETE);
        assert(captures[p].record_count==2);
        assert(captures[p].coverage.unclassified_fields>=2);
    }
    assert(selected==g_CurrentPlayer);
}
/* Test-only byte diagnostics: this small schema declares only fixture-owned
 * records complete to inspect the codec. Production schema.complete remains
 * false, and production coverage is checked to prevent any native attestation. */
static GeMpStateStatus diagnostic(uint8_t *out,size_t *size,uint8_t hash[32])
{
    GeMpRecordSchema schemas[3]={{0,GE_MP_SYSTEM_SESSION,NULL,0},*ge_mp_state_guns_schema(),{0xf0,GE_MP_GUN_SYSTEMS,NULL,0}};
    GeMpStateSchema schema={GE_MP_GUN_SCHEMA_VERSION,GE_MP_GUN_SYSTEMS | GE_MP_SYSTEM_SESSION,1,schemas,3};
    GeMpStateRecord records[80]={{0,0,NULL,0}};
    GeMpCanonicalSnapshot snapshot={0};
    GeMpCoverageReport report;
    unsigned p,h,i;size_t count=1;
    GeMpStateStatus status;
    for(p=0;p<capture_count;++p)for(h=0;h<2;++h)records[count++]=captures[p].records[h];
    for(i=0;i<reference_count;++i)records[count++]=(GeMpStateRecord){reference_ids[i],0xf0,NULL,0};
    snapshot.session=17;snapshot.enabled_systems=GE_MP_GUN_SYSTEMS | GE_MP_SYSTEM_SESSION;
    memset(snapshot.build_sha256,1,32);memset(snapshot.content_sha256,2,32);memset(snapshot.rules_sha256,3,32);
    snapshot.schema=&schema;snapshot.registry=&registry;snapshot.records=records;snapshot.record_count=count;
    snapshot.coverage.enumerated_systems=GE_MP_GUN_SYSTEMS | GE_MP_SYSTEM_SESSION;
    status=ge_mp_state_encode(&snapshot,out,sizeof(wire_bytes),size,&report);
    if(status!=GE_MP_STATE_OK)return status;
    assert(ge_mp_state_sha256(&snapshot,hash,&report)==GE_MP_STATE_OK);
    schema.complete=0;
    assert(ge_mp_state_measure(&snapshot,size,&report)==GE_MP_STATE_INCOMPLETE);
    schema.complete=1;
    snapshot.coverage=captures[0].coverage;
    snapshot.coverage.enumerated_systems |= GE_MP_SYSTEM_SESSION;
    assert(ge_mp_state_measure(&snapshot,size,&report)==GE_MP_STATE_INCOMPLETE);
    return GE_MP_STATE_OK;
}
static size_t encode(uint8_t *out,uint8_t hash[32])
{
    size_t size=0;capture();assert(diagnostic(out,&size,hash)==GE_MP_STATE_OK);return size;
}
static void setup(unsigned count)
{
    unsigned p,h;reference_count=0;player_count=count;capture_count=count;
    assert(ge_mp_registry_init(&registry,entries,80)==GE_MP_STATE_OK);
    assert(ge_original_multiplayer_players_create(count,count-1,UINT64_C(0xf123456789abcd01),UINT64_C(0xa123456789abcd01)));
    assert(ge_original_gun_frame_arena_begin(frame_memory,sizeof(frame_memory)));
    for(p=0;p<count;++p){
        assert(ge_original_multiplayer_select_player(p));
        for(h=0;h<2;++h){
            struct hand *hand=&g_CurrentPlayer->hands[h];
            uint32_t key=ge_mp_registry_key(GE_MP_GUN_KIND,(uint8_t)p,(uint16_t)h);
            assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
            assert(ge_mp_registry_acquire(&registry,key,&ids[p][h])==GE_MP_STATE_OK);
            models[p][h]=ge_original_gun_host_model(h);
            rws[p][h]=(union ModelRwData *)ge_original_gun_host_model_rwdata(h);
            memset(&nodes[p][h],0,sizeof(nodes[p][h]));memset(&rodata[p][h],0,sizeof(rodata[p][h]));
            nodes[p][h].Opcode=MODELNODE_OPCODE_SWITCH;nodes[p][h].Data=&rodata[p][h];
            headers[p][h]=(ModelFileHeader){0};headers[p][h].RootNode=&nodes[p][h];headers[p][h].numMatrices=2;
            modelInit(models[p][h],&headers[p][h],(u32 *)rws[p][h]);
            assert(rws[p][h][0].Switch.visible==1); /* original modelInitRwData */
            hand->mtxlist=dynAllocate(2*sizeof(Mtxf));memset(hand->mtxlist,0,2*sizeof(Mtxf));
            hand->mtxlist[1].m[3][3]=(float)(1+p*2+h);
            models[p][h]->render_pos=(RenderPosView *)hand->mtxlist;
            assert(ge_original_gun_host_model_peek(h)==models[p][h]);
            rwviews[p][h][0]=(GeMpGunRwView){GE_MP_GUN_RW_SWITCH,0,UINT64_C(0x9344000000000001)+p*2+h,rws[p][h],NULL,sizeof(rws[p][h][0])};
            views[p][h]=(GeMpGunModelView){models[p][h],offsetof(Model,anim),(const float *)hand->mtxlist,2,2*sizeof(Mtxf),rwviews[p][h],1,UINT32_MAX ^ 1U};
            add_actor(hand->mtxlist);add_actor(rws[p][h]);
        }
    }
    for(p=0;p<16;++p)add_actor(&reference_tokens[p]);
    resolvers=(GeMpGunResolvers){NULL,&registry,actor,content,model_view};
    assert(ge_original_multiplayer_select_local_player());
}
static void teardown(void)
{
    assert(ge_original_gun_frame_arena_finalize(NULL));
    ge_original_multiplayer_players_destroy();
}
static size_t width(unsigned type)
{
    switch(type){case GE_MP_STATE_I8:case GE_MP_STATE_U8:return 1;
    case GE_MP_STATE_I16:case GE_MP_STATE_U16:return 2;
    default:return 4;}
}
static void sensitivity_sized(void *native,unsigned type,size_t count,size_t n)
{
    size_t i;uint8_t old[8],hash[32],oldhash[32];
    size_t baseline=encode(previous_bytes,oldhash);
    for(i=0;i<count;++i){
        uint8_t *ptr=(uint8_t *)native+i*n;size_t changed;
        memcpy(old,ptr,n);
        if(type==GE_MP_STATE_F32){float f;memcpy(&f,ptr,4);f=f==1.25f?-2.5f:1.25f;memcpy(ptr,&f,4);}
        else ptr[0]^=1;
        changed=encode(wire_bytes,hash);
        assert(changed!=baseline||memcmp(wire_bytes,previous_bytes,baseline));assert(memcmp(hash,oldhash,32));
        memcpy(ptr,old,n);++mutations;
    }
    assert(encode(wire_bytes,hash)==baseline&&!memcmp(wire_bytes,previous_bytes,baseline));
}
static void sensitivity(void *native,unsigned type,size_t count)
{ sensitivity_sized(native,type,count,width(type)); }
static void direct_fields(void)
{
    unsigned p,h;
    for(p=0;p<player_count;++p)for(h=0;h<2;++h){
        struct hand *hand=&ge_original_multiplayer_player(p)->hands[h];
#define NATIVE_MUTATE(name,id,type,count,path) sensitivity_sized(&hand->path,GE_MP_STATE_##type,count,sizeof(hand->path)/count);
        GE_MP_GUN_HAND_FIELDS(NATIVE_MUTATE)
#undef NATIVE_MUTATE
        sensitivity(&models[p][h]->unk00,GE_MP_STATE_I16,1);
        sensitivity(&models[p][h]->rwdatalen,GE_MP_STATE_I16,1);
        sensitivity(&models[p][h]->scale,GE_MP_STATE_F32,1);
        sensitivity(hand->mtxlist,GE_MP_STATE_F32,32);
    }
}

static void rw_fields(void)
{
    GeMpGunRwView saved=rwviews[0][0][0];
    uint32_t saved_mask=views[0][0].classified_rw_words;
    union ModelRwData *native=rws[0][0];
    union ModelRwData saved_data=*native;
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_HEADER,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<24)-1);
    sensitivity(&native->Header.unk00,GE_MP_STATE_I8,1);
    sensitivity(&native->Header.unk01,GE_MP_STATE_I8,1);
    sensitivity(&native->Header.unk02,GE_MP_STATE_I8,1);
    sensitivity(&native->Header.ground,GE_MP_STATE_F32,1);
    sensitivity(&native->Header.pos,GE_MP_STATE_F32,3);
    sensitivity(&native->Header.unk14,GE_MP_STATE_F32,1);
    sensitivity(&native->Header.unk18,GE_MP_STATE_F32,1);
    sensitivity(&native->Header.unk1c,GE_MP_STATE_F32,1);
    sensitivity(&native->Header.unk20,GE_MP_STATE_F32,1);
    sensitivity(&native->Header.unk24,GE_MP_STATE_F32,3);
    sensitivity(&native->Header.unk30,GE_MP_STATE_F32,1);
    sensitivity(&native->Header.unk34,GE_MP_STATE_F32,3);
    sensitivity(&native->Header.unk40,GE_MP_STATE_F32,3);
    sensitivity(&native->Header.unk4c,GE_MP_STATE_F32,3);
    sensitivity(&native->Header.unk58,GE_MP_STATE_F32,1);
    sensitivity(&native->Header.unk5c,GE_MP_STATE_F32,1);
    { uint8_t hash[32],oldhash[32];size_t n=encode(previous_bytes,oldhash);
      ((uint8_t *)native)[3]=0xaf; /* actual struct padding excluded */
      assert(encode(wire_bytes,hash)==n&&!memcmp(wire_bytes,previous_bytes,n)); }
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_ROOT,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<24)-1);
    sensitivity(&((struct modeldata_root *)native)->unk00,GE_MP_STATE_U16,1);
    sensitivity(&((struct modeldata_root *)native)->unk02,GE_MP_STATE_U8,1);
    sensitivity(&((struct modeldata_root *)native)->ground,GE_MP_STATE_F32,1);
    sensitivity(&((struct modeldata_root *)native)->pos,GE_MP_STATE_F32,3);
    sensitivity(&((struct modeldata_root *)native)->subroty,GE_MP_STATE_F32,1);
    sensitivity(&((struct modeldata_root *)native)->unk18,GE_MP_STATE_F32,1);
    sensitivity(&((struct modeldata_root *)native)->unk1c,GE_MP_STATE_U32,1);
    sensitivity(&((struct modeldata_root *)native)->unk20,GE_MP_STATE_F32,1);
    sensitivity(&((struct modeldata_root *)native)->unk24,GE_MP_STATE_F32,3);
    sensitivity(&((struct modeldata_root *)native)->unk30,GE_MP_STATE_F32,1);
    sensitivity(&((struct modeldata_root *)native)->unk34,GE_MP_STATE_F32,3);
    sensitivity(&((struct modeldata_root *)native)->unk40,GE_MP_STATE_F32,3);
    sensitivity(&((struct modeldata_root *)native)->unk4c,GE_MP_STATE_F32,3);
    sensitivity(&((struct modeldata_root *)native)->unk58,GE_MP_STATE_F32,1);
    sensitivity(&((struct modeldata_root *)native)->unk5c,GE_MP_STATE_F32,1);
    { uint8_t hash[32],oldhash[32];size_t n=encode(previous_bytes,oldhash);
      ((uint8_t *)native)[3]=0xaf; /* actual struct padding excluded */
      assert(encode(wire_bytes,hash)==n&&!memcmp(wire_bytes,previous_bytes,n)); }
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_DISPLAY_LIST,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<1)-1);
    sensitivity(&native->DisplayList.unk00,GE_MP_STATE_I32,1);
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_OP07,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<1)-1);
    sensitivity(&native->Op07.index,GE_MP_STATE_I32,1);
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_LOD,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<1)-1);
    sensitivity(&native->LOD.visible,GE_MP_STATE_I32,1);
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_BSP,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<1)-1);
    sensitivity(&native->BSP.visible,GE_MP_STATE_I32,1);
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_OP11,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<1)-1);
    sensitivity(&native->Op11.unk00,GE_MP_STATE_I16,1);
    sensitivity(&native->Op11.unk02,GE_MP_STATE_I16,1);
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_GUNFIRE,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<1)-1);
    sensitivity(&native->Gunfire.visible,GE_MP_STATE_I16,1);
    sensitivity(&native->Gunfire.unk02,GE_MP_STATE_U16,1);
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_SWITCH,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<1)-1);
    sensitivity(&native->Switch.visible,GE_MP_STATE_I32,1);
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_HEAD,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<2)-1);
    memset(native,0,sizeof(*native));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_DLCOLLISION,0,UINT64_C(0x9344000000000001),native,(const struct modeldata_root *)native,sizeof(*native)};
    views[0][0].classified_rw_words=UINT32_MAX ^ ((UINT32_C(1)<<2)-1);
    *native=saved_data;rwviews[0][0][0]=saved;views[0][0].classified_rw_words=saved_mask;
}

static void pointer_sensitivity(void *field)
{
    const void *old;const void *next=&reference_tokens[4];
    uint8_t hash[32],oldhash[32];size_t n=encode(previous_bytes,oldhash);
    memcpy(&old,field,sizeof(old));
    if(old==next)next=&reference_tokens[5];
    memcpy(field,&next,sizeof(next));
    assert(encode(wire_bytes,hash)!=n||memcmp(wire_bytes,previous_bytes,n));assert(memcmp(hash,oldhash,32));
    memcpy(field,&old,sizeof(old));++mutations;
}
static void references(void)
{
    struct hand *hand=&ge_original_multiplayer_player(0)->hands[0];
    Model *model=models[0][0];
    GeMpGunRwView saved=rwviews[0][0][0];
    uint32_t mask=views[0][0].classified_rw_words;
    union ModelRwData native_saved=*rws[0][0];
    pointer_sensitivity(&hand->audioHandle);
    pointer_sensitivity(&hand->rocket);
    pointer_sensitivity(&hand->mtxlist);
    pointer_sensitivity(&model->chr);
    pointer_sensitivity(&model->obj);
    pointer_sensitivity(&model->render_pos);
    pointer_sensitivity(&model->datas);
    pointer_sensitivity(&model->attachedto);
    pointer_sensitivity(&model->attachedto_objinst);
    memset(rws[0][0],0,sizeof(*rws[0][0]));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_HEAD,0,UINT64_C(0x9344000000000001),rws[0][0],NULL,sizeof(*rws[0][0])};
    views[0][0].classified_rw_words=UINT32_MAX ^ 3U;
    pointer_sensitivity(&rws[0][0]->HeadPlaceholder.ModelFileHeader);
    pointer_sensitivity(&rws[0][0]->HeadPlaceholder.RwDatas);
    memset(rws[0][0],0,sizeof(*rws[0][0]));
    rwviews[0][0][0]=(GeMpGunRwView){GE_MP_GUN_RW_DLCOLLISION,0,UINT64_C(0x9344000000000001),rws[0][0],NULL,sizeof(*rws[0][0])};
    views[0][0].classified_rw_words=UINT32_MAX ^ 3U;
    pointer_sensitivity(&rws[0][0]->DisplayListCollisions.Vertices);
    pointer_sensitivity(&rws[0][0]->DisplayListCollisions.gdl);
    *rws[0][0]=native_saved;rwviews[0][0][0]=saved;views[0][0].classified_rw_words=mask;
}

static void malformed(void)
{
    GeMpGunModelView saved=views[0][0];GeMpGunRwView w=rwviews[0][0][0];
    uint8_t hash[32],oldhash[32];size_t n;
    GeMpGunsState untouched;memset(&untouched,0xa5,sizeof(untouched));
    assert(ge_mp_state_guns_capture(&untouched,ge_original_multiplayer_player(0),4,ids[0],&resolvers)==GE_MP_STATE_INVALID);
    assert(((uint8_t *)&untouched)[0]==0xa5);
    {
        uint64_t wrong[2]={ids[0][0]+1,ids[0][1]};
        assert(ge_mp_state_guns_capture(&untouched,ge_original_multiplayer_player(0),0,wrong,&resolvers)==GE_MP_STATE_STALE);
        assert(((uint8_t *)&untouched)[0]==0xa5);
        wrong[0]=ids[1][0];
        assert(ge_mp_state_guns_capture(&untouched,ge_original_multiplayer_player(0),0,wrong,&resolvers)==GE_MP_STATE_INVALID);
    }
    capture();assert(captures[0].values[0].omissions[0]==GE_MP_GUN_OMIT_OTHER_DOMAINS);
    fail_actor=1;capture();assert(captures[0].coverage.unresolved_references);
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_STALE_REFERENCE);fail_actor=0;
    assert(ge_mp_registry_release(&registry,reference_ids[0])==GE_MP_STATE_OK);
    capture();assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_STALE_REFERENCE);
    assert(ge_mp_registry_acquire(&registry,ge_mp_registry_key(0xf0,255,0),&reference_ids[0])==GE_MP_STATE_OK);
    fail_content=1;capture();assert(captures[0].coverage.unresolved_references);fail_content=0;
    fail_model=1;capture();assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_MODEL_VIEW);fail_model=0;
    views[0][0].model_prefix_bytes=offsetof(Model,anim)-1;capture();
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_MODEL_VIEW);views[0][0]=saved;
    views[0][0].matrix_count=65;views[0][0].matrices=(const float *)(uintptr_t)1;capture();
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_MATRIX_VIEW);views[0][0]=saved;
    views[0][0].matrix_readable_bytes=16*sizeof(float)-1;
    views[0][0].matrices=(const float *)(uintptr_t)1;capture();
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_MATRIX_VIEW);views[0][0]=saved;
    views[0][0].rw_count=33;views[0][0].rw=(const GeMpGunRwView *)(uintptr_t)1;capture();
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_RW_LAYOUT);views[0][0]=saved;
    rwviews[0][0][0].tag=0xffff;rwviews[0][0][0].data=(const union ModelRwData *)(uintptr_t)1;capture();
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_RW_TYPE);rwviews[0][0][0]=w;
    rwviews[0][0][0].readable_bytes=0;rwviews[0][0][0].data=(const union ModelRwData *)(uintptr_t)1;capture();
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_RW_LAYOUT);rwviews[0][0][0]=w;
    rwviews[0][0][0].word_index=32;rwviews[0][0][0].data=(const union ModelRwData *)(uintptr_t)1;capture();
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_RW_LAYOUT);rwviews[0][0][0]=w;
    rwviews[0][0][0].tag=GE_MP_GUN_RW_HEADER;rwviews[0][0][0].word_index=9;
    rwviews[0][0][0].data=(const union ModelRwData *)(uintptr_t)1;capture();
    assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_RW_LAYOUT);rwviews[0][0][0]=w;
    rwviews[0][0][1]=w;rwviews[0][0][1].data=(const union ModelRwData *)(uintptr_t)1;
    views[0][0].rw_count=2;capture();assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_RW_LAYOUT);views[0][0]=saved;
    views[0][0].classified_rw_words=0;capture();assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_RW_LAYOUT);views[0][0]=saved;
    rwviews[0][0][0].node_content_id=0;capture();assert(captures[0].values[0].omissions[0]&GE_MP_GUN_OMIT_CONTENT_ID);rwviews[0][0][0]=w;
    /* Finite binary32 exact bits, then reject a non-finite native field. */
    {
        float *field=&ge_original_multiplayer_player(0)->hands[0].noise;
        float old=*field;uint32_t bits=UINT32_C(0x80000000);
        *field=0;n=encode(previous_bytes,oldhash);memcpy(field,&bits,4);
        assert(encode(wire_bytes,hash)==n&&memcmp(wire_bytes,previous_bytes,n));
        bits=UINT32_C(0x7fc00001);memcpy(field,&bits,4);capture();memset(wire_bytes,0x77,sizeof(wire_bytes));
        assert(diagnostic(wire_bytes,&n,hash)==GE_MP_STATE_INVALID&&wire_bytes[0]==0x77);*field=old;
    }
    /* The full Model animation tail is outside the embedded prefix. */
    n=encode(previous_bytes,oldhash);models[0][0]->anim=(ModelAnimation *)(uintptr_t)1;
    assert(encode(wire_bytes,hash)==n&&!memcmp(wire_bytes,previous_bytes,n));models[0][0]->anim=NULL;
}

static void max_matrices_and_read_only(void)
{
    struct hand *hand=&ge_original_multiplayer_player(0)->hands[0];
    Mtxf *old=hand->mtxlist;
    GeMpGunModelView saved=views[0][0];
    struct player *player=ge_original_multiplayer_player(0);
    struct player *before=malloc(sizeof(*before));
    Model model_before;
    union ModelRwData rw_before;
    uint8_t hash[32],oldhash[32];size_t n;
    assert(before);hand->mtxlist=dynAllocate(GE_MP_GUN_MAX_MATRICES*sizeof(Mtxf));
    memset(hand->mtxlist,0,GE_MP_GUN_MAX_MATRICES*sizeof(Mtxf));
    add_actor(hand->mtxlist);models[0][0]->render_pos=(RenderPosView *)hand->mtxlist;
    views[0][0].matrices=(const float *)hand->mtxlist;views[0][0].matrix_count=GE_MP_GUN_MAX_MATRICES;
    views[0][0].matrix_readable_bytes=GE_MP_GUN_MAX_MATRICES*sizeof(Mtxf);
    sensitivity(hand->mtxlist,GE_MP_STATE_F32,GE_MP_GUN_MAX_MATRICES*16);
    memcpy(before,player,sizeof(*before));model_before=*models[0][0];rw_before=*rws[0][0];
    capture();assert(!memcmp(before,player,sizeof(*before)));
    assert(!memcmp(&model_before,models[0][0],sizeof(model_before))&&!memcmp(&rw_before,rws[0][0],sizeof(rw_before)));
    free(before);
    assert(captures[0].values[0].model_matrices[0]==captures[0].values[0].hand_matrices[0]);
    n=encode(previous_bytes,oldhash);views[0][0].matrix_count=1;
    assert(encode(wire_bytes,hash)!=n&&memcmp(hash,oldhash,32));
    views[0][0]=saved;hand->mtxlist=old;models[0][0]->render_pos=(RenderPosView *)old;
}

static void real_changes(void)
{
    uint8_t hash[32],oldhash[32];size_t n;
    struct hand *hand;struct hand saved;
    assert(ge_original_multiplayer_select_player(0));hand=&g_CurrentPlayer->hands[0];saved=*hand;
    n=encode(previous_bytes,oldhash);draw_item_in_hand(GUNRIGHT,ITEM_AK47);
    assert(hand->weapon_current_animation==14&&hand->weapon_next_weapon==ITEM_AK47);
    assert(encode(wire_bytes,hash)==n&&memcmp(wire_bytes,previous_bytes,n));
    hand->weapon_action_state=GUN_ANIM_STATE_SWITCH_LOWER;hand->field_890=19;
    n=encode(previous_bytes,oldhash);gunRequestHandWeaponChange(GUNRIGHT,ITEM_WPPK,-1);
    assert(hand->field_8B0==32&&hand->weapon_animation_trigger==1&&hand->field_8B8==-1);
    assert(encode(wire_bytes,hash)==n&&memcmp(wire_bytes,previous_bytes,n));
    hand->field_A8C=0;n=encode(previous_bytes,oldhash);sub_GAME_7F05E808(GUNRIGHT);
    assert(encode(wire_bytes,hash)==n&&memcmp(wire_bytes,previous_bytes,n));
    *hand=saved;
    rws[0][0][0].Switch.visible=0;n=encode(previous_bytes,oldhash);
    modelInit(models[0][0],&headers[0][0],(u32 *)rws[0][0]);
    assert(rws[0][0][0].Switch.visible==1);
    assert(encode(wire_bytes,hash)==n&&memcmp(wire_bytes,previous_bytes,n));
}
static void ordered_and_lifetime(void)
{
    uint8_t hash[32],oldhash[32];size_t n;GeMpGunModelView saved=views[0][0];
    GeMpGunRwView old=rwviews[0][0][0],swap;
    views[0][0].rw_count=2;views[0][0].classified_rw_words=UINT32_MAX ^ 3U;
    rwviews[0][0][1]=old;rwviews[0][0][1].word_index=1;rwviews[0][0][1].node_content_id+=100;
    rwviews[0][0][1].data=&rws[0][0][1];rws[0][0][1].Switch.visible=7;
    n=encode(previous_bytes,oldhash);swap=rwviews[0][0][0];rwviews[0][0][0]=rwviews[0][0][1];rwviews[0][0][1]=swap;
    assert(encode(wire_bytes,hash)==n&&memcmp(wire_bytes,previous_bytes,n));
    views[0][0]=saved;rwviews[0][0][0]=old;
    n=encode(previous_bytes,oldhash);
    assert(ge_mp_registry_release(&registry,ids[0][0])==GE_MP_STATE_OK);
    assert(ge_mp_state_guns_capture(&captures[0],ge_original_multiplayer_player(0),0,ids[0],&resolvers)==GE_MP_STATE_STALE);
    assert(ge_mp_registry_acquire(&registry,ge_mp_registry_key(GE_MP_GUN_KIND,0,0),&ids[0][0])==GE_MP_STATE_OK);
    assert(encode(wire_bytes,hash)==n&&memcmp(wire_bytes,previous_bytes,n));
}
static void inactive_rows(void)
{
    unsigned p,h;uint8_t hash[32],oldhash[32];size_t n;
    setup(2);capture_count=4;
    for(p=2;p<4;++p)for(h=0;h<2;++h){
        uint32_t key=ge_mp_registry_key(GE_MP_GUN_KIND,(uint8_t)p,(uint16_t)h);
        assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
        assert(ge_mp_registry_acquire(&registry,key,&ids[p][h])==GE_MP_STATE_OK);
    }
    for(p=2;p<4;++p){
        GeOriginalMultiplayerInventoryStorage storage;
        assert(ge_original_multiplayer_inventory_storage(p,&storage)&&!storage.active_player);
        for(h=0;h<2;++h){
            sensitivity(&((struct player *)storage.player)->hands[h].weapon_ammo_in_magazine,GE_MP_STATE_I32,1);
            assert(captures[p].values[h].omissions[0]&GE_MP_GUN_OMIT_MODEL_VIEW);
        }
    }
    n=encode(previous_bytes,oldhash);ge_original_multiplayer_player(0)->bondhealth=0.75f;
    assert(encode(wire_bytes,hash)==n&&!memcmp(wire_bytes,previous_bytes,n)); /* E6 owns this field */
    teardown();
}

static void transcript(unsigned count,const char *path)
{
    FILE *out=fopen(path,"wb");unsigned tick,p,h;assert(out);setup(count);
    for(tick=0;tick<32;++tick){
        uint8_t hash[32],size_bytes[4];size_t n;
        for(p=0;p<count;++p){
            assert(ge_original_multiplayer_select_player(p));
            for(h=0;h<2;++h){
                if(tick%3==0)draw_item_in_hand((GUNHAND)h,ITEM_WPPK+(s32)((tick+p+h)%5));
                else gunRequestHandWeaponChange((GUNHAND)h,ITEM_WPPK+(s32)((tick+p+h)%5),(tick&1)?1:-1);
                if(tick%4==0)sub_GAME_7F05E808((GUNHAND)h);
            }
        }
        n=encode(wire_bytes,hash);size_bytes[0]=(uint8_t)(n>>24);size_bytes[1]=(uint8_t)(n>>16);size_bytes[2]=(uint8_t)(n>>8);size_bytes[3]=(uint8_t)n;
        assert(fwrite(size_bytes,1,4,out)==4&&fwrite(wire_bytes,1,n,out)==n&&fwrite(hash,1,32,out)==32);
    }
    assert(fclose(out)==0);teardown();
}
int main(int argc,char **argv)
{
    ge_original_input_init();
    if(argc==3){transcript((unsigned)atoi(argv[1]),argv[2]);return 0;}
    assert(argc==1);setup(4);direct_fields();rw_fields();references();malformed();max_matrices_and_read_only();real_changes();ordered_and_lifetime();teardown();inactive_rows();
    printf("gun state: %u native-owner element mutations; original player/model initialization, weapon draw/change/recoil, ordered RW, invalid spans and stale IDs passed; native coverage INCOMPLETE\n",mutations);
    return 0;
}

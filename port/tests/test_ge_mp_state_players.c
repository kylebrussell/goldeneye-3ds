#include "ge_mp_state_players.h"
#include "ge_mp_state_inventory.h"
#include "ge_original_multiplayer_players.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/bondview.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GeMpRegistry registry;
static GeMpRegistryEntry registry_storage[1024];
static GeMpStatePlayersOwner owner;
static GeMpStateInventoryOwner inventory;
static GeMpStatePlayersSnapshot out;
static Model models[4];static StandTile tiles[4];static PropRecord props[4];
static ModelNode nodes[4];static ModelSkeleton skeletons[4];static ModelNode *switches[4][2];
static ModelFileTextures textures[4];static Mtx fixed[4];static Mtxf matrices[4];
static unsigned char images[4][128];static unsigned char buffers[4][2][512];
static uint64_t hand_ids[4][2],buffer_ids[4][2];
static struct Ref {unsigned player;GeMpPlayerReferenceKind kind;const void *pointer;size_t extent;uint64_t id;} refs[256];
static unsigned ref_count,mutation_count;
static uint64_t acquire(unsigned kind,unsigned ns,unsigned slot)
{
    uint32_t key=ge_mp_registry_key((uint8_t)kind,(uint8_t)ns,(uint16_t)slot);uint64_t id;
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,key,&id)==GE_MP_STATE_OK);return id;
}
static void catalog(unsigned slot,GeMpPlayerReferenceKind kind,const void *pointer,size_t extent)
{
    if(!pointer || kind==GE_MP_PLAYER_REF_HAND || kind==GE_MP_PLAYER_REF_INVENTORY_OWNER)return;
    for(unsigned hand=0;hand<2;++hand)if((uintptr_t)pointer>=(uintptr_t)&buffers[slot][hand][0]
        && (uintptr_t)pointer<=(uintptr_t)&buffers[slot][hand][512])return;
    for(unsigned i=0;i<ref_count;++i)if(refs[i].kind==kind && refs[i].pointer==pointer)return;
    assert(ref_count<256);
    unsigned code=kind==GE_MP_PLAYER_REF_PROP?0xe3:0x20;
    refs[ref_count]=(struct Ref){slot,kind,pointer,extent,acquire(code,code==0xe3?255:slot,code==0xe3?slot:ref_count)};++ref_count;
}
static GeMpStateStatus reference(void *context,uint32_t slot,uint16_t field,GeMpPlayerReferenceKind kind,
    const void *pointer,size_t extent,GeMpPlayerReference *result)
{
    (void)context;(void)field;
    GeOriginalMultiplayerInventoryStorage storage;assert(ge_original_multiplayer_inventory_storage(slot,&storage));
    if(kind==GE_MP_PLAYER_REF_INVENTORY_OWNER && pointer==storage.player) {
        uint32_t key=ge_mp_registry_key(0xe2,(uint8_t)slot,0);
        for(size_t i=0;i<registry.count;++i)if(registry.entries[i].key==key) {
            result->actor_id=((uint64_t)key<<32)|registry.entries[i].generation;result->offset=0;return GE_MP_STATE_OK;
        }
    }
    if(kind==GE_MP_PLAYER_REF_HAND)for(unsigned hand=0;hand<2;++hand)if(pointer==&storage.player->hands[hand]) {
        assert(extent==sizeof(struct hand));result->actor_id=hand_ids[slot][hand];result->offset=0;return GE_MP_STATE_OK;
    }
    if(kind==GE_MP_PLAYER_REF_HAND_BUFFER || kind==GE_MP_PLAYER_REF_TEXTURE_POOL)for(unsigned hand=0;hand<2;++hand) {
        uintptr_t first=(uintptr_t)buffers[slot][hand],address=(uintptr_t)pointer;
        if(address>=first && address-first<=512){result->actor_id=buffer_ids[slot][hand];result->offset=address-first;return GE_MP_STATE_OK;}
    }
    for(unsigned i=0;i<ref_count;++i)if(refs[i].kind==kind && refs[i].pointer==pointer && refs[i].extent==extent) {
        result->actor_id=refs[i].id;result->offset=0;return GE_MP_STATE_OK;
    }
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpStatePlayersProviders providers={NULL,reference};
static void setup(unsigned count,unsigned peer)
{
    assert(ge_original_multiplayer_players_create(count,peer%count,UINT64_C(0x123456789abcdef),UINT64_C(0x987654321fedcba)));
    assert(ge_mp_registry_init(&registry,registry_storage,1024)==GE_MP_STATE_OK);
    assert(ge_mp_state_inventory_begin(&inventory,&registry)==GE_MP_STATE_OK);
    for(unsigned slot=0;slot<4;++slot) {
        GeOriginalMultiplayerInventoryStorage storage;assert(ge_original_multiplayer_inventory_storage(slot,&storage));
        struct player *p=(struct player*)storage.player;
        for(unsigned hand=0;hand<2;++hand){hand_ids[slot][hand]=acquire(0xe4,slot,hand);buffer_ids[slot][hand]=acquire(0x21,slot,hand);}
        /* Native fields belong to independently allocated actual player rows.
         * External owners below provide identities only, never fake payloads. */
        if(slot<count) {
            p->bodyModel=&models[slot];p->prop=&props[slot];p->cameratile=&tiles[slot];p->field_2A70=&tiles[slot];
            p->autoaim_target_x=&props[slot];p->autoaim_target_y=&props[slot];
            p->previous_collision_info.current_tile_ptr=&tiles[slot];p->field_488.current_tile_ptr_for_portals=&tiles[slot];
            p->field_10C4=&fixed[slot];p->field_10C8=&fixed[slot];p->projmatrix=&fixed[slot];
            p->field_10CC=&matrices[slot];p->viewtoworldmtxf=&matrices[slot];p->projmatrixf=&matrices[slot];
            p->field_10E8=&matrices[slot];p->field_10EC=&matrices[slot];
            p->bloodImgCur=images[slot];p->bloodImgNxt=images[slot]+1;p->bloodImgBufPtrArray[0]=images[slot];p->bloodImgBufPtrArray[1]=images[slot]+1;
            for(unsigned hand=0;hand<2;++hand) {
                p->ptr_hand_weapon_buffer[hand]=(ModelFileHeader*)buffers[slot][hand];
                p->copy_of_body_obj_header[hand].RootNode=&nodes[slot];p->copy_of_body_obj_header[hand].Skeleton=&skeletons[slot];
                p->copy_of_body_obj_header[hand].Switches=switches[slot];p->copy_of_body_obj_header[hand].Textures=&textures[slot];
                p->item_related[hand].start=buffers[slot][hand];p->item_related[hand].leftpos=buffers[slot][hand]+16;
                p->item_related[hand].rightpos=(struct tex*)(buffers[slot][hand]+256);p->item_related[hand].end=(struct tex*)(buffers[slot][hand]+512);
            }
        }
#define P_COPY(type,index,expression) ((void)0)
#define P_REF(field,index,kind,pointer,extent) catalog(slot,GE_MP_PLAYER_REF_##kind,pointer,extent)
#include "../src/ge_mp_state_players_copy.inc"
#undef P_REF
#undef P_COPY
    }
    assert(ge_mp_state_players_begin(&owner,&registry)==GE_MP_STATE_OK);
    assert(ge_mp_state_players_begin(&owner,&registry)==GE_MP_STATE_STALE);
}
static void export_ok(void)
{
    const struct player *current=g_CurrentPlayer;
    GeMpStateStatus status=ge_mp_state_players_export(&owner,&providers,&out);
    if(status!=GE_MP_STATE_OK)for(size_t i=0;i<out.gap_count;++i)fprintf(stderr,"gap p%u %s kind%u reason%u\n",out.gaps[i].player,ge_mp_state_players_field_path(out.gaps[i].field),out.gaps[i].reference_kind,out.gaps[i].reason);
    assert(status==GE_MP_STATE_OK && out.record_count==4 && out.total_gaps==0 && g_CurrentPlayer==current);
    assert(out.coverage.enumerated_systems==GE_MP_SYSTEM_PLAYERS);
    const GeMpRecordSchema *schema=ge_mp_state_players_schema();
    for(unsigned slot=0;slot<4;++slot) {
        if(slot)assert(out.records[slot-1].actor_id<out.records[slot].actor_id);
        for(unsigned i=0;i<GE_MP_PLAYER_FIELD_COUNT;++i)assert(out.fields[slot][i].id==schema->fields[i].id && out.fields[slot][i].type==schema->fields[i].type && out.fields[slot][i].count==schema->fields[i].max_count);
    }
}
static size_t payload(unsigned char *bytes,size_t capacity)
{
    static const unsigned width[]={0,1,2,4,8,1,2,4,8,4,1,8,4};size_t used=0;
    export_ok();
    for(unsigned slot=0;slot<4;++slot)for(unsigned i=0;i<GE_MP_PLAYER_FIELD_COUNT;++i) {
        const GeMpStateField *f=&out.fields[slot][i];size_t count=f->count*width[f->type];
        assert(used+7+count<=capacity);memcpy(bytes+used,&f->id,2);used+=2;bytes[used++]=f->type;
        memcpy(bytes+used,&f->count,4);used+=4;memcpy(bytes+used,f->values,count);used+=count;
    }
    return used;
}
static void fingerprint(uint8_t digest[32])
{static unsigned char bytes[100000];size_t used=payload(bytes,sizeof(bytes));assert(ge_mp_sha256(bytes,used,digest)==GE_MP_STATE_OK);}
static void teardown(void)
{
    assert(ge_mp_state_players_end(&owner)==GE_MP_STATE_OK);
    assert(ge_mp_state_inventory_end(&inventory)==GE_MP_STATE_OK);ge_original_multiplayer_players_destroy();ref_count=0;
}
static __attribute__((noinline)) void mutate_scalar(void *pointer,unsigned type,const char *path)
{
    uint8_t before[32],after[32];fingerprint(before);
#define M(type_name) do { type_name saved,value;memcpy(&saved,pointer,sizeof(saved));value=saved==0?1:0;memcpy(pointer,&value,sizeof(value));fingerprint(after);if(!memcmp(before,after,32))fprintf(stderr,"missing mutation %s\n",path);assert(memcmp(before,after,32));memcpy(pointer,&saved,sizeof(saved)); } while(0)
    switch(type) {
    case 1:M(int8_t);break;case 2:M(uint8_t);break;case 3:M(int16_t);break;case 4:M(uint16_t);break;
    case 5:M(int32_t);break;case 6:M(uint32_t);break;case 7:M(float);break;default:assert(0);
    }
#undef M
    ++mutation_count;
}
static void mutations(void)
{
    for(unsigned slot=0;slot<4;++slot) {
        GeOriginalMultiplayerInventoryStorage storage;assert(ge_original_multiplayer_inventory_storage(slot,&storage));
        struct player *p=(struct player*)storage.player;uint8_t before[32],after[32];
#define P_MUTATE(expr) mutate_scalar(&(expr),_Generic((expr),signed char:1,unsigned char:2,short:3,unsigned short:4,int:5,unsigned int:6,float:7),#expr)
#include "ge_mp_state_players_mutations.inc"
#undef P_MUTATE
        p->hand_item[0]=ITEM_NOTHING;p->field_2A44[1]=ITEM_NOTHING;export_ok();
        p->pos.x=-0.0f;export_ok();uint32_t bits;memcpy(&bits,&out.values[slot].f32[0],4);assert(bits==UINT32_C(0x80000000));
        p->pos.x=0.0f;
    }
}
static void reference_mutations(void)
{
    struct player *p=ge_original_multiplayer_player(0);uint8_t before[32],after[32];
    PropRecord *target=p->autoaim_target_x;fingerprint(before);p->autoaim_target_x=&props[1];fingerprint(after);assert(memcmp(before,after,32));p->autoaim_target_x=target;
    Model *model=p->bodyModel;fingerprint(before);p->bodyModel=&models[1];fingerprint(after);assert(memcmp(before,after,32));p->bodyModel=model;
    ModelNode *node=p->copy_of_body_obj_header[0].RootNode;fingerprint(before);p->copy_of_body_obj_header[0].RootNode=&nodes[1];fingerprint(after);assert(memcmp(before,after,32));p->copy_of_body_obj_header[0].RootNode=node;
    fingerprint(before);++p->item_related[0].leftpos;fingerprint(after);assert(memcmp(before,after,32));--p->item_related[0].leftpos;
    /* E6 references the model/command owners; their payloads belong to those
     * separate domains. Never mistake these alias words for E6 integers. */
    int32_t word=p->field_5A0;fingerprint(before);p->field_5A0=0x12345678;fingerprint(after);assert(!memcmp(before,after,32));p->field_5A0=word;
}
static void failures(void)
{
    assert(ge_mp_state_players_export(&owner,NULL,&out)==GE_MP_STATE_INCOMPLETE && out.total_gaps && out.record_count==4);
    assert(ge_mp_state_players_field_path(out.gaps[0].field));
    struct player *p=ge_original_multiplayer_player(0);Model *saved=p->bodyModel;p->bodyModel=(void*)(uintptr_t)1;
    assert(ge_mp_state_players_export(&owner,&providers,&out)==GE_MP_STATE_INCOMPLETE && out.coverage.unresolved_references);p->bodyModel=saved;
    p->field_10D0=1;assert(ge_mp_state_players_export(&owner,&providers,&out)==GE_MP_STATE_INCOMPLETE && out.coverage.unclassified_fields);p->field_10D0=0;
    p->field_5C=1;assert(ge_mp_state_players_export(&owner,&providers,&out)==GE_MP_STATE_INCOMPLETE && out.coverage.unresolved_references);p->field_5C=0;
    p->pos.y=NAN;assert(ge_mp_state_players_export(&owner,&providers,&out)==GE_MP_STATE_INCOMPLETE && out.coverage.unclassified_fields);p->pos.y=0;
    uint64_t old=hand_ids[0][0];assert(ge_mp_registry_release(&registry,old)==GE_MP_STATE_OK);
    assert(ge_mp_state_players_export(&owner,&providers,&out)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_registry_acquire(&registry,ge_mp_registry_key(0xe4,0,0),&hand_ids[0][0])==GE_MP_STATE_OK);
    export_ok();
    GeMpStateSchema schema={.version=1,.registered_systems=GE_MP_SYSTEM_PLAYERS|GE_MP_SYSTEM_SESSION,.complete=0};
    GeMpRecordSchema shapes[2]={{0,GE_MP_SYSTEM_SESSION,NULL,0},*ge_mp_state_players_schema()};
    GeMpCoverageReport report;
    GeMpCanonicalSnapshot snapshot={.session=1,.enabled_systems=GE_MP_SYSTEM_PLAYERS|GE_MP_SYSTEM_SESSION,.schema=&schema,.registry=&registry,.records=out.records,.record_count=4,.coverage=out.coverage};
    schema.records=shapes;schema.record_count=2;
    memset(snapshot.build_sha256,1,32);memset(snapshot.content_sha256,2,32);memset(snapshot.rules_sha256,3,32);
    uint8_t digest[32],before[32];memset(digest,0x5a,32);memcpy(before,digest,32);
    assert(ge_mp_state_sha256(&snapshot,digest,&report)==GE_MP_STATE_INCOMPLETE && !memcmp(before,digest,32));
}
static void lifecycle(unsigned count)
{
    uint64_t ids[4];memcpy(ids,owner.ids,sizeof(ids));
    assert(ge_mp_state_players_end(&owner)==GE_MP_STATE_OK);
    assert(ge_mp_state_inventory_end(&inventory)==GE_MP_STATE_OK);ge_original_multiplayer_players_destroy();
    assert(ge_original_multiplayer_players_create(count,0,1,2));
    assert(ge_mp_state_players_begin(&owner,&registry)==GE_MP_STATE_OK);
    for(unsigned i=0;i<4;++i)assert(owner.ids[i]==ids[i]+1);
    ge_original_multiplayer_players_destroy();
    assert(ge_mp_state_players_export(&owner,&providers,&out)==GE_MP_STATE_STALE);
    assert(ge_original_multiplayer_players_create(count,0,1,2));
    /* Even if an allocator reuses every base, an old lifetime remains stale.
     * Rebind the test's cached bases to model that ABA independently of ASan's
     * deliberate freed-allocation quarantine. No native storage is altered. */
    for(unsigned i=0;i<4;++i){GeOriginalMultiplayerInventoryStorage s;assert(ge_original_multiplayer_inventory_storage(i,&s));owner.players[i]=s.player;}
    assert(ge_mp_state_players_export(&owner,&providers,&out)==GE_MP_STATE_STALE);
    assert(ge_mp_state_players_end(&owner)==GE_MP_STATE_STALE);ge_original_multiplayer_players_destroy();ref_count=0;
}
int main(int argc,char **argv)
{
    assert(argc==4);unsigned count=(unsigned)atoi(argv[1]),peer=(unsigned)atoi(argv[2]);assert(count==2 || count==4);
    setup(count,peer);
    static unsigned char bytes[100000];size_t size=payload(bytes,sizeof(bytes));FILE *file=fopen(argv[3],"wb");assert(file);assert(fwrite(bytes,1,size,file)==size);assert(!fclose(file));
    uint64_t id=0;assert(ge_mp_state_players_actor(&owner,owner.players[3],&id)==GE_MP_STATE_OK && id==owner.ids[3]);
    if(peer==0){mutations();reference_mutations();failures();lifecycle(count);}else teardown();
    printf("E6 native %u players peer%u: four physical rows, %u typed mutations, reference/lifecycle guards; aggregate incomplete\n",count,peer,mutation_count);
    return 0;
}

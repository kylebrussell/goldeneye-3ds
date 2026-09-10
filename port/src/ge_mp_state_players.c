#include "ge_mp_state_players.h"
#include "ge_original_multiplayer_players.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include <math.h>
#include <string.h>

_Static_assert(sizeof(ModelFileHeader)==(sizeof(void*)==4?32:56),"review copied header coverage");
_Static_assert(sizeof(struct texpool)==4*sizeof(void*),"review texture pool coverage");
_Static_assert(GE_MP_PLAYER_FIELD_COUNT<=GE_MP_STATE_MAX_FIELDS,"player schema capacity");
_Static_assert(sizeof(((struct player*)0)->hands)/sizeof(struct hand)==2,"review hand ownership");
_Static_assert(sizeof(((struct player*)0)->copy_of_body_obj_header)/sizeof(ModelFileHeader)==2,"review copied model metadata");
#define SHAPE(id,type,count,path,storage,index,reference) {id,GE_MP_STATE_##type,GE_MP_STATE_##type==GE_MP_STATE_ACTOR_REF?GE_MP_FIELD_NULLABLE:0,count,count},
static const GeMpFieldSchema fields[]={GE_MP_PLAYER_FIELDS(SHAPE)};
#undef SHAPE
#define LAYOUT(id,type,count,path,storage,index,reference) offsetof(GeMpPlayerValues,storage)+(index)*sizeof(((GeMpPlayerValues*)0)->storage[0]),
static const size_t value_offsets[]={GE_MP_PLAYER_FIELDS(LAYOUT)};
#undef LAYOUT
static const GeMpRecordSchema schema={GE_MP_PLAYERS_KIND,GE_MP_SYSTEM_PLAYERS,fields,GE_MP_PLAYER_FIELD_COUNT};
const GeMpRecordSchema *ge_mp_state_players_schema(void){return &schema;}
const char *ge_mp_state_players_field_path(uint16_t field)
{
#define PATH(id,type,count,path,storage,index,reference) case id:return path;
    switch(field){GE_MP_PLAYER_FIELDS(PATH) default:return NULL;}
#undef PATH
}
static uint32_t key(unsigned slot){return ge_mp_registry_key(GE_MP_PLAYERS_KIND,(uint8_t)slot,0);}
static const GeMpRegistryEntry *entry(const GeMpRegistry *registry,uint32_t value)
{for(size_t i=0;i<registry->count;++i)if(registry->entries[i].key==value)return &registry->entries[i];return NULL;}
static GeMpStateStatus native_valid(const GeMpStatePlayersOwner *owner)
{
    GeOriginalMultiplayerInventoryStorage storage;
    if(!owner || !owner->live || !owner->registry)return GE_MP_STATE_STALE;
    if(ge_original_multiplayer_player_count()!=owner->active_count)return GE_MP_STATE_STALE;
    for(unsigned i=0;i<4;++i) {
        if(!ge_original_multiplayer_inventory_storage(i,&storage) || storage.player!=owner->players[i]
                || storage.lifetime!=owner->lifetime || storage.capacity!=30
                || storage.active_player!=(i<owner->active_count)
                || g_playerPointers[i]!=(storage.active_player?storage.player:NULL))return GE_MP_STATE_STALE;
        if(ge_mp_registry_resolve(owner->registry,owner->ids[i],NULL)!=GE_MP_STATE_OK)return GE_MP_STATE_STALE;
    }
    return GE_MP_STATE_OK;
}
GeMpStateStatus ge_mp_state_players_begin(GeMpStatePlayersOwner *owner,GeMpRegistry *registry)
{
    GeMpStatePlayersOwner next={0};GeOriginalMultiplayerInventoryStorage storage;
    GeMpStateStatus status;size_t missing=0;
    if(!owner || !registry || !registry->entries || registry->count>registry->capacity
            || registry->capacity>GE_MP_STATE_MAX_RECORDS)return GE_MP_STATE_INVALID;
    if(owner->live)return GE_MP_STATE_STALE;
    next.active_count=ge_original_multiplayer_player_count();
    if(next.active_count<2 || next.active_count>4)return GE_MP_STATE_INCOMPLETE;
    for(unsigned i=0;i<4;++i) {
        if(!ge_original_multiplayer_inventory_storage(i,&storage) || !storage.player || storage.capacity!=30
                || storage.active_player!=(i<next.active_count)
                || g_playerPointers[i]!=(storage.active_player?storage.player:NULL)
                || (i && storage.lifetime!=next.lifetime))return GE_MP_STATE_INCOMPLETE;
        next.players[i]=storage.player;next.lifetime=storage.lifetime;
        for(unsigned j=0;j<i;++j)if(next.players[j]==storage.player)return GE_MP_STATE_INCOMPLETE;
    }
    for(size_t i=0;i<registry->count;++i) {
        const GeMpRegistryEntry *e=&registry->entries[i];
        if(!(e->key>>24) || e->live>1 || (e->live&&!e->generation)
                || (i && registry->entries[i-1].key>=e->key))return GE_MP_STATE_INVALID;
    }
    for(unsigned i=0;i<4;++i) {
        const GeMpRegistryEntry *e=entry(registry,key(i));
        if(!e)++missing;else if(e->live)return GE_MP_STATE_STALE;else if(e->generation==UINT32_MAX)return GE_MP_STATE_EXHAUSTED;
    }
    if(missing>registry->capacity-registry->count)return GE_MP_STATE_CAPACITY;
    for(unsigned i=0;i<4;++i) {
        if(!entry(registry,key(i))){status=ge_mp_registry_define(registry,key(i));if(status!=GE_MP_STATE_OK)return status;}
        status=ge_mp_registry_acquire(registry,key(i),&next.ids[i]);if(status!=GE_MP_STATE_OK)return status;
    }
    next.registry=registry;next.live=1;*owner=next;return GE_MP_STATE_OK;
}
GeMpStateStatus ge_mp_state_players_end(GeMpStatePlayersOwner *owner)
{
    if(!owner || !owner->live)return GE_MP_STATE_STALE;
    GeMpStateStatus result=native_valid(owner);
    for(unsigned i=0;i<4;++i){GeMpStateStatus status=ge_mp_registry_release(owner->registry,owner->ids[i]);if(status!=GE_MP_STATE_OK)result=status;}
    memset(owner,0,sizeof(*owner));return result;
}
GeMpStateStatus ge_mp_state_players_actor(void *opaque,const void *player,uint64_t *id)
{
    GeMpStatePlayersOwner *owner=opaque;GeMpStateStatus status;
    if(!id)return GE_MP_STATE_INVALID;
    status=native_valid(owner);if(status!=GE_MP_STATE_OK)return status;
    for(unsigned i=0;i<4;++i)if(owner->players[i]==player){*id=owner->ids[i];return GE_MP_STATE_OK;}
    return GE_MP_STATE_INCOMPLETE;
}
static void gap(GeMpStatePlayersSnapshot *out,unsigned player,unsigned field,unsigned index,
    GeMpPlayerReferenceKind kind,GeMpPlayerGapReason reason,GeMpStateStatus status)
{
    if(out->gap_count<GE_MP_PLAYER_MAX_GAPS)out->gaps[out->gap_count++]=(GeMpPlayerGap){(uint16_t)field,(uint16_t)index,(uint8_t)player,(uint8_t)kind,(uint8_t)reason,status};
    ++out->total_gaps;
    if(reason==GE_MP_PLAYER_GAP_REFERENCE || reason==GE_MP_PLAYER_GAP_RESOLVER)++out->coverage.unresolved_references;
    else ++out->coverage.unclassified_fields;
}
static __attribute__((noinline)) void resolve(const GeMpStatePlayersOwner *owner,const GeMpStatePlayersProviders *providers,
    GeMpStatePlayersSnapshot *out,unsigned slot,uint16_t field,unsigned index,
    GeMpPlayerReferenceKind kind,const void *pointer,size_t extent)
{
    GeMpPlayerReference result={0};GeMpStateStatus status=GE_MP_STATE_INCOMPLETE;
    if(!pointer)return;
    if(kind==GE_MP_PLAYER_REF_UNKNOWN) {
        gap(out,slot,field,index,kind,GE_MP_PLAYER_GAP_UNKNOWN,GE_MP_STATE_INCOMPLETE);
        out->fields[slot][field-256].count=0;out->fields[slot][field-255].count=0;return;
    }
    if(providers && providers->reference)status=providers->reference(providers->context,slot,field,kind,pointer,extent,&result);
    if(status==GE_MP_STATE_OK) {
        if(!result.actor_id)status=GE_MP_STATE_INCOMPLETE;
        else status=ge_mp_registry_resolve(owner->registry,result.actor_id,NULL);
        if(status==GE_MP_STATE_OK && (kind==GE_MP_PLAYER_REF_PROP || kind==GE_MP_PLAYER_REF_HAND || kind==GE_MP_PLAYER_REF_INVENTORY_OWNER) && result.offset)status=GE_MP_STATE_INCOMPLETE;
        if(status==GE_MP_STATE_OK && kind==GE_MP_PLAYER_REF_PROP && ((result.actor_id>>56)!=0xE3 || ((result.actor_id>>48)&255)!=255))status=GE_MP_STATE_INCOMPLETE;
        if(status==GE_MP_STATE_OK && kind==GE_MP_PLAYER_REF_INVENTORY_OWNER
                && (result.actor_id>>32)!=ge_mp_registry_key(0xE2,(uint8_t)slot,0))status=GE_MP_STATE_INCOMPLETE;
        if(status==GE_MP_STATE_OK && kind==GE_MP_PLAYER_REF_HAND) {
            const struct player *p=owner->players[slot];unsigned hand=pointer==&p->hands[0]?0:1;
            if((result.actor_id>>32)!=ge_mp_registry_key(0xE4,(uint8_t)slot,(uint16_t)hand))status=GE_MP_STATE_INCOMPLETE;
        }
    }
    if(status!=GE_MP_STATE_OK) {
        gap(out,slot,field,index,kind,providers&&providers->reference?GE_MP_PLAYER_GAP_REFERENCE:GE_MP_PLAYER_GAP_RESOLVER,status);
        out->fields[slot][field-256].count=0;out->fields[slot][field-255].count=0;return;
    }
    out->values[slot].refs[index]=result.actor_id;out->values[slot].offsets[index]=result.offset;
}
#define GE_PLAYER_ARRAY_I32 i32
#define GE_PLAYER_ARRAY_I16 i16
#define GE_PLAYER_ARRAY_U32 u32
#define GE_PLAYER_ARRAY_U16 u16
#define GE_PLAYER_ARRAY_U8 u8
#define GE_PLAYER_ARRAY_F32 f32
#define P_COPY(type,index,expression) v->GE_PLAYER_ARRAY_##type[index]=(expression)
#define GE_MP_PLAYER_COPY_FUNCTIONS
#include "ge_mp_state_players_copy.inc"
#undef GE_MP_PLAYER_COPY_FUNCTIONS
#undef P_COPY
GeMpStateStatus ge_mp_state_players_export(const GeMpStatePlayersOwner *owner,
    const GeMpStatePlayersProviders *providers,GeMpStatePlayersSnapshot *out)
{
    if(!out)return GE_MP_STATE_INVALID;
    memset(out,0,sizeof(*out));GeMpStateStatus status=native_valid(owner);if(status!=GE_MP_STATE_OK)return status;
#if !defined(VERSION_US) || defined(DEBUG)
    gap(out,0,0,0,0,GE_MP_PLAYER_GAP_BUILD,GE_MP_STATE_INCOMPLETE);
    return GE_MP_STATE_INCOMPLETE;
#endif
    for(unsigned slot=0;slot<4;++slot) {
        GeOriginalMultiplayerInventoryStorage storage;
        const struct player *p=owner->players[slot];GeMpPlayerValues *v=&out->values[slot];unsigned index=0;
        if(!ge_original_multiplayer_inventory_storage(slot,&storage))return GE_MP_STATE_STALE;
        for(index=0;index<GE_MP_PLAYER_FIELD_COUNT;++index)
            out->fields[slot][index]=(GeMpStateField){fields[index].id,fields[index].type,fields[index].max_count,(const unsigned char*)v+value_offsets[index]};
        ge_player_copy_native(v,p,storage.active_player);
#define P_REF(field,index,kind,pointer,extent) resolve(owner,providers,out,slot,field,index,GE_MP_PLAYER_REF_##kind,pointer,extent)
#include "ge_mp_state_players_copy.inc"
#undef P_REF
#undef P_COPY
        for(index=0;index<GE_MP_PLAYER_FIELD_COUNT;++index) {
            GeMpStateField *f=&out->fields[slot][index];
            if(f->type==GE_MP_STATE_F32)for(unsigned j=0;j<f->count;++j)if(!isfinite(((const float*)f->values)[j])) {
                gap(out,slot,f->id,j,0,GE_MP_PLAYER_GAP_NONFINITE,GE_MP_STATE_INCOMPLETE);f->count=0;break;
            }
        }
        out->records[slot]=(GeMpStateRecord){owner->ids[slot],GE_MP_PLAYERS_KIND,out->fields[slot],GE_MP_PLAYER_FIELD_COUNT};
    }
    out->record_count=4;
    if(out->total_gaps)return GE_MP_STATE_INCOMPLETE;
    out->coverage.enumerated_systems=GE_MP_SYSTEM_PLAYERS;return GE_MP_STATE_OK;
}

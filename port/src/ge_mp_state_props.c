#include "ge_mp_state_props.h"
#define GE_PROP_LIFETIME_OBSERVER_IMPLEMENTATION
#include "ge_original_prop_lifetime.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include "game/player.h"
#include <math.h>
#include <string.h>

_Static_assert(sizeof(PropRecord)==(sizeof(void*)==4?0x34:0x58),"review PropRecord coverage after native layout changes");
_Static_assert(MAX_PROPS==600 && PROPRECORD_STAN_ROOM_LEN==4,"review prop pool schema");
_Static_assert(sizeof(((struct roomproplistblock *)0)->propnums)==32,"review room block schema");
static GeMpStatePropsOwner *bound;
static uint32_t prop_key(unsigned slot){return ge_mp_registry_key(GE_MP_PROPS_KIND,255,(uint16_t)slot);}
static uint32_t pool_key(void){return ge_mp_registry_key(GE_MP_PROP_POOL_KIND,255,0);}
static int slot_of(const GeMpStatePropsOwner *owner,const void *ptr,unsigned *slot)
{
    uintptr_t base=(uintptr_t)owner->storage.props,address=(uintptr_t)ptr;
    if(!ptr || address<base || address-base>=600*sizeof(PropRecord)
            || (address-base)%sizeof(PropRecord))return 0;
    *slot=(unsigned)((address-base)/sizeof(PropRecord));return 1;
}
static const GeMpRegistryEntry *entry(const GeMpRegistry *r,uint32_t k)
{size_t i;for(i=0;i<r->count;++i)if(r->entries[i].key==k)return &r->entries[i];return NULL;}
static GeMpStateStatus native_valid(const GeMpStatePropsOwner *owner,GeOriginalPropPoolStorage *s)
{
    if(!owner || !owner->live || bound!=owner)return GE_MP_STATE_STALE;
    if(owner->fault!=GE_MP_STATE_OK)return GE_MP_STATE_INCOMPLETE;
    if(!ge_original_prop_state_pool_storage(s) || s->props!=owner->storage.props
            || s->room_heads!=owner->storage.room_heads || s->room_blocks!=owner->storage.room_blocks
            || s->lifetime!=owner->storage.lifetime || s->room_capacity!=owner->storage.room_capacity)
        return GE_MP_STATE_STALE;
    if(s->allocations_now!=owner->allocations_observed)return GE_MP_STATE_INCOMPLETE;
    return GE_MP_STATE_OK;
}
GeMpStateStatus ge_mp_state_props_begin(GeMpStatePropsOwner *owner,GeMpRegistry *registry)
{
    GeMpStatePropsOwner next={0};
    const PropRecord *p;
    uint8_t seen[600]={0};
    unsigned i,n=0;
    size_t missing=0;
    GeMpStateStatus status;
    if(!owner || !registry || !registry->entries || registry->count>registry->capacity
            || registry->capacity>GE_MP_STATE_MAX_RECORDS)return GE_MP_STATE_INVALID;
    if(owner->live || bound)return GE_MP_STATE_STALE;
    if(!ge_original_prop_state_pool_storage(&next.storage) || next.storage.prop_capacity!=600
            || next.storage.room_head_capacity!=256 || next.storage.room_block_capacity!=256
            || next.storage.room_capacity>256 || !next.storage.room_capacity
            || next.storage.allocations_now!=next.storage.allocations_at_reset)
        return GE_MP_STATE_INCOMPLETE;
    if(next.storage.active_head || next.storage.active_tail)return GE_MP_STATE_INCOMPLETE;
    for(p=next.storage.free_head;p;p=p->prev) {
        unsigned slot;
        if(!slot_of(&next,p,&slot) || seen[slot] || n++==600)return GE_MP_STATE_INCOMPLETE;
        seen[slot]=1;
    }
    if(n!=600)return GE_MP_STATE_INCOMPLETE;
    for(i=0;i<registry->count;++i) {
        const GeMpRegistryEntry *e=&registry->entries[i];
        if(!(e->key>>24) || e->live>1 || (e->live&&!e->generation)
                || (i && registry->entries[i-1].key>=e->key))return GE_MP_STATE_INVALID;
    }
    for(i=0;i<=600;++i) {
        const GeMpRegistryEntry *e=entry(registry,i<600?prop_key(i):pool_key());
        if(!e)++missing;
        else if(e->live)return GE_MP_STATE_STALE;
        else if(e->generation==UINT32_MAX)return GE_MP_STATE_EXHAUSTED;
    }
    if(missing>registry->capacity-registry->count)return GE_MP_STATE_CAPACITY;
    for(i=0;i<=600;++i) {
        uint32_t k=i<600?prop_key(i):pool_key();
        if(!entry(registry,k)) {
            status=ge_mp_registry_define(registry,k);if(status!=GE_MP_STATE_OK)return status;
        }
    }
    status=ge_mp_registry_acquire(registry,pool_key(),&next.pool_id);
    if(status!=GE_MP_STATE_OK)return status;
    next.registry=registry;next.live=1;next.allocations_observed=next.storage.allocations_now;
    *owner=next;bound=owner;return GE_MP_STATE_OK;
}
void ge_mp_state_props_allocated(void *prop)
{
    GeOriginalPropPoolStorage now;unsigned slot;GeMpStateStatus status;
    if(!bound || bound->fault!=GE_MP_STATE_OK)return;
    if(!slot_of(bound,prop,&slot) || !ge_original_prop_state_pool_storage(&now)
            || now.lifetime!=bound->storage.lifetime
            || now.allocations_now!=bound->allocations_observed+1 || bound->ids[slot]) {
        bound->fault=GE_MP_STATE_INCOMPLETE;return;
    }
    bound->allocations_observed=now.allocations_now;
    status=ge_mp_registry_acquire(bound->registry,prop_key(slot),&bound->ids[slot]);
    if(status!=GE_MP_STATE_OK)bound->fault=status;
}
void ge_mp_state_props_freeing(void *prop)
{
    unsigned slot;GeMpStateStatus status;
    if(!bound || bound->fault!=GE_MP_STATE_OK)return;
    if(!slot_of(bound,prop,&slot) || !bound->ids[slot]){bound->fault=GE_MP_STATE_INCOMPLETE;return;}
    status=ge_mp_registry_release(bound->registry,bound->ids[slot]);
    if(status!=GE_MP_STATE_OK)bound->fault=status;else bound->ids[slot]=0;
}
void ge_mp_state_props_resetting(void)
{if(bound)bound->fault=GE_MP_STATE_INCOMPLETE;}
GeMpStateStatus ge_mp_state_props_end(GeMpStatePropsOwner *owner)
{
    unsigned i;GeMpStateStatus result=GE_MP_STATE_OK,status;
    if(!owner || !owner->live || bound!=owner)return GE_MP_STATE_STALE;
    if(owner->fault!=GE_MP_STATE_OK)result=GE_MP_STATE_INCOMPLETE;
    for(i=0;i<600;++i)if(owner->ids[i]) {
        result=GE_MP_STATE_INCOMPLETE;
        status=ge_mp_registry_release(owner->registry,owner->ids[i]);
        if(status!=GE_MP_STATE_OK)result=status;
    }
    status=ge_mp_registry_release(owner->registry,owner->pool_id);
    if(status!=GE_MP_STATE_OK)result=status;
    bound=NULL;memset(owner,0,sizeof(*owner));return result;
}
GeMpStateStatus ge_mp_state_props_actor(void *opaque,const void *prop,uint64_t *id)
{
    GeMpStatePropsOwner *owner=opaque;GeOriginalPropPoolStorage now;
    unsigned slot;GeMpStateStatus status;
    if(!id)return GE_MP_STATE_INVALID;
    status=native_valid(owner,&now);if(status!=GE_MP_STATE_OK)return status;
    if(!slot_of(owner,prop,&slot) || !owner->ids[slot])return GE_MP_STATE_INCOMPLETE;
    status=ge_mp_registry_resolve(owner->registry,owner->ids[slot],NULL);
    if(status==GE_MP_STATE_OK)*id=owner->ids[slot];
    return status;
}

#define F(id,type,flags,min,max) {id,GE_MP_STATE_##type,flags,min,max}
static const GeMpFieldSchema prop_schema[]={
 F(GE_MP_FIELD_CHILD_ORDER,ACTOR_REF,GE_MP_FIELD_UNIQUE,0,600),
 F(256,U8,0,1,1),F(257,U8,0,1,1),F(258,I16,0,1,1),
 F(259,ACTOR_REF,GE_MP_FIELD_NULLABLE,0,1),F(260,ACTOR_REF,GE_MP_FIELD_NULLABLE,0,1),
 F(261,ACTOR_REF,GE_MP_FIELD_NULLABLE,0,1),F(262,ACTOR_REF,GE_MP_FIELD_NULLABLE,0,1),
 F(263,ACTOR_REF,GE_MP_FIELD_NULLABLE,0,1),F(264,ACTOR_REF,GE_MP_FIELD_NULLABLE,0,1),
 F(265,F32,0,3,3),F(266,ACTOR_REF,GE_MP_FIELD_NULLABLE,1,1),F(267,F32,0,1,1),
 F(268,ACTOR_REF,GE_MP_FIELD_NULLABLE,1,1),F(269,ACTOR_REF,GE_MP_FIELD_NULLABLE,1,1),
 F(270,ACTOR_REF,GE_MP_FIELD_NULLABLE,1,1),F(271,ACTOR_REF,GE_MP_FIELD_NULLABLE,1,1),
 F(272,U8,0,4,4),F(273,I32,0,1,1),F(274,ACTOR_REF,GE_MP_FIELD_NULLABLE,1,1)
};
static const GeMpFieldSchema pool_schema[]={
 F(GE_MP_FIELD_ACTIVE_ORDER,ACTOR_REF,GE_MP_FIELD_UNIQUE,0,600),
 F(GE_MP_FIELD_FREE_ORDER,SLOT_REF,GE_MP_FIELD_UNIQUE,0,600),
 F(GE_MP_FIELD_ROOM_MEMBER_ORDER,ACTOR_REF,0,0,3840),
 F(256,ACTOR_REF,GE_MP_FIELD_NULLABLE,1,1),F(257,ACTOR_REF,GE_MP_FIELD_NULLABLE,1,1),
 F(258,SLOT_REF,GE_MP_FIELD_NULLABLE,1,1),F(259,U32,0,1,1),F(260,I16,0,256,256),
 F(261,BOOL,0,256,256),F(262,I16,0,256,256),F(263,SLOT_REF,GE_MP_FIELD_NULLABLE,3840,3840),
 F(264,U16,0,256,256),F(265,U8,0,0,600),F(266,F32,0,0,1800),
 F(267,F32,0,0,600),F(268,U8,0,0,2400),F(269,I32,0,0,600)
};
#undef F
static const GeMpRecordSchema schemas[]={
 {GE_MP_PROPS_KIND,GE_MP_SYSTEM_PROPS,prop_schema,GE_MP_PROP_FIELDS},
 {GE_MP_PROP_POOL_KIND,GE_MP_SYSTEM_PROPS,pool_schema,GE_MP_PROP_POOL_FIELDS}
};
const GeMpRecordSchema *ge_mp_state_props_schemas(size_t *count){if(count)*count=2;return schemas;}
static GeMpStateStatus gap(GeMpStatePropsSnapshot *out,int reference)
{out->record_count=0;if(reference)++out->coverage.unresolved_references;else ++out->coverage.unclassified_fields;return GE_MP_STATE_INCOMPLETE;}
static int own_ref(const GeMpStatePropsOwner *o,const PropRecord *p,uint64_t *id)
{
    unsigned s;if(!p){*id=0;return 1;}
    if(!slot_of(o,p,&s) || !o->ids[s] || ge_mp_registry_resolve(o->registry,o->ids[s],NULL)!=GE_MP_STATE_OK)return 0;
    *id=o->ids[s];return 1;
}
static int external_ref(const GeMpStatePropsOwner *o,const GeMpStatePropsProviders *p,
    GeMpPropReferenceKind kind,const void *ptr,uint64_t *id)
{
    if(!ptr){*id=0;return 1;}
    return p->reference(p->context,kind,ptr,id)==GE_MP_STATE_OK && *id
        && ge_mp_registry_resolve(o->registry,*id,NULL)==GE_MP_STATE_OK;
}
static int finite_position(const PropRecord *p)
{return isfinite(p->pos.x)&&isfinite(p->pos.y)&&isfinite(p->pos.z)&&isfinite(p->zDepth);}

/* Valid sibling chains do not rule out a cycle through their parents. Each
 * live prop has at most one parent, so mark an entire ancestry walk before
 * retiring it. Encountering a mark from the current walk is a cycle. */
static int parents_acyclic(const GeMpStatePropsOwner *o,const PropRecord *props)
{
    uint8_t color[600]={0}; /* 0 unseen, 1 current walk, 2 finished */
    unsigned start;
    for(start=0;start<600;++start)if(o->ids[start] && !color[start]) {
        unsigned slot=start;
        while(slot<600 && !color[slot]) {
            const PropRecord *parent=props[slot].parent;
            color[slot]=1;
            if(!parent){slot=600;break;}
            if(!slot_of(o,parent,&slot) || !o->ids[slot])return 0;
        }
        if(slot<600 && color[slot]==1)return 0;
        slot=start;
        while(color[slot]==1) {
            const PropRecord *parent=props[slot].parent;
            color[slot]=2;
            if(!parent)break;
            if(!slot_of(o,parent,&slot) || !o->ids[slot])return 0;
        }
    }
    return 1;
}

GeMpStateStatus ge_mp_state_props_export(const GeMpStatePropsOwner *o,
    const GeMpStatePropsProviders *providers,GeMpStatePropsSnapshot *out)
{
    GeOriginalPropPoolStorage now;GeMpStateStatus status;
    const PropRecord *p,*prior=NULL;
    unsigned i,j,n=0,free_count=0,active_count=0,child_count=0,room_count=0;
    uint8_t free_seen[600]={0},active_seen[600]={0},child_seen[600]={0},block_seen[256]={0};
    if(!out)return GE_MP_STATE_INVALID;
    memset(out,0,sizeof(*out));status=native_valid(o,&now);if(status!=GE_MP_STATE_OK)return status;
    if(!providers || !providers->reference)return gap(out,1);
    if(ge_mp_registry_resolve(o->registry,o->pool_id,NULL)!=GE_MP_STATE_OK)return gap(out,1);
    for(i=0;i<600;++i) {
        const GeMpRegistryEntry *e=entry(o->registry,prop_key(i));
        if(!e || !!e->live!=!!o->ids[i])return gap(out,1);
    }
    for(p=now.free_head;p;p=p->prev) {
        unsigned s;
        if(!slot_of(o,p,&s) || free_seen[s] || free_count==600 || o->ids[s])return gap(out,1);
        free_seen[s]=1;out->pool.free_order[free_count]=prop_key(s);
        /* Allocate overwrites links/flags/timer/STAN/room[0], but retains
         * these typed scalars. Never inspect the freed union pointer. */
        if(!finite_position(p))return gap(out,0);
        out->pool.free_type[free_count]=p->type;
        memcpy(&out->pool.free_position[free_count*3],p->pos.f,12);
        out->pool.free_zdepth[free_count]=p->zDepth;
        memcpy(&out->pool.free_rooms[free_count*4],p->rooms,4);
        out->pool.free_unk30[free_count]=p->unk30;++free_count;
    }
    if(free_count)out->pool.free_head=out->pool.free_order[0];
    for(p=now.active_head;p;p=p->next) {
        unsigned s;
        if(!slot_of(o,p,&s) || active_seen[s] || active_count==600 || p->prev!=prior
                || p->parent || !own_ref(o,p,&out->pool.active[active_count]))return gap(out,1);
        active_seen[s]=1;prior=p;++active_count;
    }
    if(prior!=now.active_tail)return gap(out,0);
    if(!own_ref(o,now.active_head,&out->pool.head) || !own_ref(o,now.active_tail,&out->pool.tail))return gap(out,1);
    for(i=0;i<600;++i) {
        GeMpPropValues *v=&out->props[i];GeMpStateField *f=out->prop_fields[i];
        unsigned variant,child_begin=child_count;
        const PropRecord *child,*newer=NULL;
        int terminated=0;
        if(free_seen[i])continue;
        p=&now.props[i];
        if(!o->ids[i] || ge_mp_registry_resolve(o->registry,o->ids[i],NULL)!=GE_MP_STATE_OK)return gap(out,1);
        if(!finite_position(p))return gap(out,0);
        switch(p->type) {
        case PROP_TYPE_OBJ:variant=0;break;case PROP_TYPE_DOOR:variant=1;break;
        case PROP_TYPE_WEAPON:variant=2;break;case PROP_TYPE_CHR:case PROP_TYPE_VIEWER:variant=3;break;
        case PROP_TYPE_EXPLOSION:variant=4;break;case PROP_TYPE_SMOKE:variant=5;break;
        default:return gap(out,0);
        }
        if(!external_ref(o,providers,(GeMpPropReferenceKind)variant,p->voidp,&v->refs[variant])
                || !external_ref(o,providers,GE_MP_PROP_REF_STAN,p->stan,&v->refs[6]))return gap(out,1);
        if(!own_ref(o,p->parent,&v->refs[7]) || !own_ref(o,p->child,&v->refs[8])
                || !own_ref(o,p->prev,&v->refs[9]) || !own_ref(o,p->next,&v->refs[10]))return gap(out,1);
        if(p->type==PROP_TYPE_VIEWER) {
            const struct player *player=NULL;
            for(j=0;j<4;++j)if(g_playerPointers[j] && g_playerPointers[j]->prop==p) {
                if(player)return gap(out,0);
                player=g_playerPointers[j];
            }
            if(!player || !external_ref(o,providers,GE_MP_PROP_REF_PLAYER,player,&v->refs[11]))return gap(out,1);
        }
        for(child=p->child;child;child=child->prev) {
            unsigned s;
            if(!slot_of(o,child,&s) || child_seen[s] || child_count==600 || child->parent!=p
                    || child->next!=newer || active_seen[s]
                    || !own_ref(o,child,&out->child_order[child_count]))return gap(out,1);
            child_seen[s]=1;newer=child;++child_count;
        }
        v->type=p->type;v->flags=p->flags;v->timer=p->timetoregen;v->unk30=p->unk30;
        memcpy(v->position,p->pos.f,12);v->zdepth=p->zDepth;memcpy(v->rooms,p->rooms,4);
        for(j=0;j<4;++j) {
            if(p->rooms[j]==255){terminated=1;break;}
            if(p->rooms[j]>=now.room_capacity)return gap(out,0);
        }
        if(!terminated)return gap(out,0);
#define V(index,id,type,count,value) f[index]=(GeMpStateField){id,GE_MP_STATE_##type,count,value}
        V(0,GE_MP_FIELD_CHILD_ORDER,ACTOR_REF,child_count-child_begin,&out->child_order[child_begin]);
        V(1,256,U8,1,&v->type);V(2,257,U8,1,&v->flags);V(3,258,I16,1,&v->timer);
        for(j=0;j<6;++j)f[4+j]=(GeMpStateField){(uint16_t)(259+j),GE_MP_STATE_ACTOR_REF,j==variant,&v->refs[j]};
        V(10,265,F32,3,v->position);V(11,266,ACTOR_REF,1,&v->refs[6]);V(12,267,F32,1,&v->zdepth);
        for(j=7;j<=10;++j)f[13+j-7]=(GeMpStateField){(uint16_t)(268+j-7),GE_MP_STATE_ACTOR_REF,1,&v->refs[j]};
        V(17,272,U8,4,v->rooms);V(18,273,I32,1,&v->unk30);V(19,274,ACTOR_REF,1,&v->refs[11]);
#undef V
        out->records[n++]=(GeMpStateRecord){o->ids[i],GE_MP_PROPS_KIND,f,GE_MP_PROP_FIELDS};
    }
    for(i=0;i<600;++i)if(o->ids[i] && !!now.props[i].parent!=!!child_seen[i])return gap(out,0);
    if(!parents_acyclic(o,now.props))return gap(out,0);
    /* Preserve exact typed block holes and allocator links, as well as each
     * room's iteration order. Freed blocks overwrite all 16 words on reuse. */
    for(i=0;i<256;++i) {
        const s16 *block=now.room_blocks[i].propnums;
        out->pool.room_heads[i]=now.room_heads[i];
        out->pool.block_free[i]=block[0]==-2;
        out->pool.block_next[i]=-1;
        if(block[0]==-2)continue;
        if(block[15]<-1 || block[15]>=256)return gap(out,0);
        out->pool.block_next[i]=block[15];
        for(j=0;j<15;++j) {
            if(block[j]<-1 || block[j]>=600)return gap(out,0);
            if(block[j]>=0) {
                unsigned s=(unsigned)block[j];if(!o->ids[s])return gap(out,1);
                out->pool.block_members[i*15+j]=prop_key(s);
            }
        }
    }
    for(i=0;i<256;++i) {
        int block=now.room_heads[i];unsigned room_begin=room_count;
        if(block<-1 || block>=256 || (i>=now.room_capacity && block!=-1))return gap(out,0);
        while(block>=0) {
            if(block_seen[block] || out->pool.block_free[block])return gap(out,0);
            block_seen[block]=1;
            for(j=0;j<15;++j) {
                int slot=now.room_blocks[block].propnums[j];
                if(slot>=0) {
                    if(room_count==3840)return gap(out,0);
                    out->pool.room_members[room_count++]=o->ids[slot];
                }
            }
            block=out->pool.block_next[block];
        }
        out->pool.room_counts[i]=(uint16_t)(room_count-room_begin);
    }
    for(i=0;i<256;++i)if(!out->pool.block_free[i] && !block_seen[i])return gap(out,0);
    out->pool.room_capacity=now.room_capacity;
    {
        GeMpStateField *f=out->pool_fields;GeMpPropPoolValues *v=&out->pool;
#define V(index,id,type,count,value) f[index]=(GeMpStateField){id,GE_MP_STATE_##type,count,value}
        V(0,GE_MP_FIELD_ACTIVE_ORDER,ACTOR_REF,active_count,v->active);
        V(1,GE_MP_FIELD_FREE_ORDER,SLOT_REF,free_count,v->free_order);
        V(2,GE_MP_FIELD_ROOM_MEMBER_ORDER,ACTOR_REF,room_count,v->room_members);
        V(3,256,ACTOR_REF,1,&v->head);V(4,257,ACTOR_REF,1,&v->tail);V(5,258,SLOT_REF,1,&v->free_head);
        V(6,259,U32,1,&v->room_capacity);V(7,260,I16,256,v->room_heads);V(8,261,BOOL,256,v->block_free);
        V(9,262,I16,256,v->block_next);V(10,263,SLOT_REF,3840,v->block_members);V(11,264,U16,256,v->room_counts);
        V(12,265,U8,free_count,v->free_type);V(13,266,F32,free_count*3,v->free_position);
        V(14,267,F32,free_count,v->free_zdepth);V(15,268,U8,free_count*4,v->free_rooms);V(16,269,I32,free_count,v->free_unk30);
#undef V
    }
    out->records[n++]=(GeMpStateRecord){o->pool_id,GE_MP_PROP_POOL_KIND,out->pool_fields,GE_MP_PROP_POOL_FIELDS};
    out->record_count=n;out->coverage.enumerated_systems=GE_MP_SYSTEM_PROPS;
    return GE_MP_STATE_OK;
}

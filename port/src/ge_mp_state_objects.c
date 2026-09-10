#include "ge_mp_state_objects.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include <math.h>
#include <string.h>
/* Source BITFLAG(RUNTIMEBITFLAG): EMBEDDED bit6, HASPROJECTILE bit7. */
#define GE_OBJECT_EMBEDDED UINT32_C(0x40)
#define GE_OBJECT_PROJECTILE UINT32_C(0x80)
_Static_assert(AMMOTYPE_GLOBAL_MAX==13,"review native ammo slot coverage");
_Static_assert(offsetof(WeaponObjRecord,weaponnum)==sizeof(ObjectRecord),"native weapon prefix");
_Static_assert(offsetof(AmmoCrateRecord,ammoType)==sizeof(ObjectRecord),"native ammo prefix");
_Static_assert(offsetof(MultiAmmoCrateRecord,slots)==sizeof(ObjectRecord),"native multi-ammo prefix");
_Static_assert(offsetof(BodyArmourRecord,initialamount)==sizeof(ObjectRecord),"native armour prefix");
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(ObjectRecord)==0x80 && sizeof(WeaponObjRecord)==0x88,"original 32-bit object ABI");
_Static_assert(sizeof(MultiAmmoCrateRecord)==0xb4 && sizeof(BodyArmourRecord)==0x88,"original supply ABI");
#endif
size_t ge_mp_state_objects_native_size(uint8_t type)
{
    switch(type) {
    case PROPDEF_COLLECTABLE:return sizeof(WeaponObjRecord);
    case PROPDEF_MAGAZINE:return sizeof(AmmoCrateRecord);
    case PROPDEF_AMMO:return sizeof(MultiAmmoCrateRecord);
    case PROPDEF_ARMOUR:return sizeof(BodyArmourRecord);
    default:return 0;
    }
}
size_t ge_mp_state_objects_native_ammo_definitions(uint8_t ns,
    GeMpObjectDefinition *definitions,size_t capacity)
{
    if(!definitions || capacity<MAX_AMMO_CRATES)return 0;
    for(unsigned i=0;i<MAX_AMMO_CRATES;++i)
        definitions[i]=(GeMpObjectDefinition){&g_AmmoCrates[i],sizeof(g_AmmoCrates[i]),ns,PROPDEF_MAGAZINE,(uint16_t)i};
    return MAX_AMMO_CRATES;
}
static uint32_t key(const GeMpObjectDefinition *d)
{return ge_mp_registry_key(GE_MP_OBJECTS_KIND,d->creation_namespace,d->slot);}
static int descriptor_valid(const GeMpObjectDefinition *d)
{
    return d->definition && !((uintptr_t)d->definition%_Alignof(ObjectRecord))
        && ge_mp_state_objects_native_size(d->type)
        && d->bytes==ge_mp_state_objects_native_size(d->type);
}
/* Validate ALL owned identities before cleanup can mutate the registry.
 * Native lifetime expiry is independent of malformed owner storage. */
static GeMpStateStatus shape(const GeMpStateObjectsOwner *o)
{
    if(!o || !o->live)return GE_MP_STATE_STALE;
    if(o->live!=1 || !o->registry || !o->props || !o->pool_lifetime
            || o->count>GE_MP_OBJECTS_CAPACITY || !(uint32_t)o->pool_id
            || (uint32_t)(o->pool_id>>32)!=ge_mp_registry_key(GE_MP_PROP_POOL_KIND,255,0))
        return GE_MP_STATE_INVALID;
    /* resolve validates registry shape even if this old pool ID has expired. */
    if(ge_mp_registry_resolve(o->registry,o->pool_id,NULL)==GE_MP_STATE_INVALID)
        return GE_MP_STATE_INVALID;
    for(size_t i=0;i<o->count;++i)
        if(!descriptor_valid(&o->definitions[i]) || !(uint32_t)o->ids[i]
                || (uint32_t)(o->ids[i]>>32)!=key(&o->definitions[i])
                || (i && key(&o->definitions[i-1])>=key(&o->definitions[i])))
            return GE_MP_STATE_INVALID;
    for(size_t i=o->count;i<GE_MP_OBJECTS_CAPACITY;++i)
        if(o->ids[i])return GE_MP_STATE_INVALID;
    return GE_MP_STATE_OK;
}
static GeMpStateStatus valid(const GeMpStateObjectsOwner *o)
{
    GeMpStateStatus status=shape(o);if(status!=GE_MP_STATE_OK)return status;
    if(o->props->pool_id!=o->pool_id
            || o->props->storage.lifetime!=o->pool_lifetime
            || o->props->registry!=o->registry)return GE_MP_STATE_STALE;
    status=ge_mp_registry_resolve(o->registry,o->pool_id,NULL);
    if(status!=GE_MP_STATE_OK)return status;
    return ge_mp_state_props_check_lifetime(o->props);
}
static const GeMpRegistryEntry *entry(const GeMpRegistry *r,uint32_t k)
{for(size_t i=0;i<r->count;++i)if(r->entries[i].key==k)return &r->entries[i];return NULL;}
GeMpStateStatus ge_mp_state_objects_begin(GeMpStateObjectsOwner *o,
    GeMpStatePropsOwner *props,const GeMpObjectDefinition *defs,size_t count)
{
    GeMpStateStatus status;size_t missing=0;
    if(!o || !props || count>GE_MP_OBJECTS_CAPACITY || (count&&!defs))return GE_MP_STATE_INVALID;
    if(o->live)return GE_MP_STATE_STALE;
    status=ge_mp_state_props_check_lifetime(props);if(status!=GE_MP_STATE_OK)return status;
    GeMpRegistry *registry=props->registry;
    status=ge_mp_registry_resolve(registry,props->pool_id,NULL);if(status!=GE_MP_STATE_OK)return status;
    for(size_t i=0;i<count;++i) {
        if(!descriptor_valid(&defs[i])
                || (i && key(&defs[i-1])>=key(&defs[i])))return GE_MP_STATE_INVALID;
        for(size_t j=0;j<i;++j)if(defs[j].definition==defs[i].definition)return GE_MP_STATE_INVALID;
        const GeMpRegistryEntry *e=entry(registry,key(&defs[i]));
        if(!e)++missing;else if(e->live)return GE_MP_STATE_STALE;
        else if(e->generation==UINT32_MAX)return GE_MP_STATE_EXHAUSTED;
    }
    if(missing>registry->capacity-registry->count)return GE_MP_STATE_CAPACITY;
    if(count)memmove(o->definitions,defs,count*sizeof(*defs));
    defs=o->definitions;memset(o->ids,0,sizeof(o->ids));
    for(size_t i=0;i<count;++i) {
        if(!entry(registry,key(&defs[i]))) {
            status=ge_mp_registry_define(registry,key(&defs[i]));if(status!=GE_MP_STATE_OK)return status;
        }
        status=ge_mp_registry_acquire(registry,key(&defs[i]),&o->ids[i]);if(status!=GE_MP_STATE_OK)return status;
    }
    o->count=count;o->props=props;o->registry=registry;o->pool_id=props->pool_id;
    o->pool_lifetime=props->storage.lifetime;o->live=1;
    return GE_MP_STATE_OK;
}
GeMpStateStatus ge_mp_state_objects_end(GeMpStateObjectsOwner *o)
{
    GeMpStateStatus structural=shape(o);if(structural!=GE_MP_STATE_OK)return structural;
    GeMpStateStatus result=valid(o);
    for(size_t i=0;i<o->count;++i) {
        GeMpStateStatus status=ge_mp_registry_release(o->registry,o->ids[i]);
        if(status!=GE_MP_STATE_OK)result=status;
    }
    memset(o,0,sizeof(*o));return result;
}
GeMpStateStatus ge_mp_state_objects_actor(void *opaque,const void *definition,uint64_t *id)
{
    const GeMpStateObjectsOwner *o=opaque;GeMpStateStatus status=valid(o);
    if(!id)return GE_MP_STATE_INVALID;if(status!=GE_MP_STATE_OK)return status;
    for(size_t i=0;i<o->count;++i)if(o->definitions[i].definition==definition) {
        status=ge_mp_registry_resolve(o->registry,o->ids[i],NULL);
        if(status==GE_MP_STATE_OK)*id=o->ids[i];return status;
    }
    return GE_MP_STATE_INCOMPLETE;
}
#define F(id,t,min,max) {id,GE_MP_STATE_##t,0,min,max}
#define R(id,min,max) {id,GE_MP_STATE_ACTOR_REF,GE_MP_FIELD_NULLABLE,min,max}
static const GeMpFieldSchema fields[]={F(256,U8,1,1),F(257,U16,1,1),F(258,U8,1,1),
    F(259,I16,2,2),F(260,U32,3,3),F(261,F32,21,21),F(262,U8,8,8),
    R(263,1,1),R(264,1,1),R(265,1,1),R(266,1,1),R(267,1,1),
    F(268,I8,0,2),F(269,I16,0,1),R(270,0,1),F(271,I32,0,1),
    F(272,U16,0,13),F(273,U16,0,13),F(274,F32,0,2),F(275,U8,1,1),F(276,U8,1,1)};
/* 275/276 encode immutable physical namespace/type interpretation. */
#undef F
#undef R
static const GeMpRecordSchema schema={GE_MP_OBJECTS_KIND,GE_MP_SYSTEM_OBJECTS,fields,GE_MP_OBJECTS_FIELDS};
const GeMpRecordSchema *ge_mp_state_objects_schema(void){return &schema;}
static void ref(const GeMpStateObjectsOwner *o,const GeMpStateObjectsProviders *p,
    GeMpStateObjectsSnapshot *out,GeMpObjectReferenceKind kind,const void *native,uint64_t *id)
{
    if(!native)return;
    GeMpStateStatus status=GE_MP_STATE_INCOMPLETE;
    if(kind==GE_MP_OBJECT_REF_LINKED_OBJECT)status=ge_mp_state_objects_actor((void *)o,native,id);
    if(status!=GE_MP_STATE_OK && p && p->reference)status=p->reference(p->context,kind,native,id);
    if(status!=GE_MP_STATE_OK || !*id || ge_mp_registry_resolve(o->registry,*id,NULL)!=GE_MP_STATE_OK) {
        ++out->coverage.unresolved_references;out->unresolved_kinds|=1U<<kind;*id=0;
    }
}
GeMpStateStatus ge_mp_state_objects_export(const GeMpStateObjectsOwner *o,
    const GeMpStateObjectsProviders *providers,GeMpStateObjectsSnapshot *out)
{
    GeMpStateStatus status=valid(o);
    if(!out)return GE_MP_STATE_INVALID;memset(out,0,sizeof(*out));
    if(status!=GE_MP_STATE_OK){out->coverage.unclassified_fields=1;return status;}
    for(size_t i=0;i<o->count;++i) {
        const GeMpObjectDefinition *d=&o->definitions[i];const ObjectRecord *p=d->definition;
        GeMpObjectValues *v=&out->values[i];
        if((p->type!=d->type && (p->type!=0 || p->prop))
                || ge_mp_registry_resolve(o->registry,o->ids[i],NULL)!=GE_MP_STATE_OK) {
            ++out->coverage.unclassified_fields;continue;
        }
        v->type=p->type;v->variant=d->type;v->creation_namespace=d->creation_namespace;v->extrascale=p->extrascale;v->state=p->state;
        v->model_pad[0]=p->obj;v->model_pad[1]=p->pad;
        v->flags[0]=p->flags;v->flags[1]=p->flags2;v->flags[2]=p->runtime_bitflags;
        for(unsigned a=0;a<4;++a)for(unsigned b=0;b<4;++b)v->pose[a*4+b]=p->mtx.m[a][b];
        for(unsigned a=0;a<3;++a)v->pose[16+a]=p->runtime_pos.f[a];
        v->pose[19]=p->maxdamage;v->pose[20]=p->damage;
        for(unsigned a=0;a<21;++a)if(!isfinite(v->pose[a]))++out->coverage.unclassified_fields;
        v->colors[0]=p->shadecol.r;v->colors[1]=p->shadecol.g;v->colors[2]=p->shadecol.b;v->colors[3]=p->shadecol.a;
        v->colors[4]=p->nextcol.r;v->colors[5]=p->nextcol.g;v->colors[6]=p->nextcol.b;v->colors[7]=p->nextcol.a;
        if(p->prop) {
            status=ge_mp_state_props_actor(o->props,p->prop,&v->prop);
            if(status!=GE_MP_STATE_OK){++out->coverage.unresolved_references;out->unresolved_kinds|=1U<<GE_MP_OBJECT_REF_PROP;}
            else if(p->prop->obj!=p || (p->prop->type!=(d->type==PROPDEF_COLLECTABLE?PROP_TYPE_WEAPON:PROP_TYPE_OBJ)))
                ++out->coverage.unclassified_fields;
            if(!p->model){++out->coverage.unresolved_references;out->unresolved_kinds|=1U<<GE_MP_OBJECT_REF_MODEL;}
            ref(o,providers,out,GE_MP_OBJECT_REF_MODEL,p->model,&v->model);
        }
        /* objFree leaves the old Model pointer, but later construction replaces
         * it before use. Collision/union storage is selected by source flags. */
        if(p->state&PROPSTATE_EXT_COLISION_BLOCK)
            ref(o,providers,out,GE_MP_OBJECT_REF_COLLISION,p->ptr_allocated_collisiondata_block,&v->collision);
        if(p->runtime_bitflags&GE_OBJECT_EMBEDDED)
            ref(o,providers,out,GE_MP_OBJECT_REF_EMBEDMENT,p->embedment,&v->embedment);
        else if(p->runtime_bitflags&GE_OBJECT_PROJECTILE)
            ref(o,providers,out,GE_MP_OBJECT_REF_PROJECTILE,p->projectile,&v->projectile);
        switch(d->type) {
        case PROPDEF_COLLECTABLE: {
            const WeaponObjRecord *w=(const void *)p;v->weapons[0]=w->weaponnum;v->weapons[1]=w->LinkedWeaponType;v->timer=w->timer;
            ref(o,providers,out,GE_MP_OBJECT_REF_LINKED_OBJECT,w->dualweapon,&v->dualweapon);break;}
        case PROPDEF_MAGAZINE:v->ammo=(int32_t)((const AmmoCrateRecord *)p)->ammoType;break;
        case PROPDEF_AMMO:
            for(unsigned a=0;a<13;++a){v->slot_models[a]=((const MultiAmmoCrateRecord *)p)->slots[a].modelnum;
                v->quantities[a]=((const MultiAmmoCrateRecord *)p)->slots[a].quantity;}break;
        case PROPDEF_ARMOUR:v->armour[0]=((const BodyArmourRecord *)p)->initialamount;v->armour[1]=((const BodyArmourRecord *)p)->amount;
            if(!isfinite(v->armour[0]) || !isfinite(v->armour[1]))++out->coverage.unclassified_fields;break;
        }
#define V(n,member,count) out->fields[i][n]=(GeMpStateField){fields[n].id,fields[n].type,count,&v->member}
        V(0,type,1);V(1,extrascale,1);V(2,state,1);V(3,model_pad,2);V(4,flags,3);V(5,pose,21);V(6,colors,8);
        V(7,prop,1);V(8,model,1);V(9,collision,1);V(10,projectile,1);V(11,embedment,1);
        V(12,weapons,d->type==PROPDEF_COLLECTABLE?2:0);V(13,timer,d->type==PROPDEF_COLLECTABLE?1:0);
        V(14,dualweapon,d->type==PROPDEF_COLLECTABLE?1:0);V(15,ammo,d->type==PROPDEF_MAGAZINE?1:0);
        V(16,slot_models,d->type==PROPDEF_AMMO?13:0);V(17,quantities,d->type==PROPDEF_AMMO?13:0);
        V(18,armour,d->type==PROPDEF_ARMOUR?2:0);V(19,creation_namespace,1);V(20,variant,1);
#undef V
        out->records[i]=(GeMpStateRecord){o->ids[i],GE_MP_OBJECTS_KIND,out->fields[i],GE_MP_OBJECTS_FIELDS};
    }
    if(out->coverage.unclassified_fields || out->coverage.unresolved_references)return GE_MP_STATE_INCOMPLETE;
    out->record_count=o->count;out->coverage.enumerated_systems=GE_MP_SYSTEM_OBJECTS;return GE_MP_STATE_OK;
}

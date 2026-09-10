#include "ge_mp_state_model_topology.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <math.h>
#include <string.h>
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(ModelNode)==0x18,"native ModelNode ABI");
#if !defined(DEBUG) && !defined(VERSION_EU)
_Static_assert(sizeof(ModelFileHeader)==0x20,"native ModelFileHeader ABI");
#endif
#endif
size_t ge_mp_state_model_topology_native_size(uint8_t component)
{
    return component==GE_MP_TOPOLOGY_NODE?sizeof(ModelNode):component==GE_MP_TOPOLOGY_HEADER?sizeof(ModelFileHeader):0;
}
static uint32_t key(const GeMpModelTopologyDefinition *d)
{return ge_mp_registry_key(GE_MP_MODEL_TOPOLOGY_KIND,d->creation_namespace,d->slot);}
static int descriptor_valid(const GeMpModelTopologyDefinition *d)
{
    size_t alignment=d->component==GE_MP_TOPOLOGY_NODE?_Alignof(ModelNode):_Alignof(ModelFileHeader);
    return d->native && !((uintptr_t)d->native%alignment)
        && ge_mp_state_model_topology_native_size(d->component)
        && d->bytes==ge_mp_state_model_topology_native_size(d->component);
}
/* Validate ALL owned identities before cleanup can mutate the registry.
 * Native lifetime expiry is independent of malformed owner storage. */
static GeMpStateStatus shape(const GeMpStateModelTopologyOwner *o)
{
    if(!o || !o->live)return GE_MP_STATE_STALE;
    if(o->live!=1 || !o->registry || !o->props || !o->pool_lifetime
            || o->count>GE_MP_MODEL_TOPOLOGY_CAPACITY || !(uint32_t)o->pool_id
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
    for(size_t i=o->count;i<GE_MP_MODEL_TOPOLOGY_CAPACITY;++i)
        if(o->ids[i])return GE_MP_STATE_INVALID;
    return GE_MP_STATE_OK;
}
static GeMpStateStatus valid(const GeMpStateModelTopologyOwner *o)
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
GeMpStateStatus ge_mp_state_model_topology_begin(GeMpStateModelTopologyOwner *o,
    GeMpStatePropsOwner *props,const GeMpModelTopologyDefinition *defs,size_t count)
{
    GeMpStateStatus status;size_t missing=0;
    if(!o || !props || count>GE_MP_MODEL_TOPOLOGY_CAPACITY || (count&&!defs))return GE_MP_STATE_INVALID;
    if(o->live)return GE_MP_STATE_STALE;
    status=ge_mp_state_props_check_lifetime(props);if(status!=GE_MP_STATE_OK)return status;
    GeMpRegistry *registry=props->registry;
    status=ge_mp_registry_resolve(registry,props->pool_id,NULL);if(status!=GE_MP_STATE_OK)return status;
    for(size_t i=0;i<count;++i) {
        if(!descriptor_valid(&defs[i])
                || (i && key(&defs[i-1])>=key(&defs[i])))return GE_MP_STATE_INVALID;
        for(size_t j=0;j<i;++j)if(defs[j].native==defs[i].native)return GE_MP_STATE_INVALID;
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
GeMpStateStatus ge_mp_state_model_topology_end(GeMpStateModelTopologyOwner *o)
{
    GeMpStateStatus structural=shape(o);if(structural!=GE_MP_STATE_OK)return structural;
    GeMpStateStatus result=valid(o);
    for(size_t i=0;i<o->count;++i) {
        GeMpStateStatus status=ge_mp_registry_release(o->registry,o->ids[i]);
        if(status!=GE_MP_STATE_OK)result=status;
    }
    memset(o,0,sizeof(*o));return result;
}
GeMpStateStatus ge_mp_state_model_topology_actor(void *opaque,const void *definition,uint64_t *id)
{
    const GeMpStateModelTopologyOwner *o=opaque;GeMpStateStatus status=valid(o);
    if(!id)return GE_MP_STATE_INVALID;if(status!=GE_MP_STATE_OK)return status;
    for(size_t i=0;i<o->count;++i)if(o->definitions[i].native==definition) {
        status=ge_mp_registry_resolve(o->registry,o->ids[i],NULL);
        if(status==GE_MP_STATE_OK)*id=o->ids[i];return status;
    }
    return GE_MP_STATE_INCOMPLETE;
}
GeMpStateStatus ge_mp_state_model_topology_check(const GeMpStateModelTopologyOwner *o,const void *native,uint8_t component)
{
    GeMpStateStatus status=valid(o);if(status!=GE_MP_STATE_OK)return status;
    if(!ge_mp_state_model_topology_native_size(component))return GE_MP_STATE_INVALID;
    for(size_t i=0;i<o->count;++i)if(o->definitions[i].native==native) {
        if(o->definitions[i].component!=component)return GE_MP_STATE_INVALID;
        return ge_mp_registry_resolve(o->registry,o->ids[i],NULL);
    }
    return GE_MP_STATE_INCOMPLETE;
}
GeMpStateStatus ge_mp_state_model_topology_add(GeMpStateModelTopologyOwner *o,
    const GeMpModelTopologyDefinition *defs,size_t count)
{
    GeMpStateStatus status=valid(o);size_t missing=0;
    if(status!=GE_MP_STATE_OK)return status;
    if(count>GE_MP_MODEL_TOPOLOGY_CAPACITY-o->count || (count&&!defs))return GE_MP_STATE_INVALID;
    for(size_t i=0;i<o->count;++i) {
        status=ge_mp_registry_resolve(o->registry,o->ids[i],NULL);
        if(status!=GE_MP_STATE_OK)return status;
    }
    if(!count)return GE_MP_STATE_OK;
    uintptr_t first=(uintptr_t)defs,last=first+count*sizeof(*defs);
    uintptr_t own=(uintptr_t)o,own_end=own+sizeof(*o);
    if(last<first || (first<own_end && last>own))return GE_MP_STATE_INVALID;
    for(size_t i=0;i<count;++i) {
        if(!descriptor_valid(&defs[i]) || (i && key(&defs[i-1])>=key(&defs[i])))return GE_MP_STATE_INVALID;
        for(size_t j=0;j<i;++j)if(defs[j].native==defs[i].native)return GE_MP_STATE_INVALID;
        for(size_t j=0;j<o->count;++j)
            if(defs[i].native==o->definitions[j].native || key(&defs[i])==key(&o->definitions[j]))return GE_MP_STATE_INVALID;
        const GeMpRegistryEntry *e=entry(o->registry,key(&defs[i]));
        if(!e)++missing;else if(e->live)return GE_MP_STATE_STALE;
        else if(e->generation==UINT32_MAX)return GE_MP_STATE_EXHAUSTED;
    }
    if(missing>o->registry->capacity-o->registry->count)return GE_MP_STATE_CAPACITY;
    /* Every registry operation below is preflighted. Merge from the end so
     * old physical descriptors/IDs are preserved when a smaller key arrives. */
    size_t old=o->count,new_count=count,destination=old+new_count;
    while(new_count) {
        --destination;
        if(old && key(&o->definitions[old-1])>key(&defs[new_count-1])) {
            --old;o->definitions[destination]=o->definitions[old];o->ids[destination]=o->ids[old];
        } else {
            --new_count;uint32_t k=key(&defs[new_count]);
            if(!entry(o->registry,k)) {
                status=ge_mp_registry_define(o->registry,k);if(status!=GE_MP_STATE_OK)return status;
            }
            status=ge_mp_registry_acquire(o->registry,k,&o->ids[destination]);if(status!=GE_MP_STATE_OK)return status;
            o->definitions[destination]=defs[new_count];
        }
    }
    o->count+=count;return GE_MP_STATE_OK;
}
#define F(id,t,min,max) {id,GE_MP_STATE_##t,0,min,max}
static const GeMpFieldSchema fields[]={F(256,U8,1,1),F(257,U16,0,1),
    {258,GE_MP_STATE_ACTOR_REF,GE_MP_FIELD_NULLABLE,4,5},F(259,I16,0,4),
    F(260,F32,0,1),F(261,I32,0,1),F(262,U8,1,1),F(263,U16,1,1)};
#undef F
static const GeMpRecordSchema schema={GE_MP_MODEL_TOPOLOGY_KIND,GE_MP_SYSTEM_ANIMATION,fields,GE_MP_MODEL_TOPOLOGY_FIELDS};
const GeMpRecordSchema *ge_mp_state_model_topology_schema(void){return &schema;}
static void reference(const GeMpStateModelTopologyOwner *o,const GeMpStateModelTopologyProviders *p,
    GeMpStateModelTopologySnapshot *out,GeMpModelTopologyReferenceKind kind,const void *native,uint64_t *id)
{
    if(!native)return;
    GeMpStateStatus status=GE_MP_STATE_INCOMPLETE;
    if(kind==GE_MP_TOPOLOGY_REF_NODE)for(size_t i=0;i<o->count;++i)
        if(o->definitions[i].native==native && o->definitions[i].component==GE_MP_TOPOLOGY_NODE) {
            *id=o->ids[i];status=GE_MP_STATE_OK;break;
        }
    if(status!=GE_MP_STATE_OK && p && p->reference)status=p->reference(p->context,kind,native,id);
    if(status!=GE_MP_STATE_OK || !*id || ge_mp_registry_resolve(o->registry,*id,NULL)!=GE_MP_STATE_OK) {
        *id=0;++out->coverage.unresolved_references;out->unresolved_kinds|=1U<<kind;
    }
}
GeMpStateStatus ge_mp_state_model_topology_export(const GeMpStateModelTopologyOwner *o,
    const GeMpStateModelTopologyProviders *p,GeMpStateModelTopologySnapshot *out)
{
    GeMpStateStatus status=valid(o);
    if(!out)return GE_MP_STATE_INVALID;memset(out,0,sizeof(*out));
    if(status!=GE_MP_STATE_OK){out->coverage.unclassified_fields=1;return status;}
    for(size_t i=0;i<o->count;++i) {
        const GeMpModelTopologyDefinition *d=&o->definitions[i];GeMpModelTopologyValues *v=&out->values[i];
        if(ge_mp_registry_resolve(o->registry,o->ids[i],NULL)!=GE_MP_STATE_OK) {++out->coverage.unclassified_fields;continue;}
        unsigned is_node=d->component==GE_MP_TOPOLOGY_NODE,ref_count=5,loaded_count=0;
        v->component=d->component;v->creation_namespace=d->creation_namespace;v->slot=d->slot;
        if(is_node) {
            const ModelNode *n=d->native;v->opcode=n->Opcode;
            reference(o,p,out,GE_MP_TOPOLOGY_REF_RODATA,n->Data,&v->refs[0]);
            reference(o,p,out,GE_MP_TOPOLOGY_REF_NODE,n->Parent,&v->refs[1]);
            reference(o,p,out,GE_MP_TOPOLOGY_REF_NODE,n->Next,&v->refs[2]);
            reference(o,p,out,GE_MP_TOPOLOGY_REF_NODE,n->Prev,&v->refs[3]);
            reference(o,p,out,GE_MP_TOPOLOGY_REF_NODE,n->Child,&v->refs[4]);
        } else {
            const ModelFileHeader *h=d->native;v->counts[0]=h->numSwitches;v->counts[1]=h->numMatrices;
            v->counts[2]=h->numRecords;v->counts[3]=h->numtextures;v->radius=h->BoundingVolumeRadius;
            if(!isfinite(v->radius))++out->coverage.unclassified_fields;
            reference(o,p,out,GE_MP_TOPOLOGY_REF_NODE,h->RootNode,&v->refs[0]);
            reference(o,p,out,GE_MP_TOPOLOGY_REF_SKELETON,h->Skeleton,&v->refs[1]);
            reference(o,p,out,GE_MP_TOPOLOGY_REF_SWITCH_TABLE,h->Switches,&v->refs[2]);
            reference(o,p,out,GE_MP_TOPOLOGY_REF_TEXTURE_TABLE,h->Textures,&v->refs[3]);
#ifdef DEBUG
            reference(o,p,out,GE_MP_TOPOLOGY_REF_SWITCH_NAMES,h->SwitchNames,&v->refs[4]);
#else
            ref_count=4;
#endif
#ifndef VERSION_EU
            loaded_count=1;v->is_loaded=h->isLoaded;
#endif
        }
#define V(n,member,count) out->fields[i][n]=(GeMpStateField){fields[n].id,fields[n].type,count,&v->member}
        V(0,component,1);V(1,opcode,is_node);V(2,refs,ref_count);V(3,counts,is_node?0:4);
        V(4,radius,is_node?0:1);V(5,is_loaded,loaded_count);V(6,creation_namespace,1);V(7,slot,1);
#undef V
        out->records[i]=(GeMpStateRecord){o->ids[i],GE_MP_MODEL_TOPOLOGY_KIND,out->fields[i],GE_MP_MODEL_TOPOLOGY_FIELDS};
    }
    if(out->coverage.unclassified_fields || out->coverage.unresolved_references)return GE_MP_STATE_INCOMPLETE;
    out->record_count=o->count;out->coverage.enumerated_systems=GE_MP_SYSTEM_ANIMATION;return GE_MP_STATE_OK;
}

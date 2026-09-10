#include "ge_mp_state_models.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <math.h>
#include <string.h>
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(offsetof(Model,anim)==0x20 && sizeof(Model)==0xbc,"native Model prefix/full ABI");
#endif
size_t ge_mp_state_models_native_size(uint8_t extent)
{
    return extent==GE_MP_MODEL_PREFIX?offsetof(Model,anim):extent==GE_MP_MODEL_FULL?sizeof(Model):0;
}
static uint32_t key(const GeMpModelDefinition *d)
{return ge_mp_registry_key(GE_MP_MODELS_KIND,d->creation_namespace,d->slot);}
static int descriptor_valid(const GeMpModelDefinition *d)
{
    return d->model && !((uintptr_t)d->model%_Alignof(Model))
        && ge_mp_state_models_native_size(d->extent)
        && d->bytes==ge_mp_state_models_native_size(d->extent);
}
/* Validate ALL owned identities before cleanup can mutate the registry.
 * Native lifetime expiry is independent of malformed owner storage. */
static GeMpStateStatus shape(const GeMpStateModelsOwner *o)
{
    if(!o || !o->live)return GE_MP_STATE_STALE;
    if(o->live!=1 || !o->registry || !o->props || !o->pool_lifetime
            || o->count>GE_MP_MODELS_CAPACITY || !(uint32_t)o->pool_id
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
    for(size_t i=o->count;i<GE_MP_MODELS_CAPACITY;++i)
        if(o->ids[i])return GE_MP_STATE_INVALID;
    return GE_MP_STATE_OK;
}
static GeMpStateStatus valid(const GeMpStateModelsOwner *o)
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
GeMpStateStatus ge_mp_state_models_begin(GeMpStateModelsOwner *o,
    GeMpStatePropsOwner *props,const GeMpModelDefinition *defs,size_t count)
{
    GeMpStateStatus status;size_t missing=0;
    if(!o || !props || count>GE_MP_MODELS_CAPACITY || (count&&!defs))return GE_MP_STATE_INVALID;
    if(o->live)return GE_MP_STATE_STALE;
    status=ge_mp_state_props_check_lifetime(props);if(status!=GE_MP_STATE_OK)return status;
    GeMpRegistry *registry=props->registry;
    status=ge_mp_registry_resolve(registry,props->pool_id,NULL);if(status!=GE_MP_STATE_OK)return status;
    for(size_t i=0;i<count;++i) {
        if(!descriptor_valid(&defs[i])
                || (i && key(&defs[i-1])>=key(&defs[i])))return GE_MP_STATE_INVALID;
        for(size_t j=0;j<i;++j)if(defs[j].model==defs[i].model)return GE_MP_STATE_INVALID;
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
GeMpStateStatus ge_mp_state_models_end(GeMpStateModelsOwner *o)
{
    GeMpStateStatus structural=shape(o);if(structural!=GE_MP_STATE_OK)return structural;
    GeMpStateStatus result=valid(o);
    for(size_t i=0;i<o->count;++i) {
        GeMpStateStatus status=ge_mp_registry_release(o->registry,o->ids[i]);
        if(status!=GE_MP_STATE_OK)result=status;
    }
    memset(o,0,sizeof(*o));return result;
}
GeMpStateStatus ge_mp_state_models_actor(void *opaque,const void *definition,uint64_t *id)
{
    const GeMpStateModelsOwner *o=opaque;GeMpStateStatus status=valid(o);
    if(!id)return GE_MP_STATE_INVALID;if(status!=GE_MP_STATE_OK)return status;
    for(size_t i=0;i<o->count;++i)if(o->definitions[i].model==definition) {
        status=ge_mp_registry_resolve(o->registry,o->ids[i],NULL);
        if(status==GE_MP_STATE_OK)*id=o->ids[i];return status;
    }
    return GE_MP_STATE_INCOMPLETE;
}
#define F(id,t,min,max) {id,GE_MP_STATE_##t,0,min,max}
static const GeMpFieldSchema fields[]={F(256,U8,1,1),F(257,I16,2,2),F(258,F32,1,28),
    {259,GE_MP_STATE_ACTOR_REF,GE_MP_FIELD_NULLABLE,6,12},
    F(260,I8,0,4),F(261,I16,0,4),F(262,I32,0,1),F(263,U32,0,2),F(264,U8,1,1),F(265,U16,1,1)};
#undef F
static const GeMpRecordSchema schema={GE_MP_MODELS_KIND,GE_MP_SYSTEM_ANIMATION,fields,GE_MP_MODELS_FIELDS};
const GeMpRecordSchema *ge_mp_state_models_schema(void){return &schema;}
static void missing(GeMpStateModelsSnapshot *out,unsigned kind)
{++out->coverage.unresolved_references;out->unresolved_kinds|=1U<<kind;}
static void reference(const GeMpStateModelsOwner *o,const GeMpStateModelsProviders *p,
    GeMpStateModelsSnapshot *out,GeMpModelReferenceKind kind,const void *native,uint64_t *id)
{
    if(!native)return;
    GeMpStateStatus status=GE_MP_STATE_INCOMPLETE;
    if(kind==GE_MP_MODEL_REF_ATTACHED_MODEL)status=ge_mp_state_models_actor((void *)o,native,id);
    if(status!=GE_MP_STATE_OK && p && p->reference)status=p->reference(p->context,kind,native,id);
    if(status!=GE_MP_STATE_OK || !*id || ge_mp_registry_resolve(o->registry,*id,NULL)!=GE_MP_STATE_OK)
        {*id=0;missing(out,kind);}
}
static void frame(const GeMpStateModelsOwner *o,const GeMpStateModelsProviders *p,
    GeMpStateModelsSnapshot *out,GeMpModelReferenceKind kind,const Model *model,int32_t word,uint64_t *id)
{
    if(!word)return;
    if(!p || !p->frame_reference || p->frame_reference(p->context,kind,model,word,id)!=GE_MP_STATE_OK
            || !*id || ge_mp_registry_resolve(o->registry,*id,NULL)!=GE_MP_STATE_OK)
        {*id=0;missing(out,kind);}
}
/* Declaration order, excluding prefix scale, pointers/frame words and callbacks. */
#define MODEL_FLOATS(X) \
    X(1,animframe1) X(2,unk2c) X(3,endframe) X(4,speed) X(5,newspeed) X(6,oldspeed) \
    X(7,timespeed) X(8,elapsespeed) X(9,animframe2) X(10,unk5c) X(11,unk6c) \
    X(12,speed2) X(13,unk74) X(14,unk78) X(15,unk7c) X(16,unk80) X(17,unk84) \
    X(18,unk88) X(19,unk8c) X(20,animloopframe) X(21,animloopmerge) \
    X(22,playspeed) X(23,animrate) X(24,unkac) X(25,unkb0) X(26,unkb4) X(27,anim_translation_scale)
GeMpStateStatus ge_mp_state_models_export(const GeMpStateModelsOwner *o,
    const GeMpStateModelsProviders *p,GeMpStateModelsSnapshot *out)
{
    GeMpStateStatus status=valid(o);
    if(!out)return GE_MP_STATE_INVALID;memset(out,0,sizeof(*out));
    if(status!=GE_MP_STATE_OK){out->coverage.unclassified_fields=1;return status;}
    for(size_t i=0;i<o->count;++i) {
        const GeMpModelDefinition *d=&o->definitions[i];const Model *m=d->model;
        GeMpModelValues *v=&out->values[i];int full=d->extent==GE_MP_MODEL_FULL;
        if(ge_mp_registry_resolve(o->registry,o->ids[i],NULL)!=GE_MP_STATE_OK)
            {++out->coverage.unclassified_fields;continue;}
        v->extent=d->extent;v->creation_namespace=d->creation_namespace;v->slot=d->slot;
        v->prefix[0]=m->unk00;v->prefix[1]=m->rwdatalen;v->floats[0]=m->scale;
        reference(o,p,out,GE_MP_MODEL_REF_CHARACTER,m->chr,&v->refs[0]);
        reference(o,p,out,GE_MP_MODEL_REF_HEADER,m->obj,&v->refs[1]);
        reference(o,p,out,GE_MP_MODEL_REF_MATRICES,m->render_pos,&v->refs[2]);
        reference(o,p,out,GE_MP_MODEL_REF_RW,m->datas,&v->refs[3]);
        reference(o,p,out,GE_MP_MODEL_REF_ATTACHED_MODEL,m->attachedto,&v->refs[4]);
        reference(o,p,out,GE_MP_MODEL_REF_ATTACHED_NODE,m->attachedto_objinst,&v->refs[5]);
        if(full) {
#define COPY(n,name) v->floats[n]=m->name;
            MODEL_FLOATS(COPY)
#undef COPY
            v->flags[0]=m->gunhand;v->flags[1]=m->unk25;v->flags[2]=m->animlooping;v->flags[3]=m->unk27;
            v->frames[0]=m->framea;v->frames[1]=m->frameb;v->frames[2]=m->frame2a;v->frames[3]=m->frame2b;
            v->unk9c=m->unk9c;
            reference(o,p,out,GE_MP_MODEL_REF_ANIMATION,m->anim,&v->refs[6]);
            reference(o,p,out,GE_MP_MODEL_REF_ANIMATION2,m->anim2,&v->refs[7]);
            frame(o,p,out,GE_MP_MODEL_REF_FRAME_A,m,m->unk34,&v->refs[8]);
            frame(o,p,out,GE_MP_MODEL_REF_FRAME_B,m,m->unk38,&v->refs[9]);
            frame(o,p,out,GE_MP_MODEL_REF_FRAME2_A,m,m->unk64,&v->refs[10]);
            frame(o,p,out,GE_MP_MODEL_REF_FRAME2_B,m,m->unk68,&v->refs[11]);
            GeOriginalModelRootCallback root=NULL;GeOriginalModelFlipCallback flip=NULL;
            if(!ge_original_model_root_callback_read(m,&root))missing(out,GE_MP_MODEL_REF_ROOT_CALLBACK);
            else if(root && (!p || !p->root_role || p->root_role(p->context,root,&v->callbacks[0])!=GE_MP_STATE_OK
                    || !v->callbacks[0])){v->callbacks[0]=0;missing(out,GE_MP_MODEL_REF_ROOT_CALLBACK);}
            if(!ge_original_player_gait_anim_flip_callback(m,&flip))missing(out,GE_MP_MODEL_REF_FLIP_CALLBACK);
            else if(flip && (!p || !p->flip_role || p->flip_role(p->context,flip,&v->callbacks[1])!=GE_MP_STATE_OK
                    || !v->callbacks[1])){v->callbacks[1]=0;missing(out,GE_MP_MODEL_REF_FLIP_CALLBACK);}
        }
        for(unsigned n=0;n<(full?28U:1U);++n)
            if(!isfinite(v->floats[n]))++out->coverage.unclassified_fields;
#define V(n,member,count) out->fields[i][n]=(GeMpStateField){fields[n].id,fields[n].type,count,&v->member}
        V(0,extent,1);V(1,prefix,2);V(2,floats,full?28:1);V(3,refs,full?12:6);
        V(4,flags,full?4:0);V(5,frames,full?4:0);V(6,unk9c,full?1:0);V(7,callbacks,full?2:0);
        V(8,creation_namespace,1);V(9,slot,1);
#undef V
        out->records[i]=(GeMpStateRecord){o->ids[i],GE_MP_MODELS_KIND,out->fields[i],GE_MP_MODELS_FIELDS};
    }
    if(out->coverage.unclassified_fields || out->coverage.unresolved_references)return GE_MP_STATE_INCOMPLETE;
    out->record_count=o->count;out->coverage.enumerated_systems=GE_MP_SYSTEM_ANIMATION;return GE_MP_STATE_OK;
}
#undef MODEL_FLOATS

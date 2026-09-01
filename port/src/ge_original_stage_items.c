#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_default_object_internal.h"
#include "ge_original_stage_guard_actor.h"
#include "ge_original_stage_items.h"

static int item_type(uint8_t type)
{
    return type==PROPDEF_KEY||type==PROPDEF_COLLECTABLE||type==PROPDEF_HAT;
}

static int embedded_object_type(uint8_t type)
{
    /* domakedefaultobj's INSIDEANOTHEROBJ branch is shared by ordinary
     * PROP/GLASS records, setup items, and authored magazine supplies. All
     * are reparented by the same second proplvreset2 pass and must not be
     * approximated as room roots. */
    return type==PROPDEF_PROP||type==PROPDEF_GLASS
        ||item_type(type)||type==PROPDEF_MAGAZINE;
}

GeOriginalStageItemStatus ge_original_stage_item_construct_standard_exact(
    const GeOriginalStagePropConstructionRequest *request,void *definition,
    void *opaque_prop,size_t prop_size,
    const GeOriginalDefaultObjectProviders *providers,
    GeOriginalDefaultObjectPrepared *prepared)
{
    ObjectRecord *object=definition;PropRecord *prop=opaque_prop;
    GeOriginalDefaultObjectStatus status;
    if(request==NULL||request->record==NULL||definition==NULL||prop==NULL
            ||providers==NULL||prepared==NULL||!item_type(request->record->type))
        return GE_ORIGINAL_STAGE_ITEM_INVALID_ARGUMENT;
    if(request->pad_id<0
            ||(request->record->words[2]&(PROPFLAG_ASSIGNEDTOCHR
                |PROPFLAG_INSIDEANOTHEROBJ))!=0U)
        return GE_ORIGINAL_STAGE_ITEM_INVALID_BRANCH;
    if(!ge_original_stage_prop_native_bind_prop(
            request,definition,prop,prop_size))
        return GE_ORIGINAL_STAGE_ITEM_PLACEMENT_FAILED;
    ge_original_default_object_bind(providers,prepared);
    status=ge_original_default_object_construct_standard(
        definition,(int32_t)request->command_index);
    if(status!=GE_ORIGINAL_DEFAULT_OBJECT_OK)
        return status==GE_ORIGINAL_DEFAULT_OBJECT_MODEL_UNAVAILABLE
            ?GE_ORIGINAL_STAGE_ITEM_MODEL_UNAVAILABLE
            :GE_ORIGINAL_STAGE_ITEM_CONSTRUCTION_FAILED;
    status=ge_original_default_object_place_standard(definition);
    if(status!=GE_ORIGINAL_DEFAULT_OBJECT_OK)
        return GE_ORIGINAL_STAGE_ITEM_PLACEMENT_FAILED;
    if(request->record->type==PROPDEF_COLLECTABLE){
        prop->type=PROP_TYPE_WEAPON;
        ge_original_stage_guard_actor_set_gunfire_visible(prop,0);
    }
    return object->prop==prop&&object->model!=NULL
        ?GE_ORIGINAL_STAGE_ITEM_OK:GE_ORIGINAL_STAGE_ITEM_CONSTRUCTION_FAILED;
}

GeOriginalStageItemStatus ge_original_stage_item_construct_embedded_exact(
    const GeOriginalStagePropConstructionRequest *request,void *definition,
    void *opaque_prop,size_t prop_size,GeOriginalPitemModelProvider *models,
    void *opaque_owner_prop,void *collision_data,void **model_instance)
{
    ObjectRecord *object=definition;PropRecord *prop=opaque_prop;
    PropRecord *owner=opaque_owner_prop;void *header=NULL,*opaque_model=NULL;
    Model *model;float scale=0.0f;uint16_t extrascale;
    if(request==NULL||request->record==NULL||definition==NULL||prop==NULL
            ||prop_size<sizeof(*prop)||models==NULL||owner==NULL
            ||model_instance==NULL||!embedded_object_type(
                request->record->type))
        return GE_ORIGINAL_STAGE_ITEM_INVALID_ARGUMENT;
    if((request->record->words[2]&PROPFLAG_INSIDEANOTHEROBJ)==0U
            ||(request->record->words[2]&PROPFLAG_ASSIGNEDTOCHR)!=0U)
        return GE_ORIGINAL_STAGE_ITEM_INVALID_BRANCH;
    if(!ge_original_pitem_model_resolve_instance(models,request->model_id,
            &header,&opaque_model,&scale)||header==NULL||opaque_model==NULL)
        return GE_ORIGINAL_STAGE_ITEM_MODEL_UNAVAILABLE;
    model=opaque_model;
    if(ge_original_objInitPreallocatedSlice(object,header,prop,model,scale,
            collision_data)==NULL)
        return GE_ORIGINAL_STAGE_ITEM_CONSTRUCTION_FAILED;
    extrascale=(uint16_t)(request->record->words[0]>>16U);
    model->scale*=((float)extrascale/256.0f);
    if(request->record->type==PROPDEF_COLLECTABLE){
        prop->type=PROP_TYPE_WEAPON;
        ge_original_stage_guard_actor_set_gunfire_visible(prop,0);
    }
    object->runtime_bitflags|=RUNTIMEBITFLAG_HASOWNER;
    ge_original_stage_guard_actor_reparent_prop(prop,owner);
    if(prop->parent!=owner){
        (void)ge_original_pitem_model_release_instance(models,model);
        object->model=NULL;object->prop=NULL;
        return GE_ORIGINAL_STAGE_ITEM_OWNER_UNAVAILABLE;
    }
    *model_instance=model;
    return GE_ORIGINAL_STAGE_ITEM_OK;
}

GeOriginalStageItemStatus ge_original_stage_item_construct_assigned_exact(
    const GeOriginalStagePropConstructionRequest *request,void *definition,
    void *opaque_prop,size_t prop_size,GeOriginalPitemModelProvider *models,
    void *opaque_character_prop,void *collision_data,void **model_instance)
{
    ObjectRecord *object=definition;PropRecord *prop=opaque_prop;
    PropRecord *owner=opaque_character_prop;void *header=NULL,*opaque_model=NULL;
    Model *model;float scale=0.0f;uint16_t extrascale;
    if(request==NULL||request->record==NULL||definition==NULL||prop==NULL
            ||prop_size<sizeof(*prop)||models==NULL||owner==NULL
            ||model_instance==NULL
            ||(request->record->type!=PROPDEF_PROP
                &&request->record->type!=PROPDEF_GLASS))
        return GE_ORIGINAL_STAGE_ITEM_INVALID_ARGUMENT;
    if((request->record->words[2]&PROPFLAG_ASSIGNEDTOCHR)==0U
            ||(request->record->words[2]&PROPFLAG_INSIDEANOTHEROBJ)!=0U)
        return GE_ORIGINAL_STAGE_ITEM_INVALID_BRANCH;
    if(!ge_original_pitem_model_resolve_instance(models,request->model_id,
            &header,&opaque_model,&scale)||header==NULL||opaque_model==NULL)
        return GE_ORIGINAL_STAGE_ITEM_MODEL_UNAVAILABLE;
    model=opaque_model;
    if(ge_original_objInitPreallocatedSlice(object,header,prop,model,scale,
            collision_data)==NULL)
        return GE_ORIGINAL_STAGE_ITEM_CONSTRUCTION_FAILED;
    extrascale=(uint16_t)(request->record->words[0]>>16U);
    model->scale*=((float)extrascale/256.0f);
    ge_original_stage_guard_actor_reparent_prop(prop,owner);
    if(prop->parent!=owner){
        (void)ge_original_pitem_model_release_instance(models,model);
        object->model=NULL;object->prop=NULL;
        return GE_ORIGINAL_STAGE_ITEM_OWNER_UNAVAILABLE;
    }
    *model_instance=model;
    return GE_ORIGINAL_STAGE_ITEM_OK;
}

const char *ge_original_stage_item_status_name(GeOriginalStageItemStatus status)
{
    switch(status){
    case GE_ORIGINAL_STAGE_ITEM_OK:return "ok";
    case GE_ORIGINAL_STAGE_ITEM_INVALID_ARGUMENT:return "invalid argument";
    case GE_ORIGINAL_STAGE_ITEM_INVALID_BRANCH:return "invalid item branch";
    case GE_ORIGINAL_STAGE_ITEM_MODEL_UNAVAILABLE:return "model unavailable";
    case GE_ORIGINAL_STAGE_ITEM_CONSTRUCTION_FAILED:return "construction failed";
    case GE_ORIGINAL_STAGE_ITEM_PLACEMENT_FAILED:return "placement failed";
    case GE_ORIGINAL_STAGE_ITEM_OWNER_UNAVAILABLE:return "owner unavailable";
    default:return "unknown";
    }
}

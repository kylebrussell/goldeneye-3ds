#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_supplies.h"

#include <string.h>

static int ge_supply_type(uint8_t type)
{
    return type==PROPDEF_MAGAZINE||type==PROPDEF_AMMO||type==PROPDEF_ARMOUR;
}

GeOriginalStageSupplyStatus ge_original_stage_supply_construct_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition,size_t definition_size,void *opaque_prop,size_t prop_size,
    const GeOriginalStageSupplyProviders *providers,
    GeOriginalStageSupplyInstance *instance)
{
    ObjectRecord *object=definition;PropRecord *prop=opaque_prop;
    GeOriginalDefaultObjectStatus object_status;size_t slot_loads=0U;
    int32_t player_count;
    if(request==NULL||request->record==NULL||definition==NULL||prop==NULL
            ||providers==NULL||providers->default_object==NULL
            ||providers->prepared==NULL||instance==NULL
            ||!ge_supply_type(request->record->type))
        return GE_ORIGINAL_STAGE_SUPPLY_INVALID_ARGUMENT;
    memset(instance,0,sizeof(*instance));
    if(request->pad_id<0
            ||(request->flags&(PROPFLAG_INSIDEANOTHEROBJ
                               |PROPFLAG_ASSIGNEDTOCHR))!=0U)
        return GE_ORIGINAL_STAGE_SUPPLY_UNSUPPORTED_OWNERSHIP;
    if(definition_size!=ge_original_stage_prop_native_definition_size(request)
            ||!ge_original_stage_prop_native_definition_init(
                request,definition,definition_size)
            ||!ge_original_stage_prop_native_bind_prop(
                request,definition,prop,prop_size))
        return GE_ORIGINAL_STAGE_SUPPLY_INVALID_DEFINITION;
    if(providers->default_object->get_player_count==NULL)
        return GE_ORIGINAL_STAGE_SUPPLY_INVALID_ARGUMENT;
    player_count=providers->default_object->get_player_count(
        providers->default_object->context);
    if(player_count<1||player_count>4)
        return GE_ORIGINAL_STAGE_SUPPLY_INVALID_ARGUMENT;
    if(request->record->type==PROPDEF_ARMOUR){
        BodyArmourRecord *armour=definition;int32_t fixed;
        memcpy(&fixed,&armour->initialamount,sizeof(fixed));
        armour->initialamount=(float)fixed/65535.0f;
        armour->amount=armour->initialamount;
    }else if(request->record->type==PROPDEF_AMMO){
        MultiAmmoCrateRecord *ammo=definition;size_t slot;int32_t ammoqty=1;
        if(player_count>=2){
            int32_t ammo_type;
            if(providers->get_multiplayer_ammo==NULL)
                return GE_ORIGINAL_STAGE_SUPPLY_MULTIPLAYER_DEPENDENCY_UNAVAILABLE;
            if(!providers->get_multiplayer_ammo(
                    providers->default_object->context,&ammo_type,&ammoqty)
                    ||ammo_type<=AMMO_NONE||ammo_type>AMMOTYPE_GLOBAL_MAX)
                return GE_ORIGINAL_STAGE_SUPPLY_MULTIPLAYER_AMMO_INVALID;
            ammo->slots[ammo_type-1].quantity=(uint16_t)ammoqty;
        }
        if(ammoqty<=0)return GE_ORIGINAL_STAGE_SUPPLY_CANONICAL_NO_OBJECT;
        if(providers->default_object->model_load==NULL)
            return GE_ORIGINAL_STAGE_SUPPLY_INVALID_ARGUMENT;
        for(slot=0U;slot<AMMOTYPE_GLOBAL_MAX;++slot){
            if(ammo->slots[slot].quantity>0U
                    &&ammo->slots[slot].modelnum!=UINT16_MAX){
                if(!providers->default_object->model_load(
                        providers->default_object->context,
                        ammo->slots[slot].modelnum))
                    return GE_ORIGINAL_STAGE_SUPPLY_SLOT_MODEL_UNAVAILABLE;
                ++slot_loads;
            }
        }
    }
    ge_original_default_object_bind(
        providers->default_object,providers->prepared);
    object_status=ge_original_default_object_construct_standard(
        definition,(int32_t)request->command_index);
    if(object_status!=GE_ORIGINAL_DEFAULT_OBJECT_OK)
        return GE_ORIGINAL_STAGE_SUPPLY_CONSTRUCTION_FAILED;
    object_status=ge_original_default_object_place_standard(definition);
    if(object_status!=GE_ORIGINAL_DEFAULT_OBJECT_OK)
        return GE_ORIGINAL_STAGE_SUPPLY_PLACEMENT_FAILED;
    if(providers->update_room_position==NULL||providers->activate_prop==NULL
            ||providers->enable_prop==NULL)
        return GE_ORIGINAL_STAGE_SUPPLY_ACTIVATION_DEPENDENCY_UNAVAILABLE;
    if(!providers->update_room_position(
            providers->default_object->context,definition)
            ||!providers->activate_prop(
                providers->default_object->context,object->prop)
            ||!providers->enable_prop(
                providers->default_object->context,object->prop))
        return GE_ORIGINAL_STAGE_SUPPLY_ACTIVATION_FAILED;
    instance->definition=definition;instance->prop=object->prop;
    instance->model=object->model;instance->command_index=request->command_index;
    instance->slot_model_loads=slot_loads;instance->type=request->record->type;
    instance->constructed=1U;instance->activated=1U;
    return GE_ORIGINAL_STAGE_SUPPLY_OK;
}

const char *ge_original_stage_supply_status_name(
    GeOriginalStageSupplyStatus status)
{
    switch(status){
    case GE_ORIGINAL_STAGE_SUPPLY_OK:return "ok";
    case GE_ORIGINAL_STAGE_SUPPLY_INVALID_ARGUMENT:return "invalid argument";
    case GE_ORIGINAL_STAGE_SUPPLY_INVALID_DEFINITION:return "invalid definition";
    case GE_ORIGINAL_STAGE_SUPPLY_UNSUPPORTED_OWNERSHIP:return "unsupported ownership";
    case GE_ORIGINAL_STAGE_SUPPLY_MULTIPLAYER_DEPENDENCY_UNAVAILABLE:return "multiplayer dependency unavailable";
    case GE_ORIGINAL_STAGE_SUPPLY_MULTIPLAYER_AMMO_INVALID:return "invalid multiplayer ammo";
    case GE_ORIGINAL_STAGE_SUPPLY_SLOT_MODEL_UNAVAILABLE:return "slot model unavailable";
    case GE_ORIGINAL_STAGE_SUPPLY_CANONICAL_NO_OBJECT:return "canonical no object";
    case GE_ORIGINAL_STAGE_SUPPLY_CONSTRUCTION_FAILED:return "default-object construction failed";
    case GE_ORIGINAL_STAGE_SUPPLY_PLACEMENT_FAILED:return "default-object placement failed";
    case GE_ORIGINAL_STAGE_SUPPLY_ACTIVATION_DEPENDENCY_UNAVAILABLE:return "activation dependency unavailable";
    case GE_ORIGINAL_STAGE_SUPPLY_ACTIVATION_FAILED:return "activation failed";
    default:return "unknown";
    }
}

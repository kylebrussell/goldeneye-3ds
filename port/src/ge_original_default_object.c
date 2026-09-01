#include "ge_original_default_object.h"
#include "ge_original_default_object_internal.h"

#include <string.h>

#undef modelLoad
#undef getPlayerCount
#undef get_scenario

static GeOriginalDefaultObjectProviders ge_object_providers;
static GeOriginalDefaultObjectPrepared *ge_object_prepared;

static int ge_default_object_definition_header(
    const void *definition, uint16_t *extrascale,
    uint8_t *state, uint8_t *type)
{
    const PropDefHeaderRecord *header = definition;
    if (ge_dam_setup_world_definition_header(
            definition, extrascale, state, type)) return 1;
    if (header == NULL) return 0;
    if (extrascale != NULL) *extrascale = header->extrascale;
    if (state != NULL) *state = header->state;
    if (type != NULL) *type = header->type;
    return 1;
}

void ge_original_default_object_bind(
    const GeOriginalDefaultObjectProviders *providers,
    GeOriginalDefaultObjectPrepared *prepared)
{
    memset(&ge_object_providers, 0, sizeof(ge_object_providers));
    if (providers != NULL) {
        ge_object_providers = *providers;
    }
    ge_object_prepared = prepared;
    if (prepared != NULL) {
        memset(prepared, 0, sizeof(*prepared));
        prepared->pad_id = -1;
    }
}

GeOriginalDefaultObjectStatus ge_original_default_object_prepare_standard(
    void *object_definition, int32_t command_index)
{
    ObjectRecord *object = object_definition;
    uint8_t definition_type;

    if (object == NULL || ge_object_prepared == NULL || command_index < 0) {
        return GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
    }
    if (ge_object_providers.model_load == NULL
            || ge_object_providers.get_player_count == NULL
            || ge_object_providers.get_scenario == NULL) {
        return GE_ORIGINAL_DEFAULT_OBJECT_MISSING_PROVIDER;
    }
    if (!ge_default_object_definition_header(object, NULL, NULL,
                                              &definition_type)) {
        return GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
    }
    /* setupSingleMonitor's non-embedded branch is exactly domakedefaultobj;
     * Dam command 290 uses that positive bound-pad branch. */
    if (definition_type != PROPDEF_PROP && definition_type != PROPDEF_GLASS
            && definition_type != PROPDEF_MONITOR
            && definition_type != PROPDEF_MULTI_MONITOR
            && definition_type != PROPDEF_TINTED_GLASS
            && definition_type != PROPDEF_KEY
            && definition_type != PROPDEF_COLLECTABLE
            && definition_type != PROPDEF_HAT
            && definition_type != PROPDEF_CCTV
            && definition_type != PROPDEF_AUTOGUN
            && definition_type != PROPDEF_MAGAZINE
            && definition_type != PROPDEF_AMMO
            && definition_type != PROPDEF_ARMOUR
            && definition_type != PROPDEF_ALARM
            && definition_type != PROPDEF_RACK
            && definition_type != PROPDEF_GAS_RELEASING
            && definition_type != PROPDEF_VEHICHLE
            && definition_type != PROPDEF_AIRCRAFT
            && definition_type != PROPDEF_SAFE
            && definition_type != PROPDEF_TANK) {
        return GE_ORIGINAL_DEFAULT_OBJECT_UNSUPPORTED_BRANCH;
    }
    if ((object->flags & (PROPFLAG_INSIDEANOTHEROBJ
                          | PROPFLAG_ASSIGNEDTOCHR)) != 0U
            ) {
        return GE_ORIGINAL_DEFAULT_OBJECT_UNSUPPORTED_BRANCH;
    }
    if (object->pad < 0
            || (isNotBoundPad(object->pad) && g_CurrentSetup.pads == NULL)
            || (object->pad >= 10000
                && g_CurrentSetup.boundpads == NULL)) {
        return GE_ORIGINAL_DEFAULT_OBJECT_INVALID_PAD;
    }
    if (!ge_original_domakedefaultobj_standard_prefix_slice(
            33, object, command_index)) {
        return GE_ORIGINAL_DEFAULT_OBJECT_INVALID_PAD;
    }
    return GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

GeOriginalDefaultObjectStatus ge_original_default_object_construct_standard(
    void *object_definition, int32_t command_index)
{
    ObjectRecord *object = object_definition;
    ModelFileHeader *model_header = NULL;
    Model *model = NULL;
    PropRecord *prop;
    coord3d position;
    coord3d validated_position;
    StandTile *validated_stan;
    void *collision_data = NULL;
    float pitem_scale = 0.0f;
    uint8_t definition_type=PROPDEF_END;
    GeOriginalDefaultObjectStatus status;

    status = ge_original_default_object_prepare_standard(
        object_definition, command_index);
    if (status != GE_ORIGINAL_DEFAULT_OBJECT_OK) return status;
    if (ge_object_providers.resolve_model_instance == NULL) {
        return GE_ORIGINAL_DEFAULT_OBJECT_MISSING_PROVIDER;
    }

    memcpy(position.f, ge_object_prepared->position, sizeof(position.f));
    if (!ge_original_getposstan_zero_radius_slice(
            &position, (StandTile *)ge_object_prepared->stan,
            &validated_position, &validated_stan)) {
        return GE_ORIGINAL_DEFAULT_OBJECT_POSITION_FAILED;
    }
    ge_object_prepared->position_validated = 1;

    if (!ge_object_providers.resolve_model_instance(
            ge_object_providers.context, object->obj,
            (void **)&model_header, (void **)&model, &pitem_scale)
            || model_header == NULL || model == NULL
            || model->obj != model_header) {
        return GE_ORIGINAL_DEFAULT_OBJECT_MODEL_UNAVAILABLE;
    }
    if ((object->flags & PROPFLAG_00000100) != 0U) {
        if (ge_object_providers.allocate_collision == NULL) {
            return GE_ORIGINAL_DEFAULT_OBJECT_MISSING_PROVIDER;
        }
        collision_data = ge_object_providers.allocate_collision(
            ge_object_providers.context, 0x50U);
        if (collision_data == NULL) {
            return GE_ORIGINAL_DEFAULT_OBJECT_COLLISION_ALLOCATION_FAILED;
        }
    }
    prop = ge_original_objInitPreallocatedSlice(
        object, model_header, object->prop, model, pitem_scale,
        collision_data);
    if (prop == NULL) return GE_ORIGINAL_DEFAULT_OBJECT_INIT_FAILED;
    if(!ge_default_object_definition_header(object,NULL,NULL,&definition_type))
        return GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
    /* weaponAssignToHome's unassigned branch enters domakedefaultobj, whose
     * type-8 path is sub_GAME_7F051DD8: objInit followed by PROP_TYPE_WEAPON
     * and hidden gunfire relations. The Pitem instance provider initializes
     * both authored gunfire RW relations hidden, so only this exact prop-kind
     * publication remains here. */
    if(definition_type==PROPDEF_COLLECTABLE)prop->type=PROP_TYPE_WEAPON;

    if (ge_object_prepared->bound_pad
            && !ge_original_bound_pad_scale_slice(
                object, (Mtxf *)ge_object_prepared->matrix))
        return GE_ORIGINAL_DEFAULT_OBJECT_INIT_FAILED;
    model->scale *= ge_object_prepared->extra_scale;
    matrix_scalar_multiply(model->scale, ge_object_prepared->matrix[0]);
    ge_object_prepared->model_header = model_header;
    ge_object_prepared->model_instance = model;
    ge_object_prepared->prop = prop;
    ge_object_prepared->collision_data = collision_data;
    ge_object_prepared->pitem_scale = pitem_scale;
    ge_object_prepared->object_initialized = 1;
    return GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

GeOriginalDefaultObjectStatus ge_original_default_object_place_standard(
    void *object_definition)
{
    ObjectRecord *object = object_definition;
    coord3d position;
    coord3d pad_position;
    s32 result;

    if (object == NULL || ge_object_prepared == NULL)
        return GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
    if (!ge_object_prepared->object_initialized
            || ge_object_prepared->prop != object->prop
            || ge_object_prepared->model_instance != object->model)
        return GE_ORIGINAL_DEFAULT_OBJECT_NOT_CONSTRUCTED;
    memcpy(position.f, ge_object_prepared->position, sizeof(position.f));
    memcpy(pad_position.f, ge_object_prepared->position,
           sizeof(pad_position.f));
    if (object->flags & PROPFLAG_ONSCREEN) {
        result = ge_original_move_onscreen_to_pad_slice(
            object, &position, (Mtxf *)ge_object_prepared->matrix,
            (StandTile *)ge_object_prepared->stan, &pad_position);
    } else {
        result = ge_original_move_to_pad_slice(
            object, &position, (Mtxf *)ge_object_prepared->matrix,
            (StandTile *)ge_object_prepared->stan, &pad_position);
    }
    if (result < 0)
        return GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_DEPENDENCY_UNAVAILABLE;
    if (result == 0)
        return GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_FAILED;
    ge_object_prepared->placement_completed = 1;
    return GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

s32 ge_port_default_object_floor_y(StandTile *stan, f32 x, f32 z,
                                   f32 *floor_y)
{
    if (ge_object_providers.get_floor_y == NULL) return -1;
    return ge_object_providers.get_floor_y(
        ge_object_providers.context, stan, x, z, floor_y);
}

s32 ge_port_default_object_room_bounds(const coord3d *position, s32 room,
                                       f32 *top, f32 *bottom)
{
    if (ge_object_providers.get_room_object_bounds == NULL) return -1;
    return ge_object_providers.get_room_object_bounds(
        ge_object_providers.context, position->f, (int16_t)room, top, bottom);
}

s32 ge_port_default_object_walk(StandTile **stan, f32 start_x, f32 start_z,
                                f32 destination_x, f32 destination_z)
{
    if (ge_object_providers.walk_tiles == NULL) return -1;
    return ge_object_providers.walk_tiles(
        ge_object_providers.context, (void **)stan, start_x, start_z,
        destination_x, destination_z);
}

s32 ge_port_default_object_tile_rgb(StandTile *stan, f32 x, f32 z,
                                    u8 rgb[3])
{
    if (ge_object_providers.get_tile_rgb == NULL) return -1;
    return ge_object_providers.get_tile_rgb(
        ge_object_providers.context, stan, x, z, rgb);
}

void ge_port_default_object_publish_placement(const coord3d *position,
                                              StandTile *stan, u32 stage)
{
    memcpy(ge_object_prepared->placement_position, position->f,
           sizeof(ge_object_prepared->placement_position));
    ge_object_prepared->placement_stan = stan;
    ge_object_prepared->placement_stage = stage;
}

/* Canonical campaign-AI symbol. The unchanged placement arithmetic is owned
 * by ge_original_move_to_pad_slice; only world queries cross the typed 3DS
 * provider seam already used by ordinary authored object construction. */
void sub_GAME_7F04088C(ObjectRecord *baseobj, struct coord3d *pos,
                       Mtxf *matrix, StandTile *stan, struct coord3d *pos2)
{
    (void)ge_original_move_to_pad_slice(baseobj, pos, matrix, stan, pos2);
}

s32 ge_port_default_object_model_load(s32 model_id)
{
    s32 result = ge_object_providers.model_load(
        ge_object_providers.context, model_id);
    ge_object_prepared->model_load_calls++;
    return result;
}

s32 ge_port_default_object_player_count(void)
{
    return ge_object_providers.get_player_count(
        ge_object_providers.context);
}

s32 ge_port_default_object_scenario(void)
{
    return ge_object_providers.get_scenario(ge_object_providers.context);
}

u8 ge_port_default_object_type(ObjectRecord *object)
{
    u8 type = PROPDEF_END;
    ge_default_object_definition_header(object, NULL, NULL, &type);
    return type;
}

void ge_port_default_object_publish(ObjectRecord *object, s32 model_id,
                                    f32 extra_scale,
                                    const coord3d *position,
                                    const coord3d *shade_position,
                                    const Mtxf *matrix, StandTile *stan)
{
    ge_object_prepared->model_id = model_id;
    ge_object_prepared->pad_id = object->pad;
    ge_object_prepared->bound_pad = object->pad >= 10000;
    ge_object_prepared->state = ge_port_default_object_state(object);
    ge_object_prepared->flags = object->flags;
    ge_object_prepared->flags2 = object->flags2;
    ge_object_prepared->extra_scale = extra_scale;
    ge_object_prepared->damage = object->damage;
    memcpy(ge_object_prepared->position, position->f,
           sizeof(ge_object_prepared->position));
    memcpy(ge_object_prepared->shade_position, shade_position->f,
           sizeof(ge_object_prepared->shade_position));
    memcpy(ge_object_prepared->matrix, matrix->m,
           sizeof(ge_object_prepared->matrix));
    ge_object_prepared->stan = stan;
    ge_object_prepared->prepared = 1;
}

u16 ge_port_default_object_extrascale(ObjectRecord *object)
{
    uint16_t extrascale = 0U;
    (void)ge_default_object_definition_header(object, &extrascale,
                                               NULL, NULL);
    return (u16)extrascale;
}

u8 ge_port_default_object_state(ObjectRecord *object)
{
    uint8_t state = 0U;
    (void)ge_default_object_definition_header(object, NULL, &state, NULL);
    return (u8)state;
}

void ge_port_default_object_set_state(ObjectRecord *object, u8 state)
{
    if (!ge_dam_setup_world_definition_set_state(object, state)) {
        PropDefHeaderRecord *header = (PropDefHeaderRecord *)(void *)object;
        header->state = state;
    }
}

const char *ge_original_default_object_status_name(
    GeOriginalDefaultObjectStatus status)
{
    switch (status) {
    case GE_ORIGINAL_DEFAULT_OBJECT_OK: return "ok";
    case GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_DEFAULT_OBJECT_MISSING_PROVIDER: return "missing provider";
    case GE_ORIGINAL_DEFAULT_OBJECT_UNSUPPORTED_BRANCH: return "unsupported branch";
    case GE_ORIGINAL_DEFAULT_OBJECT_INVALID_PAD: return "invalid pad";
    case GE_ORIGINAL_DEFAULT_OBJECT_MODEL_UNAVAILABLE: return "model unavailable";
    case GE_ORIGINAL_DEFAULT_OBJECT_COLLISION_ALLOCATION_FAILED: return "collision allocation failed";
    case GE_ORIGINAL_DEFAULT_OBJECT_POSITION_FAILED: return "position failed";
    case GE_ORIGINAL_DEFAULT_OBJECT_INIT_FAILED: return "object init failed";
    case GE_ORIGINAL_DEFAULT_OBJECT_NOT_CONSTRUCTED: return "not constructed";
    case GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_DEPENDENCY_UNAVAILABLE: return "placement dependency unavailable";
    case GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_FAILED: return "placement failed";
    default: return "unknown";
    }
}

#include <ultra64.h>
#include <bondtypes.h>
#include "bondconstants.h"

#include "ge_original_stage_prop_materializer.h"
#include "ge_original_stage_special_objects.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static const uint8_t ge_special_types[GE_ORIGINAL_STAGE_SPECIAL_TYPE_COUNT] = {
    PROPDEF_ALARM,
    PROPDEF_CCTV,
    PROPDEF_MAGAZINE,
    PROPDEF_MONITOR,
    PROPDEF_MULTI_MONITOR,
    PROPDEF_RACK,
    PROPDEF_AUTOGUN,
    PROPDEF_AMMO,
    PROPDEF_ARMOUR,
    PROPDEF_GAS_RELEASING,
    PROPDEF_VEHICHLE,
    PROPDEF_AIRCRAFT,
    PROPDEF_UNK41,
    PROPDEF_SAFE,
    PROPDEF_TANK,
    PROPDEF_TINTED_GLASS,
    PROPDEF_SAFE_ITEM,
};

int ge_original_stage_special_type_index(uint8_t type)
{
    size_t index;
    for (index = 0U; index < GE_ORIGINAL_STAGE_SPECIAL_TYPE_COUNT; ++index)
        if (ge_special_types[index] == type) return (int)index;
    return -1;
}

const char *ge_original_stage_special_type_name(uint8_t type)
{
    switch (type) {
    case PROPDEF_ALARM: return "alarm";
    case PROPDEF_CCTV: return "cctv";
    case PROPDEF_MAGAZINE: return "magazine";
    case PROPDEF_MONITOR: return "monitor";
    case PROPDEF_MULTI_MONITOR: return "multi-monitor";
    case PROPDEF_RACK: return "rack";
    case PROPDEF_AUTOGUN: return "autogun";
    case PROPDEF_AMMO: return "multi-ammo";
    case PROPDEF_ARMOUR: return "armour";
    case PROPDEF_GAS_RELEASING: return "gas-releasing";
    case PROPDEF_VEHICHLE: return "vehicle";
    case PROPDEF_AIRCRAFT: return "aircraft";
    case PROPDEF_UNK41: return "unknown-41";
    case PROPDEF_SAFE: return "safe";
    case PROPDEF_TANK: return "tank";
    case PROPDEF_TINTED_GLASS: return "tinted-glass";
    case PROPDEF_SAFE_ITEM: return "safe-item-link";
    default: return "not-special";
    }
}

int ge_original_stage_special_audit_add(
    GeOriginalStageSpecialAudit *audit, GeStageId stage,
    const GeOriginalStageSetupRuntime *setup)
{
    size_t record_index;
    uint8_t stage_seen[GE_ORIGINAL_STAGE_SPECIAL_TYPE_COUNT] = {0};
    if (audit == NULL || setup == NULL || setup->loaded == 0U
            || (unsigned)stage >= GE_STAGE_COUNT) return 0;
    for (record_index = 0U; record_index < setup->prop_record_count;
            ++record_index) {
        const uint8_t type = setup->prop_records[record_index].type;
        const int type_index = ge_original_stage_special_type_index(type);
        GeOriginalStageSpecialTypeAudit *entry;
        if (type_index < 0) continue;
        entry = &audit->types[type_index];
        entry->type = type;
        ++entry->total;
        ++entry->by_stage[stage];
        ++audit->total;
        stage_seen[type_index] = 1U;
    }
    for (record_index = 0U;
            record_index < GE_ORIGINAL_STAGE_SPECIAL_TYPE_COUNT;
            ++record_index) {
        if (audit->types[record_index].type == 0U)
            audit->types[record_index].type = ge_special_types[record_index];
        audit->types[record_index].stage_count += stage_seen[record_index];
    }
    return 1;
}

static int ge_tinted_glass_bound_pad_centre(
    const GeOriginalStagePadPlacement *pad, float centre[3])
{
    float normal[3];
    float inverse_length;
    float length_squared;
    size_t axis;
    normal[0] = pad->up[1] * pad->look[2] - pad->up[2] * pad->look[1];
    normal[1] = pad->up[2] * pad->look[0] - pad->up[0] * pad->look[2];
    normal[2] = pad->up[0] * pad->look[1] - pad->up[1] * pad->look[0];
    length_squared = normal[0] * normal[0] + normal[1] * normal[1]
        + normal[2] * normal[2];
    if (!(length_squared > 0.0f) || !isfinite(length_squared)) return 0;
    inverse_length = 1.0f / sqrtf(length_squared);
    for (axis = 0U; axis < 3U; ++axis) normal[axis] *= inverse_length;
    /* This is the original padGetCentre axis permutation from prop.c. */
    centre[0] = pad->position[0] + (
        (pad->bounds[0] + pad->bounds[1]) * normal[0]
        + (pad->bounds[2] + pad->bounds[3]) * pad->up[0]
        + (pad->bounds[4] + pad->bounds[5]) * pad->look[0]) * 0.5f;
    centre[1] = pad->position[1] + (
        (pad->bounds[0] + pad->bounds[1]) * normal[1]
        + (pad->bounds[2] + pad->bounds[3]) * pad->up[1]
        + (pad->bounds[4] + pad->bounds[5]) * pad->look[1]) * 0.5f;
    centre[2] = pad->position[2] + (
        (pad->bounds[0] + pad->bounds[1]) * normal[2]
        + (pad->bounds[2] + pad->bounds[3]) * pad->up[2]
        + (pad->bounds[4] + pad->bounds[5]) * pad->look[2]) * 0.5f;
    return isfinite(centre[0]) && isfinite(centre[1]) && isfinite(centre[2]);
}

GeOriginalStageTintedGlassStatus ge_original_stage_tinted_glass_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageTintedGlassProviders *providers)
{
    TintedGlassRecord *glass = definition;
    if (request == NULL || request->record == NULL || definition == NULL
            || providers == NULL || providers->construct_standard == NULL
            || providers->place_standard == NULL)
        return GE_ORIGINAL_STAGE_TINTED_GLASS_INVALID_ARGUMENT;
    if (request->record->type != PROPDEF_TINTED_GLASS
            || definition_size != sizeof(*glass)
            || !ge_original_stage_prop_native_definition_init(
                request, definition, definition_size))
        return GE_ORIGINAL_STAGE_TINTED_GLASS_INVALID_DEFINITION;
    if (!request->placement_resolved || request->placement.stan == NULL)
        return GE_ORIGINAL_STAGE_TINTED_GLASS_PLACEMENT_UNRESOLVED;
    if ((glass->flags & PROPFLAG_GLASS_HASPORTAL) != 0U
            && glass->pad >= 10000) {
        float centre[3];
        float point_a[3];
        float point_b[3];
        int32_t fixed_opacity;
        size_t axis;
        if (!request->placement.is_bound_pad
                || providers->find_portal == NULL
                || !ge_tinted_glass_bound_pad_centre(
                    &request->placement, centre))
            return GE_ORIGINAL_STAGE_TINTED_GLASS_MISSING_PORTAL_SERVICE;
        for (axis = 0U; axis < 3U; ++axis) {
            point_a[axis] = centre[axis] - 10.0f * request->placement.up[axis];
            point_b[axis] = centre[axis] + 10.0f * request->placement.up[axis];
        }
        glass->portalnum = providers->find_portal(
            providers->context, point_a, point_b);
        memcpy(&fixed_opacity, &request->record->words[36],
               sizeof(fixed_opacity));
        glass->unk90 = (float)fixed_opacity / 65535.0f;
    }
    if (!providers->construct_standard(
            providers->context, definition,
            (int32_t)request->command_index))
        return GE_ORIGINAL_STAGE_TINTED_GLASS_CONSTRUCTION_FAILED;
    if (!providers->place_standard(providers->context, definition))
        return GE_ORIGINAL_STAGE_TINTED_GLASS_PLACEMENT_FAILED;
    return GE_ORIGINAL_STAGE_TINTED_GLASS_OK;
}

const char *ge_original_stage_tinted_glass_status_name(
    GeOriginalStageTintedGlassStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_TINTED_GLASS_OK: return "ok";
    case GE_ORIGINAL_STAGE_TINTED_GLASS_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_TINTED_GLASS_INVALID_DEFINITION:
        return "invalid definition";
    case GE_ORIGINAL_STAGE_TINTED_GLASS_PLACEMENT_UNRESOLVED:
        return "placement unresolved";
    case GE_ORIGINAL_STAGE_TINTED_GLASS_MISSING_PORTAL_SERVICE:
        return "missing portal service";
    case GE_ORIGINAL_STAGE_TINTED_GLASS_CONSTRUCTION_FAILED:
        return "construction failed";
    case GE_ORIGINAL_STAGE_TINTED_GLASS_PLACEMENT_FAILED:
        return "placement failed";
    default: return "unknown";
    }
}

static int ge_misc_type(uint8_t type)
{
    return type == PROPDEF_ALARM || type == PROPDEF_RACK
        || type == PROPDEF_GAS_RELEASING || type == PROPDEF_VEHICHLE
        || type == PROPDEF_AIRCRAFT || type == PROPDEF_SAFE
        || type == PROPDEF_TANK;
}

uint32_t ge_original_stage_misc_runtime_dependencies(uint8_t type)
{
    const uint32_t common = GE_ORIGINAL_STAGE_MISC_DEP_DEFAULT_OBJECT
        | GE_ORIGINAL_STAGE_MISC_DEP_PROP_PUBLICATION;
    switch (type) {
    case PROPDEF_ALARM:
        return common | GE_ORIGINAL_STAGE_MISC_DEP_ALARM_INTERACTION;
    case PROPDEF_RACK:
        return common | GE_ORIGINAL_STAGE_MISC_DEP_RACK_MATRICES;
    case PROPDEF_GAS_RELEASING:
        return common | GE_ORIGINAL_STAGE_MISC_DEP_GAS_DAMAGE_EFFECT;
    case PROPDEF_SAFE:
        return common | GE_ORIGINAL_STAGE_MISC_DEP_SAFE_RELATION;
    case PROPDEF_VEHICHLE: case PROPDEF_AIRCRAFT:
        return common | GE_ORIGINAL_STAGE_MISC_DEP_AI_LIST
            | GE_ORIGINAL_STAGE_MISC_DEP_OBJECT_ACTION_TICK
            | GE_ORIGINAL_STAGE_MISC_DEP_MOVING_MODEL_MATRICES
            | GE_ORIGINAL_STAGE_MISC_DEP_OBJECT_AUDIO;
    case PROPDEF_TANK:
        return common | GE_ORIGINAL_STAGE_MISC_DEP_TANK_PROJECTILE
            | GE_ORIGINAL_STAGE_MISC_DEP_TANK_FLOOR_COLLISION
            | GE_ORIGINAL_STAGE_MISC_DEP_OBJECT_ACTION_TICK
            | GE_ORIGINAL_STAGE_MISC_DEP_MOVING_MODEL_MATRICES
            | GE_ORIGINAL_STAGE_MISC_DEP_OBJECT_AUDIO;
    case PROPDEF_SAFE_ITEM:
        return GE_ORIGINAL_STAGE_MISC_DEP_SAFE_RELATION;
    default: return 0U;
    }
}

static GeOriginalStageMiscStatus ge_misc_construct_common(
    const GeOriginalStagePropConstructionRequest *request,
    ObjectRecord *object, const GeOriginalStageMiscProviders *providers)
{
    if (!providers->construct_standard(
            providers->context, object, (int32_t)request->command_index))
        return GE_ORIGINAL_STAGE_MISC_CONSTRUCTION_FAILED;
    if (!providers->place_standard(providers->context, object))
        return GE_ORIGINAL_STAGE_MISC_PLACEMENT_FAILED;
    if (object->prop == NULL || providers->update_room_position == NULL
            || providers->activate_prop == NULL
            || providers->enable_prop == NULL
            || !providers->update_room_position(providers->context, object)
            || !providers->activate_prop(providers->context, object->prop)
            || !providers->enable_prop(providers->context, object->prop))
        return GE_ORIGINAL_STAGE_MISC_ACTIVATION_FAILED;
    return GE_ORIGINAL_STAGE_MISC_OK;
}

GeOriginalStageMiscStatus ge_original_stage_misc_construct_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageMiscProviders *providers,
    GeOriginalStageMiscInstance *instance)
{
    ObjectRecord *object = definition;
    GeOriginalStageMiscStatus status;
    uint8_t type;
    if (request == NULL || request->record == NULL || definition == NULL
            || providers == NULL || providers->construct_standard == NULL
            || providers->place_standard == NULL || instance == NULL)
        return GE_ORIGINAL_STAGE_MISC_INVALID_ARGUMENT;
    type = request->record->type;
    if (!ge_misc_type(type)) return GE_ORIGINAL_STAGE_MISC_UNSUPPORTED_TYPE;
    memset(instance, 0, sizeof(*instance));
    if (request->pad_id < 0
            || (request->flags & (PROPFLAG_INSIDEANOTHEROBJ
                                  | PROPFLAG_ASSIGNEDTOCHR)) != 0U)
        return GE_ORIGINAL_STAGE_MISC_UNSUPPORTED_OWNERSHIP;
    if (!request->placement_resolved || request->placement.stan == NULL)
        return GE_ORIGINAL_STAGE_MISC_PLACEMENT_UNRESOLVED;
    if ((type == PROPDEF_VEHICHLE || type == PROPDEF_AIRCRAFT)
            && providers->resolve_ai_list == NULL)
        return GE_ORIGINAL_STAGE_MISC_MISSING_AI_LIST;
    if (type == PROPDEF_VEHICHLE
            && providers->set_model_switch == NULL)
        return GE_ORIGINAL_STAGE_MISC_MISSING_SWITCH_SERVICE;
    if (type == PROPDEF_TANK
            && providers->load_tank_projectiles == NULL)
        return GE_ORIGINAL_STAGE_MISC_MISSING_PROJECTILE_SERVICE;
    if (type == PROPDEF_TANK && providers->get_floor_y == NULL)
        return GE_ORIGINAL_STAGE_MISC_MISSING_FLOOR_SERVICE;
    if (definition_size
                != ge_original_stage_prop_native_definition_size(request)
            || !ge_original_stage_prop_native_definition_init(
                request, definition, definition_size))
        return GE_ORIGINAL_STAGE_MISC_INVALID_DEFINITION;
    if (type == PROPDEF_TANK) {
        if (!providers->load_tank_projectiles(providers->context))
            return GE_ORIGINAL_STAGE_MISC_MISSING_PROJECTILE_SERVICE;
    }
    status = ge_misc_construct_common(request, object, providers);
    if (status != GE_ORIGINAL_STAGE_MISC_OK) return status;
    if (type == PROPDEF_VEHICHLE) {
        VehichleRecord *vehicle = definition;
        void *resolved = NULL;
        const int32_t list_id = (int32_t)(uintptr_t)vehicle->ailist;
        if (providers->set_model_switch == NULL
                || !providers->set_model_switch(providers->context,
                    vehicle->model, 5U,
                    (vehicle->flags & UINT32_C(0x10000000)) == 0U))
            return GE_ORIGINAL_STAGE_MISC_MISSING_SWITCH_SERVICE;
        if (providers->resolve_ai_list == NULL
                || !providers->resolve_ai_list(
                    providers->context, list_id, &resolved))
            return GE_ORIGINAL_STAGE_MISC_MISSING_AI_LIST;
        vehicle->speed = vehicle->wheelxrot = vehicle->wheelyrot = 0.0f;
        vehicle->speedaim = vehicle->turnrot60 = vehicle->roty = 0.0f;
        vehicle->speedtime60 = -1.0f;
        vehicle->ailist = resolved;
        vehicle->aioffset = 0U;
        vehicle->aireturnlist = -1;
        vehicle->path = NULL;
        vehicle->nextstep = 0;
        vehicle->Sound = NULL;
    } else if (type == PROPDEF_AIRCRAFT) {
        AircraftRecord *aircraft = definition;
        void *resolved = NULL;
        const int32_t list_id = (int32_t)(uintptr_t)aircraft->ailist;
        if (providers->resolve_ai_list == NULL
                || !providers->resolve_ai_list(
                    providers->context, list_id, &resolved))
            return GE_ORIGINAL_STAGE_MISC_MISSING_AI_LIST;
        aircraft->speed = aircraft->speedaim = aircraft->rotoryrot = 0.0f;
        aircraft->rotaryspeed = aircraft->rotaryspeedaim = 0.0f;
        aircraft->yrot = 0.0f;
        aircraft->speedtime60 = aircraft->rotaryspeedtime = -1.0f;
        aircraft->ailist = resolved;
        aircraft->aioffset = 0U;
        aircraft->aireturnlist = -1;
        aircraft->nextstep = 0;
        aircraft->path = NULL;
        aircraft->Sound = NULL;
    } else if (type == PROPDEF_TANK) {
        TankRecord *tank = definition;
        float floor_y = 0.0f;
        tank->turret_vertical_angle = 0.0f;
        tank->turret_orientation_angle = 0.0f;
        tank->tank_orientation_angle = M_TAU_F
            - atan2f(tank->mtx.m[2][0], tank->mtx.m[2][2]);
        if (tank->prop != NULL) {
            if (providers->get_floor_y == NULL
                    || !providers->get_floor_y(providers->context,
                        tank->prop->stan, tank->prop->pos.f[0],
                        tank->prop->pos.f[2], &floor_y))
                return GE_ORIGINAL_STAGE_MISC_MISSING_FLOOR_SERVICE;
        }
        tank->stan_y = floor_y;
        tank->unkD0 = floor_y / 0.17000002f;
    }
    instance->definition = definition;
    instance->prop = object->prop;
    instance->model = object->model;
    instance->command_index = request->command_index;
    instance->runtime_dependencies =
        ge_original_stage_misc_runtime_dependencies(type);
    instance->type = type;
    instance->constructed = 1U;
    instance->activated = 1U;
    return GE_ORIGINAL_STAGE_MISC_OK;
}

static int ge_safe_relative_index(size_t base, int32_t relative,
                                  size_t count, size_t *result)
{
    int64_t index = (int64_t)base + relative;
    if (index < 0 || (uint64_t)index >= count) return 0;
    *result = (size_t)index;
    return 1;
}

GeOriginalStageMiscStatus ge_original_stage_safe_item_link_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageSafeItemProviders *providers)
{
    SafeObjectRecord *relation = definition;
    ObjectRecord *item;
    SafeRecord *safe;
    DoorRecord *door;
    size_t item_index, safe_index, door_index;
    if (request == NULL || request->runtime == NULL
            || request->record == NULL || definition == NULL
            || providers == NULL || providers->find_definition == NULL
            || providers->register_relation == NULL)
        return GE_ORIGINAL_STAGE_MISC_INVALID_ARGUMENT;
    if (request->record->type != PROPDEF_SAFE_ITEM
            || definition_size != sizeof(*relation)
            || !ge_original_stage_prop_native_definition_init(
                request, definition, definition_size))
        return GE_ORIGINAL_STAGE_MISC_INVALID_DEFINITION;
    if (!ge_safe_relative_index(request->command_index, relation->Index1,
                request->runtime->prop_record_count, &item_index)
            || !ge_safe_relative_index(request->command_index,
                relation->Index2, request->runtime->prop_record_count,
                &safe_index)
            || !ge_safe_relative_index(request->command_index,
                relation->Index3, request->runtime->prop_record_count,
                &door_index))
        return GE_ORIGINAL_STAGE_MISC_INVALID_DEFINITION;
    item = providers->find_definition(providers->context, item_index);
    safe = providers->find_definition(providers->context, safe_index);
    door = providers->find_definition(providers->context, door_index);
    if (item == NULL || item->prop == NULL || safe == NULL
            || safe->prop == NULL || safe->type != PROPDEF_SAFE
            || door == NULL || door->prop == NULL
            || door->type != PROPDEF_DOOR)
        return GE_ORIGINAL_STAGE_MISC_UNSUPPORTED_OWNERSHIP;
    relation->item = item;
    relation->safe = safe;
    relation->door = door;
    if (!providers->register_relation(providers->context, relation))
        return GE_ORIGINAL_STAGE_MISC_ACTIVATION_FAILED;
    item->flags2 |= PROPFLAG2_LINKEDTOSAFE;
    door->flags2 |= PROPFLAG2_LINKEDTOSAFE;
    return GE_ORIGINAL_STAGE_MISC_OK;
}

const char *ge_original_stage_misc_status_name(
    GeOriginalStageMiscStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_MISC_OK: return "ok";
    case GE_ORIGINAL_STAGE_MISC_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_STAGE_MISC_INVALID_DEFINITION:return "invalid definition";
    case GE_ORIGINAL_STAGE_MISC_UNSUPPORTED_TYPE:return "unsupported type";
    case GE_ORIGINAL_STAGE_MISC_UNSUPPORTED_OWNERSHIP:return "unsupported ownership";
    case GE_ORIGINAL_STAGE_MISC_PLACEMENT_UNRESOLVED:return "placement unresolved";
    case GE_ORIGINAL_STAGE_MISC_MISSING_AI_LIST:return "missing AI list";
    case GE_ORIGINAL_STAGE_MISC_MISSING_SWITCH_SERVICE:return "missing switch service";
    case GE_ORIGINAL_STAGE_MISC_MISSING_PROJECTILE_SERVICE:return "missing tank projectile service";
    case GE_ORIGINAL_STAGE_MISC_MISSING_FLOOR_SERVICE:return "missing tank floor service";
    case GE_ORIGINAL_STAGE_MISC_CONSTRUCTION_FAILED:return "construction failed";
    case GE_ORIGINAL_STAGE_MISC_PLACEMENT_FAILED:return "placement failed";
    case GE_ORIGINAL_STAGE_MISC_ACTIVATION_FAILED:return "activation failed";
    default:return "unknown";
    }
}

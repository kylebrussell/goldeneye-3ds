#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "game/matrixmath.h"
#include "ge_original_stage_security.h"

#include <math.h>
#include <string.h>

uint32_t ge_original_stage_security_required_capabilities(uint8_t type)
{
    if (type == PROPDEF_CCTV) return GE_ORIGINAL_STAGE_SECURITY_CCTV_REQUIRED;
    if (type == PROPDEF_AUTOGUN)
        return GE_ORIGINAL_STAGE_SECURITY_AUTOGUN_REQUIRED;
    return 0U;
}

int ge_original_stage_security_dependency_audit(
    uint8_t type, GeOriginalStageSecurityDependencyAudit *audit)
{
    if (audit == NULL || (type != PROPDEF_CCTV
            && type != PROPDEF_AUTOGUN)) return 0;
    memset(audit, 0, sizeof(*audit));
    if (type == PROPDEF_CCTV) {
        audit->tick_capabilities = GE_ORIGINAL_STAGE_SECURITY_ACTIVE_PROP_TICK
            | GE_ORIGINAL_STAGE_SECURITY_TIMER
            | GE_ORIGINAL_STAGE_SECURITY_PLAYER_PROP
            | GE_ORIGINAL_STAGE_SECURITY_STAN_LINE
            | GE_ORIGINAL_STAGE_SECURITY_ALARM
            | GE_ORIGINAL_STAGE_SECURITY_AIM;
        audit->render_capabilities = GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATIONS
            | GE_ORIGINAL_STAGE_SECURITY_CAMERA_MATRICES
            | GE_ORIGINAL_STAGE_SECURITY_LIGHTING_SHADOW;
        audit->damage_capabilities = GE_ORIGINAL_STAGE_SECURITY_OBJECT_DAMAGE
            | GE_ORIGINAL_STAGE_SECURITY_DAMAGE_EFFECTS;
    } else {
        audit->tick_capabilities = GE_ORIGINAL_STAGE_SECURITY_ACTIVE_PROP_TICK
            | GE_ORIGINAL_STAGE_SECURITY_TIMER
            | GE_ORIGINAL_STAGE_SECURITY_PLAYER_PROP
            | GE_ORIGINAL_STAGE_SECURITY_STAN_LINE
            | GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATIONS
            | GE_ORIGINAL_STAGE_SECURITY_RANDOM
            | GE_ORIGINAL_STAGE_SECURITY_AIM
            | GE_ORIGINAL_STAGE_SECURITY_GUNFIRE_SFX
            | GE_ORIGINAL_STAGE_SECURITY_PLAYER_DAMAGE
            | GE_ORIGINAL_STAGE_SECURITY_CAMERA_MATRICES;
        audit->render_capabilities = GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATIONS
            | GE_ORIGINAL_STAGE_SECURITY_CAMERA_MATRICES
            | GE_ORIGINAL_STAGE_SECURITY_BEAM_RENDER
            | GE_ORIGINAL_STAGE_SECURITY_LIGHTING_SHADOW;
        audit->damage_capabilities = GE_ORIGINAL_STAGE_SECURITY_OBJECT_DAMAGE
            | GE_ORIGINAL_STAGE_SECURITY_DAMAGE_EFFECTS;
        audit->cleanup_capabilities =
            GE_ORIGINAL_STAGE_SECURITY_SOUND_LIFECYCLE;
    }
    audit->all_capabilities = audit->tick_capabilities
        | audit->render_capabilities | audit->damage_capabilities
        | audit->cleanup_capabilities;
    return 1;
}

int ge_original_stage_security_model_audit(
    const void *definition, GeOriginalStageSecurityModelAudit *audit)
{
    const ObjectRecord *object = definition;
    const ModelFileHeader *header;
    uint32_t required;
    int32_t index;
    if (object == NULL || audit == NULL || object->model == NULL
            || object->model->obj == NULL
            || (object->type != PROPDEF_CCTV
                && object->type != PROPDEF_AUTOGUN)) return 0;
    memset(audit, 0, sizeof(*audit));
    header = object->model->obj;
    audit->num_switches = header->numSwitches;
    audit->num_matrices = header->numMatrices;
    audit->required_render_switch_mask = object->type == PROPDEF_CCTV
        ? UINT32_C(1) << 0
        : (UINT32_C(1) << 1) | (UINT32_C(1) << 2);
    audit->optional_fire_switch_mask = object->type == PROPDEF_AUTOGUN
        ? (UINT32_C(1) << 5) | (UINT32_C(1) << 7) : 0U;
    if (header->Switches != NULL) {
        for (index = 0; index < header->numSwitches && index < 32; ++index) {
            if (header->Switches[index] == NULL) continue;
            audit->switch_present_mask |= UINT32_C(1) << index;
            if (header->Switches[index]->Data != NULL)
                audit->switch_data_mask |= UINT32_C(1) << index;
        }
    }
    required = audit->required_render_switch_mask;
    audit->render_relations_ready = (uint8_t)(
        (audit->switch_present_mask & required) == required
        && (audit->switch_data_mask & required) == required
        && header->numMatrices >= (object->type == PROPDEF_CCTV ? 2 : 3));
    return 1;
}

static GeOriginalStageSecurityStatus ge_security_setup_cctv(
    const GeOriginalStagePropConstructionRequest *request,
    CCTVRecord *cctv)
{
    GeOriginalStagePadPlacement look_pad;
    coord3d relation_point;
    ModelFileHeader *header;
    ModelNode *relation;
    /* setupCctv targets the common ObjectRecord pad, not the unrelated
     * authored cctv_lookpad tail field. */
    if (cctv->pad < 0) return GE_ORIGINAL_STAGE_SECURITY_OK;
    if (!ge_original_stage_setup_pad_placement(
            request->runtime, cctv->pad, &look_pad))
        return GE_ORIGINAL_STAGE_SECURITY_LOOK_PAD_UNRESOLVED;
    if (cctv->model == NULL || cctv->model->obj == NULL
            || cctv->prop == NULL)
        return GE_ORIGINAL_STAGE_SECURITY_CONSTRUCTION_FAILED;
    header = cctv->model->obj;
    if (header->Switches == NULL || header->numSwitches <= 0
            || header->Switches[0] == NULL
            || header->Switches[0]->Data == NULL)
        return GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATION_UNAVAILABLE;
    relation = header->Switches[0];
    memcpy(relation_point.f, relation->Data, sizeof(relation_point.f));
    mtx4RotateVecInPlace(&cctv->mtx, &relation_point);
    relation_point.x += cctv->prop->pos.x;
    relation_point.y += cctv->prop->pos.y;
    relation_point.z += cctv->prop->pos.z;
    matrix_4x4_set_basis_and_position_target(&cctv->unk84,
        0.0f, 0.0f, 0.0f,
        relation_point.x - look_pad.position[0],
        relation_point.y - look_pad.position[1],
        relation_point.z - look_pad.position[2],
        0.0f, 1.0f, 0.0f);
    matrix_scalar_multiply(cctv->model->scale, cctv->unk84.m[0]);
    if (cctv->convert_to_f32 == 0) {
        int32_t fixed;
        cctv->convert_to_f32 = 1;
        memcpy(&fixed, &cctv->unkCC, sizeof(fixed));
        cctv->unkCC = ((float)fixed * M_TAU_F) / 65536.0f;
        memcpy(&fixed, &cctv->unkD0, sizeof(fixed));
        cctv->unkD0 = ((float)fixed * M_TAU_F) / 65536.0f;
        memcpy(&fixed, &cctv->unkDC, sizeof(fixed));
        cctv->unkDC = ((float)fixed * M_TAU_F) / 65536.0f;
        memcpy(&fixed, &cctv->unkE8, sizeof(fixed));
        cctv->unkE8 = (float)fixed;
    }
    cctv->unkD4 = 0;
    cctv->unkD8 = 0.0f;
    cctv->unkC8 = cctv->unkCC;
    cctv->unkC4 = atan2f(
        relation_point.x - look_pad.position[0],
        relation_point.z - look_pad.position[2]);
    cctv->timer = 0;
    return GE_ORIGINAL_STAGE_SECURITY_OK;
}

static GeOriginalStageSecurityStatus ge_security_setup_autogun(
    const GeOriginalStagePropConstructionRequest *request,
    AutogunRecord *autogun,
    const GeOriginalStageSecurityProviders *providers)
{
    GeOriginalStagePadPlacement aim_pad;
    int32_t fixed;
    void *beam;
    if (autogun->model == NULL || autogun->prop == NULL)
        return GE_ORIGINAL_STAGE_SECURITY_CONSTRUCTION_FAILED;
    autogun->unkAC = 0;
    autogun->unkB8 = -1;
    autogun->unkBC = -1;
    autogun->unkC0 = -1;
    autogun->unkC4 = NULL;
    autogun->unkC8 = NULL;
    autogun->unk90 = 0.0f;
    autogun->unk94 = 0.0f;
    autogun->rot_related = 0.0f;
    autogun->unk9C = 0.0f;
    autogun->unkA0 = 0.0f;
    autogun->unk98 = 0.0f;
    autogun->unkB0 = 0.0f;
    autogun->unkB4 = 0.0f;
    memcpy(&fixed, &autogun->speed, sizeof(fixed));
    autogun->speed = ((float)fixed * M_TAU_F) / 65536.0f;
    memcpy(&fixed, &autogun->aimdist, sizeof(fixed));
    autogun->aimdist = ((float)fixed * 100.0f) / 65536.0f;
    memcpy(&fixed, &autogun->unk88, sizeof(fixed));
    autogun->unk88 = ((float)fixed * M_TAU_F) / 65536.0f;
    memcpy(&fixed, &autogun->unk8C, sizeof(fixed));
    autogun->unk8C = ((float)fixed * M_TAU_F) / 65536.0f;
    if (providers->allocate_stage == NULL)
        return GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_UNAVAILABLE;
    beam = providers->allocate_stage(providers->context, 0x30U);
    if (beam == NULL) return GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_FAILED;
    /* setupAutogun leaves the remaining stage-pool bytes untouched and only
     * publishes the canonical freshly-created beam age. */
    *(int8_t *)beam = -1;
    autogun->beam = beam;
    autogun->is_active = 0;
    autogun->unkD4 = 0.0f;
    if (autogun->padID >= 0) {
        float xdiff, ydiff, zdiff;
        if (!ge_original_stage_setup_pad_placement(
                request->runtime, autogun->padID, &aim_pad))
            return GE_ORIGINAL_STAGE_SECURITY_LOOK_PAD_UNRESOLVED;
        xdiff = aim_pad.position[0] - autogun->prop->pos.x;
        ydiff = aim_pad.position[1] - autogun->prop->pos.y;
        zdiff = aim_pad.position[2] - autogun->prop->pos.z;
        autogun->rot_related = atan2f(xdiff, zdiff);
        autogun->unk98 = atan2f(
            ydiff, sqrtf(xdiff * xdiff + zdiff * zdiff));
    }
    return GE_ORIGINAL_STAGE_SECURITY_OK;
}

GeOriginalStageSecurityStatus ge_original_stage_security_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageSecurityProviders *providers,
    GeOriginalStageSecurityInstance *instance)
{
    ObjectRecord *object = definition;
    uint32_t required, missing;
    GeOriginalStageSecurityStatus status;
    if (request == NULL || request->record == NULL || definition == NULL
            || providers == NULL || instance == NULL
            || providers->construct_standard == NULL
            || providers->place_standard == NULL)
        return GE_ORIGINAL_STAGE_SECURITY_INVALID_ARGUMENT;
    required = ge_original_stage_security_required_capabilities(
        request->record->type);
    if (required == 0U) return GE_ORIGINAL_STAGE_SECURITY_INVALID_DEFINITION;
    memset(instance, 0, sizeof(*instance));
    instance->request = request;
    instance->definition = definition;
    instance->type = request->record->type;
    instance->required_runtime_capabilities = required;
    missing = required & ~providers->runtime_capabilities;
    instance->missing_runtime_capabilities = missing;
    if (missing != 0U)
        return GE_ORIGINAL_STAGE_SECURITY_MISSING_RUNTIME_DEPENDENCY;
    if (providers->update_room_position == NULL
            || providers->activate_prop == NULL
            || providers->enable_prop == NULL)
        return GE_ORIGINAL_STAGE_SECURITY_ACTIVATION_DEPENDENCY_UNAVAILABLE;
    if (definition_size
            != ge_original_stage_prop_native_definition_size(request)
            || !ge_original_stage_prop_native_definition_init(
                request, definition, definition_size))
        return GE_ORIGINAL_STAGE_SECURITY_INVALID_DEFINITION;
    if (!providers->construct_standard(
            providers->context, definition,
            (int32_t)request->command_index))
        return GE_ORIGINAL_STAGE_SECURITY_CONSTRUCTION_FAILED;
    if (!providers->place_standard(providers->context, definition))
        return GE_ORIGINAL_STAGE_SECURITY_PLACEMENT_FAILED;
    if (object->prop == NULL
            || !providers->update_room_position(
                providers->context, definition)
            || !providers->activate_prop(providers->context, object->prop)
            || !providers->enable_prop(providers->context, object->prop))
        return GE_ORIGINAL_STAGE_SECURITY_ACTIVATION_FAILED;
    status = request->record->type == PROPDEF_CCTV
        ? ge_security_setup_cctv(request, definition)
        : ge_security_setup_autogun(request, definition, providers);
    if (status != GE_ORIGINAL_STAGE_SECURITY_OK) return status;
    instance->prop = object->prop;
    instance->model = object->model;
    if (request->record->type == PROPDEF_AUTOGUN)
        instance->beam = ((AutogunRecord *)definition)->beam;
    instance->constructed = 1U;
    instance->runtime_ready = 1U;
    return GE_ORIGINAL_STAGE_SECURITY_OK;
}

const char *ge_original_stage_security_status_name(
    GeOriginalStageSecurityStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_SECURITY_OK: return "ok";
    case GE_ORIGINAL_STAGE_SECURITY_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_SECURITY_INVALID_DEFINITION:
        return "invalid definition";
    case GE_ORIGINAL_STAGE_SECURITY_MISSING_RUNTIME_DEPENDENCY:
        return "missing runtime dependency";
    case GE_ORIGINAL_STAGE_SECURITY_CONSTRUCTION_FAILED:
        return "default-object construction failed";
    case GE_ORIGINAL_STAGE_SECURITY_PLACEMENT_FAILED:
        return "default-object placement failed";
    case GE_ORIGINAL_STAGE_SECURITY_ACTIVATION_DEPENDENCY_UNAVAILABLE:
        return "active-prop publication unavailable";
    case GE_ORIGINAL_STAGE_SECURITY_ACTIVATION_FAILED:
        return "active-prop publication failed";
    case GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATION_UNAVAILABLE:
        return "CCTV model relation unavailable";
    case GE_ORIGINAL_STAGE_SECURITY_LOOK_PAD_UNRESOLVED:
        return "authored look pad unresolved";
    case GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_UNAVAILABLE:
        return "stage beam allocator unavailable";
    case GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_FAILED:
        return "stage beam allocation failed";
    default: return "unknown";
    }
}

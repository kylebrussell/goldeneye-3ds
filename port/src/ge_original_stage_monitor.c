#include <ultra64.h>
#include <bondtypes.h>
#include "bondconstants.h"

#include "ge_original_stage_monitor.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_original_default_object_internal.h"
#include "ge_original_stage_guard_actor.h"
#include "game/matrixmath.h"
#include "game/model.h"
#include "game/propobj.h"
#include "game/chrai.h"

#include <string.h>

extern void embedmentFree(Embedment *embedment);
extern void chrpropDetach(PropRecord *prop);

/* BITFLAG(RUNTIMEBITFLAG, ... EMBEDDED ...) is not emitted by the AIPARSE
 * header pass used by the native port. Keep the exact generated bit index. */
enum { GE_STAGE_MONITOR_RUNTIME_EMBEDDED = 1U << 6 };

static int ge_stage_monitor_initialize_controllers(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size)
{
    if (request->record->type == PROPDEF_MONITOR) {
        MonitorObjRecord *monitor = definition;
        if (definition_size != sizeof(*monitor)) return 0;
        return ge_original_stage_monitor_controller_initialize(
            &monitor->Monitor, monitor->ImageNum);
    }
    if (request->record->type == PROPDEF_MULTI_MONITOR) {
        MultiMonitorObjRecord *monitor = definition;
        size_t slot;
        if (definition_size != sizeof(*monitor)) return 0;
        for (slot = 0U; slot < 4U; ++slot)
            if (!ge_original_stage_monitor_controller_initialize(
                    &monitor->Monitor[slot], monitor->ImageNums[slot]))
                return 0;
        return 1;
    }
    return 0;
}

int ge_original_stage_monitor_owner_command_exact(
    const GeOriginalStagePropConstructionRequest *request,
    const void *definition, int32_t *owner_command_index, int *embedded)
{
    const ObjectRecord *object = definition;
    if (request == NULL || request->record == NULL || object == NULL
            || owner_command_index == NULL || embedded == NULL
            || request->command_index > INT32_MAX
            || request->record->type != PROPDEF_MONITOR) return 0;
    if (object->pad < 0
            && (object->flags & PROPFLAG_INSIDEANOTHEROBJ) == 0U) {
        *owner_command_index = (int32_t)request->command_index
            + ((const MonitorObjRecord *)definition)->OwnerOffset;
        *embedded = 1;
        return *owner_command_index >= 0;
    }
    if ((object->flags & PROPFLAG_INSIDEANOTHEROBJ) != 0U) {
        *owner_command_index = (int32_t)request->command_index + object->pad;
        *embedded = 0;
        return *owner_command_index >= 0;
    }
    return 0;
}

int ge_original_stage_monitor_bind_owned_prop_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, void *opaque_prop, size_t prop_size)
{
    ObjectRecord *object = definition;
    PropRecord *prop = opaque_prop;
    int32_t owner_command_index;
    int embedded;

    if (request == NULL || request->record == NULL || object == NULL
            || prop == NULL || prop_size < sizeof(*prop)
            || !ge_original_stage_monitor_owner_command_exact(
                request, definition, &owner_command_index, &embedded))
        return 0;

    /* setupSingleMonitor initializes owned records before their owner supplies
     * a room. Negative-pad records therefore have no pad/STAN to bind here. */
    memset(prop, 0, sizeof(*prop));
    prop->type = PROP_TYPE_OBJ;
    prop->obj = object;
    prop->stan = NULL;
    prop->rooms[0] = prop->rooms[1] = prop->rooms[2] = UINT8_MAX;
    object->prop = prop;
    return 1;
}

int ge_original_stage_monitor_construct_owned_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size, void *opaque_prop,
    size_t prop_size, GeOriginalPitemModelProvider *models,
    void *owner_definition, void *opaque_owner_prop, void *collision_data,
    int32_t player_count, int embedded, void **model_instance_out)
{
    MonitorObjRecord *monitor = definition;
    ObjectRecord *object = definition;
    ObjectRecord *owner = owner_definition;
    PropRecord *prop = opaque_prop;
    PropRecord *owner_prop = opaque_owner_prop;
    ModelFileHeader *header = NULL;
    Model *model = NULL;
    float pitem_scale = 0.0f;
    int32_t raw_damage;
    size_t switch_index;

    if (request == NULL || request->record == NULL || definition == NULL
            || definition_size
                != ge_original_stage_prop_native_definition_size(request)
            || prop == NULL
            || prop_size < sizeof(*prop) || models == NULL
            || model_instance_out == NULL)
        return 0;
    if (embedded && (owner == NULL || owner_prop == NULL
            || owner->model == NULL
            || owner->model->scale == 0.0f)) return 0;
    if (!ge_original_pitem_model_resolve_instance(
            models, request->model_id, (void **)&header, (void **)&model,
            &pitem_scale) || header == NULL || model == NULL
            || ge_original_objInitPreallocatedSlice(
                object, header, prop, model, pitem_scale,
                collision_data) == NULL) return 0;

    memcpy(&raw_damage, &object->damage, sizeof(raw_damage));
    object->damage = (float)raw_damage / 65535.0f;
    if (player_count >= 2) object->state |= PROPSTATE_RESPAWN;
    modelSetScale(model, model->scale
        * ((float)object->extrascale * (1.0f / 256.0f)));
    if (embedded) {
        switch_index = monitor->OwnerPart <= 2
            ? (size_t)monitor->OwnerPart : 3U;
        monitor->embedment = embedmentAllocate();
        model->attachedto_objinst = (ModelNode *)
            ge_original_pitem_model_instance_switch_node(
                models, owner->model, switch_index);
        if (monitor->embedment == NULL
                || model->attachedto_objinst == NULL) {
            if (monitor->embedment != NULL) embedmentFree(monitor->embedment);
            (void)ge_original_pitem_model_release_instance(models, model);
            object->model = NULL;
            object->prop = NULL;
            return 0;
        }
        object->runtime_bitflags |= GE_STAGE_MONITOR_RUNTIME_EMBEDDED;
        model->attachedto = owner->model;
        matrix_4x4_set_rotation_around_x(
            0.36651915f, &monitor->embedment->matrix);
        matrix_scalar_multiply(model->scale / owner->model->scale,
                               monitor->embedment->matrix.m[0]);
    } else if (owner_prop != NULL) {
        if (!ge_original_stage_monitor_bind_inside_owner_exact(
                definition, owner_definition, owner_prop)) return 0;
    }
    if (embedded) {
        ge_original_stage_guard_actor_reparent_prop(prop, owner_prop);
        if (prop->parent != owner_prop) return 0;
    }
    *model_instance_out = model;
    return 1;
}

int ge_original_stage_monitor_bind_inside_owner_exact(
    void *definition, void *owner_definition, void *opaque_owner_prop)
{
    ObjectRecord *object = definition;
    ObjectRecord *owner = owner_definition;
    PropRecord *owner_prop = opaque_owner_prop;
    if (object == NULL || owner == NULL || owner_prop == NULL
            || object->model == NULL || object->prop == NULL) return 0;
    /* Exact proplvreset2 second pass for PROPFLAG_INSIDEANOTHEROBJ. */
    object->runtime_bitflags |= RUNTIMEBITFLAG_HASOWNER;
    modelSetScale(object->model, object->model->scale);
    ge_original_stage_guard_actor_reparent_prop(object->prop, owner_prop);
    return object->prop->parent == owner_prop;
}

void ge_original_stage_monitor_release_owned_exact(
    void *definition, GeOriginalPitemModelProvider *models)
{
    ObjectRecord *object = definition;
    Model *model;
    if (object == NULL) return;
    model = object->model;
    if (object->prop != NULL && object->prop->parent != NULL)
        chrpropDetach(object->prop);
    if ((object->runtime_bitflags & GE_STAGE_MONITOR_RUNTIME_EMBEDDED) != 0U) {
        if (object->embedment != NULL) embedmentFree(object->embedment);
        object->embedment = NULL;
        object->runtime_bitflags &= ~GE_STAGE_MONITOR_RUNTIME_EMBEDDED;
    }
    object->runtime_bitflags &= ~RUNTIMEBITFLAG_HASOWNER;
    if (model != NULL) {
        model->attachedto = NULL;
        model->attachedto_objinst = NULL;
        if (models != NULL)
            (void)ge_original_pitem_model_release_instance(models, model);
    }
    object->model = NULL;
    object->prop = NULL;
}

int ge_original_stage_monitor_compose_attachment_exact(
    const float owner_switch_matrix[4][4],
    const float embedment_matrix[4][4], float output[4][4])
{
    if (owner_switch_matrix == NULL || embedment_matrix == NULL
            || output == NULL) return 0;
    matrix_4x4_multiply_homogeneous(
        (Mtxf *)(void *)owner_switch_matrix,
        (Mtxf *)(void *)embedment_matrix, (Mtxf *)(void *)output);
    return 1;
}

int ge_original_stage_monitor_publish_attachment_exact(
    void *definition, const float owner_matrix[4][4],
    const float owner_position[3], uint8_t owner_room,
    GeOriginalStageMonitorAttachmentPublication *publication)
{
    MonitorObjRecord *monitor = definition;
    Model *model;
    Model *owner;
    ModelRenderData renderdata;
    Mtxf owner_base;
    Mtxf *switch_matrix;

    if (monitor == NULL || owner_matrix == NULL || owner_position == NULL
            || publication == NULL || monitor->model == NULL
            || monitor->model->attachedto == NULL
            || monitor->model->attachedto_objinst == NULL
            || monitor->embedment == NULL) return 0;
    model = monitor->model;
    owner = model->attachedto;
    if (owner->obj == NULL || owner->render_pos == NULL
            || owner->obj->numMatrices <= 0 || model->obj == NULL
            || model->render_pos == NULL || model->obj->numMatrices <= 0)
        return 0;

    memcpy(owner_base.m, owner_matrix, sizeof(owner_base.m));
    owner_base.m[3][0] = owner_position[0];
    owner_base.m[3][1] = owner_position[1];
    owner_base.m[3][2] = owner_position[2];
    memset(&renderdata, 0, sizeof(renderdata));
    renderdata.basemtx = &owner_base;
    renderdata.mtxlist = &owner->render_pos[0].pos;
    instcalcmatrices(&renderdata, owner);
    switch_matrix = modelFindNodeMtx(
        owner, model->attachedto_objinst, 0);
    if (switch_matrix == NULL
            || !ge_original_stage_monitor_compose_attachment_exact(
                switch_matrix->m, monitor->embedment->matrix.m,
                model->render_pos[0].pos.m)) return 0;
    /* Exact embedded arm of sub_GAME_7F0442DC after root publication. */
    modelUpdateRelationsQuick(model, model->obj->RootNode);
    publication->segment3_matrices =
        (const float (*)[4][4])(const void *)&model->render_pos[0].pos;
    publication->segment3_matrix_count = (size_t)model->obj->numMatrices;
    publication->room = owner_room;
    return 1;
}

GeOriginalStageMonitorStatus ge_original_stage_monitor_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageMonitorProviders *providers)
{
    ObjectRecord *object = definition;
    int32_t owner_command_index;
    int embedded;
    if (request == NULL || request->record == NULL || definition == NULL
            || providers == NULL || providers->construct_standard == NULL
            || providers->place_standard == NULL)
        return GE_ORIGINAL_STAGE_MONITOR_INVALID_ARGUMENT;
    if ((request->record->type != PROPDEF_MONITOR
                && request->record->type != PROPDEF_MULTI_MONITOR)
            || definition_size
                != ge_original_stage_prop_native_definition_size(request)
            || !ge_original_stage_prop_native_definition_init(
                request, definition, definition_size))
        return GE_ORIGINAL_STAGE_MONITOR_INVALID_DEFINITION;
    /* setupSingleMonitor/setupMultiMonitor copy and select every controller
     * before branching into embedded or ordinary object construction. */
    if (!ge_stage_monitor_initialize_controllers(
            request, definition, definition_size))
        return GE_ORIGINAL_STAGE_MONITOR_INVALID_IMAGE;

    if (ge_original_stage_monitor_owner_command_exact(
            request, definition, &owner_command_index, &embedded)) {
        if (providers->construct_owned == NULL)
            return embedded
                ? GE_ORIGINAL_STAGE_MONITOR_EMBEDDED_OWNER_REQUIRED
                : GE_ORIGINAL_STAGE_MONITOR_INSIDE_OWNER_REQUIRED;
        if (!providers->construct_owned(providers->context, request,
                definition, definition_size,
                owner_command_index, embedded))
            return GE_ORIGINAL_STAGE_MONITOR_OWNER_UNAVAILABLE;
        goto constructed;
    }
    if ((object->flags & PROPFLAG_ASSIGNEDTOCHR) != 0U)
        return GE_ORIGINAL_STAGE_MONITOR_INSIDE_OWNER_REQUIRED;
    if (!request->placement_resolved || request->placement.stan == NULL)
        return GE_ORIGINAL_STAGE_MONITOR_PLACEMENT_UNRESOLVED;
    if (!providers->construct_standard(
            providers->context, definition,
            (int32_t)request->command_index))
        return GE_ORIGINAL_STAGE_MONITOR_CONSTRUCTION_FAILED;
    if (!providers->place_standard(providers->context, definition))
        return GE_ORIGINAL_STAGE_MONITOR_PLACEMENT_FAILED;
constructed:
    if (request->record->type == PROPDEF_MONITOR
            && (object->flags & PROPFLAG_MONITOR_RENDERPOSTBG) != 0U
            && object->prop != NULL)
        object->prop->flags |= PROPFLAG_RENDERPOSTBG;
    return GE_ORIGINAL_STAGE_MONITOR_OK;
}

int ge_original_stage_monitor_tick(
    void *definition, size_t definition_size, size_t screen_slot,
    GeOriginalDamMonitorRenderSnapshot *snapshot)
{
    ObjectRecord *object = definition;
    MonitorRecord *controller;
    if (definition == NULL || snapshot == NULL || object->model == NULL)
        return 0;
    if (object->type == PROPDEF_MONITOR) {
        MonitorObjRecord *monitor = definition;
        if (definition_size != sizeof(*monitor) || screen_slot != 0U)
            return 0;
        controller = &monitor->Monitor;
    } else if (object->type == PROPDEF_MULTI_MONITOR) {
        MultiMonitorObjRecord *monitor = definition;
        if (definition_size != sizeof(*monitor) || screen_slot >= 4U)
            return 0;
        controller = &monitor->Monitor[screen_slot];
    } else {
        return 0;
    }
    return ge_original_stage_monitor_render_screen_tick(
        object->model, controller, object->flags, object->flags2,
        screen_slot, snapshot);
}

const char *ge_original_stage_monitor_status_name(
    GeOriginalStageMonitorStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_MONITOR_OK: return "ok";
    case GE_ORIGINAL_STAGE_MONITOR_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_MONITOR_INVALID_DEFINITION:
        return "invalid definition";
    case GE_ORIGINAL_STAGE_MONITOR_INVALID_IMAGE: return "invalid image";
    case GE_ORIGINAL_STAGE_MONITOR_PLACEMENT_UNRESOLVED:
        return "placement unresolved";
    case GE_ORIGINAL_STAGE_MONITOR_EMBEDDED_OWNER_REQUIRED:
        return "embedded owner required";
    case GE_ORIGINAL_STAGE_MONITOR_INSIDE_OWNER_REQUIRED:
        return "inside/assigned owner required";
    case GE_ORIGINAL_STAGE_MONITOR_CONSTRUCTION_FAILED:
        return "construction failed";
    case GE_ORIGINAL_STAGE_MONITOR_PLACEMENT_FAILED:
        return "placement failed";
    case GE_ORIGINAL_STAGE_MONITOR_OWNER_UNAVAILABLE:
        return "owner unavailable";
    default: return "unknown";
    }
}

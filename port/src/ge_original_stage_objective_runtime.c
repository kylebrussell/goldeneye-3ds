#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_objective_runtime.h"
#include "ge_original_stage_objective_live.h"

#include <stdlib.h>
#include <string.h>

enum { GE_OBJECTIVE_RUNTIME_TAGGED = UINT32_C(0x10) };

/* The exact Dam mission object-state slice owns the strong original global.
 * Campaign-wide objective-runtime tests do not link that slice, so retain a
 * weak owner there while preserving the same single linked-list ABI. */
u32 *ptr_last_tag_entry_type16 __attribute__((weak));

static void ge_publish_native_tag(TagObjectRecord *tag)
{
    /* Exact set_parent_cur_tag_entry ordering from src/game/objective.c. */
    tag->NextTag = (TagObjectRecord *)ptr_last_tag_entry_type16;
    ptr_last_tag_entry_type16 = (u32 *)tag;
}

static void ge_runtime_registry_close(void *context)
{
    ge_original_stage_objective_runtime_close(context);
}

void ge_original_stage_objective_runtime_close(
    GeOriginalStageObjectiveRuntime *runtime)
{
    GeOriginalStageObjectiveRegistry *registry;
    size_t index;
    if (runtime == NULL) return;
    registry = runtime->registry;
    if (runtime->bound && registry != NULL) {
        for (index = 0U; index < registry->tag_count; ++index) {
            GeOriginalStageTagEntry *tag = &registry->tags[index];
            if (tag->blocker == GE_ORIGINAL_STAGE_OBJECTIVE_READY
                    && tag->tagged_object != NULL)
                ((ObjectRecord *)tag->tagged_object)->runtime_bitflags &=
                    ~GE_OBJECTIVE_RUNTIME_TAGGED;
        }
        /* Exact something_with_stage_objectives teardown boundary. */
        ptr_last_tag_entry_type16 = NULL;
        if (registry->runtime_close_context == runtime) {
            registry->runtime_close_context = NULL;
            registry->runtime_close = NULL;
        }
    }
    free(runtime->native_tags);
    memset(runtime, 0, sizeof(*runtime));
    ge_original_stage_objective_live_bind(NULL);
}

static void ge_result_clear(GeOriginalStageObjectiveEvaluation *result)
{
    if (result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->objective_index = SIZE_MAX;
    result->criterion_index = SIZE_MAX;
    result->value = GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
}

static GeOriginalStageObjectiveRuntimeStatus ge_block(
    GeOriginalStageObjectiveEvaluation *result, size_t criterion_index,
    GeOriginalStageObjectiveRuntimeBlocker blocker)
{
    if (result != NULL) {
        result->criterion_index = criterion_index;
        result->blocker = (uint8_t)blocker;
    }
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED;
}

static ObjectRecord *ge_live_tagged_object(
    const GeOriginalStageObjectiveCriterion *criterion)
{
    ObjectRecord *object = criterion != NULL ? criterion->tagged_object : NULL;
    if (object != NULL
            && (object->runtime_bitflags & GE_OBJECTIVE_RUNTIME_TAGGED) == 0U)
        object = NULL;
    return object;
}

/* Unchanged objIsHealthy -> objGetDestroyedLevel semantics. */
static int ge_object_is_healthy(const ObjectRecord *object)
{
    return object != NULL && (object->state & PROPSTATE_DESTROYED) == 0U;
}

static GeOriginalStageObjectiveRuntimeStatus ge_evaluate_criterion(
    GeOriginalStageObjectiveRuntime *runtime, size_t criterion_index,
    uint8_t *value, GeOriginalStageObjectiveEvaluation *result)
{
    GeOriginalStageObjectiveCriterion *criterion =
        &runtime->registry->criteria[criterion_index];
    ObjectRecord *object;
    int state;
    *value = GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE;
    if (criterion->blocker != GE_ORIGINAL_STAGE_OBJECTIVE_READY)
        return ge_block(result, criterion_index,
            GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_REGISTRY_BLOCKED);
    switch (criterion->type) {
    case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
        object = ge_live_tagged_object(criterion);
        if (object != NULL && object->prop != NULL
                && ge_object_is_healthy(object))
            *value = GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
        break;
    case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
    case PROPDEF_OBJECTIVE_FAIL_CONDITION:
        if (runtime->providers.stage_flag_set == NULL)
            return ge_block(result, criterion_index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_STAGE_FLAGS_UNAVAILABLE);
        state = runtime->providers.stage_flag_set(
            runtime->providers.context, criterion->words[0]);
        if (state < 0)
            return ge_block(result, criterion_index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_STAGE_FLAGS_UNAVAILABLE);
        if (criterion->type == PROPDEF_OBJECTIVE_COMPLETE_CONDITION) {
            if (!state) *value = GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
        } else if (state) {
            *value = GE_ORIGINAL_STAGE_OBJECTIVE_FAILED;
        }
        break;
    case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
        object = ge_live_tagged_object(criterion);
        if (object == NULL || object->prop == NULL
                || !ge_object_is_healthy(object)) {
            *value = GE_ORIGINAL_STAGE_OBJECTIVE_FAILED;
            break;
        }
        if (runtime->providers.prop_in_inventory == NULL)
            return ge_block(result, criterion_index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVENTORY_UNAVAILABLE);
        state = runtime->providers.prop_in_inventory(
            runtime->providers.context, object->prop);
        if (state < 0)
            return ge_block(result, criterion_index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVENTORY_UNAVAILABLE);
        if (!state) *value = GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
        break;
    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
        object = ge_live_tagged_object(criterion);
        if (object != NULL && object->prop != NULL) {
            if (runtime->providers.prop_in_inventory == NULL)
                return ge_block(result, criterion_index,
                    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVENTORY_UNAVAILABLE);
            state = runtime->providers.prop_in_inventory(
                runtime->providers.context, object->prop);
            if (state < 0)
                return ge_block(result, criterion_index,
                    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVENTORY_UNAVAILABLE);
            if (state) *value = GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
        }
        break;
    case PROPDEF_OBJECTIVE_PHOTOGRAPH:
        if (criterion->status == 0) {
            object = ge_live_tagged_object(criterion);
            *value = object == NULL || object->prop == NULL
                    || !ge_object_is_healthy(object)
                ? GE_ORIGINAL_STAGE_OBJECTIVE_FAILED
                : GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
        }
        break;
    case PROPDEF_OBJECTIVE_ENTER_ROOM:
    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
        if (criterion->status == 0)
            *value = GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
        break;
    case PROPDEF_OBJECTIVE_COPY_ITEM:
        if (runtime->providers.key_analyzer_complete == NULL)
            return ge_block(result, criterion_index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_KEY_ANALYZER_UNAVAILABLE);
        state = runtime->providers.key_analyzer_complete(
            runtime->providers.context);
        if (state < 0)
            return ge_block(result, criterion_index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_KEY_ANALYZER_UNAVAILABLE);
        if (!state) *value = GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
        break;
    case PROPDEF_OBJECTIVE_NULL:
        break;
    default:
        return ge_block(result, criterion_index,
            GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_UNSUPPORTED_CRITERION);
    }
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_begin(
    GeOriginalStageObjectiveRuntime *runtime,
    GeOriginalStageObjectiveRegistry *registry,
    const GeOriginalStageObjectiveRuntimeProviders *providers)
{
    size_t index;
    TagObjectRecord *native_tags;
    if (runtime == NULL || registry == NULL || !registry->initialized)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    runtime->registry = registry;
    if (providers != NULL) runtime->providers = *providers;
    if (runtime->providers.photograph_bounds_inside_view == NULL)
        runtime->providers.photograph_bounds_inside_view =
            ge_original_stage_objective_photograph_bounds_inside_view_exact;
    native_tags = registry->tag_count != 0U
        ? calloc(registry->tag_count, sizeof(*native_tags)) : NULL;
    if (registry->tag_count != 0U && native_tags == NULL) {
        memset(runtime, 0, sizeof(*runtime));
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NO_MEMORY;
    }
    runtime->native_tags = native_tags;
    runtime->native_tag_count = registry->tag_count;
    /* Exact proplvreset2 PROPDEF_TAG publication. Registry construction keeps
     * definitions opaque; binding creates the native TagObjectRecord graph in
     * authored order, then publishes each node through the original linked-
     * list ordering. This is also the list unchanged stage AI searches via
     * objFindByTagId. */
    ptr_last_tag_entry_type16 = NULL;
    for (index = 0U; index < registry->tag_count; ++index) {
        GeOriginalStageTagEntry *tag = &registry->tags[index];
        TagObjectRecord *native = &native_tags[index];
        PropDefHeaderRecord *header = (PropDefHeaderRecord *)(void *)native;
        const uint32_t word0 = tag->record->words[0];
        const uint32_t word1 = tag->record->words[1];
        header->extrascale = (uint16_t)(word0 >> 16U);
        header->state = (uint8_t)(word0 >> 8U);
        header->type = (uint8_t)word0;
        native->ID = tag->tag_id;
        native->OffsetToObj = (int16_t)word1;
        native->TaggedObject = tag->tagged_object;
        if (tag->blocker == GE_ORIGINAL_STAGE_OBJECTIVE_READY
                && tag->tagged_object != NULL)
            ((ObjectRecord *)tag->tagged_object)->runtime_bitflags |=
                GE_OBJECTIVE_RUNTIME_TAGGED;
        ge_publish_native_tag(native);
    }
    for (index = 0U; index < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++index)
        runtime->displayed_status[index] =
            GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
    runtime->bound = 1U;
    registry->runtime_close_context = runtime;
    registry->runtime_close = ge_runtime_registry_close;
    ge_original_stage_objective_live_bind(runtime);
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_evaluate(
    GeOriginalStageObjectiveRuntime *runtime, uint8_t menu,
    GeOriginalStageObjectiveEvaluation *evaluation)
{
    const GeOriginalStageObjectiveEntry *objective;
    size_t index, end;
    uint8_t status = GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE;
    ge_result_clear(evaluation);
    if (runtime == NULL || evaluation == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT;
    if (!runtime->bound || runtime->registry == NULL
            || !runtime->registry->initialized)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
    if (menu >= GE_ORIGINAL_STAGE_OBJECTIVE_MAX
            || runtime->registry->objective_by_menu[menu] < 0)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OBJECTIVE_UNAVAILABLE;
    evaluation->menu = menu;
    evaluation->objective_index = (size_t)
        runtime->registry->objective_by_menu[menu];
    objective = &runtime->registry->objectives[evaluation->objective_index];
    end = objective->first_criterion + objective->criterion_count;
    for (index = objective->first_criterion; index < end; ++index) {
        uint8_t current;
        GeOriginalStageObjectiveRuntimeStatus result =
            ge_evaluate_criterion(runtime, index, &current, evaluation);
        if (result != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) return result;
        if (status == GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE) {
            if (current != GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE)
                status = current;
        } else if (status == GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE
                && current == GE_ORIGINAL_STAGE_OBJECTIVE_FAILED) {
            status = current;
        }
    }
    evaluation->criterion_index = SIZE_MAX;
    evaluation->value = status;
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_cleanup(
    GeOriginalStageObjectiveRuntime *runtime)
{
    size_t menu;
    if (runtime == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT;
    if (!runtime->bound || runtime->registry == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
    for (menu = 0U; menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++menu) {
        int32_t objective_index =
            runtime->registry->objective_by_menu[menu];
        if (objective_index >= 0
                && (runtime->registry->objectives[objective_index].unknown_e
                    & 1U) != 0U
                && runtime->displayed_status[menu]
                    != GE_ORIGINAL_STAGE_OBJECTIVE_FAILED)
            runtime->displayed_status[menu] =
                GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE;
    }
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_collect_status_changes(
    GeOriginalStageObjectiveRuntime *runtime, int32_t selected_difficulty,
    GeOriginalStageObjectiveStatusChange *changes, size_t *change_count,
    GeOriginalStageObjectiveEvaluation *blocked_evaluation)
{
    GeOriginalStageObjectiveEvaluation evaluations[
        GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
    uint8_t valid[GE_ORIGINAL_STAGE_OBJECTIVE_MAX] = {0};
    size_t menu, output_count = 0U;
    uint8_t available_index = 0U;
    if (runtime == NULL || changes == NULL || change_count == NULL
            || blocked_evaluation == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT;
    *change_count = 0U;
    if (!runtime->bound || runtime->registry == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
    /* Resolve first so a missing live service cannot partially consume a HUD
     * transition. This differs only on the port's explicit blocker path. */
    for (menu = 0U; menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++menu) {
        GeOriginalStageObjectiveRuntimeStatus status;
        if (runtime->registry->objective_by_menu[menu] < 0) continue;
        status = ge_original_stage_objective_runtime_evaluate(
            runtime, (uint8_t)menu, &evaluations[menu]);
        if (status != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) {
            *blocked_evaluation = evaluations[menu];
            return status;
        }
        valid[menu] = 1U;
    }
    for (menu = 0U; menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++menu) {
        const GeOriginalStageObjectiveEntry *objective;
        int32_t objective_index;
        int available;
        if (!valid[menu]) continue;
        objective_index = runtime->registry->objective_by_menu[menu];
        objective = &runtime->registry->objectives[objective_index];
        available = (int32_t)objective->difficulty <= selected_difficulty;
        if (runtime->displayed_status[menu] != evaluations[menu].value) {
            runtime->displayed_status[menu] = evaluations[menu].value;
            if (available) {
                changes[output_count].menu = (uint8_t)menu;
                changes[output_count].available_index = available_index;
                changes[output_count].value = evaluations[menu].value;
                changes[output_count].reserved = 0U;
                ++output_count;
            }
        }
        if (available) ++available_index;
    }
    *change_count = output_count;
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

static GeOriginalStageObjectiveRuntimeStatus ge_criterion_pad_room(
    GeOriginalStageObjectiveRuntime *runtime, size_t criterion_index,
    int32_t pad_id, int16_t *room,
    GeOriginalStageObjectiveEvaluation *result)
{
    GeOriginalStagePadPlacement placement;
    if (!ge_original_stage_setup_pad_placement(
            runtime->registry->setup, pad_id, &placement)
            || !placement.has_stan) {
        return ge_block(result, criterion_index,
            GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PAD_STAN_UNAVAILABLE);
    }
    *room = placement.room;
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_check_room_entered(
    GeOriginalStageObjectiveRuntime *runtime, int32_t room_id,
    GeOriginalStageObjectiveEvaluation *result)
{
    int32_t index;
    ge_result_clear(result);
    if (runtime == NULL || result == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT;
    if (!runtime->bound || runtime->registry == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
    for (index = runtime->registry->enter_room_head; index >= 0;
            index = runtime->registry->criteria[index].next_same_type) {
        GeOriginalStageObjectiveCriterion *criterion =
            &runtime->registry->criteria[index];
        int16_t criterion_room;
        GeOriginalStageObjectiveRuntimeStatus status;
        if (criterion->status != 0) continue;
        status = ge_criterion_pad_room(runtime, (size_t)index,
            criterion->value_a, &criterion_room, result);
        if (status != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) return status;
        if (room_id == criterion_room) criterion->status = 1;
    }
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_check_deposit(
    GeOriginalStageObjectiveRuntime *runtime, int32_t weapon_num,
    int32_t room_id, GeOriginalStageObjectiveEvaluation *result)
{
    int32_t index;
    ge_result_clear(result);
    if (runtime == NULL || result == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT;
    if (!runtime->bound || runtime->registry == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
    for (index = runtime->registry->deposit_room_head; index >= 0;
            index = runtime->registry->criteria[index].next_same_type) {
        GeOriginalStageObjectiveCriterion *criterion =
            &runtime->registry->criteria[index];
        int16_t criterion_room;
        GeOriginalStageObjectiveRuntimeStatus status;
        if (criterion->status != 0 || weapon_num != criterion->value_a)
            continue;
        status = ge_criterion_pad_room(runtime, (size_t)index,
            criterion->value_b, &criterion_room, result);
        if (status != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) return status;
        if (room_id == criterion_room) criterion->status = 1;
    }
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_take_picture(
    GeOriginalStageObjectiveRuntime *runtime,
    GeOriginalStageObjectiveEvaluation *result)
{
    int32_t index;
    ge_result_clear(result);
    if (runtime == NULL || result == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT;
    if (!runtime->bound || runtime->registry == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
    for (index = runtime->registry->photograph_head; index >= 0;
            index = runtime->registry->criteria[index].next_same_type) {
        GeOriginalStageObjectiveCriterion *criterion =
            &runtime->registry->criteria[index];
        ObjectRecord *object;
        int captured;
        if (criterion->status != 0) continue;
        if (criterion->blocker != GE_ORIGINAL_STAGE_OBJECTIVE_READY)
            return ge_block(result, (size_t)index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_REGISTRY_BLOCKED);
        object = ge_live_tagged_object(criterion);
        if (object == NULL || object->prop == NULL
                || (object->prop->flags & PROPFLAG_ONSCREEN) == 0U
                || object->prop->zDepth < 0.0f
                || !ge_object_is_healthy(object)) continue;
        if (runtime->providers.photograph_bounds_inside_view == NULL)
            return ge_block(result, (size_t)index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PHOTOGRAPH_UNAVAILABLE);
        captured = runtime->providers.photograph_bounds_inside_view(
            runtime->providers.context, object, object->prop);
        if (captured < 0)
            return ge_block(result, (size_t)index,
                GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PHOTOGRAPH_UNAVAILABLE);
        if (captured) criterion->status = 1;
    }
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

const char *ge_original_stage_objective_runtime_status_name(
    GeOriginalStageObjectiveRuntimeStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK: return "ok";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NO_MEMORY: return "no memory";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND: return "not bound";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OBJECTIVE_UNAVAILABLE:
        return "objective unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED: return "blocked";
    default: return "unknown";
    }
}

const char *ge_original_stage_objective_runtime_blocker_name(
    GeOriginalStageObjectiveRuntimeBlocker blocker)
{
    switch (blocker) {
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_READY: return "ready";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_REGISTRY_BLOCKED:
        return "tagged live object unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVENTORY_UNAVAILABLE:
        return "player inventory unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_STAGE_FLAGS_UNAVAILABLE:
        return "stage flags unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_KEY_ANALYZER_UNAVAILABLE:
        return "key analyzer state unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PAD_STAN_UNAVAILABLE:
        return "authored pad STAN room unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PHOTOGRAPH_UNAVAILABLE:
        return "photograph projection unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_UNSUPPORTED_CRITERION:
        return "unsupported criterion";
    default: return "unknown";
    }
}

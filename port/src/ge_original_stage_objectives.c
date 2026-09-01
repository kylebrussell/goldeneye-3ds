#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>

#include "ge_original_stage_objectives.h"

#include <stdlib.h>
#include <string.h>

static int ge_objective_is_object(uint8_t type)
{
    switch (type) {
    case PROPDEF_DOOR: case PROPDEF_PROP: case PROPDEF_KEY:
    case PROPDEF_ALARM: case PROPDEF_CCTV: case PROPDEF_MAGAZINE:
    case PROPDEF_COLLECTABLE: case PROPDEF_MONITOR:
    case PROPDEF_MULTI_MONITOR: case PROPDEF_RACK: case PROPDEF_AUTOGUN:
    case PROPDEF_HAT: case PROPDEF_AMMO: case PROPDEF_ARMOUR:
    case PROPDEF_GAS_RELEASING: case PROPDEF_VEHICHLE:
    case PROPDEF_AIRCRAFT: case PROPDEF_UNK41: case PROPDEF_GLASS:
    case PROPDEF_SAFE: case PROPDEF_TANK: case PROPDEF_TINTED_GLASS:
        return 1;
    default: return 0;
    }
}

static int ge_objective_is_criterion(uint8_t type)
{
    return type >= PROPDEF_OBJECTIVE_DESTROY_OBJECT
        && type <= PROPDEF_OBJECTIVE_COPY_ITEM;
}

static int ge_objective_uses_tag(uint8_t type)
{
    return type == PROPDEF_OBJECTIVE_DESTROY_OBJECT
        || type == PROPDEF_OBJECTIVE_COLLECT_OBJECT
        || type == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT
        || type == PROPDEF_OBJECTIVE_PHOTOGRAPH;
}

static size_t ge_record_index(const GeOriginalStageSetupRuntime *setup,
                              const GeOriginalStagePropRecord *record)
{
    return (size_t)(record - setup->prop_records);
}

static int32_t ge_find_tag_index(
    const GeOriginalStageObjectiveRegistry *registry, uint16_t tag_id)
{
    int32_t index;
    for (index = registry->tag_head; index >= 0;
            index = registry->tags[index].next_tag) {
        if (registry->tags[index].tag_id == tag_id) return index;
    }
    return GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
}

static GeOriginalStageObjectiveStatus ge_count_records(
    const GeOriginalStageSetupRuntime *setup, size_t *tag_count,
    size_t *text_count, size_t *objective_count, size_t *criterion_count)
{
    size_t index;
    int inside_objective = 0;
    *tag_count = *text_count = *objective_count = *criterion_count = 0U;
    for (index = 0U; index < setup->prop_record_count; ++index) {
        const uint8_t type = setup->prop_records[index].type;
        if (type == PROPDEF_TAG) ++*tag_count;
        if (type == PROPDEF_WATCH_MENU_OBJECTIVE_TEXT) ++*text_count;
        if (type == PROPDEF_OBJECTIVE_START) {
            if (inside_objective) return GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ORDER;
            inside_objective = 1;
            ++*objective_count;
        } else if (type == PROPDEF_OBJECTIVE_END) {
            if (!inside_objective) return GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ORDER;
            inside_objective = 0;
        } else if (inside_objective && ge_objective_is_criterion(type)) {
            ++*criterion_count;
        }
    }
    return inside_objective ? GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ORDER
        : GE_ORIGINAL_STAGE_OBJECTIVE_OK;
}

static void ge_bind_tag_object(
    GeOriginalStageObjectiveRegistry *registry,
    GeOriginalStageTagEntry *tag,
    const GeOriginalStageObjectiveProviders *providers)
{
    const GeOriginalStagePropRecord *target =
        &registry->setup->prop_records[tag->target_command_index];
    if (!ge_objective_is_object(target->type)) {
        tag->blocker = GE_ORIGINAL_STAGE_OBJECTIVE_TARGET_NOT_OBJECT;
    } else if (providers == NULL
            || providers->object_definition_by_command == NULL) {
        tag->blocker = GE_ORIGINAL_STAGE_OBJECTIVE_NO_OBJECT_PROVIDER;
    } else {
        tag->tagged_object = providers->object_definition_by_command(
            providers->context, tag->target_command_index, target);
        tag->blocker = tag->tagged_object != NULL
            ? GE_ORIGINAL_STAGE_OBJECTIVE_READY
            : GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_OBJECT_UNAVAILABLE;
    }
    if (tag->blocker != GE_ORIGINAL_STAGE_OBJECTIVE_READY)
        ++registry->blocked_tag_count;
}

GeOriginalStageObjectiveStatus ge_original_stage_objectives_build(
    GeOriginalStageObjectiveRegistry *registry,
    const GeOriginalStageSetupRuntime *setup,
    const GeOriginalStageObjectiveProviders *providers)
{
    GeOriginalStageObjectiveRegistry next;
    GeOriginalStageObjectiveStatus status;
    size_t tag_capacity, text_capacity, objective_capacity, criteria_capacity;
    size_t record_index;
    int32_t current_objective = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    if (registry == NULL || setup == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ARGUMENT;
    if (!setup->loaded || setup->prop_records == NULL
            || setup->prop_record_count == 0U)
        return GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_SETUP;
    memset(&next, 0, sizeof(next));
    next.tag_head = next.text_head = next.enter_room_head =
        next.deposit_room_head = next.photograph_head =
            GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    next.objective_count = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    for (record_index = 0U;
            record_index < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++record_index)
        next.objective_by_menu[record_index] =
            GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    status = ge_count_records(setup, &tag_capacity, &text_capacity,
        &objective_capacity, &criteria_capacity);
    if (status != GE_ORIGINAL_STAGE_OBJECTIVE_OK) return status;
    next.tags = tag_capacity != 0U
        ? calloc(tag_capacity, sizeof(*next.tags)) : NULL;
    next.texts = text_capacity != 0U
        ? calloc(text_capacity, sizeof(*next.texts)) : NULL;
    next.objectives = objective_capacity != 0U
        ? calloc(objective_capacity, sizeof(*next.objectives)) : NULL;
    next.criteria = criteria_capacity != 0U
        ? calloc(criteria_capacity, sizeof(*next.criteria)) : NULL;
    if ((tag_capacity != 0U && next.tags == NULL)
            || (text_capacity != 0U && next.texts == NULL)
            || (objective_capacity != 0U && next.objectives == NULL)
            || (criteria_capacity != 0U && next.criteria == NULL)) {
        ge_original_stage_objectives_close(&next);
        return GE_ORIGINAL_STAGE_OBJECTIVE_NO_MEMORY;
    }
    next.setup = setup;
    for (record_index = 0U; record_index < setup->prop_record_count;
            ++record_index) {
        const GeOriginalStagePropRecord *record =
            &setup->prop_records[record_index];
        if (record->type == PROPDEF_TAG) {
            GeOriginalStageTagEntry *tag = &next.tags[next.tag_count];
            if (record->relation_count == 0U || record->relations[0] == NULL) {
                status = GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_RELATION;
                goto fail;
            }
            tag->record = record;
            tag->command_index = record_index;
            tag->target_command_index = ge_record_index(
                setup, record->relations[0]);
            tag->tag_id = record->tag_id;
            tag->next_tag = next.tag_head;
            next.tag_head = (int32_t)next.tag_count++;
            ge_bind_tag_object(&next, tag, providers);
        } else if (record->type == PROPDEF_WATCH_MENU_OBJECTIVE_TEXT) {
            GeOriginalStageObjectiveTextEntry *text =
                &next.texts[next.text_count];
            text->record = record;
            text->command_index = record_index;
            text->menu = record->words[1];
            text->reserved = (uint16_t)(record->words[2] >> 16U);
            text->text_id = (uint16_t)record->words[2];
            text->next_text = next.text_head;
            next.text_head = (int32_t)next.text_count++;
        } else if (record->type == PROPDEF_OBJECTIVE_START) {
            GeOriginalStageObjectiveEntry *objective;
            const uint32_t menu = record->words[1];
            if (current_objective >= 0
                    || menu >= GE_ORIGINAL_STAGE_OBJECTIVE_MAX) {
                status = GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ORDER;
                goto fail;
            }
            current_objective = (int32_t)next.objective_entry_count;
            objective = &next.objectives[next.objective_entry_count++];
            objective->record = record;
            objective->command_index = record_index;
            objective->end_command_index = SIZE_MAX;
            objective->first_criterion = next.criterion_count;
            objective->menu = menu;
            objective->reserved = (uint16_t)(record->words[2] >> 16U);
            objective->text_id = (uint16_t)record->words[2];
            objective->unknown_c = (uint16_t)(record->words[3] >> 16U);
            objective->unknown_e = (uint8_t)(record->words[3] >> 8U);
            objective->difficulty = (int8_t)record->words[3];
            /* Exact add_ptr_to_objective semantics: later entries replace a
             * menu slot and objective_count is the greatest authored menu. */
            next.objective_by_menu[menu] = current_objective;
            if (next.objective_count < (int32_t)menu)
                next.objective_count = (int32_t)menu;
        } else if (record->type == PROPDEF_OBJECTIVE_END) {
            GeOriginalStageObjectiveEntry *objective;
            if (current_objective < 0) {
                status = GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ORDER;
                goto fail;
            }
            objective = &next.objectives[current_objective];
            objective->end_command_index = record_index;
            objective->criterion_count = next.criterion_count
                - objective->first_criterion;
            current_objective = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
        } else if (current_objective >= 0
                && ge_objective_is_criterion(record->type)) {
            GeOriginalStageObjectiveCriterion *criterion =
                &next.criteria[next.criterion_count];
            size_t word_index;
            criterion->record = record;
            criterion->command_index = record_index;
            criterion->objective_index = (size_t)current_objective;
            criterion->next_same_type = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
            criterion->tag_index = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
            criterion->type = record->type;
            for (word_index = 0U; word_index < 4U; ++word_index)
                criterion->words[word_index] = word_index + 1U
                    < record->word_count ? record->words[word_index + 1U] : 0U;
            if (record->type == PROPDEF_OBJECTIVE_ENTER_ROOM) {
                criterion->value_a = (int32_t)criterion->words[0];
                criterion->status = (int32_t)criterion->words[1];
                criterion->value_b = (int32_t)criterion->words[2];
                criterion->next_same_type = next.enter_room_head;
                next.enter_room_head = (int32_t)next.criterion_count;
            } else if (record->type
                    == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM) {
                criterion->value_a = (int32_t)criterion->words[0];
                criterion->value_b = (int32_t)criterion->words[1];
                criterion->status = (int32_t)criterion->words[2];
                criterion->next_same_type = next.deposit_room_head;
                next.deposit_room_head = (int32_t)next.criterion_count;
            } else if (record->type == PROPDEF_OBJECTIVE_PHOTOGRAPH) {
                criterion->value_a = (int32_t)criterion->words[0];
                criterion->status = (int32_t)criterion->words[1];
                criterion->next_same_type = next.photograph_head;
                next.photograph_head = (int32_t)next.criterion_count;
            }
            ++next.criterion_count;
        }
    }
    if (current_objective >= 0) {
        status = GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ORDER;
        goto fail;
    }
    /* Tag lookup in the original status bodies occurs after the tag list has
     * been completed.  Bind criterion relations only after the ordered pass. */
    for (record_index = 0U; record_index < next.criterion_count;
            ++record_index) {
        GeOriginalStageObjectiveCriterion *criterion =
            &next.criteria[record_index];
        if (ge_objective_uses_tag(criterion->type)) {
            const int32_t tag_index = ge_find_tag_index(
                &next, (uint16_t)criterion->words[0]);
            if (tag_index < 0) {
                status = GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_RELATION;
                goto fail;
            }
            criterion->tag_index = tag_index;
            criterion->tagged_object = next.tags[tag_index].tagged_object;
            criterion->blocker = next.tags[tag_index].blocker;
            if (criterion->blocker != GE_ORIGINAL_STAGE_OBJECTIVE_READY)
                ++next.blocked_criterion_count;
        }
    }
    next.initialized = 1U;
    ge_original_stage_objectives_close(registry);
    *registry = next;
    return GE_ORIGINAL_STAGE_OBJECTIVE_OK;

fail:
    ge_original_stage_objectives_close(&next);
    return status;
}

const GeOriginalStageTagEntry *ge_original_stage_objectives_find_tag(
    const GeOriginalStageObjectiveRegistry *registry, uint16_t tag_id)
{
    const int32_t index = registry != NULL && registry->initialized
        ? ge_find_tag_index(registry, tag_id)
        : GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    return index >= 0 ? &registry->tags[index] : NULL;
}

void ge_original_stage_objectives_close(
    GeOriginalStageObjectiveRegistry *registry)
{
    if (registry == NULL) return;
    if (registry->runtime_close != NULL)
        registry->runtime_close(registry->runtime_close_context);
    free(registry->tags);
    free(registry->texts);
    free(registry->objectives);
    free(registry->criteria);
    memset(registry, 0, sizeof(*registry));
}

const char *ge_original_stage_objective_status_name(
    GeOriginalStageObjectiveStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_OBJECTIVE_OK: return "ok";
    case GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_SETUP: return "invalid setup";
    case GE_ORIGINAL_STAGE_OBJECTIVE_NO_MEMORY: return "no memory";
    case GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ORDER:
        return "invalid objective order";
    case GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_RELATION:
        return "invalid objective relation";
    default: return "unknown";
    }
}

const char *ge_original_stage_objective_blocker_name(
    GeOriginalStageObjectiveBlocker blocker)
{
    switch (blocker) {
    case GE_ORIGINAL_STAGE_OBJECTIVE_READY: return "ready";
    case GE_ORIGINAL_STAGE_OBJECTIVE_NO_OBJECT_PROVIDER:
        return "object provider unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_OBJECT_UNAVAILABLE:
        return "tagged live object unavailable";
    case GE_ORIGINAL_STAGE_OBJECTIVE_TARGET_NOT_OBJECT:
        return "tag target is not an object";
    default: return "unknown";
    }
}

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include "bondconstants.h"
#include "ge_original_stage_interactive_objects.h"

#include <stdlib.h>
#include <string.h>

static int ge_interactive_type(uint8_t type)
{
    return type == PROPDEF_DOOR || type == PROPDEF_KEY
        || type == PROPDEF_COLLECTABLE || type == PROPDEF_HAT;
}

static uint32_t ge_interactive_skip_mask(
    const GeOriginalStageInteractiveProviders *providers)
{
    uint32_t mask = UINT32_C(1) << (providers->difficulty + 4U);
    if (providers->player_count >= 2U)
        mask |= UINT32_C(1) << (providers->player_count + 20U);
    return mask;
}

static void ge_interactive_set_blocker(
    GeOriginalStageInteractiveRuntime *runtime,
    GeOriginalStageInteractiveEntry *entry,
    GeOriginalStageInteractiveBlocker blocker)
{
    entry->blocker = blocker;
    if ((unsigned)blocker < GE_ORIGINAL_STAGE_INTERACTIVE_BLOCKER_COUNT)
        ++runtime->report.blocker_counts[blocker];
}

static int32_t ge_interactive_entry_for_record(
    const GeOriginalStageInteractiveRuntime *runtime,
    const GeOriginalStagePropRecord *record)
{
    size_t index;
    for (index = 0U; index < runtime->entry_count; ++index)
        if (runtime->entries[index].record == record) return (int32_t)index;
    return -1;
}

static void ge_interactive_construct(
    GeOriginalStageInteractiveRuntime *runtime,
    GeOriginalStageInteractiveEntry *entry,
    const GeOriginalStagePropConstructionRequest *request,
    uint32_t skip_mask)
{
    const uint8_t type = entry->type;
    const uint32_t flags = request->record->words[2];
    const uint32_t flags2 = request->record->words[3];
    int result;
    if ((flags2 & skip_mask) != 0U) {
        ge_interactive_set_blocker(runtime, entry,
            GE_ORIGINAL_STAGE_INTERACTIVE_DIFFICULTY_FILTERED);
        return;
    }
    if (type != PROPDEF_DOOR && (flags & PROPFLAG_ASSIGNEDTOCHR) != 0U) {
        ++runtime->report.expected_item_constructions;
        if (runtime->providers.resolve_assigned_item == NULL
                || !runtime->providers.resolve_assigned_item(
                    runtime->providers.context, request,
                    &entry->prop, &entry->model_instance)
                || entry->prop == NULL || entry->model_instance == NULL) {
            ge_interactive_set_blocker(runtime, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_ASSIGNED_ITEM_SERVICE);
            return;
        }
        entry->externally_owned = 1U;
        goto constructed;
    }
    if (type == PROPDEF_COLLECTABLE && entry->weapon_id == ITEM_UNARMED) {
        ge_interactive_set_blocker(runtime, entry,
            GE_ORIGINAL_STAGE_INTERACTIVE_CANONICAL_NO_OBJECT);
        return;
    }
    if (type != PROPDEF_DOOR
            && (flags & PROPFLAG_INSIDEANOTHEROBJ) != 0U) {
        const int64_t owner=(int64_t)entry->command_index+request->pad_id;
        if (owner < 0 || owner >= (int64_t)runtime->setup->prop_record_count) {
            ge_interactive_set_blocker(runtime, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_EMBEDDED_ITEM_SERVICE);
            return;
        }
        entry->owner_command_index=(int32_t)owner;
        ++runtime->report.expected_item_constructions;
        if (runtime->providers.model_available == NULL
                || !runtime->providers.model_available(
                    runtime->providers.context, request->model_id)) {
            ge_interactive_set_blocker(runtime, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_MODEL_UNAVAILABLE);
            return;
        }
        if (type == PROPDEF_COLLECTABLE
                && (runtime->providers.load_projectile_models == NULL
                    || !runtime->providers.load_projectile_models(
                        runtime->providers.context, entry->weapon_id))) {
            ge_interactive_set_blocker(runtime, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_PROJECTILE_MODEL_SERVICE);
            return;
        }
        if (runtime->providers.construct_embedded_item == NULL
                || runtime->providers.release_object == NULL) {
            ge_interactive_set_blocker(runtime, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_EMBEDDED_ITEM_SERVICE);
            return;
        }
        result=runtime->providers.construct_embedded_item(
            runtime->providers.context,request,entry->owner_command_index,
            entry->definition,entry->definition_size,&entry->prop,
            &entry->model_instance);
        if(!result||entry->prop==NULL||entry->model_instance==NULL){
            ge_interactive_set_blocker(runtime,entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_CONSTRUCTION_FAILED);
            return;
        }
        goto constructed;
    }
    if (type != PROPDEF_DOOR && request->pad_id < 0) {
        ge_interactive_set_blocker(runtime, entry,
            GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_NEGATIVE_PAD);
        return;
    }
    if (type == PROPDEF_DOOR && request->placement_resolved
            && request->placement.stan == NULL) {
        /* Exact setupDoor calls getposstan(pad->pos, pad->stan, 0). A NULL
         * authored/matched STAN returns zero and the function exits without
         * doorInit, prop publication, room registration or door linkage. */
        ++runtime->report.canonical_skipped_door_no_stan;
        ge_interactive_set_blocker(runtime, entry,
            GE_ORIGINAL_STAGE_INTERACTIVE_CANONICAL_NO_STAN);
        return;
    }
    if (!request->placement_resolved || request->placement.stan == NULL
            || request->placement.room < 0) {
        ge_interactive_set_blocker(runtime, entry,
            GE_ORIGINAL_STAGE_INTERACTIVE_PLACEMENT_UNRESOLVED);
        return;
    }
    if (type == PROPDEF_DOOR)
        ++runtime->report.expected_door_constructions;
    else ++runtime->report.expected_item_constructions;
    entry->room = request->placement.room;
    if (runtime->providers.model_available == NULL
            || !runtime->providers.model_available(
                runtime->providers.context, request->model_id)) {
        ge_interactive_set_blocker(runtime, entry,
            GE_ORIGINAL_STAGE_INTERACTIVE_MODEL_UNAVAILABLE);
        return;
    }
    if (type == PROPDEF_DOOR) {
        if (runtime->providers.construct_door == NULL
                || runtime->providers.release_object == NULL) {
            ge_interactive_set_blocker(runtime, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_DOOR_CONSTRUCTION_SERVICE);
            return;
        }
        result = runtime->providers.construct_door(
            runtime->providers.context, request, entry->definition,
            entry->definition_size, &entry->prop, &entry->model_instance);
    } else {
        if (type == PROPDEF_COLLECTABLE) {
            if (runtime->providers.load_projectile_models == NULL
                    || !runtime->providers.load_projectile_models(
                        runtime->providers.context, entry->weapon_id)) {
                ge_interactive_set_blocker(runtime, entry,
                    GE_ORIGINAL_STAGE_INTERACTIVE_PROJECTILE_MODEL_SERVICE);
                return;
            }
        }
        if (runtime->providers.construct_default_object == NULL
                || runtime->providers.release_object == NULL) {
            ge_interactive_set_blocker(runtime, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_DEFAULT_OBJECT_SERVICE);
            return;
        }
        result = runtime->providers.construct_default_object(
            runtime->providers.context, request, entry->definition,
            entry->definition_size, &entry->prop, &entry->model_instance);
    }
    if (!result || entry->prop == NULL || entry->model_instance == NULL) {
        ge_interactive_set_blocker(runtime, entry,
            GE_ORIGINAL_STAGE_INTERACTIVE_CONSTRUCTION_FAILED);
        return;
    }
constructed:
    entry->constructed = 1U;
    ++runtime->report.constructed;
    if (type == PROPDEF_DOOR) ++runtime->report.constructed_doors;
    else ++runtime->report.constructed_items;
    ge_interactive_set_blocker(runtime, entry,
        GE_ORIGINAL_STAGE_INTERACTIVE_READY);
}

static void ge_interactive_link_doors(
    GeOriginalStageInteractiveRuntime *runtime)
{
    size_t index;
    for (index = 0U; index < runtime->entry_count; ++index) {
        GeOriginalStageInteractiveEntry *entry = &runtime->entries[index];
        int32_t linked;
        if (entry->type != PROPDEF_DOOR || entry->record->relation_count == 0U)
            continue;
        linked = ge_interactive_entry_for_record(
            runtime, entry->record->relations[0]);
        entry->linked_entry = linked;
        if (linked < 0 || runtime->entries[linked].type != PROPDEF_DOOR) {
            if (entry->blocker == GE_ORIGINAL_STAGE_INTERACTIVE_READY) {
                --runtime->report.blocker_counts[entry->blocker];
                ge_interactive_set_blocker(runtime, entry,
                    GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DOOR_RELATION);
            }
            continue;
        }
        /* A canonical linked pair normally points in both directions. Process
         * its storage/link callback once, in setup-record order. */
        if ((size_t)linked <= index) continue;
        if (!entry->constructed || !runtime->entries[linked].constructed)
            continue;
        if (runtime->providers.link_doors == NULL
                || !runtime->providers.link_doors(
                    runtime->providers.context, entry->definition,
                    runtime->entries[linked].definition)) {
            --runtime->report.blocker_counts[entry->blocker];
            ge_interactive_set_blocker(runtime, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_DOOR_LINK_SERVICE);
            continue;
        }
        ++runtime->report.linked_door_pairs;
    }
}

int ge_original_stage_interactive_materialize(
    const GeOriginalStageSetupRuntime *setup,
    const GeOriginalStageInteractiveProviders *providers,
    GeOriginalStageInteractiveRuntime *runtime)
{
    GeOriginalStageInteractiveRuntime next;
    uint32_t skip_mask;
    size_t count = 0U;
    size_t record_index;
    size_t entry_index = 0U;
    if (setup == NULL || providers == NULL || runtime == NULL
            || setup->loaded == 0U || providers->difficulty > 2U
            || providers->player_count == 0U || providers->player_count > 4U)
        return 0;
    memset(&next, 0, sizeof(next));
    for (record_index = 0U; record_index < setup->prop_record_count;
            ++record_index)
        count += ge_interactive_type(setup->prop_records[record_index].type);
    if (count != 0U) {
        next.entries = calloc(count, sizeof(*next.entries));
        if (next.entries == NULL) return 0;
    }
    next.setup = setup;
    next.providers = *providers;
    next.entry_count = count;
    next.report.records = count;
    skip_mask = ge_interactive_skip_mask(providers);
    for (record_index = 0U; record_index < setup->prop_record_count;
            ++record_index) {
        const GeOriginalStagePropRecord *record =
            &setup->prop_records[record_index];
        GeOriginalStageInteractiveEntry *entry;
        GeOriginalStagePropConstructionRequest request;
        if (!ge_interactive_type(record->type)) continue;
        entry = &next.entries[entry_index++];
        entry->record = record;
        entry->command_index = record_index;
        entry->linked_entry = -1;
        entry->model_id = record->model_id;
        entry->pad_id = record->pad_id;
        entry->owner_command_index = -1;
        entry->room = -1;
        entry->type = record->type;
        switch (record->type) {
        case PROPDEF_DOOR: ++next.report.door_records; break;
        case PROPDEF_KEY: ++next.report.key_records; break;
        case PROPDEF_COLLECTABLE:
            ++next.report.collectable_records;
            entry->weapon_id = (int8_t)(record->words[32] >> 24U);
            break;
        case PROPDEF_HAT: ++next.report.hat_records; break;
        default: break;
        }
        if (!ge_original_stage_prop_construction_request(
                setup, record_index, &request)) {
            ge_interactive_set_blocker(&next, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DEFINITION);
            continue;
        }
        entry->definition_size =
            ge_original_stage_prop_native_definition_size(&request);
        if (entry->definition_size == 0U) {
            ge_interactive_set_blocker(&next, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DEFINITION);
            continue;
        }
        entry->definition = calloc(1U, entry->definition_size);
        if (entry->definition == NULL
                || !ge_original_stage_prop_native_definition_init(
                    &request, entry->definition, entry->definition_size)) {
            ge_interactive_set_blocker(&next, entry,
                GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DEFINITION);
            continue;
        }
        ++next.report.definitions;
        next.report.definition_bytes += entry->definition_size;
        ge_interactive_construct(&next, entry, &request, skip_mask);
    }
    ge_interactive_link_doors(&next);
    next.loaded = 1U;
    *runtime = next;
    return 1;
}

const GeOriginalStageInteractiveEntry *ge_original_stage_interactive_entry(
    const GeOriginalStageInteractiveRuntime *runtime, size_t index)
{
    return runtime != NULL && runtime->loaded != 0U
            && index < runtime->entry_count
        ? &runtime->entries[index] : NULL;
}

size_t ge_original_stage_interactive_expected_door_count(
    const GeOriginalStageInteractiveRuntime *runtime)
{
    return runtime != NULL && runtime->loaded != 0U
        ? runtime->report.expected_door_constructions : 0U;
}

size_t ge_original_stage_interactive_expected_item_count(
    const GeOriginalStageInteractiveRuntime *runtime)
{
    return runtime != NULL && runtime->loaded != 0U
        ? runtime->report.expected_item_constructions : 0U;
}

size_t ge_original_stage_interactive_live_item_count(
    const GeOriginalStageInteractiveRuntime *runtime)
{
    return runtime != NULL && runtime->loaded != 0U
        ? runtime->report.constructed_items : 0U;
}

int ge_original_stage_interactive_active_item(
    const GeOriginalStageInteractiveRuntime *runtime,size_t active_index,
    size_t *command_index,void **prop)
{
    size_t index,cursor=0U;
    if(runtime==NULL||!runtime->loaded||command_index==NULL||prop==NULL)
        return 0;
    for(index=0U;index<runtime->entry_count;++index){
        const GeOriginalStageInteractiveEntry *entry=&runtime->entries[index];
        if(entry->type==PROPDEF_DOOR||!entry->constructed)continue;
        if(cursor++==active_index){
            *command_index=entry->command_index;*prop=entry->prop;return 1;
        }
    }
    return 0;
}

size_t ge_original_stage_interactive_root_item_count(
    const GeOriginalStageInteractiveRuntime *runtime)
{
    size_t index,count=0U;
    if(runtime==NULL||!runtime->loaded)return 0U;
    for(index=0U;index<runtime->entry_count;++index){
        const GeOriginalStageInteractiveEntry *entry=&runtime->entries[index];
        if(entry->type!=PROPDEF_DOOR&&entry->constructed
                &&!entry->externally_owned&&entry->owner_command_index<0)
            ++count;
    }
    return count;
}

int ge_original_stage_interactive_root_item(
    const GeOriginalStageInteractiveRuntime *runtime,size_t root_index,
    size_t *command_index,void **prop)
{
    size_t index,cursor=0U;
    if(runtime==NULL||!runtime->loaded||command_index==NULL||prop==NULL)
        return 0;
    for(index=0U;index<runtime->entry_count;++index){
        const GeOriginalStageInteractiveEntry *entry=&runtime->entries[index];
        if(entry->type==PROPDEF_DOOR||!entry->constructed
                ||entry->externally_owned||entry->owner_command_index>=0)
            continue;
        if(cursor++==root_index){
            *command_index=entry->command_index;*prop=entry->prop;return 1;
        }
    }
    return 0;
}

void ge_original_stage_interactive_close(
    GeOriginalStageInteractiveRuntime *runtime)
{
    size_t index;
    if (runtime == NULL) return;
    if (runtime->providers.release_object != NULL) {
        for (index = runtime->entry_count; index > 0U; --index) {
            GeOriginalStageInteractiveEntry *entry = &runtime->entries[index - 1U];
            if (entry->constructed && !entry->externally_owned)
                runtime->providers.release_object(runtime->providers.context,
                    entry->definition, entry->prop, entry->model_instance);
        }
    }
    for (index = 0U; index < runtime->entry_count; ++index)
        free(runtime->entries[index].definition);
    free(runtime->entries);
    memset(runtime, 0, sizeof(*runtime));
}

const char *ge_original_stage_interactive_blocker_name(
    GeOriginalStageInteractiveBlocker blocker)
{
    switch (blocker) {
    case GE_ORIGINAL_STAGE_INTERACTIVE_READY: return "ready";
    case GE_ORIGINAL_STAGE_INTERACTIVE_DIFFICULTY_FILTERED:
        return "difficulty/player filtered";
    case GE_ORIGINAL_STAGE_INTERACTIVE_CANONICAL_NO_OBJECT:
        return "canonical no object";
    case GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DEFINITION:
        return "invalid definition";
    case GE_ORIGINAL_STAGE_INTERACTIVE_PLACEMENT_UNRESOLVED:
        return "placement unresolved";
    case GE_ORIGINAL_STAGE_INTERACTIVE_CANONICAL_NO_STAN:
        return "canonical setupDoor skip: no STAN";
    case GE_ORIGINAL_STAGE_INTERACTIVE_ASSIGNED_ITEM_SERVICE:
        return "assigned item service";
    case GE_ORIGINAL_STAGE_INTERACTIVE_EMBEDDED_ITEM_SERVICE:
        return "embedded item service";
    case GE_ORIGINAL_STAGE_INTERACTIVE_MODEL_UNAVAILABLE:
        return "model unavailable";
    case GE_ORIGINAL_STAGE_INTERACTIVE_PROJECTILE_MODEL_SERVICE:
        return "projectile model service";
    case GE_ORIGINAL_STAGE_INTERACTIVE_DEFAULT_OBJECT_SERVICE:
        return "default object service";
    case GE_ORIGINAL_STAGE_INTERACTIVE_DOOR_CONSTRUCTION_SERVICE:
        return "door construction service";
    case GE_ORIGINAL_STAGE_INTERACTIVE_CONSTRUCTION_FAILED:
        return "construction failed";
    case GE_ORIGINAL_STAGE_INTERACTIVE_DOOR_LINK_SERVICE:
        return "door link service";
    case GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DOOR_RELATION:
        return "invalid door relation";
    case GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_NEGATIVE_PAD:
        return "negative pad without embedded-owner flag";
    default: return "unknown";
    }
}

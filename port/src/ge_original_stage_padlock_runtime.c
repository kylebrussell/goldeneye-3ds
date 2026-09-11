#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_padlock_runtime.h"

#include <stdlib.h>
#include <string.h>

/* Owned by the retained prop/chrprop scheduler slice. */
extern LockDoorRecord *g_LevelLoadPropLockDoor;
extern bool doorIsPadlockFree(DoorRecord *door);
static GeOriginalStagePadlockRuntime *active_runtime;

int ge_original_stage_padlock_runtime_is_free(void *context, void *definition)
{
    ObjectRecord *object = definition;
    if (context == NULL || context != active_runtime || object == NULL
            || object->type != PROPDEF_DOOR) return 0;
    return doorIsPadlockFree((DoorRecord *)definition);
}

void ge_original_stage_padlock_runtime_close(
    GeOriginalStagePadlockRuntime *runtime)
{
    if (runtime == NULL) return;
    if (active_runtime == runtime) {
        LockDoorRecord *links = runtime->links;
        size_t index;
        g_LevelLoadPropLockDoor = NULL;
        for (index = 0U; index < runtime->relation_count; ++index) {
            if (links[index].door != NULL)
                links[index].door->runtime_bitflags
                    &= ~RUNTIMEBITFLAG_PADLOCKEDDOOR;
        }
        active_runtime = NULL;
    }
    free(runtime->links);
    memset(runtime, 0, sizeof(*runtime));
}

int ge_original_stage_padlock_runtime_materialize(
    GeOriginalStagePadlockRuntime *runtime,
    const GeOriginalStageSetupRuntime *setup, void *context,
    void *(*definition_by_command)(void *context, size_t command_index))
{
    LockDoorRecord *links;
    size_t index, capacity = 0U;
    if (runtime == NULL || setup == NULL || !setup->loaded
            || definition_by_command == NULL
            || (setup->prop_record_count != 0U && setup->prop_records == NULL)
            || (active_runtime != NULL && active_runtime != runtime)) return 0;
    ge_original_stage_padlock_runtime_close(runtime);
    for (index = 0U; index < setup->prop_record_count; ++index)
        if (setup->prop_records[index].type == PROPDEF_LOCK_DOOR) ++capacity;
    links = capacity != 0U ? calloc(capacity, sizeof(*links)) : NULL;
    if (capacity != 0U && links == NULL) return 0;
    runtime->links = links;
    active_runtime = runtime;
    g_LevelLoadPropLockDoor = NULL;
    for (index = 0U; index < setup->prop_record_count; ++index) {
        const GeOriginalStagePropRecord *record = &setup->prop_records[index];
        ObjectRecord *first, *second;
        int64_t first_index, second_index;
        LockDoorRecord *link;
        if (record->type != PROPDEF_LOCK_DOOR) continue;
        if (record->words == NULL || record->word_count < 4U) goto invalid;
        first_index = (int64_t)index + (int32_t)record->words[1];
        second_index = (int64_t)index + (int32_t)record->words[2];
        if (first_index < 0 || second_index < 0
                || (uint64_t)first_index >= setup->prop_record_count
                || (uint64_t)second_index >= setup->prop_record_count)
            goto invalid;
        first = definition_by_command(context, (size_t)first_index);
        second = definition_by_command(context, (size_t)second_index);
        /* Exact proplvreset2 PROPDEF_LOCK_DOOR endpoint checks and publication.
         * The serialized union indices become native object pointers. */
        if (first == NULL || first->prop == NULL || second == NULL
                || first->type != PROPDEF_DOOR || second->prop == NULL) {
            ++runtime->omitted_count;
            continue;
        }
        link = &links[runtime->relation_count++];
        link->type = PROPDEF_LOCK_DOOR;
        link->door = (DoorRecord *)first;
        link->lock = second;
        link->next = g_LevelLoadPropLockDoor;
        g_LevelLoadPropLockDoor = link;
        first->runtime_bitflags |= RUNTIMEBITFLAG_PADLOCKEDDOOR;
    }
    return 1;
invalid:
    ge_original_stage_padlock_runtime_close(runtime);
    return 0;
}

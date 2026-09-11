#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_switch_runtime.h"

#include <stdlib.h>
#include <string.h>

/* Owned by the retained prop/chrprop scheduler slice. */
extern ObjectRecord *g_LevelLoadPropSwitch;
static GeOriginalStageSwitchRuntime *active_runtime;

void ge_original_stage_switch_runtime_close(
    GeOriginalStageSwitchRuntime *runtime)
{
    if (runtime == NULL) return;
    if (active_runtime == runtime) {
        LinkRecord *links = runtime->links;
        size_t index;
        g_LevelLoadPropSwitch = NULL;
        for (index = 0U; index < runtime->relation_count; ++index) {
            if (links[index].first != NULL
                    && links[index].first->obj != NULL)
                links[index].first->obj->runtime_bitflags
                    &= ~RUNTIMEBITFLAG_00000001;
        }
        active_runtime = NULL;
    }
    free(runtime->links);
    memset(runtime, 0, sizeof(*runtime));
}

int ge_original_stage_switch_runtime_materialize(
    GeOriginalStageSwitchRuntime *runtime,
    const GeOriginalStageSetupRuntime *setup, void *context,
    void *(*definition_by_command)(void *context, size_t command_index))
{
    LinkRecord *links;
    size_t index, capacity = 0U;
    if (runtime == NULL || setup == NULL || !setup->loaded
            || definition_by_command == NULL
            || (setup->prop_record_count != 0U && setup->prop_records == NULL)
            || (active_runtime != NULL && active_runtime != runtime)) return 0;
    ge_original_stage_switch_runtime_close(runtime);
    for (index = 0U; index < setup->prop_record_count; ++index)
        if (setup->prop_records[index].type == PROPDEF_SWITCH) ++capacity;
    links = capacity != 0U ? calloc(capacity, sizeof(*links)) : NULL;
    if (capacity != 0U && links == NULL) return 0;
    runtime->links = links;
    active_runtime = runtime;
    g_LevelLoadPropSwitch = NULL;
    for (index = 0U; index < setup->prop_record_count; ++index) {
        const GeOriginalStagePropRecord *record = &setup->prop_records[index];
        ObjectRecord *first, *second;
        int64_t first_index, second_index;
        LinkRecord *link;
        if (record->type != PROPDEF_SWITCH) continue;
        if (record->words == NULL || record->word_count < 4U) goto invalid;
        first_index = (int64_t)index + (int32_t)record->words[1];
        second_index = (int64_t)index + (int32_t)record->words[2];
        if (first_index < 0 || second_index < 0
                || (uint64_t)first_index >= setup->prop_record_count
                || (uint64_t)second_index >= setup->prop_record_count)
            goto invalid;
        first = definition_by_command(context, (size_t)first_index);
        second = definition_by_command(context, (size_t)second_index);
        /* Exact proplvreset2 PROPDEF_SWITCH endpoint checks and publication.
         * The serialized union indices become native PropRecord pointers. */
        if (first == NULL || first->prop == NULL || second == NULL
                || second->type != PROPDEF_DOOR || second->prop == NULL) {
            ++runtime->omitted_count;
            continue;
        }
        link = &links[runtime->relation_count++];
        link->type = PROPDEF_SWITCH;
        link->first = first->prop;
        link->second = second->prop;
        link->next = (LinkRecord *)(void *)g_LevelLoadPropSwitch;
        g_LevelLoadPropSwitch = (ObjectRecord *)(void *)link;
        first->runtime_bitflags |= RUNTIMEBITFLAG_00000001;
    }
    return 1;
invalid:
    ge_original_stage_switch_runtime_close(runtime);
    return 0;
}

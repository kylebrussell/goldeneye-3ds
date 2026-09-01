#include "ge_original_stage_mission_runtime.h"

#include "bondtypes.h"
#include "ge_original_stage_setup.h"

#include <string.h>

extern stagesetup g_CurrentSetup;
extern ChrRecord *g_ActiveChrs;
extern s32 g_ActiveChrsCount;
extern s32 objectiveregisters1;
extern void init_obj_register_difficulty_vals(void);
extern void alloc_false_GUARDdata_to_exec_global_action(void);

/* Exact canonical chr.c data initializer. Other AI modifier globals already
 * have retained owners in the unchanged guard/action slices; this one did not
 * become link-visible until campaign background AI initialization was live. */
f32 g_AiReactionSpeed = 1.0f;

static size_t ge_stage_background_list_count(const stagesetup *setup)
{
    size_t count = 0U;
    size_t index;

    if (setup == NULL || setup->ailists == NULL) return 0U;
    for (index = 0U; setup->ailists[index].ailist != NULL; ++index) {
        if (setup->ailists[index].ID >= 0x1000) ++count;
    }
    return count;
}

static uint64_t ge_stage_background_offset_hash(void)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    s32 index;

    for (index = 0; index < g_ActiveChrsCount; ++index) {
        const uint32_t fields[2] = {
            (uint32_t)g_ActiveChrs[index].aioffset,
            (uint32_t)g_ActiveChrs[index].chrnum,
        };
        size_t byte_index;
        const uint8_t *bytes = (const uint8_t *)fields;
        for (byte_index = 0U; byte_index < sizeof(fields); ++byte_index) {
            hash ^= bytes[byte_index];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

void ge_original_stage_mission_runtime_reset_globals(
    GeOriginalStageMissionRuntime *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    init_obj_register_difficulty_vals();
    state->globals_reset = 1U;
}

GeOriginalStageMissionRuntimeStatus ge_original_stage_mission_runtime_begin(
    GeOriginalStageMissionRuntime *state,
    const GeOriginalStageSetupRuntime *setup)
{
    size_t expected;
    s32 index;

    if (state == NULL || setup == NULL || setup->setup == NULL
            || setup->loaded == 0U) {
        return GE_ORIGINAL_STAGE_MISSION_RUNTIME_INVALID_ARGUMENT;
    }
    if (state->globals_reset == 0U) {
        return GE_ORIGINAL_STAGE_MISSION_RUNTIME_SETUP_UNBOUND;
    }
    if (g_CurrentSetup.ailists != setup->setup->ailists
            || g_CurrentSetup.propDefs != setup->setup->propDefs) {
        return GE_ORIGINAL_STAGE_MISSION_RUNTIME_SETUP_UNBOUND;
    }
    expected = ge_stage_background_list_count(setup->setup);

    /* Original proplvreset2 order: allocate one synthetic ChrRecord for every
     * setup AI list in the 0x1000 background namespace after props. */
    alloc_false_GUARDdata_to_exec_global_action();
    if ((expected != 0U && g_ActiveChrs == NULL)
            || g_ActiveChrsCount < 0
            || (size_t)g_ActiveChrsCount != expected) {
        return GE_ORIGINAL_STAGE_MISSION_RUNTIME_ALLOCATION_FAILED;
    }
    for (index = 0; index < g_ActiveChrsCount; ++index) {
        if (g_ActiveChrs[index].chrnum != 0xfe
                || g_ActiveChrs[index].ailist == NULL
                || g_ActiveChrs[index].aioffset != 0
                || g_ActiveChrs[index].aireturnlist != -1
                || g_ActiveChrs[index].actiontype != ACT_NULL) {
            return GE_ORIGINAL_STAGE_MISSION_RUNTIME_ACTOR_MISMATCH;
        }
    }
    state->setup = setup;
    state->authored_background_list_count = expected;
    state->live_background_actor_count = (size_t)g_ActiveChrsCount;
    state->ai_offset_hash = ge_stage_background_offset_hash();
    state->objective_registers = (uint32_t)objectiveregisters1;
    state->globals_reset = 1U;
    state->initialized = 1U;
    return GE_ORIGINAL_STAGE_MISSION_RUNTIME_OK;
}

GeOriginalStageMissionRuntimeStatus ge_original_stage_mission_runtime_observe_tick(
    GeOriginalStageMissionRuntime *state)
{
    if (state == NULL || state->initialized == 0U || state->setup == NULL) {
        return GE_ORIGINAL_STAGE_MISSION_RUNTIME_INVALID_ARGUMENT;
    }
    if (g_CurrentSetup.ailists != state->setup->setup->ailists
            || g_CurrentSetup.propDefs != state->setup->setup->propDefs) {
        return GE_ORIGINAL_STAGE_MISSION_RUNTIME_SETUP_UNBOUND;
    }
    if (g_ActiveChrsCount < 0
            || (size_t)g_ActiveChrsCount
                != state->live_background_actor_count
            || (g_ActiveChrsCount != 0 && g_ActiveChrs == NULL)) {
        return GE_ORIGINAL_STAGE_MISSION_RUNTIME_ACTOR_MISMATCH;
    }
    state->ai_offset_hash = ge_stage_background_offset_hash();
    state->objective_registers = (uint32_t)objectiveregisters1;
    ++state->observed_ticks;
    return GE_ORIGINAL_STAGE_MISSION_RUNTIME_OK;
}

int ge_original_stage_mission_runtime_actor_offset(
    const GeOriginalStageMissionRuntime *state, int32_t ai_list_id,
    uint16_t *offset)
{
    size_t background_index = 0U;
    size_t list_index;

    if (state == NULL || state->initialized == 0U || state->setup == NULL
            || state->setup->setup == NULL
            || state->setup->setup->ailists == NULL || offset == NULL)
        return 0;
    for (list_index = 0U;
            state->setup->setup->ailists[list_index].ailist != NULL;
            ++list_index) {
        if (state->setup->setup->ailists[list_index].ID < 0x1000) continue;
        if (state->setup->setup->ailists[list_index].ID == ai_list_id)
            break;
        ++background_index;
    }
    if (state->setup->setup->ailists[list_index].ailist == NULL
            || g_ActiveChrs == NULL || background_index >= (size_t)g_ActiveChrsCount)
        return 0;
    /* The allocator preserves setup-table order. Keep resolving that authored
     * actor even after a canonical jump_to_ai_list changes its current list. */
    *offset = g_ActiveChrs[background_index].aioffset;
    return 1;
}

void ge_original_stage_mission_runtime_close(
    GeOriginalStageMissionRuntime *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
}

const char *ge_original_stage_mission_runtime_status_name(
    GeOriginalStageMissionRuntimeStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_MISSION_RUNTIME_OK: return "ok";
    case GE_ORIGINAL_STAGE_MISSION_RUNTIME_INVALID_ARGUMENT:
        return "invalid_argument";
    case GE_ORIGINAL_STAGE_MISSION_RUNTIME_SETUP_UNBOUND:
        return "setup_unbound";
    case GE_ORIGINAL_STAGE_MISSION_RUNTIME_ALLOCATION_FAILED:
        return "allocation_failed";
    case GE_ORIGINAL_STAGE_MISSION_RUNTIME_ACTOR_MISMATCH:
        return "actor_mismatch";
    }
    return "unknown";
}

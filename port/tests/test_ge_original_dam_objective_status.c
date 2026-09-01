#include "ge_original_dam_objective_status.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static size_t default_hud_count;
static char default_hud[GE_ORIGINAL_DAM_OBJECTIVE_STATUS_MESSAGE_CAPACITY];

typedef struct DamObjectiveHarness {
    uint32_t flags;
    size_t hud_count;
    char hud[GE_ORIGINAL_STAGE_OBJECTIVE_MAX]
            [GE_ORIGINAL_DAM_OBJECTIVE_STATUS_MESSAGE_CAPACITY];
} DamObjectiveHarness;

void hudmsgBottomShow(char *message)
{
    ++default_hud_count;
    strncpy(default_hud, message, sizeof(default_hud) - 1U);
}

int lvlGetSelectedDifficulty(void)
{
    return 2;
}

int ge_original_stage_objective_photograph_bounds_inside_view_exact(
    void *context, const void *object, const void *prop)
{
    (void)context;
    (void)object;
    (void)prop;
    return -1;
}

static int stage_flag_set(void *context, uint32_t flags)
{
    return ((((DamObjectiveHarness *)context)->flags & flags) != 0U);
}

static void capture_hud(void *context, const char *message)
{
    DamObjectiveHarness *harness = context;
    assert(harness->hud_count < GE_ORIGINAL_STAGE_OBJECTIVE_MAX);
    strncpy(harness->hud[harness->hud_count], message,
        GE_ORIGINAL_DAM_OBJECTIVE_STATUS_MESSAGE_CAPACITY - 1U);
    ++harness->hud_count;
}

int main(void)
{
    GeOriginalStageObjectiveRegistry registry = {0};
    GeOriginalStageObjectiveEntry objectives[4] = {{0}};
    GeOriginalStageObjectiveCriterion criteria[10] = {{0}};
    ObjectRecord tagged_objects[4] = {{0}};
    PropRecord tagged_props[4] = {{0}};
    GeOriginalStageObjectiveRuntimeProviders runtime_providers = {0};
    GeOriginalStageObjectiveRuntime runtime = {0};
    GeOriginalDamObjectiveStatusProviders providers = {0};
    GeOriginalDamObjectiveStatusSnapshot snapshot;
    DamObjectiveHarness harness = {0};
    size_t index;
    registry.initialized = 1U;
    registry.objectives = objectives;
    registry.objective_entry_count = 4U;
    registry.criteria = criteria;
    registry.criterion_count = 10U;
    registry.objective_count = 3;
    for (index = 0U; index < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++index)
        registry.objective_by_menu[index] = -1;
    for (index = 0U; index < 4U; ++index)
        registry.objective_by_menu[index] = (int32_t)index;
    objectives[0].menu = 0U;
    objectives[0].difficulty = 1;
    objectives[0].first_criterion = 0U;
    objectives[0].criterion_count = 4U;
    objectives[1].menu = 1U;
    objectives[1].difficulty = 2;
    objectives[1].first_criterion = 4U;
    objectives[1].criterion_count = 3U;
    objectives[2].menu = 2U;
    objectives[2].difficulty = 2;
    objectives[2].first_criterion = 7U;
    objectives[2].criterion_count = 2U;
    objectives[3].menu = 3U;
    objectives[3].difficulty = 0;
    objectives[3].first_criterion = 9U;
    objectives[3].criterion_count = 1U;
    for (index = 0U; index < 4U; ++index) {
        tagged_objects[index].prop = &tagged_props[index];
        tagged_objects[index].state = PROPSTATE_DESTROYED;
        tagged_objects[index].runtime_bitflags = UINT32_C(0x10);
        tagged_props[index].obj = &tagged_objects[index];
        criteria[index].type = PROPDEF_OBJECTIVE_DESTROY_OBJECT;
        criteria[index].tagged_object = &tagged_objects[index];
    }
    criteria[4].type = PROPDEF_OBJECTIVE_COMPLETE_CONDITION;
    criteria[4].words[0] = UINT32_C(0x100);
    criteria[5].type = PROPDEF_OBJECTIVE_FAIL_CONDITION;
    criteria[5].words[0] = UINT32_C(0x2000);
    criteria[6].type = PROPDEF_OBJECTIVE_FAIL_CONDITION;
    criteria[6].words[0] = UINT32_C(0x800);
    criteria[7].type = PROPDEF_OBJECTIVE_COMPLETE_CONDITION;
    criteria[7].words[0] = UINT32_C(0x400);
    criteria[8].type = PROPDEF_OBJECTIVE_FAIL_CONDITION;
    criteria[8].words[0] = UINT32_C(0x200);
    criteria[9].type = PROPDEF_OBJECTIVE_COMPLETE_CONDITION;
    criteria[9].words[0] = UINT32_C(0x1000);
    runtime_providers.context = &harness;
    runtime_providers.stage_flag_set = stage_flag_set;
    assert(ge_original_stage_objective_runtime_begin(
        &runtime, &registry, &runtime_providers)
            == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
    providers.context = &harness;
    providers.hud_bottom_show = capture_hud;

    /* Dam objective A's four tagged alarms begin destroyed in this focused
     * post-damage fixture; B-D await their exact authored stage flags. */
    assert(ge_original_dam_objective_status_present(
        2, &providers, &snapshot) == GE_ORIGINAL_DAM_OBJECTIVE_STATUS_OK);
    assert(snapshot.message_count == 1U && !snapshot.mission_complete
        && !snapshot.mission_failed);
    assert(strcmp(snapshot.messages[0], "objective a: completed\n") == 0);

    harness.flags = UINT32_C(0x100) | UINT32_C(0x400)
        | UINT32_C(0x1000);
    assert(ge_original_dam_objective_status_present(
        2, &providers, &snapshot) == GE_ORIGINAL_DAM_OBJECTIVE_STATUS_OK);
    assert(snapshot.message_count == 3U && snapshot.mission_complete
        && !snapshot.mission_failed);
    assert(strcmp(snapshot.messages[0], "objective b: completed\n") == 0);
    assert(strcmp(snapshot.messages[1], "objective c: completed\n") == 0);
    assert(strcmp(snapshot.messages[2], "objective d: completed\n") == 0);
    assert(objectiveIsAllComplete());

    harness.flags |= UINT32_C(0x200);
    assert(ge_original_dam_objective_status_present(
        2, &providers, &snapshot) == GE_ORIGINAL_DAM_OBJECTIVE_STATUS_OK);
    assert(snapshot.message_count == 1U && !snapshot.mission_complete
        && snapshot.mission_failed);
    assert(strcmp(snapshot.messages[0], "objective c: failed\n") == 0);
    assert(!objectiveIsAllComplete());

    harness.flags &= ~UINT32_C(0x200);
    runtime.providers.stage_flag_set = NULL;
    assert(ge_original_dam_objective_status_present(
        2, &providers, &snapshot)
            == GE_ORIGINAL_DAM_OBJECTIVE_STATUS_RUNTIME_BLOCKED);
    assert(snapshot.message_count == 0U);
    assert(snapshot.blocked_evaluation.blocker
        == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_STAGE_FLAGS_UNAVAILABLE);
    assert(!objectiveIsAllComplete());
    assert(ge_original_dam_objective_status_last(&snapshot)
        == GE_ORIGINAL_DAM_OBJECTIVE_STATUS_RUNTIME_BLOCKED);
    assert(snapshot.blocked_evaluation.blocker
        == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_STAGE_FLAGS_UNAVAILABLE);
    assert(ge_original_dam_objective_status_present(2,
        &(GeOriginalDamObjectiveStatusProviders){0}, &snapshot)
            == GE_ORIGINAL_DAM_OBJECTIVE_STATUS_HUD_UNAVAILABLE);

    runtime.providers.stage_flag_set = stage_flag_set;
    display_objective_status_text_on_status_change();
    assert(ge_original_dam_objective_status_last(&snapshot)
        == GE_ORIGINAL_DAM_OBJECTIVE_STATUS_OK);
    assert(default_hud_count == 1U && snapshot.message_count == 1U);
    assert(strcmp(default_hud, "objective c: completed\n") == 0);

    ge_original_stage_objective_runtime_close(&runtime);
    assert(harness.hud_count == 5U);
    puts("Dam objective completion/failure HUD boundary: exact");
    return 0;
}

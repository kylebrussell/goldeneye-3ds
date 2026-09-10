/* Campaign lifecycle fixtures, not player routes or recorded completions.
 * Authored setup/objective records are real. Props, room binding, inventory,
 * flags and photo visibility are controlled providers at the native boundary. */
#include "ge_asset_pack.h"
#include "ge_3ds_save_provider.h"
#include "ge_original_stage_objective_runtime.h"
#include "ge_original_stage_objective_live.h"
#include "ge_original_mission_result.h"
#include "ge_original_dam_mission_exit_services.h"
#include "ge_stage_assets.h"
#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

s32 selected_folder_num = FOLDER1;
struct player *g_CurrentPlayer;
enum CAMERAMODE g_CameraMode;
static int current_difficulty, current_ticks, title_requests;
static LEVELID current_level;
extern s32 cameraBufferToggle, cameraFrameCounter2;
DIFFICULTY lvlGetSelectedDifficulty(void) { return current_difficulty; }
s32 getMissiontimer(void) { return current_ticks; }
LEVELID bossGetStageNum(void) { return current_level; }
void bossRunTitleStage(void) { ++title_requests; }

typedef struct Fixture {
    GeOriginalStageSetupRuntime setup;
    GeOriginalStageObjectiveRegistry registry;
    GeOriginalStageObjectiveRuntime runtime;
    ObjectRecord *objects;
    PropRecord *props;
    uint8_t *inventory;
    uint32_t flags;
    int key;
    int photo;
    StandTile room;
} Fixture;

static void *provide_object(void *context, size_t index,
    const GeOriginalStagePropRecord *record)
{
    Fixture *f = context;
    ObjectRecord *o = &f->objects[index];
    switch (record->type) {
    case PROPDEF_DOOR: case PROPDEF_PROP: case PROPDEF_KEY:
    case PROPDEF_ALARM: case PROPDEF_CCTV: case PROPDEF_MAGAZINE:
    case PROPDEF_COLLECTABLE: case PROPDEF_MONITOR:
    case PROPDEF_MULTI_MONITOR: case PROPDEF_RACK: case PROPDEF_AUTOGUN:
    case PROPDEF_HAT: case PROPDEF_AMMO: case PROPDEF_ARMOUR:
    case PROPDEF_GAS_RELEASING: case PROPDEF_VEHICHLE:
    case PROPDEF_AIRCRAFT: case PROPDEF_UNK41: case PROPDEF_GLASS:
    case PROPDEF_SAFE: case PROPDEF_TANK: case PROPDEF_TINTED_GLASS: break;
    default: return NULL;
    }
    o->prop = &f->props[index];
    o->prop->obj = o;
    o->prop->type = PROP_TYPE_OBJ;
    o->prop->flags = PROPFLAG_ONSCREEN;
    o->prop->zDepth = 1.0f;
    return o;
}

static int in_inventory(void *context, const void *prop)
{
    Fixture *f = context;
    ptrdiff_t index = (const PropRecord *)prop - f->props;
    assert(index >= 0 && (size_t)index < f->setup.prop_record_count);
    return f->inventory[index];
}
static int flag_set(void *context, uint32_t flags)
{ return (((Fixture *)context)->flags & flags) != 0U; }
static int copied_key(void *context) { return ((Fixture *)context)->key; }
static int photo_visible(void *context, const void *obj, const void *prop)
{ assert(obj != NULL && prop != NULL); return ((Fixture *)context)->photo; }
/* Runtime fallback is deliberately unavailable: this fixture always binds
 * an explicit photo provider and does not claim to exercise projection. */
int ge_original_stage_objective_photograph_bounds_inside_view_exact(
    void *context, const void *obj, const void *prop)
{ (void)context; (void)obj; (void)prop; return -1; }

static void fixture_open(Fixture *f, GeAssetPack *pack, GeStageId stage)
{
    size_t i;
    GeOriginalStageObjectiveProviders registry_providers;
    GeOriginalStageObjectiveRuntimeProviders providers;
    memset(f, 0, sizeof(*f));
    assert(ge_original_stage_setup_load(pack,
        ge_stage_asset_descriptor(stage), &f->setup) == GE_ORIGINAL_STAGE_SETUP_OK);
    f->objects = calloc(f->setup.prop_record_count, sizeof(*f->objects));
    f->props = calloc(f->setup.prop_record_count, sizeof(*f->props));
    f->inventory = calloc(f->setup.prop_record_count, 1);
    assert(f->objects && f->props && f->inventory);
    f->room.room = 7;
    /* Both wrong-room and correct-room events are tested. These are fixture
     * STAN bindings, not an assertion about any stage's physical exit room. */
    for (i = 0; i < f->setup.pad_count; ++i)
        ((PadRecord *)f->setup.pads_storage)[i].stan = &f->room;
    for (i = 0; i < f->setup.boundpad_count; ++i)
        ((BoundPadRecord *)f->setup.boundpads_storage)[i].stan = &f->room;
    registry_providers = (GeOriginalStageObjectiveProviders){f, provide_object};
    assert(ge_original_stage_objectives_build(&f->registry, &f->setup,
        &registry_providers) == GE_ORIGINAL_STAGE_OBJECTIVE_OK);
    assert(f->registry.blocked_criterion_count == 0);
    providers = (GeOriginalStageObjectiveRuntimeProviders){
        f, in_inventory, flag_set, copied_key, photo_visible};
    assert(ge_original_stage_objective_runtime_begin(&f->runtime,
        &f->registry, &providers) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
}

static void fixture_close(Fixture *f)
{
    ge_original_stage_objectives_close(&f->registry);
    assert(ge_original_stage_objective_live_count() == 0);
    free(f->inventory);
    free(f->props);
    free(f->objects);
    ge_original_stage_setup_close(&f->setup);
}

static void reject_wrong_events(Fixture *f)
{
    GeOriginalStageObjectiveEvaluation evaluation;
    size_t i;
    assert(ge_original_stage_objective_runtime_check_room_entered(&f->runtime,
        8, &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
    assert(ge_original_stage_objective_runtime_check_deposit(&f->runtime,
        -1, 7, &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
    for (i = 0; i < f->registry.criterion_count; ++i) {
        GeOriginalStageObjectiveCriterion *c = &f->registry.criteria[i];
        if (c->type == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM)
            assert(ge_original_stage_objective_runtime_check_deposit(&f->runtime,
                c->value_a, 8, &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
    }
    assert(ge_original_stage_objective_runtime_take_picture(&f->runtime,
        &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
    for (i = 0; i < f->registry.criterion_count; ++i) {
        const GeOriginalStageObjectiveCriterion *c = &f->registry.criteria[i];
        if (c->type == PROPDEF_OBJECTIVE_ENTER_ROOM
                || c->type == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM
                || c->type == PROPDEF_OBJECTIVE_PHOTOGRAPH)
            assert(c->status == 0);
    }
}

static void snapshot(Fixture *f, int difficulty,
    GeOriginalMissionOutcomeInput *input)
{
    size_t i;
    memset(input, 0, sizeof(*input));
    input->difficulty = difficulty;
    for (i = 0; i < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++i) {
        int32_t index = f->registry.objective_by_menu[i];
        GeOriginalStageObjectiveEvaluation evaluation;
        const GeOriginalStageObjectiveEntry *objective;
        if (index < 0) continue;
        objective = &f->registry.objectives[index];
        assert(ge_original_stage_objective_live_evaluate((uint8_t)i,
            &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
        input->objectives[i].text_id = objective->text_id;
        input->objectives[i].enabled_difficulty = objective->difficulty;
        input->objectives[i].status = evaluation.value;
    }
}

static int enabled(Fixture *f, size_t criterion, int difficulty)
{
    return f->registry.objectives[f->registry.criteria[criterion].objective_index]
        .difficulty <= difficulty;
}

static void complete(Fixture *f, int difficulty)
{
    size_t i;
    GeOriginalStageObjectiveEvaluation evaluation;
    f->flags = 0;
    f->key = f->photo = 1;
    memset(f->inventory, 0, f->setup.prop_record_count);
    for (i = 0; i < f->registry.criterion_count; ++i) {
        GeOriginalStageObjectiveCriterion *c = &f->registry.criteria[i];
        ObjectRecord *obj = c->tagged_object;
        if (!enabled(f, i, difficulty)) continue;
        switch (c->type) {
        case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
            obj->state |= PROPSTATE_DESTROYED; break;
        case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
            f->flags |= c->words[0]; break;
        case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
            obj->state &= ~PROPSTATE_DESTROYED;
            f->inventory[obj->prop - f->props] = 1; break;
        case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            obj->state &= ~PROPSTATE_DESTROYED; break;
        default: break;
        }
    }
    assert(ge_original_stage_objective_runtime_check_room_entered(&f->runtime,
        7, &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
    for (i = 0; i < f->registry.criterion_count; ++i) {
        GeOriginalStageObjectiveCriterion *c = &f->registry.criteria[i];
        if (c->type == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM)
            assert(ge_original_stage_objective_runtime_check_deposit(&f->runtime,
                c->value_a, 7, &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
    }
    assert(ge_original_stage_objective_runtime_take_picture(&f->runtime,
        &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
}

static size_t perturb_each_criterion(Fixture *f, int difficulty)
{
    size_t i, count = 0;
    GeOriginalMissionOutcomeInput input;
    GeOriginalMissionOutcome outcome;
    for (i = 0; i < f->registry.criterion_count; ++i) {
        GeOriginalStageObjectiveCriterion *c = &f->registry.criteria[i];
        ObjectRecord *obj = c->tagged_object;
        GeOriginalStageObjectiveEvaluation evaluation;
        uint8_t expected = GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
        if (!enabled(f, i, difficulty)) continue;
        complete(f, difficulty);
        switch (c->type) {
        case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
            obj->state &= ~PROPSTATE_DESTROYED; break;
        case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
            f->flags &= ~c->words[0]; break;
        case PROPDEF_OBJECTIVE_FAIL_CONDITION:
            f->flags |= c->words[0]; expected = GE_ORIGINAL_STAGE_OBJECTIVE_FAILED; break;
        case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
            obj->state |= PROPSTATE_DESTROYED;
            expected = GE_ORIGINAL_STAGE_OBJECTIVE_FAILED; break;
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
            f->inventory[obj->prop - f->props] = 1; break;
        case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            c->status = 0; obj->state |= PROPSTATE_DESTROYED;
            expected = GE_ORIGINAL_STAGE_OBJECTIVE_FAILED; break;
        case PROPDEF_OBJECTIVE_ENTER_ROOM:
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
            c->status = 0; break;
        case PROPDEF_OBJECTIVE_COPY_ITEM:
            f->key = 0; break;
        case PROPDEF_OBJECTIVE_NULL: continue;
        default: assert(!"unsupported campaign criterion");
        }
        assert(ge_original_stage_objective_runtime_evaluate(&f->runtime,
            (uint8_t)f->registry.objectives[c->objective_index].menu,
            &evaluation) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
        assert(evaluation.value == expected);
        snapshot(f, difficulty, &input);
        assert(ge_original_mission_outcome_evaluate_exact(&input, &outcome));
        assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_FAILED
            && outcome.incomplete_objective_count >= 1);
        ++count;
    }
    complete(f, difficulty);
    return count;
}

int main(int argc, char **argv)
{
    /* GeStageId is an asset-registry index; it is not LEVEL_SOLO_SEQUENCE
     * (the registry stores Cradle before Caverns). Never cast between them. */
    static const GeStageId stages[20] = {
        GE_STAGE_DAM, GE_STAGE_FACILITY, GE_STAGE_RUNWAY, GE_STAGE_SURFACE,
        GE_STAGE_BUNKER1, GE_STAGE_SILO, GE_STAGE_FRIGATE, GE_STAGE_SURFACE2,
        GE_STAGE_BUNKER2, GE_STAGE_STATUE, GE_STAGE_ARCHIVES, GE_STAGE_STREETS,
        GE_STAGE_DEPOT, GE_STAGE_TRAIN, GE_STAGE_JUNGLE, GE_STAGE_CONTROL,
        GE_STAGE_CAVERNS, GE_STAGE_CRADLE, GE_STAGE_AZTEC, GE_STAGE_EGYPT};
    static const int targets[20][3] = {
        {0,160,0},{0,0,125},{300,0,0},{0,210,0},{0,0,240},
        {180,0,0},{0,270,0},{0,0,255},{90,0,0},{0,195,0},
        {0,0,80},{105,0,0},{0,100,0},{0,0,325},{225,0,0},
        {0,600,0},{0,0,570},{135,0,0},{0,540,0},{0,0,360}};
    GeAssetPack pack;
    Ge3dsSaveProvider save;
    GeOriginalMissionResultProviders providers;
    char path[] = "/tmp/ge-campaign-save.XXXXXX";
    int fd, difficulty, mission;
    size_t total_criteria = 0;
    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    fd = mkstemp(path); assert(fd >= 0); close(fd); assert(unlink(path) == 0);
    assert(ge_3ds_save_provider_init(&save, path, FOLDER1) == GE_3DS_SAVE_PROVIDER_OK);
    puts("stage\tdifficulty\tobjectives\tcriterion_failures\tresult_abort_death\trestart\tsave_roundtrip\tgameplay_completion");
    for (difficulty = DIFFICULTY_AGENT; difficulty <= DIFFICULTY_007; ++difficulty) {
        for (mission = SP_LEVEL_DAM; mission < SP_LEVEL_MAX; ++mission) {
            Fixture f;
            GeOriginalMissionOutcomeInput initial, input, restarted;
            GeOriginalMissionOutcome outcome;
            GeOriginalMissionResultSnapshot result;
            uint32_t before[GE_3DS_SAVE_SLOT_BYTES / sizeof(uint32_t)];
            size_t failures;
            int time = difficulty < DIFFICULTY_007 && targets[mission][difficulty]
                ? targets[mission][difficulty] : 100;
            fixture_open(&f, &pack, stages[mission]);
            reject_wrong_events(&f);
            current_difficulty = difficulty;
            current_ticks = time * 60 + 59;
            current_level = f.setup.descriptor->level_id;
            title_requests = 0;
            ge_original_dam_mission_exit_services_reset();
            assert(ge_original_mission_result_set_current_mission(mission));
            ge_3ds_save_provider_make_mission_result_providers(&save, &providers);
            ge_original_mission_result_bind(&providers);
            snapshot(&f, difficulty, &initial);
            assert(ge_original_mission_outcome_evaluate_exact(&initial, &outcome));
            assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_FAILED);
            memcpy(before, save.slot_words, sizeof(before));
            ge_original_campaign_end_level_dispatch_exact();
            ge_original_mission_result_snapshot(&result);
            assert(title_requests == 1 && result.apply_calls == 0);
            assert(memcmp(before, save.slot_words, sizeof(before)) == 0);
            failures = perturb_each_criterion(&f, difficulty);
            snapshot(&f, difficulty, &input);
            assert(ge_original_mission_outcome_evaluate_exact(&input, &outcome));
            if (outcome.status != GE_ORIGINAL_MISSION_OUTCOME_COMPLETED)
                fprintf(stderr, "%s difficulty %d incomplete fixture\n", f.setup.descriptor->key, difficulty);
            assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_COMPLETED);
            input.mission_failed_or_aborted = 1;
            assert(ge_original_mission_outcome_evaluate_exact(&input, &outcome));
            assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_ABORTED);
            input.bond_kia = 1;
            assert(ge_original_mission_outcome_evaluate_exact(&input, &outcome));
            assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_KIA);
            input.mission_failed_or_aborted = input.bond_kia = 0;
            assert(ge_original_mission_outcome_evaluate_exact(&input, &outcome));
            memcpy(before, save.slot_words, sizeof(before));
            /* Execute the shared canonical AI_EndLevel service, including
             * its framebuffer-drain gate, against live objective providers. */
            cameraBufferToggle = 1;
            ge_original_campaign_end_level_dispatch_exact();
            assert(cameraFrameCounter2 == 1 && title_requests == 1);
            assert(memcmp(before, save.slot_words, sizeof(before)) == 0);
            cameraBufferToggle = 0;
            ge_original_campaign_end_level_dispatch_exact();
            assert(title_requests == 2);
            ge_original_mission_result_snapshot(&result);
            assert(save.status == GE_3DS_SAVE_PROVIDER_OK);
            assert(result.mission == mission && result.difficulty == difficulty);
            if (difficulty == DIFFICULTY_007) {
                assert(memcmp(before, save.slot_words, sizeof(before)) == 0);
                assert(result.completion_mutations == 0 && result.cheat_mutations == 0);
            } else {
                assert(ge_3ds_save_provider_stage_time(&save, mission, difficulty) == time);
                assert(result.cheat_target_seconds == targets[mission][difficulty]);
                assert(result.completion_mutations == 1);
                assert(result.cheat_mutations == (unsigned)(targets[mission][difficulty] != 0));
                /* Slower repeats cannot erase a best time or re-unlock a cheat. */
                assert(ge_original_mission_result_apply_exact(difficulty, (time + 1) * 60));
                assert(ge_3ds_save_provider_stage_time(&save, mission, difficulty) == time);
            }
            memcpy(before, save.slot_words, sizeof(before));
            ge_3ds_save_provider_close(&save);
            assert(ge_3ds_save_provider_init(&save, path, FOLDER1) == GE_3DS_SAVE_PROVIDER_OK);
            /* Disk encoding regenerates the CRC words; the native cache is
             * allowed to retain the previous CRC until this reload. */
            assert(save.loads == 1 && memcmp((uint8_t *)before + 8,
                (uint8_t *)save.slot_words + 8, sizeof(before) - 8) == 0);
            if (difficulty == DIFFICULTY_00 && mission == SP_LEVEL_EGYPT) {
                assert(ge_3ds_save_provider_007_unlocked(&save));
                /* Match canonical fileGetSaveStageDifficultyTime: custom 007
                 * returns the max-time availability sentinel when unlocked. */
                assert(ge_3ds_save_provider_stage_time(&save, SP_LEVEL_DAM,
                    DIFFICULTY_007) == 0x3ff);
            }
            printf("%s\t%d\t%u\t%lu\tfixture-pass\t", f.setup.descriptor->key,
                difficulty, outcome.enabled_objective_count, (unsigned long)failures);
            fixture_close(&f);
            fixture_open(&f, &pack, stages[mission]);
            snapshot(&f, difficulty, &restarted);
            assert(memcmp(&initial, &restarted, sizeof(initial)) == 0);
            fixture_close(&f);
            puts("fixture-pass\tfixture-pass\tunverified");
            total_criteria += failures;
        }
    }
    ge_original_mission_result_bind(NULL);
    ge_3ds_save_provider_close(&save);
    ge_asset_pack_close(&pack);
    assert(unlink(path) == 0);
    fprintf(stderr, "Campaign: 80 lifecycle fixtures, %lu criterion perturbations; no gameplay completion asserted\n",
        (unsigned long)total_criteria);
    return 0;
}

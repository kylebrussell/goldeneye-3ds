#include "ge_asset_pack.h"
#include "ge_original_stage_objective_runtime.h"
#include "ge_original_stage_objective_live.h"
#include "ge_stage_assets.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
#include "game/bondview.h"
#pragma GCC diagnostic pop

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct RuntimeHarness {
    const GeOriginalStageSetupRuntime *setup;
    ObjectRecord *objects;
    PropRecord *props;
    const PropRecord *inventory_prop;
    uint32_t stage_flags;
    int key_complete;
    int photo_visible;
} RuntimeHarness;

static uint32_t exact_stage_flags;
static Mtxf photo_matrix;
struct player *g_CurrentPlayer;
extern u32 *ptr_last_tag_entry_type16;

Mtxf *getsubmatrix(Model *model)
{
    (void)model;
    return &photo_matrix;
}

Mtxf *modelFindNodeMtx(Model *model, ModelNode *node, s32 index)
{
    (void)model;
    (void)node;
    (void)index;
    return &photo_matrix;
}

f32 chrpropSumMatrixPosX(
    struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix)
{
    (void)matrix;
    return bbox->Bounds.xmin;
}

f32 chrpropSumMatrixNegX(
    struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix)
{
    (void)matrix;
    return bbox->Bounds.xmax;
}

f32 chrpropSumMatrixPosY(
    struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix)
{
    (void)matrix;
    return bbox->Bounds.ymin;
}

f32 chrpropSumMatrixNegY(
    struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix)
{
    (void)matrix;
    return bbox->Bounds.ymax;
}

f32 chrpropSumMatrixPosZ(
    struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix)
{
    (void)matrix;
    return bbox->Bounds.zmin;
}

f32 chrpropSumMatrixNegZ(
    struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix)
{
    (void)matrix;
    return bbox->Bounds.zmax;
}

f32 getPlayer_c_screenwidth(void)
{
    return g_CurrentPlayer->c_screenwidth;
}

f32 getPlayer_c_screenheight(void)
{
    return g_CurrentPlayer->c_screenheight;
}

f32 getPlayer_c_screenleft(void)
{
    return g_CurrentPlayer->c_screenleft;
}

f32 getPlayer_c_screentop(void)
{
    return g_CurrentPlayer->c_screentop;
}

bool chrHasStageFlag(ChrRecord *self, s32 flags)
{
    (void)self;
    return (exact_stage_flags & (uint32_t)flags) != 0U;
}

static void test_exact_live_services(void)
{
    struct player *player = calloc(1U, sizeof(*player));
    PropRecord wanted = {0}, other = {0};
    InvItem first = {0}, second = {0};
    union ModelRoData node_data = {0};
    ModelNode node = {0};
    ModelFileHeader model_file = {0};
    Model model = {0};
    ObjectRecord photo_object = {0}, unrelated = {0};
    PropRecord photo_prop = {0};
    assert(player != NULL);
    first.type = INV_ITEM_WEAPON;
    first.next = &second;
    first.prev = &second;
    second.type = INV_ITEM_PROP;
    second.type_inv_item.type_prop.prop = &wanted;
    second.next = &first;
    second.prev = &first;
    player->ptr_inventory_first_in_cycle = &first;
    assert(ge_original_stage_objective_prop_in_inventory_exact(
        player, &wanted) == 1);
    assert(ge_original_stage_objective_prop_in_inventory_exact(
        player, &other) == 0);
    assert(ge_original_stage_objective_prop_in_inventory_exact(
        NULL, &wanted) == -1);
    player->copiedgoldeneye = 1;
    assert(ge_original_stage_objective_key_analyzer_complete_exact(player)
        == 1);
    assert(ge_original_stage_objective_key_analyzer_complete_exact(NULL)
        == -1);
    exact_stage_flags = UINT32_C(0x400);
    assert(ge_original_stage_objective_stage_flag_set_exact(
        NULL, UINT32_C(0x400)) == 1);
    assert(ge_original_stage_objective_stage_flag_set_exact(
        NULL, UINT32_C(0x800)) == 0);
    player->c_screenleft = 0.0f;
    player->c_screentop = 0.0f;
    player->c_screenwidth = 320.0f;
    player->c_screenheight = 240.0f;
    player->c_halfwidth = 160.0f;
    player->c_halfheight = 120.0f;
    player->c_recipscalex = 1.0f;
    player->c_recipscaley = 1.0f;
    g_CurrentPlayer = player;
    photo_matrix.m[0][0] = 1.0f;
    photo_matrix.m[1][1] = 1.0f;
    photo_matrix.m[2][2] = 1.0f;
    photo_matrix.m[3][2] = -100.0f;
    node.Opcode = MODELNODE_OPCODE_BBOX;
    node.Data = &node_data;
    node_data.BoundingBox.Bounds.xmin = -10;
    node_data.BoundingBox.Bounds.xmax = 10;
    node_data.BoundingBox.Bounds.ymin = -10;
    node_data.BoundingBox.Bounds.ymax = 10;
    node_data.BoundingBox.Bounds.zmin = -1;
    node_data.BoundingBox.Bounds.zmax = 1;
    model_file.RootNode = &node;
    model.obj = &model_file;
    photo_object.model = &model;
    photo_object.prop = &photo_prop;
    photo_prop.obj = &photo_object;
    photo_prop.flags = PROPFLAG_ONSCREEN;
    assert(ge_original_stage_objective_photograph_binding_ready_exact(
        player, &photo_object, &photo_prop) == 1);
    assert(ge_original_stage_objective_photograph_bounds_inside_view_exact(
        player, &photo_object, &photo_prop) == 1);
    photo_matrix.m[3][0] = 100000.0f;
    assert(ge_original_stage_objective_photograph_bounds_inside_view_exact(
        player, &photo_object, &photo_prop) == 0);
    photo_prop.flags = 0;
    assert(ge_original_stage_objective_photograph_bounds_inside_view_exact(
        player, &photo_object, &photo_prop) == 0);
    assert(ge_original_stage_objective_photograph_bounds_inside_view_exact(
        player, &unrelated, &photo_prop) == -1);
    assert(ge_original_stage_objective_photograph_bounds_inside_view_exact(
        NULL, &photo_object, &photo_prop) == -1);
    g_CurrentPlayer = NULL;
    free(player);
}

static int is_object(uint8_t type)
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

static void *provide_object(void *opaque, size_t command_index,
                            const GeOriginalStagePropRecord *record)
{
    RuntimeHarness *harness = opaque;
    ObjectRecord *object;
    PropRecord *prop;
    assert(harness != NULL && record != NULL
        && command_index < harness->setup->prop_record_count);
    if (!is_object(record->type)) return NULL;
    object = &harness->objects[command_index];
    prop = &harness->props[command_index];
    object->prop = prop;
    prop->obj = object;
    prop->type = PROP_TYPE_OBJ;
    prop->flags = PROPFLAG_ONSCREEN;
    prop->zDepth = 1.0f;
    return object;
}

static int prop_in_inventory(void *opaque, const void *prop)
{
    const RuntimeHarness *harness = opaque;
    return prop == harness->inventory_prop;
}

static int stage_flag_set(void *opaque, uint32_t flags)
{
    const RuntimeHarness *harness = opaque;
    return (harness->stage_flags & flags) != 0U;
}

static int key_analyzer_complete(void *opaque)
{
    return ((const RuntimeHarness *)opaque)->key_complete;
}

static int photograph_bounds_inside_view(
    void *opaque, const void *object, const void *prop)
{
    const RuntimeHarness *harness = opaque;
    assert(object != NULL && prop != NULL);
    return harness->photo_visible;
}

static void bind_criterion_pad(
    GeOriginalStageSetupRuntime *setup, int32_t pad_id, StandTile *tile,
    int16_t room)
{
    assert(setup != NULL && tile != NULL && pad_id >= 0);
    memset(tile, 0, sizeof(*tile));
    tile->room = room;
    if (pad_id >= 10000) {
        size_t index = (size_t)(pad_id - 10000);
        assert(index < setup->boundpad_count);
        ((BoundPadRecord *)setup->boundpads_storage)[index].stan = tile;
    } else {
        size_t index = (size_t)pad_id;
        assert(index < setup->pad_count);
        ((PadRecord *)setup->pads_storage)[index].stan = tile;
    }
}

static TagObjectRecord *find_native_tag(uint16_t tag_id)
{
    TagObjectRecord *tag = (TagObjectRecord *)ptr_last_tag_entry_type16;
    while (tag != NULL && tag->ID != tag_id) tag = tag->NextTag;
    return tag;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    size_t stage_index, criteria_by_type[PROPDEF_OBJECTIVE_COPY_ITEM + 1U] = {0};
    size_t evaluated = 0U, blocked = 0U, event_room = 0U;
    size_t event_deposit = 0U, event_photo = 0U;
    int inventory_blocker = 0, stage_flag_blocker = 0;
    int key_blocker = 0, room_blocker = 0, deposit_blocker = 0;
    int photograph_blocker = 0, registry_blocker = 0;
    int failure_precedence = 0, hud_blocker_atomic = 0;
    int cleanup_semantics = 0;
    if (argc == 2 && strcmp(argv[1], "--photo-only") == 0) {
        test_exact_live_services();
        puts("Exact objective photograph projection: inside/outside/blocker OK");
        return 0;
    }
    assert(argc == 2);
    test_exact_live_services();
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup = {0};
        GeOriginalStageObjectiveRegistry registry = {0};
        GeOriginalStageObjectiveProviders registry_providers;
        GeOriginalStageObjectiveRuntimeProviders runtime_providers;
        GeOriginalStageObjectiveRuntime runtime;
        RuntimeHarness harness = {0};
        size_t index;
        assert(descriptor != NULL);
        assert(ge_original_stage_setup_load(&pack, descriptor, &setup)
            == GE_ORIGINAL_STAGE_SETUP_OK);
        harness.setup = &setup;
        harness.objects = calloc(setup.prop_record_count,
            sizeof(*harness.objects));
        harness.props = calloc(setup.prop_record_count,
            sizeof(*harness.props));
        assert(harness.objects != NULL && harness.props != NULL);
        registry_providers.context = &harness;
        registry_providers.object_definition_by_command = provide_object;
        assert(ge_original_stage_objectives_build(&registry, &setup,
            &registry_providers) == GE_ORIGINAL_STAGE_OBJECTIVE_OK);
        memset(&runtime_providers, 0, sizeof(runtime_providers));
        runtime_providers.context = &harness;
        runtime_providers.prop_in_inventory = prop_in_inventory;
        runtime_providers.stage_flag_set = stage_flag_set;
        runtime_providers.key_analyzer_complete = key_analyzer_complete;
        runtime_providers.photograph_bounds_inside_view =
            photograph_bounds_inside_view;
        assert(ge_original_stage_objective_runtime_begin(
            &runtime, &registry, &runtime_providers)
                == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
        assert(runtime.native_tag_count == registry.tag_count);
        assert((registry.tag_count == 0U) == (runtime.native_tags == NULL));
        {
            TagObjectRecord *native =
                (TagObjectRecord *)ptr_last_tag_entry_type16;
            int32_t tag_index = registry.tag_head;
            size_t native_count = 0U;
            while (tag_index >= 0) {
                const GeOriginalStageTagEntry *expected =
                    &registry.tags[tag_index];
                assert(native != NULL);
                assert(native->ID == expected->tag_id);
                assert(native->TaggedObject == expected->tagged_object);
                assert(native->OffsetToObj
                    == (int16_t)expected->record->words[1]);
                native = native->NextTag;
                tag_index = expected->next_tag;
                ++native_count;
            }
            assert(native == NULL && native_count == registry.tag_count);
        }
        if (strcmp(descriptor->key, "dam") == 0) {
            TagObjectRecord *tag4 = find_native_tag(4U);
            TagObjectRecord *tag5 = find_native_tag(5U);
            ObjectRecord *object4;
            ObjectRecord *object5;
            assert(tag4 != NULL && tag5 != NULL);
            assert(tag4->TaggedObject != NULL && tag5->TaggedObject != NULL);
            object4 = tag4->TaggedObject;
            object5 = tag5->TaggedObject;
            assert(object4->prop != NULL && object5->prop != NULL);
            assert((object4->runtime_bitflags & UINT32_C(0x10)) != 0U);
            assert((object5->runtime_bitflags & UINT32_C(0x10)) != 0U);
            assert((object4->state & PROPSTATE_DESTROYED) == 0U);
            assert((object5->state & PROPSTATE_DESTROYED) == 0U);
        }
        assert(ge_original_stage_objective_live_count()
            == registry.objective_count + 1);
        assert(objectiveGetCount() == registry.objective_count + 1);
        if (!hud_blocker_atomic) {
            for (index = 0U; index < registry.criterion_count; ++index) {
                if (registry.criteria[index].type
                        == PROPDEF_OBJECTIVE_COMPLETE_CONDITION) {
                    GeOriginalStageObjectiveStatusChange changes[
                        GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
                    GeOriginalStageObjectiveEvaluation result;
                    uint8_t displayed_before[
                        GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
                    size_t change_count = SIZE_MAX;
                    memcpy(displayed_before, runtime.displayed_status,
                        sizeof(displayed_before));
                    runtime.providers.stage_flag_set = NULL;
                    assert(ge_original_stage_objective_live_collect_status_changes(
                        3, changes, &change_count, &result)
                            == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED);
                    assert(result.blocker
                        == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_STAGE_FLAGS_UNAVAILABLE);
                    assert(change_count == 0U);
                    assert(memcmp(displayed_before, runtime.displayed_status,
                        sizeof(displayed_before)) == 0);
                    runtime.providers.stage_flag_set = stage_flag_set;
                    hud_blocker_atomic = 1;
                    break;
                }
            }
        }
        for (index = 0U; index < registry.tag_count; ++index) {
            if (registry.tags[index].blocker
                    == GE_ORIGINAL_STAGE_OBJECTIVE_READY) {
                const ObjectRecord *object =
                    registry.tags[index].tagged_object;
                assert(object != NULL
                    && (object->runtime_bitflags & UINT32_C(0x10)) != 0U);
            }
        }
        for (index = 0U; index < registry.criterion_count; ++index)
            ++criteria_by_type[registry.criteria[index].type];
        for (index = 0U; index < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++index) {
            GeOriginalStageObjectiveEvaluation evaluation;
            GeOriginalStageObjectiveRuntimeStatus status;
            if (registry.objective_by_menu[index] < 0) continue;
            {
                int8_t difficulty;
                const GeOriginalStageObjectiveEntry *objective =
                    &registry.objectives[
                        registry.objective_by_menu[index]];
                assert(ge_original_stage_objective_live_difficulty(
                    (uint8_t)index, &difficulty));
                assert(difficulty == objective->difficulty);
                {
                    uint16_t text_id = 0U;
                    assert(ge_original_stage_objective_live_text_id(
                        (uint8_t)index, &text_id));
                    assert(text_id == objective->text_id);
                }
            }
            status = ge_original_stage_objective_live_evaluate(
                (uint8_t)index, &evaluation);
            if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) {
                assert(evaluation.value <= GE_ORIGINAL_STAGE_OBJECTIVE_FAILED);
                assert(objectiveGetStatus_WEAK((int32_t)index, 0)
                    == (int32_t)evaluation.value);
                ++evaluated;
            } else {
                assert(status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED
                    && evaluation.blocker
                        == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_REGISTRY_BLOCKED);
                ++blocked;
            }
        }
        for (index = 0U; index < registry.criterion_count; ++index) {
            GeOriginalStageObjectiveCriterion *criterion =
                &registry.criteria[index];
            const GeOriginalStageObjectiveEntry *objective =
                &registry.objectives[criterion->objective_index];
            GeOriginalStageObjectiveEvaluation evaluation;
            GeOriginalStageObjectiveRuntimeProviders saved = runtime.providers;
            GeOriginalStageObjectiveRuntimeStatus status;
            uint8_t saved_criterion_blocker = criterion->blocker;
            if (!registry_blocker && criterion->tag_index >= 0) {
                criterion->blocker =
                    GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_OBJECT_UNAVAILABLE;
                status = ge_original_stage_objective_runtime_evaluate(
                    &runtime, (uint8_t)objective->menu, &evaluation);
                if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED
                        && evaluation.blocker
                            == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_REGISTRY_BLOCKED)
                    registry_blocker = 1;
                criterion->blocker = saved_criterion_blocker;
            }
            if (!failure_precedence
                    && criterion->type == PROPDEF_OBJECTIVE_FAIL_CONDITION) {
                uint32_t saved_flags = harness.stage_flags;
                harness.stage_flags |= criterion->words[0];
                status = ge_original_stage_objective_runtime_evaluate(
                    &runtime, (uint8_t)objective->menu, &evaluation);
                if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) {
                    assert(evaluation.value
                        == GE_ORIGINAL_STAGE_OBJECTIVE_FAILED);
                    failure_precedence = 1;
                }
                harness.stage_flags = saved_flags;
            }
            if (!stage_flag_blocker
                    && (criterion->type == PROPDEF_OBJECTIVE_COMPLETE_CONDITION
                        || criterion->type == PROPDEF_OBJECTIVE_FAIL_CONDITION)) {
                runtime.providers.stage_flag_set = NULL;
                status = ge_original_stage_objective_runtime_evaluate(
                    &runtime, (uint8_t)objective->menu, &evaluation);
                if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED
                        && evaluation.blocker
                            == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_STAGE_FLAGS_UNAVAILABLE)
                    stage_flag_blocker = 1;
            } else if (!inventory_blocker
                    && (criterion->type == PROPDEF_OBJECTIVE_COLLECT_OBJECT
                        || criterion->type == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT)) {
                runtime.providers.prop_in_inventory = NULL;
                status = ge_original_stage_objective_runtime_evaluate(
                    &runtime, (uint8_t)objective->menu, &evaluation);
                if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED
                        && evaluation.blocker
                            == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVENTORY_UNAVAILABLE)
                    inventory_blocker = 1;
            } else if (!key_blocker
                    && criterion->type == PROPDEF_OBJECTIVE_COPY_ITEM) {
                runtime.providers.key_analyzer_complete = NULL;
                status = ge_original_stage_objective_runtime_evaluate(
                    &runtime, (uint8_t)objective->menu, &evaluation);
                if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED
                        && evaluation.blocker
                            == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_KEY_ANALYZER_UNAVAILABLE)
                    key_blocker = 1;
            }
            runtime.providers = saved;
        }
        if (registry.enter_room_head >= 0) {
            StandTile *tiles = calloc(registry.criterion_count, sizeof(*tiles));
            int32_t cursor;
            assert(tiles != NULL);
            if (!room_blocker) {
                GeOriginalStageObjectiveLiveStatus live;
                objectivestatusCheckRoomEntered(123);
                ge_original_stage_objective_live_status(&live);
                assert(live.last_status
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED);
                assert(live.last_blocker
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PAD_STAN_UNAVAILABLE);
                room_blocker = 1;
            }
            for (cursor = registry.enter_room_head; cursor >= 0;
                    cursor = registry.criteria[cursor].next_same_type)
                bind_criterion_pad(&setup, registry.criteria[cursor].value_a,
                    &tiles[cursor], 123);
            objectivestatusCheckRoomEntered(123);
            {
                GeOriginalStageObjectiveLiveStatus live;
                ge_original_stage_objective_live_status(&live);
                assert(live.last_status
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK
                    && live.calls[GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_ROOM] >= 1U);
            }
            for (cursor = registry.enter_room_head; cursor >= 0;
                    cursor = registry.criteria[cursor].next_same_type) {
                assert(registry.criteria[cursor].status != 0);
                ++event_room;
            }
            free(tiles);
        }
        if (registry.deposit_room_head >= 0) {
            StandTile *tiles = calloc(registry.criterion_count, sizeof(*tiles));
            int32_t cursor;
            assert(tiles != NULL);
            if (!deposit_blocker) {
                GeOriginalStageObjectiveCriterion *criterion =
                    &registry.criteria[registry.deposit_room_head];
                GeOriginalStageObjectiveLiveStatus live;
                objectivestatusCheckDeposit(criterion->value_a, 124);
                ge_original_stage_objective_live_status(&live);
                assert(live.last_status
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED);
                assert(live.last_blocker
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PAD_STAN_UNAVAILABLE);
                deposit_blocker = 1;
            }
            for (cursor = registry.deposit_room_head; cursor >= 0;
                    cursor = registry.criteria[cursor].next_same_type)
                bind_criterion_pad(&setup, registry.criteria[cursor].value_b,
                    &tiles[cursor], 124);
            for (cursor = registry.deposit_room_head; cursor >= 0;
                    cursor = registry.criteria[cursor].next_same_type) {
                objectivestatusCheckDeposit(
                    registry.criteria[cursor].value_a, 124);
                {
                    GeOriginalStageObjectiveLiveStatus live;
                    ge_original_stage_objective_live_status(&live);
                    assert(live.last_status
                        == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
                }
                assert(registry.criteria[cursor].status != 0);
                ++event_deposit;
            }
            free(tiles);
        }
        if (registry.photograph_head >= 0) {
            int32_t cursor;
            runtime.providers.photograph_bounds_inside_view = NULL;
            objectiveTakePictureHandler();
            {
                GeOriginalStageObjectiveLiveStatus live;
                ge_original_stage_objective_live_status(&live);
                assert(live.last_status
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED);
                assert(live.last_blocker
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PHOTOGRAPH_UNAVAILABLE);
            }
            photograph_blocker = 1;
            runtime.providers.photograph_bounds_inside_view =
                photograph_bounds_inside_view;
            harness.photo_visible = 1;
            objectiveTakePictureHandler();
            {
                GeOriginalStageObjectiveLiveStatus live;
                ge_original_stage_objective_live_status(&live);
                assert(live.last_status
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
            }
            for (cursor = registry.photograph_head; cursor >= 0;
                    cursor = registry.criteria[cursor].next_same_type) {
                assert(registry.criteria[cursor].status != 0);
                ++event_photo;
            }
        }
        {
            GeOriginalStageObjectiveEvaluation result;
            GeOriginalStageObjectiveStatusChange changes[
                GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
            size_t change_count;
            int all_complete = -1;
            assert(ge_original_stage_objective_live_all_complete(
                3, &all_complete, &result)
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
            assert(all_complete == 0 || registry.objective_count < 0);
            assert(ge_original_stage_objective_live_collect_status_changes(
                3, changes, &change_count, &result)
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
            assert(change_count <= GE_ORIGINAL_STAGE_OBJECTIVE_MAX);
            for (index = 0U; index < change_count; ++index) {
                assert(changes[index].menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX);
                assert(runtime.displayed_status[changes[index].menu]
                    == changes[index].value);
            }
            assert(ge_original_stage_objective_live_collect_status_changes(
                3, changes, &change_count, &result)
                    == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK);
            assert(change_count == 0U);
        }
        if (!cleanup_semantics) {
            for (index = 0U; index < registry.objective_entry_count;
                    ++index) {
                const GeOriginalStageObjectiveEntry *objective =
                    &registry.objectives[index];
                if ((objective->unknown_e & 1U) != 0U) {
                    runtime.displayed_status[objective->menu] =
                        GE_ORIGINAL_STAGE_OBJECTIVE_FAILED;
                    cleanupObjectives();
                    assert(runtime.displayed_status[objective->menu]
                        == GE_ORIGINAL_STAGE_OBJECTIVE_FAILED);
                    runtime.displayed_status[objective->menu] =
                        GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
                    cleanupObjectives();
                    assert(runtime.displayed_status[objective->menu]
                        == GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE);
                    cleanup_semantics = 1;
                    break;
                }
            }
        }
        printf("%s: %lu objective evaluated, %lu registry-blocked\n",
            descriptor->key, (unsigned long)evaluated,
            (unsigned long)blocked);
        ge_original_stage_objectives_close(&registry);
        assert(runtime.bound == 0U && runtime.registry == NULL
            && runtime.providers.context == NULL);
        assert(runtime.native_tags == NULL
            && runtime.native_tag_count == 0U
            && ptr_last_tag_entry_type16 == NULL);
        for (index = 0U; index < setup.prop_record_count; ++index)
            assert((harness.objects[index].runtime_bitflags
                & UINT32_C(0x10)) == 0U);
        {
            GeOriginalStageObjectiveLiveStatus live;
            objectivestatusCheckRoomEntered(1);
            ge_original_stage_objective_live_status(&live);
            assert(live.last_status
                == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND);
        }
        ge_original_stage_setup_close(&setup);
        free(harness.props);
        free(harness.objects);
    }
    ge_asset_pack_close(&pack);
    printf("Campaign objective runtime: %lu evaluated/%lu blocked; "
           "%lu room, %lu deposit, %lu photograph events\n",
        (unsigned long)evaluated, (unsigned long)blocked,
        (unsigned long)event_room, (unsigned long)event_deposit,
        (unsigned long)event_photo);
    for (stage_index = PROPDEF_OBJECTIVE_DESTROY_OBJECT;
            stage_index <= PROPDEF_OBJECTIVE_COPY_ITEM; ++stage_index)
        printf("criterion %lu: %lu\n", (unsigned long)stage_index,
            (unsigned long)criteria_by_type[stage_index]);
    assert(evaluated + blocked == 80U);
    assert(event_room == 5U && event_deposit == 4U && event_photo == 2U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_DESTROY_OBJECT] == 92U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_COMPLETE_CONDITION] == 47U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_FAIL_CONDITION] == 46U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_COLLECT_OBJECT] == 16U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_DEPOSIT_OBJECT] == 1U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_PHOTOGRAPH] == 2U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_NULL] == 0U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_ENTER_ROOM] == 5U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM] == 4U);
    assert(criteria_by_type[PROPDEF_OBJECTIVE_COPY_ITEM] == 1U);
    assert(inventory_blocker && stage_flag_blocker && key_blocker
        && room_blocker && deposit_blocker && photograph_blocker
        && registry_blocker && failure_precedence && hud_blocker_atomic
        && cleanup_semantics);
    return 0;
}

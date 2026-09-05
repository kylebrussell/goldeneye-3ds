#include "ge_original_pitem_models.h"
#include "ge_original_door.h"
#include "ge_original_door_runtime.h"
#include "ge_original_dam_world.h"
#include "ge_original_default_object.h"
#include "ge_original_stage_interactive_objects.h"
#include "ge_original_stage_items.h"
#include "ge_original_stage_monitor.h"
#include "ge_original_stage_monitor_surface.h"
#include "ge_original_model_scene.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_original_stage_setup.h"
#include "ge_original_stage_supplies.h"
#include "ge_stage_assets.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"

#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ConstructionHarness {
    GeOriginalPitemModelProvider *models;
    size_t calls;
    size_t facility_calls;
    size_t runway_calls;
    size_t bbox_models;
    size_t bsp_models;
    size_t switch_models;
    size_t scene_models;
    size_t scene_parts;
    size_t scene_triangles;
    size_t instance_scene_parts;
    size_t instance_scene_vertices;
    size_t instance_scene_batches;
    size_t instance_scene_triangles;
    size_t room_objects[256];
    size_t room_scene_vertices[256];
    size_t room_scene_batches[256];
    uint8_t scene_model_seen[340];
    uint8_t texture_seen[65536];
    size_t scene_textures;
    void *instances[128];
    int32_t instance_model_ids[128];
    int runway;
    int track_rooms;
} ConstructionHarness;

typedef struct InteractiveHarness {
    GeOriginalPitemModelProvider *models;
    const GeStanNativeMap *stan;
    GeOriginalDefaultObjectPrepared prepared;
    size_t constructions;
    size_t releases;
    size_t projectile_loads;
    size_t door_links;
    PropRecord *owner_props;
    size_t owner_prop_count;
    size_t embedded_constructions;
    unsigned supply_phase;
    size_t supply_room_updates;
    size_t supply_activations;
    size_t supply_enables;
} InteractiveHarness;

typedef struct StageStanHarness {
    uint8_t *blob;
    void *storage;
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
} StageStanHarness;

typedef struct CanonicalDoorHarness {
    GeOriginalPitemModelProvider *models;
    const GeStanNativeMap *stan;
    void *collisions[GE_ORIGINAL_DOOR_NATIVE_CAPACITY];
    size_t collision_count;
    size_t constructions;
    size_t releases;
    size_t linked_pairs;
    size_t swinging;
    size_t flexi;
    size_t eye_iris;
    size_t aztec_chair;
    size_t vertical_fallaway;
    int32_t global_timer;
    int32_t clock_timer;
    size_t portal_lookups;
    size_t portal_open_calls;
    size_t portal_close_calls;
    size_t collision_tests;
    size_t sound_events;
} CanonicalDoorHarness;

typedef struct MonitorHarness {
    GeOriginalPitemModelProvider *models;
    const GeStanNativeMap *stan;
    const GeOriginalStagePropConstructionRequest *request;
    GeOriginalDefaultObjectPrepared prepared;
    PropRecord prop;
    void *collision;
    size_t construct_calls;
    size_t place_calls;
} MonitorHarness;

stagesetup g_CurrentSetup;
s32 g_ClockTimer;
f32 g_GlobalTimerDelta;

/* Polling before/after a full snapshot must observe exactly the same
 * generation, including opening, closing and articulated door fixtures. */
static int checked_door_snapshot(const void *door, GeOriginalDoorRuntimePublication *out)
{
    uint32_t before = UINT32_MAX, after = UINT32_MAX;
    int polled = ge_original_door_runtime_generation(door, &before);
    int snapped = ge_original_door_runtime_snapshot(door, out);
    assert(polled == snapped);
    if (snapped) {
        assert(before == out->generation);
        assert(ge_original_door_runtime_generation(door, &after));
        assert(after == before);
    }
    return snapped;
}

u32 randomGetNext(void)
{
    static u32 state = UINT32_C(0x6d2b79f5);
    state = state * UINT32_C(1664525) + UINT32_C(1013904223);
    return state;
}

/* The campaign-generic promoted DoorRecord path intentionally bypasses the
 * legacy Dam-only definition side table. */
int ge_dam_setup_world_door_setup(
    const void *definition, GeOriginalDamDoorSetup *setup)
{
    (void)definition;
    (void)setup;
    return 0;
}

int ge_dam_setup_world_definition_header(
    const void *definition, uint16_t *extrascale,
    uint8_t *state, uint8_t *type)
{
    (void)definition;
    (void)extrascale;
    (void)state;
    (void)type;
    return 0;
}

int ge_dam_setup_world_definition_set_state(void *definition, uint8_t state)
{
    (void)definition;
    (void)state;
    return 0;
}

static int32_t canonical_door_model_load(void *context, int32_t model_id)
{
    CanonicalDoorHarness *harness = context;
    return ge_original_pitem_model_load(harness->models, model_id);
}

static int canonical_door_model_available(void *context, int32_t model_id)
{
    return canonical_door_model_load(context, model_id) != 0;
}

static int canonical_door_resolve_model(
    void *context, int32_t model_id, void **header,
    void **model, float *scale)
{
    CanonicalDoorHarness *harness = context;
    return ge_original_pitem_model_resolve_instance(
        harness->models, model_id, header, model, scale);
}

static void *canonical_door_allocate_collision(
    void *context, uint32_t size_bytes)
{
    CanonicalDoorHarness *harness = context;
    void *collision;
    assert(size_bytes == 0x50U
           && harness->collision_count
                < GE_ORIGINAL_DOOR_NATIVE_CAPACITY);
    collision = calloc(1U, size_bytes);
    assert(collision != NULL);
    harness->collisions[harness->collision_count++] = collision;
    return collision;
}

static int canonical_door_walk(
    void *context, void **stan, float start_x, float start_z,
    float destination_x, float destination_z)
{
    CanonicalDoorHarness *harness = context;
    return ge_original_stan_walk_tiles_between_points(
        harness->stan, (GeStanNativeTile **)stan,
        start_x, start_z, destination_x, destination_z);
}

static int canonical_door_tile_rgb(
    void *context, void *stan, float x, float z, uint8_t rgb[3])
{
    const GeStanNativeTile *tile = stan;
    uint16_t mid;
    (void)context;
    (void)x;
    (void)z;
    if (tile == NULL || rgb == NULL) return -1;
    mid = (uint16_t)tile->mid;
    rgb[0] = (uint8_t)(((mid >> 8) & 0xfU) * 0x11U);
    rgb[1] = (uint8_t)(((mid >> 4) & 0xfU) * 0x11U);
    rgb[2] = (uint8_t)((mid & 0xfU) * 0x11U);
    return 1;
}

static int32_t canonical_door_find_portal(
    void *context, int32_t room_a, int32_t room_b,
    const float point_a[3], const float point_b[3])
{
    CanonicalDoorHarness *harness = context;
    assert(room_a >= 0 && room_b >= 0 && room_a != room_b);
    assert(point_a != NULL && point_b != NULL);
    ++harness->portal_lookups;
    return 7;
}

static void canonical_door_set_portal_open(
    void *context, int32_t portal, int open)
{
    CanonicalDoorHarness *harness = context;
    assert(portal == 7 && (open == 0 || open == 1));
    if (open) ++harness->portal_open_calls;
    else ++harness->portal_close_calls;
}

static int32_t canonical_door_global_timer(void *context)
{
    return ((CanonicalDoorHarness *)context)->global_timer;
}

static int32_t canonical_door_clock_timer(void *context)
{
    return ((CanonicalDoorHarness *)context)->clock_timer;
}

static int canonical_door_collision_test(void *context, void *prop)
{
    CanonicalDoorHarness *harness = context;
    assert(prop != NULL);
    ++harness->collision_tests;
    return 1;
}

static void canonical_door_sound_event(
    void *context, void *door, GeOriginalDoorSoundEvent event)
{
    CanonicalDoorHarness *harness = context;
    assert(door != NULL && event >= GE_ORIGINAL_DOOR_SOUND_START_OPEN
           && event <= GE_ORIGINAL_DOOR_SOUND_FINISH_CLOSE);
    ++harness->sound_events;
}

static int canonical_door_construct(
    void *context, const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    void **prop_out, void **model_out)
{
    CanonicalDoorHarness *harness = context;
    DoorRecord *door = definition;
    PropRecord *prop;
    GeOriginalDoorStatus status;
    GeOriginalDoorRuntimePublication closed_publication;
    GeOriginalDoorRuntimePublication moved_publication;
    GeOriginalDoorRuntimePublication repeated_publication;
    float authored_open_position;
    size_t row;
    size_t column;
    (void)definition_size;
    if (door->doorType == DOORTYPE_SWINGING) ++harness->swinging;
    else if (door->doorType >= DOORTYPE_FLEXI1
            && door->doorType <= DOORTYPE_FLEXI3) ++harness->flexi;
    else if (door->doorType == DOORTYPE_EYE
            || door->doorType == DOORTYPE_IRIS) ++harness->eye_iris;
    else if (door->doorType == DOORTYPE_AZTECCHAIR)
        ++harness->aztec_chair;
    else if (door->doorType == DOORTYPE_VERTICAL
            || door->doorType == DOORTYPE_FALLAWAY)
        ++harness->vertical_fallaway;
    prop = calloc(1U, ge_original_stage_prop_native_prop_size());
    assert(prop != NULL && ge_original_stage_prop_native_bind_prop(
        request, definition, prop,
        ge_original_stage_prop_native_prop_size()));
    status = ge_original_door_construct(
        definition, (int32_t)request->command_index);
    if (status != GE_ORIGINAL_DOOR_OK) {
        fprintf(stderr,
            "canonical door construct failed: command=%zu model=%d pad=%d "
            "status=%s\n", request->command_index, request->model_id,
            request->pad_id, ge_original_door_status_name(status));
    }
    assert(status == GE_ORIGINAL_DOOR_OK);
    assert(((ObjectRecord *)definition)->model != NULL);
    assert(checked_door_snapshot(
        definition, &closed_publication));
    for (size_t part_index = 0U; part_index <
            ge_original_pitem_model_scene_part_count(harness->models, request->model_id);
            ++part_index) {
        GeOriginalPitemModelScenePart part;
        GeOriginalModelSceneInput input = {0};
        GeOriginalModelScene scene;
        assert(ge_original_pitem_model_scene_part(harness->models,
            request->model_id, part_index, &part));
        input.blob = part.blob;
        input.blob_size = part.blob_size;
        input.primary_offset = part.primary_offset;
        input.secondary_offset = part.secondary_offset;
        input.segment4_offset = part.segment4_offset;
        input.segment3_matrices = closed_publication.matrices;
        input.segment3_matrix_count = closed_publication.matrix_count;
        input.matrix[0][0] = input.matrix[1][1] = input.matrix[2][2] = input.matrix[3][3] = 1.0f;
        GeOriginalModelSceneStatus query_status = ge_original_model_scene_build(&input, NULL, &scene);
        if (query_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED)
            fprintf(stderr, "DOOR_QUERY command=%zu model=%d part=%zu status=%d translation=%f,%f,%f matrices=%zu\n",
                request->command_index, request->model_id, part_index, query_status,
                closed_publication.matrices[0][3][0], closed_publication.matrices[0][3][1],
                closed_publication.matrices[0][3][2], (size_t)closed_publication.matrix_count);
        assert(query_status == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    }
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            assert(isfinite(closed_publication.matrix[row][column]));
    authored_open_position = door->openPosition;
    door->openPosition = door->maxFrac * 0.5f;
    assert(checked_door_snapshot(
        definition, &moved_publication));
    assert(checked_door_snapshot(
        definition, &repeated_publication)
        && repeated_publication.generation
            == moved_publication.generation);
    if (door->maxFrac != 0.0f)
        assert(moved_publication.generation
               != closed_publication.generation);
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            assert(isfinite(moved_publication.matrix[row][column]));
    if (door->doorType == DOORTYPE_EYE
            || door->doorType == DOORTYPE_IRIS) {
        size_t part_count = ge_original_pitem_model_scene_part_count(
            harness->models, request->model_id);
        size_t part_index;
        size_t articulated_parts = 0U;
        assert(memcmp(closed_publication.matrix, moved_publication.matrix,
                      sizeof(closed_publication.matrix)) == 0);
        assert(closed_publication.articulated == 1U
               && moved_publication.articulated == 1U
               && closed_publication.matrix_count
                    == (door->doorType == DOORTYPE_EYE ? 3U : 13U)
               && moved_publication.matrix_count
                    == closed_publication.matrix_count
               && part_count > 0U);
        for (part_index = 0U; part_index < part_count; ++part_index) {
            GeOriginalPitemModelScenePart part;
            GeOriginalModelSceneInput input = {0};
            GeOriginalModelScene scene;
            assert(ge_original_pitem_model_scene_part(
                harness->models, request->model_id, part_index, &part));
            assert(part.matrix_index < moved_publication.matrix_count);
            articulated_parts += part.matrix_index != 0U;
            input.blob = part.blob;
            input.blob_size = part.blob_size;
            input.primary_offset = part.primary_offset;
            input.secondary_offset = part.secondary_offset;
            input.segment4_offset = part.segment4_offset;
            input.segment3_matrices = moved_publication.matrices;
            input.segment3_matrix_count = moved_publication.matrix_count;
            input.matrix[0][0] = 1.0f;
            input.matrix[1][1] = 1.0f;
            input.matrix[2][2] = 1.0f;
            input.matrix[3][3] = 1.0f;
            assert(ge_original_model_scene_build(&input, NULL, &scene)
                   == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                   && scene.required_vertex_count > 0U
                   && scene.required_batch_count > 0U);
        }
        assert(articulated_parts == part_count
               && memcmp(closed_publication.matrices,
                         moved_publication.matrices,
                         sizeof(closed_publication.matrices)) != 0);
    } else if (door->maxFrac != 0.0f) {
        assert(memcmp(closed_publication.matrix, moved_publication.matrix,
                      sizeof(closed_publication.matrix)) != 0);
    }
    door->openPosition = authored_open_position;
    assert(((DoorRecord *)definition)->TintDist
                == (int32_t)request->record->words[48]
           && ((DoorRecord *)definition)->CullDist
                == (int16_t)(request->record->words[49] >> 16U)
           && ((DoorRecord *)definition)->soundType
                == (int8_t)(request->record->words[49] >> 8U)
           && ((DoorRecord *)definition)->fadeTime60
                == (int8_t)request->record->words[49]);
    *prop_out = prop;
    *model_out = ((ObjectRecord *)definition)->model;
    ++harness->constructions;
    return 1;
}

static int canonical_door_link(
    void *context, void *first_definition, void *second_definition)
{
    CanonicalDoorHarness *harness = context;
    assert(ge_original_door_runtime_link_pair(
        first_definition, second_definition));
    ++harness->linked_pairs;
    return 1;
}

static void canonical_door_release(
    void *context, void *definition, void *prop, void *model)
{
    CanonicalDoorHarness *harness = context;
    ObjectRecord *object = definition;
    assert(object != NULL && object->prop == prop && object->model == model);
    assert(ge_original_door_release(definition));
    assert(ge_original_pitem_model_release_instance(harness->models, model));
    free(object->ptr_allocated_collisiondata_block);
    free(prop);
    ++harness->releases;
}

static void canonical_dam_gate_tick_until(
    CanonicalDoorHarness *harness, void *tick_root,
    void *first, void *second, int phase)
{
    GeOriginalDoorRuntimePublication a;
    GeOriginalDoorRuntimePublication b;
    size_t tick;
    for (tick = 0U; tick < 4096U; ++tick) {
        ++harness->global_timer;
        assert(ge_original_door_runtime_tick(tick_root)
               == GE_ORIGINAL_DOOR_RUNTIME_OK);
        assert(checked_door_snapshot(first, &a)
               && checked_door_snapshot(second, &b));
        /* The unchanged 0x40000000 interlock may never expose both secure
         * sections at once. This is the defining Dam-gate invariant. */
        assert(!(a.open_position > 0.0f && b.open_position > 0.0f));
        if (phase == 0 && a.open_state == DOORSTATE_STATIONARY
                && a.open_position == 0.0f
                && b.open_state == DOORSTATE_STATIONARY
                && fabsf(b.open_position - b.max_frac) < 0.0001f)
            return;
        if (phase == 1 && a.open_state == DOORSTATE_STATIONARY
                && a.open_position == 0.0f
                && b.open_state == DOORSTATE_STATIONARY
                && b.open_position == 0.0f)
            return;
        if (phase == 2 && a.open_state == DOORSTATE_STATIONARY
                && fabsf(a.open_position - a.max_frac) < 0.0001f
                && b.open_state == DOORSTATE_STATIONARY
                && b.open_position == 0.0f)
            return;
    }
    assert(!"Dam gate runtime failed to settle");
}

static void audit_dam_interlocked_door_runtime(
    CanonicalDoorHarness *harness,
    const GeOriginalStageInteractiveRuntime *interactive)
{
    GeOriginalDoorRuntimeProviders providers = {0};
    GeOriginalDoorRuntimeState state;
    GeOriginalDoorRuntimePublication initial[2];
    GeOriginalDoorRuntimePublication second_open[2];
    GeOriginalDoorRuntimePublication both_closed[2];
    GeOriginalDoorRuntimePublication first_reopened[2];
    void *doors[2] = {NULL, NULL};
    size_t portal_open_before = harness->portal_open_calls;
    size_t portal_close_before = harness->portal_close_calls;
    size_t index;
    for (index = 0U; index < interactive->entry_count; ++index) {
        const GeOriginalStageInteractiveEntry *entry =
            &interactive->entries[index];
        if (entry->constructed && entry->type == PROPDEF_DOOR
                && entry->command_index >= 267U
                && entry->command_index <= 268U)
            doors[entry->command_index - 267U] = entry->definition;
    }
    assert(doors[0] != NULL && doors[1] != NULL);
    assert(((DoorRecord *)doors[0])->linkedDoorOffset == 1
           && ((DoorRecord *)doors[1])->linkedDoorOffset == -1);
    assert((((ObjectRecord *)doors[0])->flags2 & PROPFLAG2_40000000) != 0U
           && (((ObjectRecord *)doors[1])->flags2
                & PROPFLAG2_40000000) != 0U);

    harness->clock_timer = 4;
    harness->global_timer = 1;
    providers.context = harness;
    providers.global_timer = canonical_door_global_timer;
    providers.clock_timer = canonical_door_clock_timer;
    providers.test_collision = canonical_door_collision_test;
    providers.sound_event = canonical_door_sound_event;
    ge_original_door_runtime_bind(&providers, &state);
    assert(checked_door_snapshot(doors[0], &initial[0])
           && checked_door_snapshot(doors[1], &initial[1]));
    assert(fabsf(initial[0].open_position - initial[0].max_frac) < 0.0001f
           && initial[1].open_position == 0.0f
           && initial[0].collision_edges > 0
           && initial[1].collision_edges > 0
           && initial[0].portal_number == -1
           && initial[1].portal_number == -1);

    assert(ge_original_door_runtime_activate(
               doors[1], DOORSTATE_OPENING)
           == GE_ORIGINAL_DOOR_RUNTIME_OK);
    canonical_dam_gate_tick_until(
        harness, doors[1], doors[0], doors[1], 0);
    assert(checked_door_snapshot(doors[0], &second_open[0])
           && checked_door_snapshot(doors[1], &second_open[1]));
    assert(second_open[0].generation > initial[0].generation
           && second_open[1].generation > initial[1].generation
           && second_open[0].collision_edges > 0
           && second_open[1].collision_edges > 0
           && memcmp(initial[0].collision_polygon,
                     second_open[0].collision_polygon,
                     sizeof(initial[0].collision_polygon)) != 0
           && memcmp(initial[1].collision_polygon,
                     second_open[1].collision_polygon,
                     sizeof(initial[1].collision_polygon)) != 0);

    assert(ge_original_door_runtime_activate(
               doors[1], DOORSTATE_CLOSING)
           == GE_ORIGINAL_DOOR_RUNTIME_OK);
    canonical_dam_gate_tick_until(
        harness, doors[1], doors[0], doors[1], 1);
    assert(checked_door_snapshot(doors[0], &both_closed[0])
           && checked_door_snapshot(doors[1], &both_closed[1]));
    assert(both_closed[0].generation >= second_open[0].generation
           && both_closed[1].generation > second_open[1].generation
           && both_closed[0].collision_edges > 0
           && both_closed[1].collision_edges > 0);

    assert(ge_original_door_runtime_activate(
               doors[0], DOORSTATE_OPENING)
           == GE_ORIGINAL_DOOR_RUNTIME_OK);
    canonical_dam_gate_tick_until(
        harness, doors[0], doors[0], doors[1], 2);
    assert(checked_door_snapshot(doors[0], &first_reopened[0])
           && checked_door_snapshot(doors[1], &first_reopened[1]));
    assert(first_reopened[0].collision_edges > 0
           && first_reopened[1].collision_edges > 0
           && first_reopened[0].generation > both_closed[0].generation
           && state.completed_opens == 2U
           && state.completed_closes == 2U
           && state.collision_tests == harness->collision_tests
           && state.collision_tests > 0U
           && state.bbox_rebuilds > 0U
           && state.status == GE_ORIGINAL_DOOR_RUNTIME_OK
           && state.portal_open_events == 0U
           && state.portal_close_events == 0U
           && harness->portal_open_calls == portal_open_before
           && harness->portal_close_calls == portal_close_before
           && harness->sound_events == state.sound_events
           && state.sound_events == 8U);
    printf("dam gate lifecycle: generation %u/%u -> %u/%u -> %u/%u "
           "-> %u/%u; 2 open/2 close, %u collision tests, "
           "portal -1/-1 (authored NO_PORTAL_CLOSE)\n",
           initial[0].generation, initial[1].generation,
           second_open[0].generation, second_open[1].generation,
           both_closed[0].generation, both_closed[1].generation,
           first_reopened[0].generation, first_reopened[1].generation,
           state.collision_tests);
    ge_original_door_runtime_bind(NULL, NULL);
}

static int interactive_model_available(void *context, int32_t model_id)
{
    InteractiveHarness *harness = context;
    return ge_original_pitem_model_available(harness->models, model_id);
}

static int interactive_projectiles(void *context, int8_t weapon_id)
{
    InteractiveHarness *harness = context;
    (void)weapon_id;
    ++harness->projectile_loads;
    return 1;
}

static int32_t interactive_item_model_load(void *context,int32_t model_id)
{return interactive_model_available(context,model_id);}

static int32_t interactive_item_player_count(void *context)
{(void)context;return 1;}

static int32_t interactive_item_scenario(void *context)
{(void)context;return SCENARIO_NORMAL;}

static int interactive_item_resolve(void *context,int32_t model_id,
    void **header,void **model,float *scale)
{
    InteractiveHarness *harness=context;
    return ge_original_pitem_model_resolve_instance(
        harness->models,model_id,header,model,scale);
}

static void *interactive_item_collision(void *context,uint32_t size)
{(void)context;return calloc(1U,size);}

static int interactive_item_floor(void *context,void *stan,float x,float z,
    float *floor_y)
{
    InteractiveHarness *harness=context;
    if(harness==NULL||harness->stan==NULL||stan==NULL||floor_y==NULL)return 0;
    *floor_y=ge_original_stan_get_position_y(harness->stan,stan,x,z);
    return isfinite(*floor_y);
}

static int interactive_item_room_bounds(void *context,
    const float position[3],int16_t room,float *top,float *bottom)
{(void)context;(void)position;(void)room;(void)top;(void)bottom;return 0;}

static int interactive_item_walk(void *context,void **stan,float sx,float sz,
    float dx,float dz)
{
    InteractiveHarness *harness=context;
    return ge_original_stan_walk_tiles_between_points(
        harness->stan,(GeStanNativeTile **)stan,sx,sz,dx,dz);
}

static int interactive_item_rgb(void *context,void *stan,float x,float z,
    uint8_t rgb[3])
{return canonical_door_tile_rgb(context,stan,x,z,rgb);}

static int supply_update_room(void *context,void *definition)
{
    InteractiveHarness *harness=context;ObjectRecord *object=definition;
    assert(harness!=NULL&&object!=NULL&&object->prop!=NULL
           &&object->model!=NULL&&harness->supply_phase==0U);
    harness->supply_phase=1U;++harness->supply_room_updates;return 1;
}

static int supply_activate(void *context,void *opaque_prop)
{
    InteractiveHarness *harness=context;PropRecord *prop=opaque_prop;
    assert(harness!=NULL&&prop!=NULL&&harness->supply_phase==1U);
    harness->supply_phase=2U;++harness->supply_activations;return 1;
}

static int supply_enable(void *context,void *opaque_prop)
{
    InteractiveHarness *harness=context;PropRecord *prop=opaque_prop;
    assert(harness!=NULL&&prop!=NULL&&harness->supply_phase==2U);
    prop->flags|=PROPFLAG_ENABLED;harness->supply_phase=3U;
    ++harness->supply_enables;return 1;
}

static int interactive_construct_standard_item(
    void *context,const GeOriginalStagePropConstructionRequest *request,
    void *definition,size_t definition_size,void **prop,void **model_instance)
{
    InteractiveHarness *harness=context;PropRecord *allocated;
    GeOriginalDefaultObjectProviders providers={0};
    GeOriginalStageItemStatus status;(void)definition_size;
    allocated=calloc(1U,sizeof(*allocated));if(allocated==NULL)return 0;
    providers.context=harness;providers.model_load=interactive_item_model_load;
    providers.get_player_count=interactive_item_player_count;
    providers.get_scenario=interactive_item_scenario;
    providers.resolve_model_instance=interactive_item_resolve;
    providers.allocate_collision=interactive_item_collision;
    providers.get_floor_y=interactive_item_floor;
    providers.get_room_object_bounds=interactive_item_room_bounds;
    providers.walk_tiles=interactive_item_walk;
    providers.get_tile_rgb=interactive_item_rgb;
    status=ge_original_stage_item_construct_standard_exact(
        request,definition,allocated,sizeof(*allocated),&providers,
        &harness->prepared);
    if(status!=GE_ORIGINAL_STAGE_ITEM_OK){free(allocated);return 0;}
    *prop=allocated;*model_instance=((ObjectRecord *)definition)->model;
    ++harness->constructions;return 1;
}

static int interactive_construct(
    void *context, const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    void **prop, void **model_instance)
{
    InteractiveHarness *harness = context;
    void *header = NULL;
    float scale = 0.0f;
    (void)definition_size;
    if (!ge_original_pitem_model_resolve_instance(
            harness->models, request->model_id,
            &header, model_instance, &scale)) return 0;
    assert(header != NULL && *model_instance != NULL && scale > 0.0f);
    *prop = definition;
    ++harness->constructions;
    return 1;
}

static int interactive_construct_embedded(
    void *context,const GeOriginalStagePropConstructionRequest *request,
    int32_t owner_command_index,void *definition,size_t definition_size,
    void **prop,void **model_instance)
{
    InteractiveHarness *harness=context;PropRecord *allocated;
    GeOriginalStageItemStatus status;(void)definition_size;
    if(harness==NULL||owner_command_index<0
            ||(size_t)owner_command_index>=harness->owner_prop_count)return 0;
    allocated=calloc(1U,sizeof(*allocated));if(allocated==NULL)return 0;
    status=ge_original_stage_item_construct_embedded_exact(
        request,definition,allocated,sizeof(*allocated),harness->models,
        &harness->owner_props[owner_command_index],NULL,model_instance);
    if(status!=GE_ORIGINAL_STAGE_ITEM_OK){free(allocated);return 0;}
    assert(allocated->parent==&harness->owner_props[owner_command_index]
           &&(((ObjectRecord *)definition)->runtime_bitflags
                &RUNTIMEBITFLAG_HASOWNER)!=0U);
    *prop=allocated;++harness->embedded_constructions;
    ++harness->constructions;return 1;
}

static int interactive_link_doors(
    void *context, void *first_definition, void *second_definition)
{
    InteractiveHarness *harness = context;
    assert(first_definition != NULL && second_definition != NULL
           && first_definition != second_definition);
    ++harness->door_links;
    return 1;
}

static void interactive_release(
    void *context, void *definition, void *prop, void *model_instance)
{
    InteractiveHarness *harness = context;
    assert(definition != NULL&&prop!=NULL&&model_instance != NULL);
    assert(ge_original_pitem_model_release_instance(
        harness->models, model_instance));
    free(((ObjectRecord *)definition)->ptr_allocated_collisiondata_block);
    if(definition!=prop)free(prop);
    ++harness->releases;
}

static size_t interactive_validate_owned_item_graph(
    const GeOriginalStageInteractiveRuntime *runtime,
    const InteractiveHarness *harness)
{
    uint8_t *seen;size_t owner_index,entry_index,embedded=0U,external=0U;
    size_t root_count;
    assert(runtime!=NULL&&runtime->loaded&&harness!=NULL
           &&harness->owner_props!=NULL);
    seen=calloc(runtime->entry_count,1U);assert(seen!=NULL);
    for(entry_index=0U;entry_index<runtime->entry_count;++entry_index){
        const GeOriginalStageInteractiveEntry *entry=&runtime->entries[entry_index];
        if(entry->constructed&&entry->owner_command_index>=0)++embedded;
        if(entry->constructed&&entry->externally_owned)++external;
    }
    for(owner_index=0U;owner_index<harness->owner_prop_count;++owner_index){
        const PropRecord *newer=NULL;
        const PropRecord *child=harness->owner_props[owner_index].child;
        size_t steps=0U;
        while(child!=NULL){
            size_t found=runtime->entry_count;
            assert(++steps<=embedded&&child->parent
                   ==&harness->owner_props[owner_index]);
            /* chrpropReparent stores the newest child at owner->child, walks
             * toward older siblings through prev, and points each older
             * sibling's next back toward the newer one. */
            assert(child->next==newer);
            for(entry_index=0U;entry_index<runtime->entry_count;++entry_index){
                const GeOriginalStageInteractiveEntry *entry=
                    &runtime->entries[entry_index];
                if(entry->constructed
                        &&entry->owner_command_index==(int32_t)owner_index
                        &&entry->prop==child){found=entry_index;break;}
            }
            assert(found<runtime->entry_count&&!seen[found]);
            seen[found]=1U;newer=child;child=child->prev;
        }
    }
    for(entry_index=0U;entry_index<runtime->entry_count;++entry_index){
        const GeOriginalStageInteractiveEntry *entry=&runtime->entries[entry_index];
        if(entry->constructed&&entry->owner_command_index>=0)assert(seen[entry_index]);
    }
    root_count=ge_original_stage_interactive_root_item_count(runtime);
    assert(root_count==runtime->report.constructed_items-embedded-external);
    for(entry_index=0U;entry_index<root_count;++entry_index){
        size_t command;void *opaque_prop;PropRecord *prop;
        assert(ge_original_stage_interactive_root_item(
            runtime,entry_index,&command,&opaque_prop)&&opaque_prop!=NULL);
        prop=opaque_prop;
        assert(prop->parent==NULL&&command<runtime->setup->prop_record_count);
    }
    {
        size_t command=0U;void *prop=NULL;
        assert(!ge_original_stage_interactive_root_item(
            runtime,root_count,&command,&prop));
    }
    free(seen);return embedded;
}

static void stage_stan_open(
    GeAssetPack *pack, const GeStageAssetDescriptor *descriptor,
    GeOriginalStageSetupRuntime *setup, StageStanHarness *stan)
{
    const GeAssetPackEntry *entry = ge_asset_pack_find(
        pack, descriptor->collision_path);
    size_t native_size;
    memset(stan, 0, sizeof(*stan));
    assert(entry != NULL && entry->data_size > 0U
           && entry->data_size <= SIZE_MAX);
    stan->blob = malloc((size_t)entry->data_size);
    assert(stan->blob != NULL
           && ge_asset_pack_read(pack, descriptor->collision_path,
               stan->blob, (size_t)entry->data_size, NULL) == GE_ASSET_PACK_OK
           && ge_stan_collision_open(stan->blob, (size_t)entry->data_size,
               &stan->surface) == GE_STAN_COLLISION_OK
           && ge_stan_native_required_size(
               &stan->surface, &native_size) == GE_STAN_COLLISION_OK);
    stan->storage = malloc(native_size);
    assert(stan->storage != NULL
           && ge_stan_native_materialize(&stan->surface,
               descriptor->level_scale, stan->storage, native_size,
               &stan->native) == GE_STAN_COLLISION_OK);
    /* Some campaign setups contain unused pad names that are not present in
     * that mission's STAN. Bind every exact match so the interactive audit can
     * distinguish a record-local placement blocker from a whole-stage load
     * failure. The production all-or-nothing bind remains unchanged. */
    setup->pad_stan_count = setup->bound_pad_stan_count = 0U;
    for (size_t index = 0U; index < setup->pad_count; ++index) {
        PadRecord *pad = &((PadRecord *)setup->pads_storage)[index];
        pad->stan = (StandTile *)ge_original_stan_match_tile_name(
            &stan->native, pad->plink);
        setup->pad_stan_count += pad->stan != NULL;
    }
    for (size_t index = 0U; index < setup->boundpad_count; ++index) {
        BoundPadRecord *pad =
            &((BoundPadRecord *)setup->boundpads_storage)[index];
        pad->stan = (StandTile *)ge_original_stan_match_tile_name(
            &stan->native, pad->plink);
        setup->bound_pad_stan_count += pad->stan != NULL;
    }
}

static void stage_stan_close(StageStanHarness *stan)
{
    free(stan->storage);
    free(stan->blob);
    memset(stan, 0, sizeof(*stan));
}

static int32_t monitor_model_load(void *context, int32_t model_id)
{
    MonitorHarness *harness = context;
    return ge_original_pitem_model_load(harness->models, model_id);
}

static int32_t monitor_player_count(void *context)
{
    (void)context;
    return 1;
}

static int32_t monitor_scenario(void *context)
{
    (void)context;
    return 0;
}

static int monitor_resolve_model(void *context, int32_t model_id,
    void **header, void **model, float *scale)
{
    MonitorHarness *harness = context;
    return ge_original_pitem_model_resolve_instance(
        harness->models, model_id, header, model, scale);
}

static void *monitor_allocate_collision(void *context, uint32_t size_bytes)
{
    MonitorHarness *harness = context;
    assert(harness->collision == NULL && size_bytes == 0x50U);
    harness->collision = calloc(1U, size_bytes);
    return harness->collision;
}

static int monitor_floor_y(void *context, void *stan, float x, float z,
                           float *floor_y)
{
    MonitorHarness *harness = context;
    *floor_y = ge_original_stan_get_position_y(
        harness->stan, stan, x, z);
    return isfinite(*floor_y);
}

static int monitor_room_bounds(void *context, const float position[3],
                               int16_t room, float *top, float *bottom)
{
    (void)context;
    (void)position;
    (void)room;
    (void)top;
    (void)bottom;
    return 0;
}

static int monitor_walk(void *context, void **stan, float start_x,
                        float start_z, float destination_x,
                        float destination_z)
{
    MonitorHarness *harness = context;
    return ge_original_stan_walk_tiles_between_points(
        harness->stan, (GeStanNativeTile **)stan,
        start_x, start_z, destination_x, destination_z);
}

static int monitor_tile_rgb(void *context, void *stan, float x, float z,
                            uint8_t rgb[3])
{
    const GeStanNativeTile *tile = stan;
    uint16_t mid;
    (void)context;
    (void)x;
    (void)z;
    if (tile == NULL || rgb == NULL) return -1;
    mid = (uint16_t)tile->mid;
    rgb[0] = (uint8_t)(((mid >> 8) & 0xfU) * 0x11U);
    rgb[1] = (uint8_t)(((mid >> 4) & 0xfU) * 0x11U);
    rgb[2] = (uint8_t)((mid & 0xfU) * 0x11U);
    return 1;
}

static int monitor_construct_standard(
    void *context, void *definition, int32_t command_index)
{
    MonitorHarness *harness = context;
    assert(harness->request != NULL
           && command_index == (int32_t)harness->request->command_index);
    memset(&harness->prop, 0, sizeof(harness->prop));
    assert(ge_original_stage_prop_native_bind_prop(
        harness->request, definition, &harness->prop,
        sizeof(harness->prop)));
    ++harness->construct_calls;
    return ge_original_default_object_construct_standard(
        definition, command_index) == GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

static int monitor_place_standard(void *context, void *definition)
{
    MonitorHarness *harness = context;
    ++harness->place_calls;
    return ge_original_default_object_place_standard(definition)
        == GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

static int monitor_materializer_callback(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    (void)context;
    (void)request;
    return 1;
}

static void audit_stage_monitors(GeAssetPack *pack)
{
    GeOriginalPitemModelProvider *models;
    GeOriginalPitemModelStatus model_status;
    size_t stage_index;
    size_t standard_single = 0U;
    size_t embedded_single = 0U;
    size_t inside_single = 0U;
    size_t multi = 0U;
    size_t screen_ticks = 0U;
    size_t non_screen_slots = 0U;
    size_t animated_stages = 0U;
    uint8_t image_seen[GE_ORIGINAL_STAGE_MONITOR_IMAGE_COUNT] = {0};
    models = ge_original_pitem_model_provider_create(
        pack, 341U, 4U, &model_status);
    assert(models != NULL && model_status == GE_ORIGINAL_PITEM_MODEL_OK);
    g_ClockTimer = 1;
    g_GlobalTimerDelta = 1.0f;
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup;
        StageStanHarness stage_stan;
        size_t record_index;
        size_t stage_ticks = 0U;
        assert(descriptor != NULL && ge_original_stage_setup_load(
            pack, descriptor, &setup) == GE_ORIGINAL_STAGE_SETUP_OK);
        stage_stan_open(pack, descriptor, &setup, &stage_stan);
        g_CurrentSetup = *setup.setup;
        for (record_index = 0U; record_index < setup.prop_record_count;
                ++record_index) {
            const GeOriginalStagePropRecord *record =
                ge_original_stage_setup_prop_record(&setup, record_index);
            GeOriginalStagePropConstructionRequest request;
            GeOriginalStagePropMaterializerProviders classifier = {
                .context = models,
                .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_MONITOR,
                .model_available = ge_original_pitem_model_available,
                .construct_special_object = monitor_materializer_callback,
            };
            GeOriginalStageMonitorProviders monitor_providers = {0};
            GeOriginalDefaultObjectProviders object_providers;
            GeOriginalStageMonitorStatus status;
            MonitorHarness harness = {0};
            ObjectRecord *object;
            void *definition;
            size_t definition_size;
            size_t slot_count;
            size_t slot;
            if (record->type != PROPDEF_MONITOR
                    && record->type != PROPDEF_MULTI_MONITOR) continue;
            assert(ge_original_stage_prop_construction_request(
                &setup, record_index, &request));
            definition_size =
                ge_original_stage_prop_native_definition_size(&request);
            assert(definition_size == (record->type == PROPDEF_MONITOR
                ? sizeof(MonitorObjRecord) : sizeof(MultiMonitorObjRecord)));
            definition = malloc(definition_size);
            assert(definition != NULL);
            harness.models = models;
            harness.stan = &stage_stan.native;
            harness.request = &request;
            memset(&object_providers, 0, sizeof(object_providers));
            object_providers.context = &harness;
            object_providers.model_load = monitor_model_load;
            object_providers.get_player_count = monitor_player_count;
            object_providers.get_scenario = monitor_scenario;
            object_providers.resolve_model_instance = monitor_resolve_model;
            object_providers.allocate_collision = monitor_allocate_collision;
            object_providers.get_floor_y = monitor_floor_y;
            object_providers.get_room_object_bounds = monitor_room_bounds;
            object_providers.walk_tiles = monitor_walk;
            object_providers.get_tile_rgb = monitor_tile_rgb;
            ge_original_default_object_bind(
                &object_providers, &harness.prepared);
            monitor_providers.context = &harness;
            monitor_providers.construct_standard =
                monitor_construct_standard;
            monitor_providers.place_standard = monitor_place_standard;
            status = ge_original_stage_monitor_construct(
                &request, definition, definition_size, &monitor_providers);
            if (record->type == PROPDEF_MONITOR && record->pad_id < 0
                    && (record->words[2]
                        & PROPFLAG_INSIDEANOTHEROBJ) == 0U) {
                assert(status
                    == GE_ORIGINAL_STAGE_MONITOR_EMBEDDED_OWNER_REQUIRED);
                ++embedded_single;
                free(definition);
                continue;
            }
            if ((record->words[2]
                    & (PROPFLAG_INSIDEANOTHEROBJ
                       | PROPFLAG_ASSIGNEDTOCHR)) != 0U) {
                assert(status
                    == GE_ORIGINAL_STAGE_MONITOR_INSIDE_OWNER_REQUIRED);
                ++inside_single;
                free(definition);
                continue;
            }
            assert(ge_original_stage_prop_classify(record, &classifier).blocker
                   == GE_ORIGINAL_STAGE_PROP_READY);
            if (status != GE_ORIGINAL_STAGE_MONITOR_OK) {
                fprintf(stderr, "%s monitor command %zu model %d pad %d: %s\n",
                    descriptor->key, record_index, record->model_id,
                    record->pad_id,
                    ge_original_stage_monitor_status_name(status));
            }
            assert(status == GE_ORIGINAL_STAGE_MONITOR_OK
                   && harness.construct_calls == 1U
                   && harness.place_calls == 1U);
            object = definition;
            assert(object->model != NULL && object->prop == &harness.prop);
            slot_count = record->type == PROPDEF_MONITOR ? 1U : 4U;
            for (slot = 0U; slot < slot_count; ++slot) {
                GeOriginalDamMonitorRenderSnapshot snapshot;
                const uint8_t image = record->type == PROPDEF_MONITOR
                    ? (uint8_t)((MonitorObjRecord *)definition)->ImageNum
                    : ((MultiMonitorObjRecord *)definition)->ImageNums[slot];
                assert(image < GE_ORIGINAL_STAGE_MONITOR_IMAGE_COUNT
                       && ge_original_stage_monitor_command_list(image)
                            != NULL);
                image_seen[image] = 1U;
                if (ge_original_stage_monitor_tick(
                        definition, definition_size, slot, &snapshot)) {
                    size_t screen_part_index;
                    GeOriginalPitemModelScenePart screen_part;
                    GeOriginalModelSceneInput screen_input = {0};
                    GeOriginalModelScene screen_query;
                    GeOriginalModelScene screen_scene;
                    GeDamRoomWorldVertex *screen_vertices;
                    GeDamRoomDrawBatch *screen_batches;
                    GeDamRoomSceneStorage screen_storage;
                    GeOriginalStageMonitorSurfaceResult surface_result;
                    assert(snapshot.texture_config != NULL
                           && snapshot.width > 0U && snapshot.height > 0U);
                    assert(ge_original_pitem_model_scene_part_for_node(
                        models, record->model_id, snapshot.switch_node,
                        &screen_part_index, &screen_part));
                    assert(screen_part.node == snapshot.switch_node
                           && screen_part_index
                                < ge_original_pitem_model_scene_part_count(
                                    models, record->model_id));
                    screen_input.blob = screen_part.blob;
                    screen_input.blob_size = screen_part.blob_size;
                    screen_input.primary_offset = screen_part.primary_offset;
                    screen_input.secondary_offset =
                        screen_part.secondary_offset;
                    screen_input.segment4_offset =
                        screen_part.segment4_offset;
                    screen_input.matrix[0][0] = 1.0f;
                    screen_input.matrix[1][1] = 1.0f;
                    screen_input.matrix[2][2] = 1.0f;
                    screen_input.matrix[3][3] = 1.0f;
                    assert(ge_original_model_scene_build(
                        &screen_input, NULL, &screen_query)
                        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
                    screen_vertices = calloc(
                        screen_query.required_vertex_count,
                        sizeof(*screen_vertices));
                    screen_batches = calloc(
                        screen_query.required_batch_count,
                        sizeof(*screen_batches));
                    assert(screen_vertices != NULL && screen_batches != NULL);
                    screen_storage.vertices = screen_vertices;
                    screen_storage.vertex_capacity =
                        screen_query.required_vertex_count;
                    screen_storage.batches = screen_batches;
                    screen_storage.batch_capacity =
                        screen_query.required_batch_count;
                    assert(ge_original_model_scene_build(
                        &screen_input, &screen_storage, &screen_scene)
                        == GE_ORIGINAL_MODEL_SCENE_OK);
                    assert(ge_original_stage_monitor_surface_apply_part(
                        &snapshot, &screen_storage, 0U,
                        screen_scene.batch_count, &surface_result)
                        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK);
                    assert(surface_result.texture_id == snapshot.texture_id
                           && surface_result.batch_index
                                < screen_scene.batch_count);
                    free(screen_batches);
                    free(screen_vertices);
                    ++screen_ticks;
                    ++stage_ticks;
                } else {
                    ModelNode *screen = object->model->obj->Switches[slot];
                    /* propobj.c still dispatches all four multi-monitor slots;
                     * process_monitor_animation_microcode itself performs the
                     * canonical non-DLCOLLISION no-op for authored unused
                     * switch parts such as model 336 slots 2/3. */
                    assert(record->type == PROPDEF_MULTI_MONITOR
                           && (screen == NULL || screen->Data == NULL
                               || (screen->Opcode & 0xffU)
                                    != MODELNODE_OPCODE_DLCOLLISION
                               || screen->Data->DisplayListCollisions
                                    .numVertices < 4));
                    ++non_screen_slots;
                }
            }
            if (record->type == PROPDEF_MONITOR) ++standard_single;
            else ++multi;
            assert(ge_original_pitem_model_release_instance(
                models, object->model));
            free(harness.collision);
            free(definition);
        }
        if (stage_ticks != 0U) ++animated_stages;
        memset(&g_CurrentSetup, 0, sizeof(g_CurrentSetup));
        stage_stan_close(&stage_stan);
        ge_original_stage_setup_close(&setup);
    }
    assert(standard_single == 204U && embedded_single == 16U
           && inside_single == 2U && multi == 55U
           && screen_ticks == 400U && non_screen_slots == 24U
           && screen_ticks + non_screen_slots == 424U
           && animated_stages == 15U);
    for (stage_index = 0U;
            stage_index < GE_ORIGINAL_STAGE_MONITOR_IMAGE_COUNT;
            ++stage_index) {
        if (stage_index == 0U || stage_index == 4U || stage_index == 6U
                || stage_index == 7U || stage_index == 9U
                || stage_index == 10U || (stage_index >= 23U
                    && stage_index <= 47U) || stage_index == 50U) {
            assert(image_seen[stage_index] == 0U);
        }
    }
    ge_original_default_object_bind(NULL, NULL);
    ge_original_pitem_model_provider_destroy(models);
    printf("campaign monitors: %zu standard/%zu multi, %zu exact screen "
           "ticks + %zu authored no-op slot across %zu stages; "
           "%zu embedded/%zu inside blocked on "
           "canonical owner graph\n", standard_single, multi, screen_ticks,
           non_screen_slots, animated_stages, embedded_single, inside_single);
}

static ModelNode *find_opcode(ModelNode *root, uint16_t wanted)
{
    ModelNode *node = root;
    size_t safety = 0U;
    while (node != NULL && safety++ < 128U) {
        if ((node->Opcode & 0xffU) == wanted) return node;
        if (node->Child != NULL) node = node->Child;
        else {
            while (node != NULL && node->Next == NULL) node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
    return NULL;
}

static int construct_default(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    ConstructionHarness *harness = context;
    size_t definition_size =
        ge_original_stage_prop_native_definition_size(request);
    ObjectRecord *definition;
    ModelFileHeader *header;
    Model *model;
    ModelNode *bbox;
    void *header_void = NULL;
    void *model_void = NULL;
    float scale = 0.0f;
    size_t before_vertices = harness->instance_scene_vertices;
    size_t before_batches = harness->instance_scene_batches;
    assert(definition_size == sizeof(ObjectRecord));
    definition = malloc(definition_size);
    assert(definition != NULL);
    assert(ge_original_stage_prop_native_definition_init(
        request, definition, definition_size));
    assert(ge_original_pitem_model_load(harness->models,
                                       request->model_id));
    assert(ge_original_pitem_model_resolve_instance(
        harness->models, request->model_id, &header_void, &model_void,
        &scale));
    header = header_void;
    model = model_void;
    assert(header != NULL && model != NULL && model->obj == header);
    assert(header->RootNode != NULL && header->Skeleton != NULL
           && header->numMatrices > 0 && header->Textures != NULL);
    assert(model->render_pos != NULL && model->datas != NULL
           && scale > 0.0f);
    assert(harness->calls < sizeof(harness->instances)
            / sizeof(harness->instances[0]));
    harness->instances[harness->calls] = model;
    harness->instance_model_ids[harness->calls] = request->model_id;
    bbox = find_opcode(header->RootNode, MODELNODE_OPCODE_BBOX);
    assert(bbox != NULL && bbox->Data != NULL);
    assert(isfinite(bbox->Data->BoundingBox.Bounds.xmin)
           && bbox->Data->BoundingBox.Bounds.xmin
                <= bbox->Data->BoundingBox.Bounds.xmax);
    assert(find_opcode(header->RootNode, MODELNODE_OPCODE_DLCOLLISION) != NULL);
    {
        size_t part_count = ge_original_pitem_model_scene_part_count(
            harness->models, request->model_id);
        size_t part_index;
        assert(part_count > 0U);
        for (part_index = 0U; part_index < part_count; ++part_index) {
            GeOriginalPitemModelScenePart part;
            GeOriginalModelSceneInput input = {0};
            GeOriginalModelScene scene;
            assert(ge_original_pitem_model_scene_part(
                harness->models, request->model_id, part_index, &part));
            input.blob = part.blob;
            input.blob_size = part.blob_size;
            input.primary_offset = part.primary_offset;
            input.secondary_offset = part.secondary_offset;
            input.segment4_offset = part.segment4_offset;
            input.room_id = (uint8_t)request->placement.room;
            input.matrix[0][0] = 1.0f; input.matrix[1][1] = 1.0f;
            input.matrix[2][2] = 1.0f; input.matrix[3][3] = 1.0f;
            memcpy(input.position, request->placement.position,
                   sizeof(input.position));
            assert(ge_original_model_scene_build(&input, NULL, &scene)
                   == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
            assert(scene.required_vertex_count > 0U
                   && scene.required_batch_count > 0U
                   && scene.triangle_count > 0U);
            {
                GeDamRoomWorldVertex *vertices = calloc(
                    scene.required_vertex_count, sizeof(*vertices));
                GeDamRoomDrawBatch *batches = calloc(
                    scene.required_batch_count, sizeof(*batches));
                GeDamRoomSceneStorage storage = {
                    vertices, scene.required_vertex_count,
                    batches, scene.required_batch_count,
                };
                size_t batch_index;
                assert(vertices != NULL && batches != NULL
                       && ge_original_model_scene_build(
                            &input, &storage, &scene)
                            == GE_ORIGINAL_MODEL_SCENE_OK);
                for (batch_index = 0U;
                        batch_index < scene.batch_count; ++batch_index) {
                    const uint16_t texture =
                        batches[batch_index].texture.texture_id;
                    if (batches[batch_index].texture_valid != 0U
                            && batches[batch_index].material.texture_enabled
                                != 0U
                            && batches[batch_index].material.texture_source
                                == GE_PICA_TEXTURE_SOURCE_RARE_ID
                            && harness->texture_seen[texture] == 0U) {
                        harness->texture_seen[texture] = 1U;
                        ++harness->scene_textures;
                    }
                }
                free(batches);
                free(vertices);
            }
            harness->instance_scene_vertices +=
                scene.required_vertex_count;
            harness->instance_scene_batches += scene.required_batch_count;
            harness->instance_scene_triangles += scene.triangle_count;
            ++harness->instance_scene_parts;
        }
    }
    if (request->model_id >= 0 && request->model_id < 340
            && harness->scene_model_seen[request->model_id] == 0U) {
        size_t part_count = ge_original_pitem_model_scene_part_count(
            harness->models, request->model_id);
        size_t part_index;
        assert(part_count > 0U && header->numMatrices == 1);
        for (part_index = 0U; part_index < part_count; ++part_index) {
            GeOriginalPitemModelScenePart part;
            GeOriginalModelSceneInput input = {0};
            GeOriginalModelScene scene;
            assert(ge_original_pitem_model_scene_part(
                harness->models, request->model_id, part_index, &part));
            input.blob = part.blob;
            input.blob_size = part.blob_size;
            input.primary_offset = part.primary_offset;
            input.secondary_offset = part.secondary_offset;
            input.segment4_offset = part.segment4_offset;
            input.matrix[0][0] = 1.0f; input.matrix[1][1] = 1.0f;
            input.matrix[2][2] = 1.0f; input.matrix[3][3] = 1.0f;
            assert(ge_original_model_scene_build(&input, NULL, &scene)
                   == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
            assert(scene.required_vertex_count > 0U
                   && scene.required_batch_count > 0U
                   && scene.triangle_count > 0U);
            harness->scene_triangles += scene.triangle_count;
            ++harness->scene_parts;
        }
        harness->scene_model_seen[request->model_id] = 1U;
        ++harness->scene_models;
    }
    ++harness->bbox_models;
    harness->bsp_models += find_opcode(
        header->RootNode, MODELNODE_OPCODE_BSP) != NULL;
    harness->switch_models += find_opcode(
        header->RootNode, MODELNODE_OPCODE_SWITCH) != NULL;
    ++harness->calls;
    if (harness->track_rooms) {
        assert(request->placement.room >= 0 && request->placement.room < 256);
        ++harness->room_objects[request->placement.room];
        harness->room_scene_vertices[request->placement.room] +=
            harness->instance_scene_vertices - before_vertices;
        harness->room_scene_batches[request->placement.room] +=
            harness->instance_scene_batches - before_batches;
    }
    if (harness->runway) ++harness->runway_calls;
    else ++harness->facility_calls;
    free(definition);
    return 1;
}

static void materialize_stage(GeAssetPack *pack,
                              const GeStageAssetDescriptor *descriptor,
                              ConstructionHarness *harness,
                              size_t expected_calls)
{
    GeOriginalStageSetupRuntime setup;
    GeOriginalStagePropMaterializerReport report;
    /* model_available and construct share a materializer context. Route the
     * canonical availability callback through the provider explicitly. */
    size_t index;
    size_t before_parts = harness->instance_scene_parts;
    size_t before_vertices = harness->instance_scene_vertices;
    size_t before_batches = harness->instance_scene_batches;
    size_t before_triangles = harness->instance_scene_triangles;
    int32_t dependencies[32];
    size_t dependency_count;
    assert(ge_original_stage_setup_load(pack, descriptor, &setup)
           == GE_ORIGINAL_STAGE_SETUP_OK);
    dependency_count = ge_original_stage_prop_model_dependencies(
        &setup, GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT,
        dependencies, sizeof(dependencies) / sizeof(dependencies[0]));
    assert(dependency_count <= sizeof(dependencies) / sizeof(dependencies[0]));
    printf("%s ordinary PitemZ dependencies: %zu models, %zu instances\n",
           descriptor->key, dependency_count, expected_calls);
    for (index = 0U; index < dependency_count; ++index)
        assert(ge_original_pitem_model_available(
            harness->models, dependencies[index]));
    /* The materializer has one context for all services. Dependencies were
     * proven above, so classification needs only an ID-domain predicate here. */
    /* Construction needs its richer harness, so exercise each exact ready
     * request in original record order after classification. */
    memset(&report, 0, sizeof(report));
    for (index = 0U; index < setup.prop_record_count; ++index) {
        GeOriginalStagePropConstructionRequest request;
        GeOriginalStagePropClassification classification;
        const GeOriginalStagePropRecord *record =
            ge_original_stage_setup_prop_record(&setup, index);
        GeOriginalStagePropMaterializerProviders classify = {
            .context = harness->models,
            .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT,
            .model_available = ge_original_pitem_model_available,
            .construct_default_object = construct_default,
        };
        classification = ge_original_stage_prop_classify(record, &classify);
        if (classification.blocker != GE_ORIGINAL_STAGE_PROP_READY) continue;
        assert(ge_original_stage_prop_construction_request(
            &setup, index, &request));
        assert(construct_default(harness, &request));
        ++report.constructed;
    }
    assert(report.constructed == expected_calls);
    printf("%s ordinary scene: %zu part/%zu vertex/%zu batch/%zu triangle\n",
           descriptor->key,
           harness->instance_scene_parts - before_parts,
           harness->instance_scene_vertices - before_vertices,
           harness->instance_scene_batches - before_batches,
           harness->instance_scene_triangles - before_triangles);
    ge_original_stage_setup_close(&setup);
}

static void materialize_supported_stage(
    GeAssetPack *pack, const GeStageAssetDescriptor *descriptor,
    int initial_resident_only)
{
    static const uint8_t streets_initial[] = {
        7U, 8U, 9U, 2U, 1U, 16U, 5U, 17U, 4U, 6U,
    };
    GeOriginalStageSetupRuntime setup;
    GeOriginalPitemModelProvider *models;
    GeOriginalPitemModelStatus status;
    GeOriginalPitemModelStats stats;
    ConstructionHarness harness = {0};
    const GeAssetPackEntry *collision_entry;
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
    uint8_t *collision_blob;
    void *native_storage;
    size_t native_size;
    const int streets = strcmp(descriptor->key, "streets") == 0;
    size_t dependencies;
    size_t instances;
    size_t index;
    size_t ready = 0U;

    assert(ge_original_stage_setup_load(pack, descriptor, &setup)
           == GE_ORIGINAL_STAGE_SETUP_OK);
    collision_entry = ge_asset_pack_find(pack, descriptor->collision_path);
    assert(collision_entry != NULL && collision_entry->data_size > 0U
           && collision_entry->data_size <= SIZE_MAX);
    collision_blob = malloc((size_t)collision_entry->data_size);
    assert(collision_blob != NULL
           && ge_asset_pack_read(pack, descriptor->collision_path,
               collision_blob, (size_t)collision_entry->data_size, NULL)
                == GE_ASSET_PACK_OK
           && ge_stan_collision_open(
               collision_blob, (size_t)collision_entry->data_size, &surface)
                == GE_STAN_COLLISION_OK
           && ge_stan_native_required_size(&surface, &native_size)
                == GE_STAN_COLLISION_OK);
    native_storage = malloc(native_size);
    assert(native_storage != NULL
           && ge_stan_native_materialize(
               &surface, descriptor->level_scale,
               native_storage, native_size, &native)
                == GE_STAN_COLLISION_OK
           && ge_original_stage_setup_bind_stan(&setup, &native)
                == GE_ORIGINAL_STAGE_SETUP_OK);
    dependencies = ge_original_stage_prop_model_dependencies(
        &setup, GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT, NULL, 0U);
    instances = ge_original_stage_setup_prop_type_count(&setup, PROPDEF_PROP)
        + ge_original_stage_setup_prop_type_count(&setup, PROPDEF_GLASS);
    assert(dependencies > 0U && instances > 0U);
    models = ge_original_pitem_model_provider_create(
        pack, dependencies, instances, &status);
    assert(models != NULL && status == GE_ORIGINAL_PITEM_MODEL_OK);
    harness.models = models;
    harness.track_rooms = 1;
    for (index = 0U; index < setup.prop_record_count; ++index) {
        GeOriginalStagePropConstructionRequest request;
        const GeOriginalStagePropRecord *record =
            ge_original_stage_setup_prop_record(&setup, index);
        GeOriginalStagePropMaterializerProviders providers = {
            .context = models,
            .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT,
            .model_available = ge_original_pitem_model_available,
            .construct_default_object = construct_default,
        };
        if (ge_original_stage_prop_classify(record, &providers).blocker
                != GE_ORIGINAL_STAGE_PROP_READY)
            continue;
        assert(ge_original_stage_prop_construction_request(
            &setup, index, &request));
        assert(request.placement_resolved != 0U
               && request.placement.stan != NULL
               && request.placement.room >= 0);
        if (initial_resident_only) {
            assert(streets);
            size_t room_index;
            int resident = 0;
            for (room_index = 0U;
                    room_index < sizeof(streets_initial); ++room_index) {
                if (request.placement.room == streets_initial[room_index]) {
                    resident = 1;
                    break;
                }
            }
            if (!resident) continue;
        }
        assert(construct_default(&harness, &request));
        ++ready;
    }
    ge_original_pitem_model_get_stats(models, &stats);
    assert(ready > 0U && ready == stats.instantiated_models
           && harness.instance_scene_vertices > 0U
           && harness.instance_scene_batches > 0U);
    if (initial_resident_only) {
        assert(harness.instance_scene_vertices + 10197U < 65536U
               && harness.instance_scene_batches + 944U < 65536U);
    } else if (streets) {
        size_t covered = 0U;
        size_t worst_vertices = 0U;
        size_t worst_batches = 0U;
        uint8_t selected_rooms[256] = {0};
        size_t rank;
        for (index = 0U; index < 256U; ++index)
            covered += harness.room_objects[index];
        assert(covered == 105U);
        for (rank = 0U; rank < 10U; ++rank) {
            size_t best_room = SIZE_MAX;
            for (index = 0U; index < 256U; ++index) {
                if (selected_rooms[index] == 0U
                        && (best_room == SIZE_MAX
                            || harness.room_scene_vertices[index]
                                > harness.room_scene_vertices[best_room]))
                    best_room = index;
            }
            assert(best_room != SIZE_MAX);
            selected_rooms[best_room] = 1U;
            worst_vertices += harness.room_scene_vertices[best_room];
            worst_batches += harness.room_scene_batches[best_room];
        }
        assert(ready == 105U && harness.instance_scene_vertices == 73623U
               && harness.instance_scene_batches == 6898U
               && harness.instance_scene_triangles == 24541U
               && harness.instance_scene_vertices + 20094U > 65536U);
        printf("streets worst ten object rooms: %zu vertex/%zu batch; "
               "with all-connected background %zu/%zu\n",
               worst_vertices, worst_batches,
               worst_vertices + 20094U, worst_batches + 1900U);
    }
    printf("%s %s ordinary overlay: %zu/%zu object, %zu model, "
           "%zu vertex/%zu batch/%zu triangle/%zu texture; scene %zu/%zu; "
           "%zu fixed + %zu blob + %zu resource + %zu instance bytes\n",
           descriptor->key,
           initial_resident_only ? "initial-resident" : "all-stage",
           ready, instances, stats.loaded_models,
           harness.instance_scene_vertices, harness.instance_scene_batches,
           harness.instance_scene_triangles, harness.scene_textures,
           harness.instance_scene_vertices
                + (initial_resident_only ? 10197U
                                         : streets ? 20094U : 0U),
           harness.instance_scene_batches
                + (initial_resident_only ? 944U
                                         : streets ? 1900U : 0U),
           stats.fixed_capacity_bytes, stats.source_blob_bytes,
           stats.native_resource_bytes, stats.native_instance_bytes);
    ge_original_pitem_model_provider_destroy(models);
    free(native_storage);
    free(collision_blob);
    ge_original_stage_setup_close(&setup);
}

static size_t audit_canonical_door_stage(
    GeAssetPack *pack, const GeStageAssetDescriptor *descriptor,
    GeOriginalPitemModelProvider *models)
{
    GeOriginalStageSetupRuntime setup;
    StageStanHarness stan;
    CanonicalDoorHarness harness = {0};
    GeOriginalDoorPrepared prepared;
    GeOriginalDoorProviders door_providers = {0};
    GeOriginalStageInteractiveRuntime interactive;
    GeOriginalStageInteractiveProviders interactive_providers = {0};
    size_t constructed;
    assert(descriptor != NULL && ge_original_stage_setup_load(
        pack, descriptor, &setup) == GE_ORIGINAL_STAGE_SETUP_OK);
    stage_stan_open(pack, descriptor, &setup, &stan);
    assert(setup.setup != NULL);
    g_CurrentSetup = *setup.setup;
    harness.models = models;
    harness.stan = &stan.native;
    door_providers.context = &harness;
    door_providers.model_load = canonical_door_model_load;
    door_providers.resolve_model_instance = canonical_door_resolve_model;
    door_providers.allocate_collision = canonical_door_allocate_collision;
    door_providers.walk_tiles = canonical_door_walk;
    door_providers.get_tile_rgb = canonical_door_tile_rgb;
    door_providers.find_portal = canonical_door_find_portal;
    door_providers.set_portal_open = canonical_door_set_portal_open;
    ge_original_door_bind(&door_providers, &prepared);
    interactive_providers.context = &harness;
    interactive_providers.difficulty = 0U;
    interactive_providers.player_count = 1U;
    interactive_providers.model_available = canonical_door_model_available;
    interactive_providers.construct_door = canonical_door_construct;
    interactive_providers.link_doors = canonical_door_link;
    interactive_providers.release_object = canonical_door_release;
    assert(ge_original_stage_interactive_materialize(
        &setup, &interactive_providers, &interactive));
    assert(interactive.report.constructed == harness.constructions
           && harness.collision_count == harness.constructions
           && interactive.report.blocker_counts[
                GE_ORIGINAL_STAGE_INTERACTIVE_CONSTRUCTION_FAILED] == 0U);
    if (descriptor == ge_stage_asset_dam()) {
        assert(interactive.report.door_records == 18U
               && interactive.report.constructed_doors == 18U
               && harness.constructions == 18U
               && harness.linked_pairs == 5U
               && harness.portal_lookups == 4U);
        audit_dam_interlocked_door_runtime(&harness, &interactive);
    } else {
        assert(interactive.report.door_records > 2U
               && harness.constructions > 2U);
    }
    constructed = harness.constructions;
    ge_original_stage_interactive_close(&interactive);
    assert(harness.releases == constructed);
    ge_original_door_bind(NULL, NULL);
    memset(&g_CurrentSetup, 0, sizeof(g_CurrentSetup));
    stage_stan_close(&stan);
    ge_original_stage_setup_close(&setup);
    printf("%s canonical doors: %zu/%zu constructed/released, "
           "%zu linked pair; %zu swing, %zu flexi, %zu eye/iris, "
           "%zu chair, %zu vertical/fallaway\n",
           descriptor->key, constructed, harness.releases,
           harness.linked_pairs, harness.swinging, harness.flexi,
           harness.eye_iris, harness.aztec_chair,
           harness.vertical_fallaway);
    return constructed;
}

static void audit_campaign_embedded_defaults(GeAssetPack *pack,
    GeOriginalPitemModelProvider *models)
{
    size_t stage_index;
    size_t embedded = 0U;
    size_t assigned = 0U;
    size_t embedded_without_root_placement = 0U;
    size_t embedded_door_owner = 0U;
    size_t embedded_by_stage[GE_STAGE_COUNT] = {0};
    size_t assigned_by_stage[GE_STAGE_COUNT] = {0};
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup;
        size_t command;
        assert(descriptor != NULL && ge_original_stage_setup_load(
            pack, descriptor, &setup) == GE_ORIGINAL_STAGE_SETUP_OK);
        for (command = 0U; command < setup.prop_record_count; ++command) {
            const GeOriginalStagePropRecord *record =
                &setup.prop_records[command];
            GeOriginalStagePropConstructionRequest request;
            PropRecord owner = {0};
            PropRecord *prop;
            ObjectRecord *object;
            void *definition;
            void *model_instance = NULL;
            void *collision;
            size_t definition_size;
            uint32_t owner_flags;
            if (record->type != PROPDEF_PROP
                    && record->type != PROPDEF_GLASS)
                continue;
            owner_flags = record->words[2]
                & (PROPFLAG_INSIDEANOTHEROBJ | PROPFLAG_ASSIGNEDTOCHR);
            if (owner_flags != PROPFLAG_INSIDEANOTHEROBJ
                    && owner_flags != PROPFLAG_ASSIGNEDTOCHR)
                continue;
            if (owner_flags == PROPFLAG_INSIDEANOTHEROBJ) {
                const int64_t owner_command =
                    (int64_t)command + record->pad_id;
                assert(owner_command >= 0
                       && owner_command
                            < (int64_t)setup.prop_record_count);
                if (setup.prop_records[owner_command].type == PROPDEF_DOOR) {
                    assert(stage_index == GE_STAGE_SURFACE2
                           && command == 36U && owner_command == 34);
                    ++embedded_door_owner;
                }
            }
            assert(ge_original_stage_prop_construction_request(
                &setup, command, &request));
            definition_size = ge_original_stage_prop_native_definition_size(
                &request);
            definition = calloc(1U, definition_size);
            prop = calloc(1U, sizeof(*prop));
            collision = calloc(1U, 0x50U);
            assert(definition_size != 0U && definition != NULL && prop != NULL
                   && collision != NULL
                   && ge_original_stage_prop_native_definition_init(
                        &request, definition, definition_size));
            if (owner_flags == PROPFLAG_INSIDEANOTHEROBJ
                    && record->pad_id < 0) {
                /* Negative setup-command owner offsets are not root pads.
                 * Prove the strict root binder rejects them while unchanged
                 * embedded objInit still constructs the authored child. */
                assert(!request.placement_resolved
                       || request.placement.has_stan == 0U);
                assert(!ge_original_stage_prop_native_bind_prop(
                    &request, definition, prop, sizeof(*prop)));
                ++embedded_without_root_placement;
            }
            assert((owner_flags == PROPFLAG_INSIDEANOTHEROBJ
                        ? ge_original_stage_item_construct_embedded_exact(
                            &request, definition, prop, sizeof(*prop), models,
                            &owner, collision, &model_instance)
                        : ge_original_stage_item_construct_assigned_exact(
                            &request, definition, prop, sizeof(*prop), models,
                            &owner, collision, &model_instance))
                        == GE_ORIGINAL_STAGE_ITEM_OK);
            object = definition;
            assert(model_instance != NULL && object->model == model_instance
                   && object->prop == prop && prop->parent == &owner
                   && owner.child == prop);
            if (owner_flags == PROPFLAG_INSIDEANOTHEROBJ)
                assert((object->runtime_bitflags
                        & RUNTIMEBITFLAG_HASOWNER) != 0U);
            assert(ge_original_pitem_model_release_instance(
                models, model_instance));
            free(collision);
            free(prop);
            free(definition);
            if (owner_flags == PROPFLAG_INSIDEANOTHEROBJ) {
                ++embedded;
                ++embedded_by_stage[stage_index];
            } else {
                ++assigned;
                ++assigned_by_stage[stage_index];
            }
        }
        ge_original_stage_setup_close(&setup);
    }
    assert(embedded == 16U && embedded_by_stage[GE_STAGE_SURFACE] == 12U
           && embedded_by_stage[GE_STAGE_SURFACE2] == 1U
           && embedded_by_stage[GE_STAGE_CAVERNS] == 3U
           && embedded_without_root_placement == 13U
           && embedded_door_owner == 1U
           && assigned == 2U && assigned_by_stage[GE_STAGE_SILO] == 1U
           && assigned_by_stage[GE_STAGE_BUNKER2] == 1U);
    printf("campaign owned ordinary objects: %zu embedded exact owner graphs "
           "(surface1=%zu surface2=%zu caverns=%zu); %zu assigned exact "
           "character graphs (silo=%zu bunker2=%zu); %zu bypass root "
           "placement, %zu deferred door owner\n", embedded,
           embedded_by_stage[GE_STAGE_SURFACE],
           embedded_by_stage[GE_STAGE_SURFACE2],
           embedded_by_stage[GE_STAGE_CAVERNS], assigned,
           assigned_by_stage[GE_STAGE_SILO],
           assigned_by_stage[GE_STAGE_BUNKER2],
           embedded_without_root_placement, embedded_door_owner);
}

static void audit_campaign_supplies(GeAssetPack *pack,
    GeOriginalPitemModelProvider *models,InteractiveHarness *harness)
{
    size_t stage_index,total=0U,constructed=0U,filtered=0U,owned=0U;
    size_t owned_constructed=0U;
    size_t magazines=0U,multi_ammo=0U,armour=0U,slot_loads=0U;
    size_t unique_slot_models=0U,max_slot_models=0U;
    uint8_t slot_model_seen[341]={0};
    for(stage_index=0U;stage_index<GE_STAGE_COUNT;++stage_index){
        const GeStageAssetDescriptor *descriptor=
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup;StageStanHarness stan;
        size_t command,stage_constructed=0U;
        assert(descriptor!=NULL&&ge_original_stage_setup_load(
            pack,descriptor,&setup)==GE_ORIGINAL_STAGE_SETUP_OK);
        stage_stan_open(pack,descriptor,&setup,&stan);
        harness->stan=&stan.native;g_CurrentSetup=*setup.setup;
        for(command=0U;command<setup.prop_record_count;++command){
            const GeOriginalStagePropRecord *record=&setup.prop_records[command];
            GeOriginalStagePropConstructionRequest request;
            GeOriginalStagePropMaterializerProviders classifier={0};
            GeOriginalStagePropClassification classification;
            GeOriginalDefaultObjectProviders object_providers={0};
            GeOriginalStageSupplyProviders providers={0};
            GeOriginalStageSupplyInstance instance;
            PropRecord *prop;ObjectRecord *object;void *definition;
            size_t definition_size;GeOriginalStageSupplyStatus status;
            int32_t raw_amount;
            if(record->type!=PROPDEF_MAGAZINE&&record->type!=PROPDEF_AMMO
                    &&record->type!=PROPDEF_ARMOUR)continue;
            ++total;
            if(record->type==PROPDEF_MAGAZINE)++magazines;
            else if(record->type==PROPDEF_AMMO)++multi_ammo;
            else ++armour;
            assert(ge_original_stage_prop_construction_request(
                &setup,command,&request));
            classifier.context=harness;
            classifier.capabilities=GE_ORIGINAL_STAGE_PROP_CAP_SUPPLY;
            classifier.model_available=interactive_model_available;
            classifier.construct_special_object=monitor_materializer_callback;
            classification=ge_original_stage_prop_classify(record,&classifier);
            if(record->pad_id<0||(record->words[2]
                    &(PROPFLAG_INSIDEANOTHEROBJ|PROPFLAG_ASSIGNEDTOCHR))!=0U){
                PropRecord owner_prop={0};void *model_instance=NULL;
                void *collision;
                assert(record->type==PROPDEF_MAGAZINE
                    &&record->pad_id<0
                    &&(record->words[2]
                        &(PROPFLAG_INSIDEANOTHEROBJ|PROPFLAG_ASSIGNEDTOCHR))
                            ==PROPFLAG_INSIDEANOTHEROBJ
                    &&classification.blocker==GE_ORIGINAL_STAGE_PROP_READY);
                if(getenv("GE_STAGE_SUPPLY_VERBOSE")!=NULL)
                    printf("%s owned supply command %zu type %u model %d "
                           "pad %d flags %08x\n",descriptor->key,command,
                           record->type,record->model_id,record->pad_id,
                           (unsigned)record->words[2]);
                definition_size=ge_original_stage_prop_native_definition_size(
                    &request);
                definition=calloc(1U,definition_size);
                prop=calloc(1U,sizeof(*prop));collision=calloc(1U,0x50U);
                assert(definition_size!=0U&&definition!=NULL&&prop!=NULL
                       &&collision!=NULL
                       &&ge_original_stage_prop_native_definition_init(
                            &request,definition,definition_size)
                       &&ge_original_stage_item_construct_embedded_exact(
                            &request,definition,prop,sizeof(*prop),models,
                            &owner_prop,collision,&model_instance)
                            ==GE_ORIGINAL_STAGE_ITEM_OK
                       &&model_instance!=NULL&&prop->parent==&owner_prop
                       &&owner_prop.child==prop
                       &&((ObjectRecord *)definition)->prop==prop
                       &&((ObjectRecord *)definition)->model==model_instance
                       &&(((ObjectRecord *)definition)->runtime_bitflags
                            &RUNTIMEBITFLAG_HASOWNER)!=0U);
                assert(ge_original_pitem_model_release_instance(
                    models,model_instance));
                free(collision);free(prop);free(definition);
                ++owned;++owned_constructed;continue;
            }
            if(classification.blocker!=GE_ORIGINAL_STAGE_PROP_READY){
                GeOriginalPitemModelStats model_stats;
                ge_original_pitem_model_get_stats(models,&model_stats);
                fprintf(stderr,"%s supply classification command %zu model %d "
                    "pad %d type %u: %s (%s opcode %u)\n",descriptor->key,command,
                    record->model_id,record->pad_id,record->type,
                    ge_original_stage_prop_blocker_name(classification.blocker),
                    ge_original_pitem_model_status_name(
                        ge_original_pitem_model_last_status(models)),
                    model_stats.last_unsupported_opcode);
            }
            assert(classification.blocker==GE_ORIGINAL_STAGE_PROP_READY);
            definition_size=ge_original_stage_prop_native_definition_size(&request);
            assert(definition_size!=0U);
            definition=malloc(definition_size);prop=calloc(1U,sizeof(*prop));
            assert(definition!=NULL&&prop!=NULL
                   &&ge_original_stage_prop_native_definition_init(
                        &request,definition,definition_size));
            object=definition;
            if(record->type==PROPDEF_MAGAZINE)
                assert(((AmmoCrateRecord *)definition)->ammoType
                       ==(AMMOTYPE)record->words[32]);
            else if(record->type==PROPDEF_AMMO){
                MultiAmmoCrateRecord *ammo=definition;size_t slot;
                size_t record_slot_models=0U;
                for(slot=0U;slot<AMMOTYPE_GLOBAL_MAX;++slot){
                    assert(ammo->slots[slot].modelnum
                                ==(uint16_t)(record->words[32U+slot]>>16U)
                           &&ammo->slots[slot].quantity
                                ==(uint16_t)record->words[32U+slot]);
                    if(ammo->slots[slot].quantity>0U
                            &&ammo->slots[slot].modelnum!=UINT16_MAX){
                        assert(ammo->slots[slot].modelnum<341U);
                        if(!slot_model_seen[ammo->slots[slot].modelnum]){
                            slot_model_seen[ammo->slots[slot].modelnum]=1U;
                            ++unique_slot_models;
                        }
                        ++record_slot_models;
                    }
                }
                if(record_slot_models>max_slot_models)
                    max_slot_models=record_slot_models;
            }else{
                memcpy(&raw_amount,&((BodyArmourRecord *)definition)->initialamount,
                    sizeof(raw_amount));
                assert(raw_amount==(int32_t)record->words[32]);
            }
            if((record->words[3]&(UINT32_C(1)<<4U))!=0U){
                ++filtered;free(prop);free(definition);continue;
            }
            /* getposstan's exact zero-radius failure is a canonical no-object
             * branch, not permission to invent a placement. */
            if(!request.placement_resolved
                    ||(request.placement.has_stan==0U
                       &&request.placement.plink!=NULL
                       &&request.placement.plink[0]=='\0')){
                free(prop);free(definition);continue;
            }
            object_providers.context=harness;
            object_providers.model_load=interactive_item_model_load;
            object_providers.get_player_count=interactive_item_player_count;
            object_providers.get_scenario=interactive_item_scenario;
            object_providers.resolve_model_instance=interactive_item_resolve;
            object_providers.allocate_collision=interactive_item_collision;
            object_providers.get_floor_y=interactive_item_floor;
            object_providers.get_room_object_bounds=interactive_item_room_bounds;
            object_providers.walk_tiles=interactive_item_walk;
            object_providers.get_tile_rgb=interactive_item_rgb;
            providers.default_object=&object_providers;
            providers.prepared=&harness->prepared;
            providers.update_room_position=supply_update_room;
            providers.activate_prop=supply_activate;
            providers.enable_prop=supply_enable;
            harness->supply_phase=0U;
            status=ge_original_stage_supply_construct_exact(&request,
                definition,definition_size,prop,sizeof(*prop),&providers,&instance);
            if(status!=GE_ORIGINAL_STAGE_SUPPLY_OK)
                fprintf(stderr,"%s supply command %zu model %d type %u: %s\n",
                    descriptor->key,command,record->model_id,record->type,
                    ge_original_stage_supply_status_name(status));
            assert(status==GE_ORIGINAL_STAGE_SUPPLY_OK
                   &&harness->supply_phase==3U&&instance.constructed
                   &&instance.activated&&instance.definition==definition
                   &&instance.prop==prop&&instance.model==object->model
                   &&(prop->flags&PROPFLAG_ENABLED)!=0U);
            if(record->type==PROPDEF_ARMOUR){
                BodyArmourRecord *body=definition;
                assert(fabsf(body->initialamount-(float)raw_amount/65535.0f)
                           <0.00001f&&body->amount==body->initialamount);
            }
            slot_loads+=instance.slot_model_loads;++constructed;
            ++stage_constructed;
            assert(ge_original_pitem_model_release_instance(models,object->model));
            free(object->ptr_allocated_collisiondata_block);free(prop);free(definition);
        }
        printf("%s exact root supplies: %zu\n",
            descriptor->key,stage_constructed);
        harness->stan=NULL;memset(&g_CurrentSetup,0,sizeof(g_CurrentSetup));
        stage_stan_close(&stan);ge_original_stage_setup_close(&setup);
    }
    assert(total==magazines+multi_ammo+armour&&magazines==48U
           &&multi_ammo==12U&&armour==50U&&total==110U
           &&owned_constructed==owned&&owned_constructed==44U
           &&harness->supply_room_updates==constructed
           &&harness->supply_activations==constructed
           &&harness->supply_enables==constructed);
    printf("campaign supplies: %zu/%zu exact root construction; "
           "%zu magazine/%zu multi-ammo/%zu armour, %zu slot preload, "
           "%zu filtered/%zu exact canonical owner-graph magazine\n",
           constructed,total,magazines,
           multi_ammo,armour,slot_loads,filtered,owned);
    printf("multi-ammo nested models: %zu unique, %zu maximum per crate\n",
        unique_slot_models,max_slot_models);
}

static void audit_interactive_stages(GeAssetPack *pack)
{
    GeOriginalPitemModelProvider *models;
    GeOriginalPitemModelStatus model_status;
    InteractiveHarness harness = {0};
    size_t total_records = 0U;
    size_t total_doors = 0U;
    size_t total_items = 0U;
    size_t stage_index;
    models = ge_original_pitem_model_provider_create(
        pack, 341U, 256U, &model_status);
    assert(models != NULL && model_status == GE_ORIGINAL_PITEM_MODEL_OK);
    assert(ge_original_door_capacity() == 64U);
    harness.models = models;
    audit_campaign_embedded_defaults(pack, models);
    audit_campaign_supplies(pack,models,&harness);
    for (stage_index = 1U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup;
        StageStanHarness stan;
        GeOriginalStageInteractiveRuntime interactive;
        GeOriginalStageInteractiveProviders providers = {
            .context = &harness,
            .difficulty = 0U,
            .player_count = 1U,
            .model_available = interactive_model_available,
            .load_projectile_models = interactive_projectiles,
            .construct_default_object = interactive_construct_standard_item,
            .construct_embedded_item = interactive_construct_embedded,
            .release_object = interactive_release,
        };
        size_t entry_index,expected_embedded=0U,expected_assigned=0U;
        size_t live_items;
        size_t embedded_before=harness.embedded_constructions;
        size_t releases_before=harness.releases;
        assert(descriptor != NULL
               && ge_original_stage_setup_load(
                   pack, descriptor, &setup) == GE_ORIGINAL_STAGE_SETUP_OK);
        stage_stan_open(pack, descriptor, &setup, &stan);
        harness.stan=&stan.native;g_CurrentSetup=*setup.setup;
        harness.owner_prop_count=setup.prop_record_count;
        harness.owner_props=calloc(harness.owner_prop_count,
                                   sizeof(*harness.owner_props));
        assert(harness.owner_props!=NULL);
        for(entry_index=0U;entry_index<setup.prop_record_count;++entry_index){
            const GeOriginalStagePropRecord *record=&setup.prop_records[entry_index];
            int64_t owner=(int64_t)entry_index+record->pad_id;
            harness.owner_props[entry_index].type=PROP_TYPE_OBJ;
            if((record->type==PROPDEF_KEY||record->type==PROPDEF_COLLECTABLE
                    ||record->type==PROPDEF_HAT)
                    &&(record->words[3]&(UINT32_C(1)<<4U))==0U
                    &&(record->words[2]&PROPFLAG_ASSIGNEDTOCHR)!=0U)
                ++expected_assigned;
            if((record->type==PROPDEF_KEY||record->type==PROPDEF_COLLECTABLE
                    ||record->type==PROPDEF_HAT)
                    &&(record->words[2]&PROPFLAG_INSIDEANOTHEROBJ)!=0U
                    &&(record->words[2]&PROPFLAG_ASSIGNEDTOCHR)==0U
                    &&(record->words[3]&(UINT32_C(1)<<4U))==0U
                    &&!(record->type==PROPDEF_COLLECTABLE
                        &&(int8_t)(record->words[32]>>24U)==ITEM_UNARMED)
                    &&owner>=0&&owner<(int64_t)setup.prop_record_count)
                ++expected_embedded;
        }
        assert(ge_original_stage_interactive_materialize(
            &setup, &providers, &interactive));
        if(interactive.report.constructed_items
                !=interactive.report.expected_item_constructions
                    -expected_assigned)
            fprintf(stderr,"%s item closure: %zu expected/%zu live; "
                "%zu model, %zu projectile, %zu service, %zu construction blocker\n",
                descriptor->key,
                interactive.report.expected_item_constructions
                    -expected_assigned,
                interactive.report.constructed_items,
                interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_MODEL_UNAVAILABLE],
                interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_PROJECTILE_MODEL_SERVICE],
                interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_EMBEDDED_ITEM_SERVICE],
                interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_CONSTRUCTION_FAILED]);
        assert(interactive.report.constructed_items
                    ==interactive.report.expected_item_constructions
                        -expected_assigned
               &&harness.embedded_constructions-embedded_before
                    ==expected_embedded);
        assert(interactive.report.records == interactive.entry_count
               && interactive.report.definitions == interactive.entry_count
               && interactive.report.door_records
                    == ge_original_stage_setup_prop_type_count(
                        &setup, PROPDEF_DOOR)
               && interactive.report.key_records
                    == ge_original_stage_setup_prop_type_count(
                        &setup, PROPDEF_KEY)
               && interactive.report.collectable_records
                    == ge_original_stage_setup_prop_type_count(
                        &setup, PROPDEF_COLLECTABLE)
               && interactive.report.hat_records
                    == ge_original_stage_setup_prop_type_count(
                        &setup, PROPDEF_HAT)
               && interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DEFINITION] == 0U
               && interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DOOR_RELATION] == 0U);
        assert(interactive.report.constructed_doors
               <= interactive.report.expected_door_constructions
               && ge_original_stage_interactive_expected_door_count(
                    &interactive)
                    == interactive.report.expected_door_constructions);
        assert(interactive.report.constructed_items
                    <= interactive.report.expected_item_constructions
               && ge_original_stage_interactive_expected_item_count(
                    &interactive)
                    == interactive.report.expected_item_constructions
               && ge_original_stage_interactive_live_item_count(&interactive)
                    == interactive.report.constructed_items);
        {
            size_t graph_embedded=interactive_validate_owned_item_graph(
                &interactive,&harness);
            assert(graph_embedded==expected_embedded);
            if(descriptor->stage==GE_STAGE_FACILITY){
                assert(graph_embedded==12U);
                assert(ge_original_stage_interactive_root_item_count(
                    &interactive)==1U);
            }
        }
        if (descriptor->stage == GE_STAGE_AZTEC) {
            assert(interactive.report.door_records == 20U
                   && interactive.report.expected_door_constructions == 19U
                   && interactive.report.canonical_skipped_door_no_stan
                        == 1U
                   && interactive.report.blocker_counts[
                        GE_ORIGINAL_STAGE_INTERACTIVE_CANONICAL_NO_STAN]
                        == 1U);
        }
        for (entry_index = 0U; entry_index < interactive.entry_count;
                ++entry_index) {
            const GeOriginalStageInteractiveEntry *entry =
                ge_original_stage_interactive_entry(&interactive, entry_index);
            const uint32_t *words;
            uint16_t extrascale;
            uint8_t state;
            uint8_t type;
            assert(entry != NULL && entry->definition != NULL
                   && entry->record != NULL);
            words = entry->record->words;
            assert(ge_original_stage_prop_native_definition_header(
                entry->definition, &extrascale, &state, &type));
            /* objInit canonically publishes runtime collision/state bits in
             * the definition header.  Extra scale and command type remain
             * authored invariants; state is deliberately live after an
             * exact construction. */
            assert(extrascale == (uint16_t)(words[0] >> 16U)
                   && type == entry->type);
            if(entry->prop==NULL)
                assert(state == (uint8_t)(words[0] >> 8U));
            if (entry->type == PROPDEF_DOOR) {
                const DoorRecord *door = entry->definition;
                GeOriginalStagePropConstructionRequest request;
                DoorRecord *uncleared;
                size_t word;
                assert(ge_original_stage_prop_construction_request(
                    &setup, entry->command_index, &request));
                if (request.placement_resolved
                        && request.placement.stan != NULL) {
                    assert(request.placement.is_bound_pad);
                } else {
                    assert(entry->blocker
                            == GE_ORIGINAL_STAGE_INTERACTIVE_PLACEMENT_UNRESOLVED
                           || entry->blocker
                            == GE_ORIGINAL_STAGE_INTERACTIVE_CANONICAL_NO_STAN);
                }
                assert(door->linkedDoorOffset == (int32_t)words[32]
                       && door->doorFlags == (uint16_t)(words[38] >> 16U)
                       && door->doorType == (uint16_t)words[38]
                       && door->keyflags == words[39]
                       && door->autoCloseFrames == words[40]
                       && door->doorOpenSound == words[41]);
                if (entry->blocker
                        == GE_ORIGINAL_STAGE_INTERACTIVE_MODEL_UNAVAILABLE
                        || entry->blocker
                        == GE_ORIGINAL_STAGE_INTERACTIVE_PLACEMENT_UNRESOLVED) {
                    printf("%s blocked door command %zu: model %d, pad %d, "
                           "type %u, %s\n", descriptor->key,
                           entry->command_index, entry->record->model_id,
                           entry->record->pad_id, (unsigned)door->doorType,
                           ge_original_stage_interactive_blocker_name(
                               entry->blocker));
                }
                assert(door->TintDist == (int32_t)words[48]
                       && door->CullDist == (int16_t)(words[49] >> 16U)
                       && door->soundType == (int8_t)(words[49] >> 8U)
                       && door->fadeTime60 == (int8_t)words[49]);
                for (word = 50U; word < 64U; ++word)
                    assert(words[word] == 0U);
                /* The ABI adapter owns zero-initialization of native runtime
                 * fields; construction callbacks need not provide calloc. */
                uncleared = malloc(sizeof(*uncleared));
                assert(uncleared != NULL);
                memset(uncleared, 0xa5, sizeof(*uncleared));
                assert(ge_original_stage_prop_native_definition_init(
                    &request, uncleared, sizeof(*uncleared)));
                assert(uncleared->linkedDoor == NULL
                       && uncleared->unkcc == NULL
                       && uncleared->openedTime == 0U
                       && uncleared->portalNumber == 0
                       && uncleared->openSoundState == NULL
                       && uncleared->closeSoundState == NULL
                       && uncleared->lastcalc60i == 0);
                free(uncleared);
            } else if (entry->type == PROPDEF_KEY) {
                assert(((const KeyRecord *)entry->definition)->keyflags
                       == words[32]);
            } else if (entry->type == PROPDEF_COLLECTABLE) {
                const WeaponObjRecord *weapon = entry->definition;
                assert(weapon->weaponnum == (int8_t)(words[32] >> 24U)
                       && weapon->LinkedWeaponType
                            == (int8_t)(words[32] >> 16U)
                       && weapon->timer == (int16_t)words[32]
                       && words[33] == 0U && weapon->dualweapon == NULL);
            }
        }
        total_records += interactive.report.records;
        total_doors += interactive.report.door_records;
        total_items += interactive.report.key_records
            + interactive.report.collectable_records
            + interactive.report.hat_records;
        printf("%s interactive audit: %zu door/%zu key/%zu collectable/"
               "%zu hat; blockers %zu model/%zu assigned/%zu embedded/"
               "%zu placement/%zu projectile/%zu default/%zu door/"
               "%zu filtered\n",
               descriptor->key, interactive.report.door_records,
               interactive.report.key_records,
               interactive.report.collectable_records,
               interactive.report.hat_records,
               interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_MODEL_UNAVAILABLE],
               interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_ASSIGNED_ITEM_SERVICE],
               interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_EMBEDDED_ITEM_SERVICE],
               interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_PLACEMENT_UNRESOLVED],
               interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_PROJECTILE_MODEL_SERVICE],
               interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_DEFAULT_OBJECT_SERVICE],
               interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_DOOR_CONSTRUCTION_SERVICE],
               interactive.report.blocker_counts[
                    GE_ORIGINAL_STAGE_INTERACTIVE_DIFFICULTY_FILTERED]);
        live_items=interactive.report.constructed_items;
        ge_original_stage_interactive_close(&interactive);
        assert(harness.releases-releases_before
                ==live_items);
        free(harness.owner_props);harness.owner_props=NULL;
        harness.owner_prop_count=0U;
        harness.stan=NULL;memset(&g_CurrentSetup,0,sizeof(g_CurrentSetup));
        stage_stan_close(&stan);
        ge_original_stage_setup_close(&setup);
    }
    /* Exercise the concrete exact Pitem instance/lifecycle callback boundary
     * on Facility after the campaign-wide definition/blocker audit. */
    {
        const GeStageAssetDescriptor *descriptor = ge_stage_asset_facility();
        GeOriginalStageSetupRuntime setup;
        StageStanHarness stan;
        GeOriginalStageInteractiveRuntime interactive;
        GeOriginalStageInteractiveProviders providers = {
            .context = &harness,
            .difficulty = 0U,
            .player_count = 1U,
            .model_available = interactive_model_available,
            .load_projectile_models = interactive_projectiles,
            .construct_default_object = interactive_construct,
            .construct_door = interactive_construct,
            .link_doors = interactive_link_doors,
            .release_object = interactive_release,
        };
        size_t before = harness.constructions;
        assert(ge_original_stage_setup_load(
            pack, descriptor, &setup) == GE_ORIGINAL_STAGE_SETUP_OK);
        stage_stan_open(pack, descriptor, &setup, &stan);
        assert(ge_original_stage_interactive_materialize(
            &setup, &providers, &interactive));
        assert(interactive.report.constructed > 0U
               && harness.constructions - before
                    == interactive.report.constructed);
        {
            size_t active,prior=0U;
            for(active=0U;active
                    <ge_original_stage_interactive_live_item_count(&interactive);
                    ++active){
                size_t command;void *prop;
                assert(ge_original_stage_interactive_active_item(
                    &interactive,active,&command,&prop)&&prop!=NULL);
                if(active!=0U)assert(command>prior);
                assert(setup.prop_records[command].type==PROPDEF_KEY
                    ||setup.prop_records[command].type==PROPDEF_COLLECTABLE
                    ||setup.prop_records[command].type==PROPDEF_HAT);
                prior=command;
            }
        }
        ge_original_stage_interactive_close(&interactive);
        assert(harness.releases == harness.constructions);
        stage_stan_close(&stan);
        ge_original_stage_setup_close(&setup);
    }
    assert(audit_canonical_door_stage(
               pack, ge_stage_asset_dam(), models) == 18U);
    assert(audit_canonical_door_stage(
               pack, ge_stage_asset_facility(), models) == 46U);
    assert(audit_canonical_door_stage(
               pack, ge_stage_asset_descriptor(GE_STAGE_RUNWAY), models) > 0U);
    assert(audit_canonical_door_stage(
               pack, ge_stage_asset_descriptor(GE_STAGE_BUNKER1), models)
           == 17U);
    assert(audit_canonical_door_stage(
               pack, ge_stage_asset_descriptor(GE_STAGE_TRAIN), models)
           == 53U);
    assert(audit_canonical_door_stage(
               pack, ge_stage_asset_descriptor(GE_STAGE_CAVERNS), models)
           == 25U);
    assert(audit_canonical_door_stage(
               pack, ge_stage_asset_descriptor(GE_STAGE_AZTEC), models)
           == 19U);
    ge_original_pitem_model_provider_destroy(models);
    assert(total_records == total_doors + total_items);
    printf("19-stage interactive ABI audit: %zu record, %zu door, %zu item; "
           "%zu exact Pitem construction/release, %zu projectile load, "
           "%zu linked pair\n", total_records, total_doors, total_items,
           harness.constructions, harness.projectile_loads,
           harness.door_links);
}

typedef struct AttachmentTextureTest { uint16_t ids[32]; size_t count; int reject; } AttachmentTextureTest;
static int attachment_texture_visit(void *context, uint16_t image_id)
{
    AttachmentTextureTest *visit = context;
    assert(visit->count < 32U);
    visit->ids[visit->count++] = image_id;
    return !visit->reject;
}
static void test_attachment_texture_dependencies(GeAssetPack *pack)
{
    GeOriginalPitemModelStatus status;
    GeOriginalPitemModelStats before = {0}, after = {0};
    GeOriginalPitemModelProvider *models = ge_original_pitem_model_provider_create(pack, 2U, 1U, &status);
    AttachmentTextureTest visit = {0};
    void *header_ptr = NULL, *instance = NULL;
    float scale;
    assert(models != NULL);
    assert(!ge_original_pitem_model_visit_texture_ids(models, PROP_HATBERETBLUE, &visit, attachment_texture_visit));
    assert(visit.count == 0U);
    assert(ge_original_pitem_model_resolve_instance(models, PROP_HATBERETBLUE, &header_ptr, &instance, &scale));
    ge_original_pitem_model_get_stats(models, &before);
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        visit.count = 0U;
        assert(ge_original_pitem_model_visit_texture_ids(models, PROP_HATBERETBLUE, &visit, attachment_texture_visit));
        assert(visit.count == 2U && visit.ids[0] == 1777U && visit.ids[1] == 1778U);
    }
    ge_original_pitem_model_get_stats(models, &after);
    assert(memcmp(&before, &after, sizeof(before)) == 0);
    visit.count = 0U; visit.reject = 1;
    assert(!ge_original_pitem_model_visit_texture_ids(models, PROP_HATBERETBLUE, &visit, attachment_texture_visit));
    assert(visit.count == 1U);
    visit.count = 0U; visit.reject = 0;
    ModelFileHeader *header = header_ptr;
    const u32 saved = header->Textures[1].TextureID;
    header->Textures[1].TextureID = UINT32_C(0x07000000);
    assert(!ge_original_pitem_model_visit_texture_ids(models, PROP_HATBERETBLUE, &visit, attachment_texture_visit));
    assert(visit.count == 0U);
    header->Textures[1].TextureID = UINT32_C(0x05000001);
    assert(ge_original_pitem_model_visit_texture_ids(models, PROP_HATBERETBLUE, &visit, attachment_texture_visit));
    assert(visit.count == 1U && visit.ids[0] == 1777U);
    header->Textures[1].TextureID = saved;
    assert(ge_original_pitem_model_release_instance(models, instance));
    ge_original_pitem_model_provider_destroy(models);
    puts("Attachment texture dependencies: cold/repeat, exact blue-beret IDs, no allocation/state changes, rejection and embedded images passed");
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalPitemModelProvider *models;
    GeOriginalPitemModelStatus status;
    GeOriginalPitemModelStats stats;
    ConstructionHarness harness = {0};
    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    test_attachment_texture_dependencies(&pack);
    models = ge_original_pitem_model_provider_create(&pack, 32U, 128U,
                                                      &status);
    assert(models != NULL && status == GE_ORIGINAL_PITEM_MODEL_OK);
    harness.models = models;
    materialize_stage(&pack, ge_stage_asset_facility(), &harness, 109U);
    assert(harness.facility_calls == 109U && harness.bsp_models > 0U);
    harness.runway = 1;
    materialize_stage(&pack, ge_stage_asset_descriptor(GE_STAGE_RUNWAY),
                      &harness, 14U);
    assert(harness.runway_calls == 14U);
    ge_original_pitem_model_get_stats(models, &stats);
    assert(stats.loaded_models >= 12U && stats.instantiated_models == 123U);
    assert(harness.scene_models == 19U && harness.scene_parts >= 19U
           && harness.scene_triangles > 0U);
    assert(harness.instance_scene_parts >= 123U
           && harness.instance_scene_vertices > 0U
           && harness.instance_scene_vertices < 65536U
           && harness.instance_scene_batches > 0U
           && harness.instance_scene_batches < 65536U
           && harness.instance_scene_triangles > 0U);
    assert(stats.fixed_capacity_bytes > 0U
           && stats.source_blob_bytes > 0U && stats.native_resource_bytes > 0U
           && stats.native_instance_bytes > 0U
           && stats.last_unsupported_opcode == 0U);
    printf("PitemZ Facility/Runway: %zu models/%zu scene parts/%zu triangles, "
           "%zu instances -> %zu part/%zu vertex/%zu batch/%zu triangle, "
           "%zu fixed + %zu blob + %zu resource + %zu instance bytes\n",
           stats.loaded_models, harness.scene_parts, harness.scene_triangles,
           stats.instantiated_models, harness.instance_scene_parts,
           harness.instance_scene_vertices, harness.instance_scene_batches,
           harness.instance_scene_triangles,
           stats.fixed_capacity_bytes, stats.source_blob_bytes,
           stats.native_resource_bytes, stats.native_instance_bytes);
    {
        size_t index;
        for (index = 0U; index < harness.calls; ++index)
            assert(ge_original_pitem_model_release_instance(
                models, harness.instances[index]));
        ge_original_pitem_model_get_stats(models, &stats);
        assert(stats.instantiated_models == 0U
               && stats.native_instance_bytes == 0U);
        {
            void *header = NULL;
            void *instance = NULL;
            float scale = 0.0f;
            assert(ge_original_pitem_model_resolve_instance(
                models, harness.instance_model_ids[0],
                &header, &instance, &scale));
            assert(header != NULL && instance != NULL && scale > 0.0f);
            assert(ge_original_pitem_model_release_instance(
                models, instance));
        }
        /* Facility's canonical assigned AK-47 is model 184.  Keep its exact
         * GROUPSIMPLE/DL/GUNFIRE relocation boundary independently covered,
         * rather than relying only on the stage-guard attachment test. */
        {
            void *header_raw = NULL;
            void *instance = NULL;
            float scale = 0.0f;
            ModelFileHeader *header;
            size_t part_count;
            GeOriginalPitemModelGunfire gunfire;
            assert(ge_original_pitem_model_resolve_instance(
                models, 184, &header_raw, &instance, &scale));
            header = header_raw;
            assert(header != NULL && instance != NULL && scale > 0.0f);
            assert(find_opcode(header->RootNode,
                               MODELNODE_OPCODE_GROUPSIMPLE) != NULL);
            assert(find_opcode(header->RootNode, MODELNODE_OPCODE_DL) != NULL);
            {
                ModelNode *dl = find_opcode(header->RootNode, MODELNODE_OPCODE_DL);
                uint32_t origin = UINT32_MAX;
                assert(ge_original_native_model_hit_vertex_offset(
                    dl->Data->DisplayList.BaseAddr,
                    dl->Data->DisplayList.Vertices, &origin));
                assert(origin == 0x100U
                    && dl->Data->DisplayList.numVertices == 100U);
                assert(!ge_original_native_model_hit_vertex_offset(
                    dl->Data->DisplayList.BaseAddr,
                    dl->Data->DisplayList.Vertices + 1, &origin));
                assert(!ge_original_native_model_hit_vertex_offset(
                    NULL, dl->Data->DisplayList.Vertices, &origin));
            }
            assert(find_opcode(header->RootNode,
                               MODELNODE_OPCODE_GUNFIRE) != NULL);
            assert(ge_original_pitem_model_instance_gunfire_count(
                models,instance)==1U);
            assert(ge_original_pitem_model_instance_gunfire(
                models,instance,0U,&gunfire));
            assert(gunfire.image_id==2119U&&gunfire.image_width==32U
                &&gunfire.image_height==32U&&gunfire.matrix_index==0U
                &&gunfire.visible==0U);
            assert(fabsf(gunfire.offset[0]-(-407.0f))<0.01f);
            assert(fabsf(gunfire.size[0]-533.0f)<0.01f);
            part_count = ge_original_pitem_model_scene_part_count(
                models, 184);
            assert(part_count > 0U);
            assert(ge_original_pitem_model_release_instance(
                models, instance));
        }
        /* The unchanged startup constructors load these three exact PitemZ
         * records before the wallet menus.  Keep their relocation and scene
         * traversal boundary covered so the 3DS startup cannot regress to a
         * hand-authored logo mesh. */
        {
            static const int32_t startup_models[]={
                PROP_LEGALPAGE,PROP_NINTENDOLOGO,PROP_GOLDENEYELOGO,
            };
            size_t startup_index;
            for(startup_index=0U;startup_index<sizeof(startup_models)
                    /sizeof(startup_models[0]);++startup_index){
                void *header_raw=NULL;
                void *instance=NULL;
                float scale=0.0f;
                size_t part_count;
                assert(ge_original_pitem_model_load(
                    models,startup_models[startup_index]));
                part_count=ge_original_pitem_model_scene_part_count(
                    models,startup_models[startup_index]);
                assert(part_count>0U);
                assert(ge_original_pitem_model_resolve_instance(
                    models,startup_models[startup_index],
                    &header_raw,&instance,&scale));
                assert(header_raw!=NULL&&instance!=NULL&&scale>0.0f);
                assert(ge_original_pitem_model_instance_scene_part_count(
                    models,instance)>0U);
                assert(ge_original_pitem_model_release_instance(
                    models,instance));
            }
            {
                GeOriginalPitemEmbeddedTexture texture;
                assert(ge_original_pitem_model_embedded_texture(
                    models,PROP_LEGALPAGE,UINT32_C(0x05000090),&texture));
                assert(texture.pixels!=NULL&&texture.available_bytes>=2048U
                    &&texture.width==32U&&texture.height==32U
                    &&texture.render_depth==2U);
                assert(!ge_original_pitem_model_embedded_texture(
                    models,PROP_LEGALPAGE,UINT32_C(0x00000090),&texture));
            }
        }
        /* PwalletbondZ is one shared authored model whose switch table selects
         * the folder, paper, stamps and photographs for every frontend page.
         * Prove that scene enumeration follows the live instance relation
         * state instead of flattening all mutually-exclusive children. */
        {
            void *header_raw = NULL;
            void *instance = NULL;
            float scale = 0.0f;
            ModelFileHeader *header;
            size_t all_parts;
            size_t hidden_parts;
            size_t folder_parts;
            assert(ge_original_pitem_model_load(models, PROP_WALLETBOND));
            assert(ge_original_pitem_model_resolve_instance(
                models, PROP_WALLETBOND, &header_raw, &instance, &scale));
            header = header_raw;
            assert(header != NULL && instance != NULL && scale > 0.0f
                   && header->numSwitches > SW_BROSNANCOVER);
            all_parts = ge_original_pitem_model_scene_part_count(
                models, PROP_WALLETBOND);
            assert(all_parts > 0U);
            assert(ge_original_pitem_model_instance_disable_switches(
                models, instance));
            hidden_parts =
                ge_original_pitem_model_instance_scene_part_count(
                    models, instance);
            assert(hidden_parts < all_parts);
            assert(ge_original_pitem_model_instance_set_switch(
                models, instance, SW_COVER, 1));
            assert(ge_original_pitem_model_instance_set_switch(
                models, instance, SW_PHOTOCOVER, 1));
            assert(ge_original_pitem_model_instance_set_switch(
                models, instance, SW_BROSNANCOVER, 1));
            folder_parts =
                ge_original_pitem_model_instance_scene_part_count(
                    models, instance);
            assert(folder_parts > hidden_parts && folder_parts < all_parts);
            {
                size_t part_index;
                for (part_index = 0U; part_index < folder_parts;
                        ++part_index) {
                    GeOriginalPitemModelScenePart part;
                    assert(ge_original_pitem_model_instance_scene_part(
                        models, instance, part_index, &part));
                    assert(part.node != NULL && part.blob != NULL
                           && part.primary_offset != UINT32_MAX);
                }
            }
            assert(ge_original_pitem_model_instance_disable_switches(
                models, instance));
            assert(ge_original_pitem_model_instance_set_switch(
                models, instance, SW_TABS, 1));
            assert(ge_original_pitem_model_instance_set_switch(
                models, instance, SW_SLIDES, 1));
            assert(ge_original_pitem_model_instance_set_switch(
                models, instance, SW_PICS, 1));
            {
                const ModelNode *pictures =
                    ge_original_pitem_model_instance_switch_node(
                        models, instance, GFXHIT0_PICS);
                GeOriginalPitemModelScenePart picture_part;
                size_t picture_part_index;
                const size_t picture_parts =
                    ge_original_pitem_model_instance_scene_part_count(
                        models, instance);
                size_t part_index;
                int found = 0;
                assert(pictures != NULL && picture_parts > 0U);
                assert(ge_original_pitem_model_scene_part_for_node(
                    models, PROP_WALLETBOND, pictures,
                    &picture_part_index, &picture_part));
                assert((((const ModelNode *)picture_part.node)->Opcode & 0xffU)
                        == MODELNODE_OPCODE_DLCOLLISION);
                assert(picture_part.vertex_count == 80U);
                for (part_index = 0U; part_index < picture_parts;
                        ++part_index) {
                    GeOriginalPitemModelScenePart part;
                    assert(ge_original_pitem_model_instance_scene_part(
                        models, instance, part_index, &part));
                    if (part.segment4_offset
                            != picture_part.segment4_offset) continue;
                    assert(part.segment4_offset <= part.blob_size
                           && picture_part.vertex_count
                                <= (part.blob_size - part.segment4_offset)
                                    / 16U);
                    found = 1;
                }
                assert(found);
            }
            assert(ge_original_pitem_model_release_instance(
                models, instance));
        }
    }
    ge_original_pitem_model_provider_destroy(models);
    materialize_supported_stage(
        &pack, ge_stage_asset_descriptor_by_key("streets"), 0);
    materialize_supported_stage(
        &pack, ge_stage_asset_descriptor_by_key("streets"), 1);
    materialize_supported_stage(
        &pack, ge_stage_asset_descriptor_by_key("depot"), 0);
    audit_interactive_stages(&pack);
    audit_stage_monitors(&pack);
    ge_asset_pack_close(&pack);
    puts("canonical PitemZ provider passed");
    return 0;
}

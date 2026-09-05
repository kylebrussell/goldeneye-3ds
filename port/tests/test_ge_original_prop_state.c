#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/player.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_dam_intro.h"
#include "ge_original_dam_world.h"
#include "ge_original_default_object.h"
#include "ge_original_door.h"
#include "ge_original_door_collision.h"
#include "ge_original_door_collision_internal.h"
#include "ge_original_door_runtime.h"
#include "ge_original_door_scene.h"
#include "ge_original_model104.h"
#include "ge_original_model178.h"
#include "ge_original_model62.h"
#include "ge_original_prop_state.h"

extern stagesetup UsetupdamZ;
extern PropRecord g_Props[MAX_PROPS];

static void check_active_snapshot(void)
{
    GeOriginalPropActiveSet active;
    assert(ge_original_prop_state_snapshot_active(&active));
    for (size_t i = 0U; i < MAX_PROPS; ++i)
        assert(ge_original_prop_state_active_set_contains(&active, &g_Props[i])
            == ge_original_prop_state_is_active(&g_Props[i]));
    assert(!ge_original_prop_state_active_set_contains(&active, NULL));
    assert(!ge_original_prop_state_active_set_contains(&active, (char *)g_Props + 1));
    assert(!ge_original_prop_state_snapshot_active(NULL));
}
extern DoorRecord *ge_port_door_runtime_native_definition(void *definition);

typedef struct PropStateHarness {
    GeOriginalPropState props;
    struct player player;
    struct player_data player_permissions;
    ChrRecord guard;
    union {
        max_align_t alignment;
        unsigned char bytes[sizeof(DoorRecord)];
    } definitions[4];
    uint32_t definition_count;
    uint32_t model_load_calls;
    int32_t loaded_model_id;
    GeOriginalModel62 model62;
    GeOriginalModel104 model104;
    GeOriginalModel178 model178[2];
    uint32_t model178_resolve_count;
    uint8_t model62_blob[GE_ORIGINAL_MODEL62_BLOB_SIZE];
    uint8_t model104_blob[GE_ORIGINAL_MODEL104_BLOB_SIZE];
    uint8_t model178_blob[GE_ORIGINAL_MODEL178_BLOB_SIZE];
    int use_model62;
    ModelFileHeader model_header;
    Model model;
    union CollisionStorage {
        max_align_t alignment;
        unsigned char bytes[0x50];
    } collisions[4];
    uint32_t collision_next;
    uint32_t model_resolve_calls;
    uint32_t collision_allocation_calls;
    uint32_t floor_calls;
    uint32_t room_bounds_calls;
    uint32_t walk_calls;
    uint32_t tile_rgb_calls;
    uint32_t portal_room_calls;
    uint32_t portal_find_calls;
    uint32_t portal_toggle_calls;
    int32_t global_timer;
    int32_t clock_timer;
    uint32_t door_sound_calls;
    float floor_y;
    struct {
        uint32_t id : 24;
        uint8_t room;
        int16_t mid;
        int16_t tail;
    } stan;
} PropStateHarness;

static int32_t runtime_global_timer(void *context)
{ return ((PropStateHarness *)context)->global_timer; }
static int32_t runtime_clock_timer(void *context)
{ return ((PropStateHarness *)context)->clock_timer; }
static void runtime_sound(void *context, void *door,
                          GeOriginalDoorSoundEvent event)
{
    PropStateHarness *harness = context;
    (void)door; (void)event;
    harness->door_sound_calls++;
}

static int16_t read_scene_be16(const uint8_t *bytes)
{
    return (int16_t)(uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static int32_t model_load(void *context, int32_t model_id)
{
    PropStateHarness *harness = context;
    harness->model_load_calls++;
    harness->loaded_model_id = model_id;
    if (model_id == 104)
        return ge_original_model104_model_load(&harness->model104, model_id);
    if (model_id == 178)
        return ge_original_model178_model_load(&harness->model178[0], model_id)
            && ge_original_model178_model_load(&harness->model178[1], model_id);
    return harness->use_model62
        ? ge_original_model62_model_load(&harness->model62, model_id) : 1;
}

static int32_t get_player_count(void *context)
{
    (void)context;
    return 1;
}

static int32_t get_scenario(void *context)
{
    (void)context;
    return 0;
}

static int resolve_model_instance(void *context, int32_t model_id,
                                  void **model_header, void **model_instance,
                                  float *pitem_scale)
{
    PropStateHarness *harness = context;

    assert(model_header != NULL);
    assert(model_instance != NULL);
    assert(pitem_scale != NULL);
    if (model_id == 104) {
        harness->model_resolve_calls++;
        return ge_original_model104_resolve_instance(
            &harness->model104, model_id, model_header, model_instance,
            pitem_scale);
    }
    if (model_id == 178) {
        const uint32_t instance = harness->model178_resolve_count < 2U
            ? harness->model178_resolve_count++ : 1U;
        harness->model_resolve_calls++;
        return ge_original_model178_resolve_instance(
            &harness->model178[instance], model_id, model_header, model_instance,
            pitem_scale);
    }
    assert(model_id == 62);
    if (harness->use_model62) {
        harness->model_resolve_calls++;
        return ge_original_model62_resolve_instance(
            &harness->model62, model_id, model_header, model_instance,
            pitem_scale);
    }
    memset(&harness->model_header, 0, sizeof(harness->model_header));
    memset(&harness->model, 0, sizeof(harness->model));
    harness->model.obj = &harness->model_header;
    *model_header = &harness->model_header;
    *model_instance = &harness->model;
    *pitem_scale = 0.1f;
    harness->model_resolve_calls++;
    return 1;
}

static void *allocate_collision(void *context, uint32_t size_bytes)
{
    PropStateHarness *harness = context;

    assert(size_bytes == sizeof(harness->collisions[0].bytes));
    assert(harness->collision_next < 4U);
    memset(harness->collisions[harness->collision_next].bytes, 0,
           sizeof(harness->collisions[harness->collision_next].bytes));
    harness->collision_allocation_calls++;
    return harness->collisions[harness->collision_next++].bytes;
}

static int get_floor_y(void *context, void *stan, float x, float z,
                       float *floor_y)
{
    PropStateHarness *harness = context;
    (void)x; (void)z;
    assert(stan == &harness->stan);
    *floor_y = harness->floor_y;
    harness->floor_calls++;
    return 1;
}

static int get_room_object_bounds(void *context, const float position[3],
                                  int16_t room, float *top, float *bottom)
{
    PropStateHarness *harness = context;
    assert(position != NULL && room == 135);
    harness->room_bounds_calls++;
    return ge_original_prop_state_room_object_at_position(
        position, room, top, bottom) != NULL;
}

static int walk_tiles(void *context, void **stan, float start_x,
                      float start_z, float destination_x,
                      float destination_z)
{
    PropStateHarness *harness = context;
    (void)start_x; (void)start_z; (void)destination_x; (void)destination_z;
    assert(*stan == &harness->stan);
    harness->walk_calls++;
    return 1;
}

static int get_tile_rgb(void *context, void *stan, float x, float z,
                        uint8_t rgb[3])
{
    PropStateHarness *harness = context;
    uint16_t mid = (uint16_t)harness->stan.mid;
    (void)x; (void)z;
    assert(stan == &harness->stan);
    rgb[0] = (uint8_t)(((mid >> 8) & 0xfU) * 0x11U);
    rgb[1] = (uint8_t)(((mid >> 4) & 0xfU) * 0x11U);
    rgb[2] = (uint8_t)((mid & 0xfU) * 0x11U);
    harness->tile_rgb_calls++;
    return 1;
}

static int portal_rooms(void *context, const void *bound_pad,
                        int32_t *room_a, int32_t *room_b,
                        float point_a[3], float point_b[3])
{
    PropStateHarness *harness=context;
    assert(bound_pad==&UsetupdamZ.boundpads[6]
           ||bound_pad==&UsetupdamZ.boundpads[9]);
    *room_a=135;*room_b=136;
    memcpy(point_a,UsetupdamZ.boundpads[6].pos.f,sizeof(float)*3U);
    memcpy(point_b,point_a,sizeof(float)*3U);point_b[0]+=1.0f;
    harness->portal_room_calls++;return 1;
}

static int32_t find_portal(void *context,int32_t room_a,int32_t room_b,
                           const float point_a[3],const float point_b[3])
{
    PropStateHarness *harness=context;(void)point_a;(void)point_b;
    assert(room_a==135&&room_b==136);harness->portal_find_calls++;return 7;
}

static void set_portal_open(void *context,int32_t portal,int open)
{
    PropStateHarness *harness=context;assert(portal==7);assert(open==0);
    harness->portal_toggle_calls++;
}

static stagesetup *load_setup(void *context, int32_t stage_id)
{
    (void)context;
    return stage_id == LEVELID_DAM ? &UsetupdamZ : NULL;
}

static float get_room_scale_reciprocal(void *context)
{
    (void)context;
    return 1.0f / 0.23363999f;
}

static void *resolve_stan(void *context, const char *name)
{
    PropStateHarness *harness = context;
    assert(name != NULL);
    return &harness->stan;
}

static void *allocate_definition(void *context, uint8_t type,
                                 size_t size_bytes)
{
    PropStateHarness *harness = context;
    (void)type;
    assert(size_bytes <= sizeof(harness->definitions[0].bytes));
    assert(harness->definition_count < 4U);
    return &harness->definitions[harness->definition_count++];
}

static void *allocate_prop(void *context, void *definition)
{
    PropStateHarness *harness = context;
    return ge_original_prop_state_allocate(&harness->props, definition);
}

static void activate_prop(void *context, void *prop)
{
    PropStateHarness *harness = context;
    ge_original_prop_state_activate(&harness->props, prop);
}

static void enable_prop(void *context, void *prop)
{
    PropStateHarness *harness = context;
    ge_original_prop_state_enable(&harness->props, prop);
}

static void register_room(void *context, void *prop, int16_t room)
{
    PropStateHarness *harness = context;
    ge_original_prop_state_register_room(&harness->props, prop, room);
}

int main(int argc, char **argv)
{
    PropStateHarness harness;
    GeOriginalSetupPadState pad_state;
    GeOriginalDamWorldState world;
    GeOriginalDefaultObjectPrepared prepared;
    GeOriginalDefaultObjectPrepared glass_prepared;
    GeOriginalDoorPrepared door_prepared[2];
    GeOriginalDoorPrepared door_scratch;
    GeOriginalSetupPadProviders pad_providers;
    GeOriginalDamWorldProviders world_providers;
    GeOriginalDefaultObjectProviders object_providers;
    GeOriginalDoorProviders door_providers;
    GeOriginalDoorRuntimeProviders door_runtime_providers;
    GeOriginalDoorRuntimeState door_runtime_state;
    GeOriginalDoorCollisionState door_collision_state;
    const GeOriginalDamWorldEntry *entries[3];
    PropRecord *player_prop;
    PropRecord *guard_prop = NULL;
    PropRecord *object_prop;
    ObjectRecord *object;
    uint8_t object_state;
    uint32_t i;

    assert(argc == 1 || argc == 4);
    memset(&harness, 0, sizeof(harness));
    if (argc == 4) {
        FILE *model_file = fopen(argv[1], "rb");
        assert(model_file != NULL);
        assert(fread(harness.model62_blob, 1, sizeof(harness.model62_blob),
                     model_file) == sizeof(harness.model62_blob));
        assert(fgetc(model_file) == EOF);
        assert(fclose(model_file) == 0);
        assert(ge_original_model62_relocate(
            &harness.model62, harness.model62_blob,
            sizeof(harness.model62_blob)) == GE_ORIGINAL_MODEL62_OK);
        harness.use_model62 = 1;
        model_file = fopen(argv[2], "rb");
        assert(model_file != NULL);
        assert(fread(harness.model104_blob, 1,
                     sizeof(harness.model104_blob), model_file)
               == sizeof(harness.model104_blob));
        assert(fgetc(model_file) == EOF);
        assert(fclose(model_file) == 0);
        assert(ge_original_model104_relocate(
            &harness.model104, harness.model104_blob,
            sizeof(harness.model104_blob)) == GE_ORIGINAL_MODEL104_OK);
        model_file = fopen(argv[3], "rb");
        assert(model_file != NULL);
        assert(fread(harness.model178_blob,1,sizeof(harness.model178_blob),model_file)
               ==sizeof(harness.model178_blob));
        assert(fgetc(model_file)==EOF);assert(fclose(model_file)==0);
        assert(ge_original_model178_relocate(&harness.model178[0],
            harness.model178_blob,sizeof(harness.model178_blob))==GE_ORIGINAL_MODEL178_OK);
        assert(ge_original_model178_relocate(&harness.model178[1],
            harness.model178_blob,sizeof(harness.model178_blob))==GE_ORIGINAL_MODEL178_OK);
    }
    harness.stan.room = 135;
    harness.stan.mid = 0x0f00;
    harness.floor_y = 100.0f;
    assert(ge_original_prop_state_reset(&harness.props, 137U));
    assert(ge_original_prop_state_native_prop_size() == sizeof(PropRecord));
    player_prop = ge_original_prop_state_allocate_player(&harness.props);
    assert(player_prop != NULL);
    player_prop->type = PROP_TYPE_VIEWER;
    player_prop->stan = (StandTile *)&harness.stan;
    player_prop->rooms[0] = 135;
    player_prop->rooms[1] = 0xffU;
    ge_original_prop_state_activate(&harness.props, player_prop);
    ge_original_prop_state_enable(&harness.props, player_prop);
    ge_original_prop_state_register_room(&harness.props, player_prop, 135);
    harness.player.prop = player_prop;
    harness.player.field_AC = 1;
    harness.player.eyeheight = 100.0f;
    harness.player.field_488.collision_radius = 20.0f;
    harness.player_permissions.player_perspective_height = 1.0f;
    ge_original_bond_input_bind_player(
        &harness.player, &harness.player_permissions);
    ge_original_bond_input_provider_reset_normal_dam();

    memset(&pad_providers, 0, sizeof(pad_providers));
    pad_providers.context = &harness;
    pad_providers.load_setup = load_setup;
    pad_providers.get_room_scale_reciprocal = get_room_scale_reciprocal;
    pad_providers.resolve_stan = resolve_stan;
    ge_original_setup_pad_bind(&pad_providers, &pad_state);
    ge_original_setup_pad_load(LEVELID_DAM);
    assert(pad_state.loaded);

    memset(&world_providers, 0, sizeof(world_providers));
    world_providers.context = &harness;
    world_providers.allocate_definition = allocate_definition;
    world_providers.allocate_prop = allocate_prop;
    world_providers.activate_prop = activate_prop;
    world_providers.enable_prop = enable_prop;
    world_providers.register_room = register_room;
    ge_dam_setup_world_materializer_bind(&world_providers, &world);
    ge_dam_setup_world_materialize_first_authored();

    assert(world.loaded);
    assert(world.definitions_materialized == 3U);
    assert(harness.props.allocation_calls == 4U);
    assert(harness.props.activation_calls == 1U);
    assert(harness.props.enable_calls == 1U);
    assert(harness.props.room_registration_calls == 4U);
    assert(ge_original_prop_state_active_count() == 1U);
    check_active_snapshot();
    assert(ge_original_prop_state_is_active(player_prop));
    assert(ge_original_prop_state_is_enabled(player_prop));
    assert(ge_original_prop_state_room_contains(135, player_prop));

    entries[0] = &world.first_glass;
    entries[1] = &world.first_object;
    entries[2] = &world.first_door;
    for (i = 0; i < 3U; i++) {
        assert(entries[i]->prop != NULL);
        assert(entries[i]->room == 135);
        assert(!ge_original_prop_state_is_active(entries[i]->prop));
        assert(!ge_original_prop_state_is_enabled(entries[i]->prop));
        assert(ge_original_prop_state_room_contains(135, entries[i]->prop));
    }
    assert(ge_dam_setup_world_materialize_linked_door());
    assert(world.second_door.command_index == 268);
    assert(world.definitions_materialized == 4U);
    assert(harness.props.allocation_calls == 5U);
    assert(harness.props.room_registration_calls == 5U);
    assert(world.second_door.prop != NULL);
    assert(!ge_original_prop_state_is_active(world.second_door.prop));
    assert(ge_original_prop_state_room_contains(135, world.second_door.prop));

    memset(&object_providers, 0, sizeof(object_providers));
    object_providers.context = &harness;
    object_providers.model_load = model_load;
    object_providers.get_player_count = get_player_count;
    object_providers.get_scenario = get_scenario;
    object_providers.resolve_model_instance = resolve_model_instance;
    object_providers.allocate_collision = allocate_collision;
    object_providers.get_floor_y = get_floor_y;
    object_providers.get_room_object_bounds = get_room_object_bounds;
    object_providers.walk_tiles = walk_tiles;
    object_providers.get_tile_rgb = get_tile_rgb;
    ge_original_default_object_bind(&object_providers, &glass_prepared);
    assert((harness.use_model62
               ? ge_original_default_object_construct_standard(
                   world.first_glass.definition,
                   world.first_glass.command_index)
               : ge_original_default_object_prepare_standard(
                   world.first_glass.definition,
                   world.first_glass.command_index))
           == GE_ORIGINAL_DEFAULT_OBJECT_OK);
    assert(glass_prepared.bound_pad);
    assert(glass_prepared.model_id == 104);
    assert(glass_prepared.pad_id == 10076);
    assert(glass_prepared.flags == 0x04000b62U);
    assert(glass_prepared.stan == UsetupdamZ.boundpads[76].stan);
    for (i = 0; i < 3U; i++) {
        float expected_shade = glass_prepared.position[i]
            + UsetupdamZ.boundpads[76].up.f[i]
              * ((UsetupdamZ.boundpads[76].bbox.ymin
                  - UsetupdamZ.boundpads[76].bbox.ymax) * 0.5f);
        assert(fabsf(glass_prepared.shade_position[i] - expected_shade)
               < 0.0001f);
    }
    assert(harness.walk_calls == 1U);
    if (harness.use_model62) {
        float queried_top;
        float queried_bottom;
        assert(glass_prepared.object_initialized);
        assert(glass_prepared.model_header == &harness.model104.header);
        assert(glass_prepared.model_instance == &harness.model104.model);
        assert(glass_prepared.collision_data
               == harness.collisions[0].bytes);
        assert(ge_original_default_object_place_standard(
                   world.first_glass.definition)
               == GE_ORIGINAL_DEFAULT_OBJECT_OK);
        assert(glass_prepared.placement_completed);
        assert(((PropRecord *)world.first_glass.prop)->stan != NULL);
        assert(((struct collision_data *)glass_prepared.collision_data)->edges
               >= 4);
        assert(ge_dam_setup_world_activate_entry(&world.first_glass));
        assert(ge_original_prop_state_room_object_at_position(
                   ((PropRecord *)world.first_glass.prop)->pos.f, 135,
                   &queried_top, &queried_bottom)
               == world.first_glass.definition);
        assert(isfinite(queried_top) && isfinite(queried_bottom));
        assert(harness.walk_calls == 2U);
        assert(harness.tile_rgb_calls == 1U);
        assert(ge_original_prop_state_active_count() == 2U);
    }
    harness.walk_calls = 0U;
    harness.tile_rgb_calls = 0U;
    harness.model_load_calls = 0U;
    harness.model_resolve_calls = 0U;
    harness.collision_allocation_calls = 0U;
    ge_original_default_object_bind(&object_providers, &prepared);
    assert(ge_original_default_object_construct_standard(
               world.first_object.definition,
               world.first_object.command_index)
           == GE_ORIGINAL_DEFAULT_OBJECT_OK);
    assert(prepared.prepared);
    assert(prepared.model_id == 62);
    assert(prepared.pad_id == 358);
    assert(prepared.model_load_calls == 1U);
    assert(harness.model_load_calls == 1U);
    assert(harness.loaded_model_id == 62);
    assert(fabsf(prepared.extra_scale - 2.0f) < 0.0001f);
    assert(fabsf(prepared.damage - 250.0f) < 0.0001f);
    assert(prepared.flags == 0x00000101U);
    assert(prepared.flags2 == 0x00000200U);
    assert(prepared.stan == UsetupdamZ.pads[358].stan);
    for (i = 0; i < 3U; i++) {
        assert(fabsf(prepared.position[i] - UsetupdamZ.pads[358].pos.f[i])
               < 0.0001f);
        assert(fabsf(prepared.shade_position[i]
                     - UsetupdamZ.pads[358].pos.f[i]) < 0.0001f);
    }
    assert(fabsf(prepared.matrix[3][3] - 1.0f) < 0.0001f);
    assert(prepared.position_validated);
    assert(prepared.object_initialized);
    assert(harness.model_resolve_calls == 1U);
    assert(harness.collision_allocation_calls == 1U);
    assert(prepared.model_header == (harness.use_model62
               ? (void *)&harness.model62.header : (void *)&harness.model_header));
    assert(prepared.model_instance == (harness.use_model62
               ? (void *)&harness.model62.model : (void *)&harness.model));
    assert(prepared.prop == world.first_object.prop);
    assert(prepared.collision_data == (harness.use_model62
               ? (void *)harness.collisions[1].bytes
               : (void *)harness.collisions[0].bytes));
    assert(fabsf(prepared.pitem_scale - 0.1f) < 0.0001f);
    assert(fabsf((harness.use_model62 ? harness.model62.model.scale
                                     : harness.model.scale) - 0.2f) < 0.0001f);

    object = world.first_object.definition;
    object_prop = world.first_object.prop;
    assert(object->model == (harness.use_model62
               ? &harness.model62.model : &harness.model));
    assert(object->prop == world.first_object.prop);
    assert((void *)object->ptr_allocated_collisiondata_block
           == (harness.use_model62
               ? (void *)harness.collisions[1].bytes
               : (void *)harness.collisions[0].bytes));
    assert(ge_dam_setup_world_definition_header(
        object, NULL, &object_state, NULL));
    assert((object_state & PROPSTATE_EXT_COLISION_BLOCK) != 0U);
    assert(object->model->unk00 == -1);
    assert(object->model->chr == NULL);
    assert(object_prop->type == PROP_TYPE_OBJ);
    assert(object_prop->obj == object);
    /* This exact objInit boundary deliberately precedes moveToPad. */
    assert(object_prop->pos.x == 0.0f);
    assert(object_prop->pos.y == 0.0f);
    assert(object_prop->pos.z == 0.0f);
    assert(object_prop->stan == NULL);
    assert(object->runtime_pos.x == 0.0f);
    assert(object->runtime_pos.y == 0.0f);
    assert(object->runtime_pos.z == 0.0f);
    assert(object->maxdamage == 0.0f);
    assert(object->shadecol.r == 0U && object->shadecol.g == 0U
           && object->shadecol.b == 0U && object->shadecol.a == 0U);
    assert(object->nextcol.r == 0U && object->nextcol.g == 0U
           && object->nextcol.b == 0U && object->nextcol.a == 0U);

    if (harness.use_model62) {
        ModelRoData_BoundingBoxRecord *bbox =
            &harness.model62.bbox_data.BoundingBox;
        struct collision_data *generated_collision =
            object->ptr_allocated_collisiondata_block;
        float expected_x = prepared.position[0]
            - prepared.matrix[1][0] * -12.0f;
        float expected_y = harness.floor_y
            - prepared.matrix[1][1] * -12.0f + 4.0f;
        float expected_z = prepared.position[2]
            - prepared.matrix[1][2] * -12.0f;
        float expected_bottom = expected_y
            + (prepared.matrix[0][1] >= 0.0f
               ? bbox->Bounds.xmin : bbox->Bounds.xmax)
                * prepared.matrix[0][1]
            + (prepared.matrix[1][1] >= 0.0f
               ? bbox->Bounds.ymin : bbox->Bounds.ymax)
                * prepared.matrix[1][1]
            + (prepared.matrix[2][1] >= 0.0f
               ? bbox->Bounds.zmin : bbox->Bounds.zmax)
                * prepared.matrix[2][1];
        float expected_top = expected_y
            + (prepared.matrix[0][1] <= 0.0f
               ? bbox->Bounds.xmin : bbox->Bounds.xmax)
                * prepared.matrix[0][1]
            + (prepared.matrix[1][1] <= 0.0f
               ? bbox->Bounds.ymin : bbox->Bounds.ymax)
                * prepared.matrix[1][1]
            + (prepared.matrix[2][1] <= 0.0f
               ? bbox->Bounds.zmin : bbox->Bounds.zmax)
                * prepared.matrix[2][1];
        float queried_top;
        float queried_bottom;
        float query_position[3];
        int edge;
        assert(ge_original_default_object_place_standard(object)
               == GE_ORIGINAL_DEFAULT_OBJECT_OK);
        assert(prepared.placement_completed
               && prepared.placement_stage
                  == GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_COMPLETE);
        assert(harness.floor_calls == 1U && harness.room_bounds_calls == 1U);
        assert(harness.walk_calls == 1U && harness.tile_rgb_calls == 1U);
        assert(fabsf(object_prop->pos.x - expected_x) < 0.0001f);
        assert(fabsf(object_prop->pos.y - expected_y) < 0.0001f);
        assert(fabsf(object_prop->pos.z - expected_z) < 0.0001f);
        assert(object_prop->stan == (StandTile *)&harness.stan);
        assert(object->shadecol.r == 63U && object->shadecol.g == 0U
               && object->shadecol.b == 0U && object->shadecol.a == 132U);
        assert(generated_collision->edges >= 4
               && generated_collision->edges <= 8);
        assert(fabsf(generated_collision->bottom - expected_bottom) < 0.0001f);
        assert(fabsf(generated_collision->top - expected_top) < 0.0001f);
        for (edge = 0; edge < generated_collision->edges; edge++) {
            assert(isfinite(generated_collision->polygon[edge].x));
            assert(isfinite(generated_collision->polygon[edge].y));
        }
    assert(ge_dam_setup_world_activate_entry(&world.first_object));
    assert(ge_original_prop_state_active_count() == 3U);
    check_active_snapshot();
    {
        float scene_matrix[4][4];
        float scene_position[3];
        uint8_t scene_room = UINT8_MAX;

        assert(ge_original_prop_state_object_scene_transform(
            object, object_prop, scene_matrix, scene_position, &scene_room));
        assert(scene_room == 135U);
        assert(memcmp(scene_matrix, object->mtx.m,
                      sizeof(scene_matrix)) == 0);
        assert(memcmp(scene_position, object->runtime_pos.f,
                      sizeof(scene_position)) == 0);
        {
            float identity[4][4] = {{0}};
            const float (*scene_matrices)[4][4] = NULL;
            size_t scene_matrix_count = 0U;
            identity[0][0] = identity[1][1] = 1.0f;
            identity[2][2] = identity[3][3] = 1.0f;
            assert(!ge_original_prop_state_object_scene_matrix_bank(
                object, object_prop, &scene_matrices, &scene_matrix_count));
            assert(scene_matrices == NULL && scene_matrix_count == 0U);
            assert(ge_original_prop_state_publish_scene_visibility(
                object_prop, 1, identity));
            {
                GeOriginalPropActiveSet active;
                PropRecord expected = *object_prop;
                Mtxf expected_matrix = {0};
                if (harness.use_model62)
                    expected_matrix = harness.model62.model.render_pos[0].pos;
                assert(ge_original_prop_state_snapshot_active(&active));
                assert(ge_original_prop_state_publish_scene_visibility_with_active_set(
                    object_prop, 0, identity, &active));
                assert(!(object_prop->flags & PROPFLAG_ONSCREEN));
                assert(ge_original_prop_state_publish_scene_visibility_with_active_set(
                    object_prop, 1, identity, &active));
                assert(memcmp(object_prop, &expected, sizeof(expected)) == 0);
                if (harness.use_model62)
                    assert(memcmp(&harness.model62.model.render_pos[0].pos,
                        &expected_matrix, sizeof(expected_matrix)) == 0);
            }
            assert((object_prop->flags & PROPFLAG_ONSCREEN) != 0U);
            assert(fabsf(object_prop->zDepth + object_prop->pos.z)
                   < 0.0001f);
            if (harness.use_model62) {
                const Mtxf *published =
                    &harness.model62.model.render_pos[0].pos;
                int row;
                int column;
                for (row = 0; row < 3; ++row) {
                    for (column = 0; column < 3; ++column) {
                        assert(fabsf(published->m[row][column]
                                     - object->mtx.m[row][column])
                               < 0.0001f);
                    }
                }
                assert(fabsf(published->m[3][0]
                             - object->runtime_pos.x) < 0.0001f);
                assert(fabsf(published->m[3][1]
                             - object->runtime_pos.y) < 0.0001f);
                assert(fabsf(published->m[3][2]
                             - object->runtime_pos.z) < 0.0001f);
                assert(fabsf(published->m[3][3] - 1.0f) < 0.0001f);
                assert(ge_original_prop_state_object_scene_matrix_bank(
                    object, object_prop, &scene_matrices,
                    &scene_matrix_count));
                assert(scene_matrices
                           == (const float (*)[4][4])(const void *)
                                harness.model62.model.render_pos
                       && scene_matrix_count
                            == (size_t)harness.model62.header.numMatrices);
                assert(memcmp(scene_matrices[0], published->m,
                              sizeof(scene_matrices[0])) == 0);
            }
            assert(ge_original_prop_state_publish_scene_visibility(
                object_prop, 0, identity));
            assert((object_prop->flags & PROPFLAG_ONSCREEN) == 0U);
            assert(!ge_original_prop_state_object_scene_matrix_bank(
                object, object_prop, &scene_matrices, &scene_matrix_count));
            assert(scene_matrices == NULL && scene_matrix_count == 0U);
        }
    }

    memset(&door_providers,0,sizeof(door_providers));
    door_providers.context=&harness;door_providers.model_load=model_load;
    door_providers.resolve_model_instance=resolve_model_instance;
    door_providers.allocate_collision=allocate_collision;
    door_providers.walk_tiles=walk_tiles;door_providers.get_tile_rgb=get_tile_rgb;
    door_providers.portal_rooms=portal_rooms;door_providers.find_portal=find_portal;
    door_providers.set_portal_open=set_portal_open;
    door_providers.register_room=register_room;
    ge_original_door_bind(&door_providers,&door_scratch);
    if(harness.use_model62) {
        GeOriginalDoorRuntimePublication first_publication;
        GeOriginalDoorRuntimePublication second_publication;
        GeOriginalDoorScenePublication first_scene;
        GeOriginalDoorScenePublication second_scene;
        rect4f *guard_polygon = NULL;
        s32 guard_edges = 0;
        f32 guard_top = 0.0f;
        f32 guard_bottom = 0.0f;
        float first_initial_position[3];

        assert(ge_original_door_construct(world.first_door.definition,
            world.first_door.command_index)==GE_ORIGINAL_DOOR_OK);
#if defined(GE_PORT_MS_INHERITS)
        assert(ge_port_door_runtime_native_definition(
                   world.first_door.definition)
               == (DoorRecord *)world.first_door.definition);
#endif
        door_prepared[0] = door_scratch;
        assert(ge_original_door_construct(world.second_door.definition,
            world.second_door.command_index)==GE_ORIGINAL_DOOR_OK);
#if defined(GE_PORT_MS_INHERITS)
        assert(ge_port_door_runtime_native_definition(
                   world.second_door.definition)
               == (DoorRecord *)world.second_door.definition);
#endif
        door_prepared[1] = door_scratch;
        assert(door_prepared[0].constructed&&door_prepared[0].model_id==178);
        assert(door_prepared[0].pad_id==6&&door_prepared[0].portal_number==-1);
        assert(door_prepared[0].portal_lookup_attempted);
        assert(door_prepared[0].door_flags==12&&door_prepared[0].door_type==0);
        assert(fabsf(door_prepared[0].max_frac-0.95f)<0.0001f);
        assert(fabsf(door_prepared[0].perim_frac-1.0f)<0.0001f);
        assert(fabsf(door_prepared[0].open_position
                     -door_prepared[0].max_frac)<0.0001f);
        assert(((struct collision_data *)door_prepared[0].collision_data)->edges>=4);
        assert(door_prepared[1].constructed&&door_prepared[1].model_id==178);
        assert(door_prepared[1].pad_id==9&&door_prepared[1].portal_number==-1);
        assert(door_prepared[1].linked_door_offset==-1);
        assert(door_prepared[1].open_position==0.0f);
        assert(harness.portal_room_calls==2U&&harness.portal_find_calls==0U);
        assert(harness.portal_toggle_calls==0U);
        assert(door_prepared[0].second_room_registered);
        assert(door_prepared[1].second_room_registered);
        assert(((PropRecord *)world.first_door.prop)->type==PROP_TYPE_DOOR);
        assert(((PropRecord *)world.first_door.prop)->rooms[1]==136U);
        assert(ge_original_prop_state_room_contains(136,world.first_door.prop));
        assert(((PropRecord *)world.second_door.prop)->type==PROP_TYPE_DOOR);
        assert(((PropRecord *)world.second_door.prop)->rooms[1]==136U);
        assert(ge_original_prop_state_room_contains(136,world.second_door.prop));
        assert(ge_dam_setup_world_activate_entry(&world.first_door));
        assert(ge_dam_setup_world_activate_entry(&world.second_door));
        assert(ge_original_prop_state_active_count()==5U);
        assert(ge_dam_setup_world_link_authored_doors());
        assert(ge_original_door_runtime_link_pair(
            world.first_door.definition, world.second_door.definition));
        assert(ge_original_door_runtime_snapshot(
            world.first_door.definition, &first_publication));
        assert(ge_original_door_runtime_snapshot(
            world.second_door.definition, &second_publication));
        assert(fabsf(first_publication.open_position-0.95f)<0.0001f);
        assert(second_publication.open_position==0.0f);
        assert(first_publication.clipped_vertices==NULL);
        assert(second_publication.clipped_vertices==NULL);
        memcpy(first_initial_position, first_publication.position,
               sizeof(first_initial_position));
        ge_original_door_collision_bind(NULL, &door_collision_state);
        player_prop->pos.x = 1000000.0f;
        player_prop->pos.y = 1000000.0f;
        player_prop->pos.z = 1000000.0f;
        harness.player.stanHeight = 1000000.0f;
        harness.player.field_70 = 1000000.0f;
        assert(ge_original_door_collision_test(
            NULL, world.first_door.prop) == 1);
        player_prop->pos.x = first_publication.collision_polygon[0][0];
        player_prop->pos.y = first_publication.collision_bottom;
        player_prop->pos.z = first_publication.collision_polygon[0][1];
        harness.player.stanHeight = first_publication.collision_bottom;
        harness.player.field_70 = first_publication.collision_bottom;
        harness.player.eyeheight = first_publication.collision_top
            - first_publication.collision_bottom + 20.0f;
        assert(ge_original_door_collision_test(
            NULL, world.first_door.prop) == 0);
        assert(door_collision_state.clear_results == 1U);
        assert(door_collision_state.blocked_results == 1U);
        assert(door_collision_state.status == GE_ORIGINAL_DOOR_COLLISION_OK);
        player_prop->pos.x = 1000000.0f;
        player_prop->pos.y = 1000000.0f;
        player_prop->pos.z = 1000000.0f;
        harness.player.stanHeight = 1000000.0f;
        harness.player.field_70 = 1000000.0f;
        guard_prop = ge_original_prop_state_allocate_player(&harness.props);
        assert(guard_prop != NULL);
        memset(&harness.guard, 0, sizeof(harness.guard));
        guard_prop->type = PROP_TYPE_CHR;
        guard_prop->chr = &harness.guard;
        harness.guard.prop = guard_prop;
        {
            GeOriginalCharacterSceneState scene_state;
            guard_prop->flags = PROPFLAG_ENABLED;
            guard_prop->zDepth = 1234.5f;
            assert(ge_original_prop_state_observe_character_scene_state(
                guard_prop, &scene_state));
            assert(scene_state.flags == PROPFLAG_ENABLED);
            assert(scene_state.zdepth == 1234.5f);
            assert((guard_prop->flags & PROPFLAG_ONSCREEN) == 0U);
            assert(guard_prop->zDepth == 1234.5f);
            guard_prop->flags |= PROPFLAG_ONSCREEN;
            guard_prop->zDepth = 4321.5f;
            assert(ge_original_prop_state_observe_character_scene_state(
                guard_prop, &scene_state));
            assert((scene_state.flags & PROPFLAG_ONSCREEN) != 0U);
            assert(scene_state.zdepth == 4321.5f);
            assert((guard_prop->flags & PROPFLAG_ONSCREEN) != 0U);
            assert(guard_prop->zDepth == 4321.5f);
            guard_prop->flags &= (u8)~PROPFLAG_ONSCREEN;
        }
        harness.guard.actiontype = ACT_STAND;
        harness.guard.chrwidth = 20.0f;
        harness.guard.chrheight = first_publication.collision_top
            - first_publication.collision_bottom + 40.0f;
        harness.guard.ground = first_publication.collision_bottom;
        for (i = 0; i < (uint32_t)first_publication.collision_edges; i++) {
            guard_prop->pos.x += first_publication.collision_polygon[i][0];
            guard_prop->pos.z += first_publication.collision_polygon[i][1];
        }
        guard_prop->pos.x /= (float)first_publication.collision_edges;
        guard_prop->pos.y = harness.guard.ground;
        guard_prop->pos.z /= (float)first_publication.collision_edges;
        ge_original_prop_state_register_room(&harness.props, guard_prop, 135);
        ge_original_door_chrUpdateCollisionBounds_exact(
            guard_prop, &guard_polygon, &guard_edges,
            &guard_top, &guard_bottom);
        assert(guard_polygon == &harness.guard.collision_bounds);
        assert(guard_edges == 4);
        assert(guard_bottom == harness.guard.ground);
        assert(guard_top == harness.guard.ground + harness.guard.chrheight);
        assert(ge_original_door_collision_test(
            NULL, world.first_door.prop) == 0);
        assert(harness.guard.collision_bounds.f[0]
               == guard_prop->pos.x + harness.guard.chrwidth);
        assert(harness.guard.collision_bounds.f[3]
               == guard_prop->pos.z + harness.guard.chrwidth);
        harness.guard.actiontype = ACT_DEAD;
        guard_edges = -1;
        ge_original_door_chrUpdateCollisionBounds_exact(
            guard_prop, &guard_polygon, &guard_edges,
            &guard_top, &guard_bottom);
        assert(guard_edges == 0);
        harness.guard.actiontype = ACT_STAND;
        guard_prop->pos.x = 1000000.0f;
        guard_prop->pos.y = 1000000.0f;
        guard_prop->pos.z = 1000000.0f;
        harness.guard.ground = 1000000.0f;
        assert(ge_original_door_collision_test(
            NULL, world.first_door.prop) == 1);
        guard_prop->chr = NULL;
        assert(ge_original_door_collision_test(
            NULL, world.first_door.prop) == 0);
        assert(door_collision_state.status
               == GE_ORIGINAL_DOOR_COLLISION_MISSING_CHARACTER_PROVIDER);
        assert(door_collision_state.missing_character_calls > 0U);
        guard_prop->chr = &harness.guard;
        memset(&door_runtime_providers, 0, sizeof(door_runtime_providers));
        door_runtime_providers.context = &harness;
        door_runtime_providers.global_timer = runtime_global_timer;
        door_runtime_providers.clock_timer = runtime_clock_timer;
        door_runtime_providers.test_collision =
            ge_original_door_collision_test;
        door_runtime_providers.sound_event = runtime_sound;
        ge_original_door_runtime_bind(
            &door_runtime_providers, &door_runtime_state);
        harness.clock_timer = 1;
        assert(ge_original_door_runtime_activate(
            world.first_door.definition, DOORSTATE_CLOSING)
            == GE_ORIGINAL_DOOR_RUNTIME_OK);
        while (door_runtime_state.completed_closes == 0U
                && harness.global_timer < 1024) {
            harness.global_timer++;
            assert(ge_original_door_runtime_tick(
                world.first_door.definition)
                == GE_ORIGINAL_DOOR_RUNTIME_OK);
            assert(ge_original_door_runtime_snapshot(
                world.first_door.definition, &first_publication));
            assert(ge_original_door_runtime_snapshot(
                world.second_door.definition, &second_publication));
            assert(first_publication.clipped_vertices!=NULL);
            assert(second_publication.clipped_vertices!=NULL);
            assert(first_publication.clipped_vertex_count
                   ==GE_ORIGINAL_MODEL178_VERTEX_COUNT);
            assert(second_publication.clipped_vertex_count
                   ==GE_ORIGINAL_MODEL178_VERTEX_COUNT);
            assert(first_publication.clipped_vertices
                   !=second_publication.clipped_vertices);
        }
        assert(door_runtime_state.completed_closes == 1U);
        assert(door_runtime_state.collision_tests > 0U);
        assert(door_collision_state.tests
               == door_runtime_state.collision_tests + 5U);
        assert(door_collision_state.blocked_results == 3U);
        assert(door_collision_state.status == GE_ORIGINAL_DOOR_COLLISION_OK);
        assert(door_runtime_state.bbox_rebuilds > 0U);
        assert(door_runtime_state.clipped_vertex_rebuilds > 0U);
        assert(first_publication.generation > 0U);
        assert(second_publication.generation > 0U);
        assert(first_publication.open_position == 0.0f);
        assert(first_publication.position[0] != first_initial_position[0]
               || first_publication.position[1] != first_initial_position[1]
               || first_publication.position[2] != first_initial_position[2]);
        assert(first_publication.matrix[3][0]
               ==first_publication.position[0]);
        assert(first_publication.matrix[3][1]
               ==first_publication.position[1]);
        assert(first_publication.matrix[3][2]
               ==first_publication.position[2]);
        assert(first_publication.collision_edges>=4
               &&first_publication.collision_edges<=8);
        assert(isfinite(first_publication.collision_top));
        assert(isfinite(first_publication.collision_bottom));
        assert(ge_original_door_scene_prepare(
            world.first_door.definition, harness.model178_blob,
            sizeof(harness.model178_blob), &first_scene)
            == GE_ORIGINAL_DOOR_SCENE_OK);
        assert(ge_original_door_scene_prepare(
            world.second_door.definition, harness.model178_blob,
            sizeof(harness.model178_blob), &second_scene)
            == GE_ORIGINAL_DOOR_SCENE_OK);
        assert(first_scene.uses_clipped_vertices == 1U);
        assert(second_scene.uses_clipped_vertices == 1U);
        assert(first_scene.runtime.generation == first_publication.generation);
        assert(second_scene.runtime.generation
               == second_publication.generation);
        assert(first_scene.input.blob == first_scene.blob);
        assert(first_scene.input.primary_offset == UINT32_C(0x520));
        assert(first_scene.input.segment4_offset == UINT32_C(0xa8));
        assert(first_scene.input.room_id == 135U);
        assert(memcmp(first_scene.input.matrix, first_publication.matrix,
                      sizeof(first_publication.matrix)) == 0);
        assert(memcmp(first_scene.input.position, first_publication.position,
                      sizeof(first_publication.position)) == 0);
        {
            const Vertex *runtime_vertices =
                first_publication.clipped_vertices;
            const uint8_t *scene_vertex = first_scene.blob + 0xa8U;
            assert(read_scene_be16(scene_vertex + 0U)
                   == runtime_vertices[0].coord.x);
            assert(read_scene_be16(scene_vertex + 2U)
                   == runtime_vertices[0].coord.y);
            assert(read_scene_be16(scene_vertex + 4U)
                   == runtime_vertices[0].coord.z);
            assert(read_scene_be16(scene_vertex + 8U)
                   == runtime_vertices[0].s);
            assert(read_scene_be16(scene_vertex + 10U)
                   == runtime_vertices[0].t);
            assert(scene_vertex[12] == runtime_vertices[0].r);
            assert(scene_vertex[15] == runtime_vertices[0].a);
        }
        assert(harness.door_sound_calls == 2U);
        {
            ObjectRecord *door = world.first_door.definition;
            float scene_matrix[4][4];
            float scene_position[3];
            uint8_t scene_room = UINT8_MAX;

            assert(ge_original_prop_state_object_scene_transform(
                door, world.first_door.prop, scene_matrix, scene_position,
                &scene_room));
            assert(scene_room == 135U);
            assert(memcmp(scene_matrix, door->mtx.m,
                          sizeof(scene_matrix)) == 0);
            assert(memcmp(scene_position, door->runtime_pos.f,
                          sizeof(scene_position)) == 0);
        }
    }
        memcpy(query_position, object->runtime_pos.f,
               sizeof(query_position));
        assert(ge_original_prop_state_room_object_at_position(
                   query_position, 135, &queried_top, &queried_bottom)
               == object);
        assert(fabsf(queried_bottom - generated_collision->bottom) < 0.0001f);
        assert(fabsf(queried_top - generated_collision->top) < 0.0001f);
    } else {
        assert(ge_original_default_object_place_standard(object)
               == GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_FAILED);
    }

    assert(!ge_original_prop_state_room_contains(134,
                                                  world.first_object.prop));
    ge_original_prop_state_register_room(&harness.props,
                                         world.first_object.prop, 137);
    assert(harness.props.allocation_calls == (harness.use_model62 ? 6U : 5U));
    assert(harness.props.room_registration_calls
           == (harness.use_model62 ? 8U : 7U));

    puts("Authored Dam records entered exact original prop state");
    return 0;
}

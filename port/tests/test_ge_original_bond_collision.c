#include "ge_original_bond_movement.h"
#include "ge_original_animation_root.h"
#include "ge_original_player_spawn.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"
#ifdef GE_TEST_PLAYER_GAIT_CHAIN
#include "ge_original_player_gait.h"
#include "ge_original_player_spawn_internal.h"
typedef int PLAYERFLAG;
#include "game/bondview.h"
#endif

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bondtypes.h>

/* bondview.c owns this table in the full game.  The focused collision binary
 * retains the intro-spawn slice without the gait fixture that otherwise
 * provides it, so mirror the zero-terminated production contract here. */
s32 g_bondviewBondDeathAnimations[] = { 1, 0 };
s32 g_bondviewBondDeathAnimationsCount;

typedef struct CollisionHarness {
    PropRecord prop;
    GeOriginalPlayerViewState player;
    GeOriginalBondMovementStatus movement;
    GeOriginalAnimationRootSample root_sample;
} CollisionHarness;

s32 g_ClockTimer;
f32 g_GlobalTimerDelta;

/* The movement fixture retains bondviewCalcUpdatePlayerCollision unchanged.
 * Its room-entry notification is covered by the objective runtime fixture;
 * this isolated STAN test supplies only the original call boundary. */
void objectivestatusCheckRoomEntered(s32 room_id)
{
    (void)room_id;
}

static int sample_head_root_velocity(void *context,
                                     float speed_forwards,
                                     float speed_sideways,
                                     int32_t clock_timer,
                                     float global_timer_delta,
                                     float velocity[3])
{
    CollisionHarness *harness = context;
    assert(harness != NULL);
    assert(isfinite(speed_forwards));
    assert(isfinite(speed_sideways));
    assert(clock_timer == 1);
    assert(global_timer_delta == 1.0f);
    return ge_original_animation_root_sample_velocity(
        &harness->root_sample, speed_forwards, speed_sideways,
        clock_timer, global_timer_delta, velocity);
}

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    assert(file != NULL);
    assert(fseek(file, 0L, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0L);
    assert(fseek(file, 0L, SEEK_SET) == 0);
    *size = (size_t)length;
    bytes = malloc(*size);
    assert(bytes != NULL);
    assert(fread(bytes, 1U, *size, file) == *size);
    assert(fclose(file) == 0);
    return bytes;
}

static void *allocate_prop(void *context)
{
    CollisionHarness *harness = context;
    memset(&harness->prop, 0, sizeof(harness->prop));
    return &harness->prop;
}

static void prop_service(void *context, void *prop)
{
    CollisionHarness *harness = context;
    assert(prop == &harness->prop);
}

static void room_service(void *context, void *prop, int16_t room)
{
    (void)room;
    prop_service(context, prop);
}

static void commit_spawn(CollisionHarness *harness,
                         const GeStanNativeMap *native,
                         GeStanNativeTile *tile, float x, float z)
{
    const float floor_y = ge_original_stan_get_position_y(native, tile, x, z);
    GeOriginalPlayerSpawnConfig config = {
        .position = {x, floor_y + 175.0f, z},
        .floor_y = floor_y,
        .eye_height = 175.0f,
        .look_angle_radians = 0.0f,
        .stan = tile,
        .room = (int16_t)tile->room,
    };
    assert(ge_original_player_spawn_commit(&config));
}

int main(int argc, char **argv)
{
    CollisionHarness harness = {0};
    GeOriginalPlayerSpawnProviders spawn_providers = {
        .context = &harness,
        .allocate_prop = allocate_prop,
        .activate_prop = prop_service,
        .enable_prop = prop_service,
        .deregister_room = room_service,
        .register_room = room_service,
    };
    GeOriginalBondMovementProviders movement_providers = {
        .context = &harness,
        .sample_head_root_velocity = sample_head_root_velocity,
    };
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
    GeStanNativeTile *spawn;
    GeStanNativeTile *linked;
    uint8_t *bytes;
    uint8_t *native_bytes;
    uint8_t *animation_bytes;
    GeOriginalAnimationRoot *sprint_root;
#ifdef GE_TEST_PLAYER_GAIT_CHAIN
    uint8_t *walk_frames;
    uint8_t *sprint_frames;
    GeOriginalAnimationRoot *walk_root;
    GeOriginalPlayerGait *gait;
    GeOriginalPlayerGaitStatus gait_status;
    GeOriginalPlayerGaitTick gait_tick;
    size_t walk_frames_size;
    size_t sprint_frames_size;
#endif
    size_t size;
    size_t native_size;
    size_t animation_size;
    float spawn_x = 0.0f;
    float spawn_z = 0.0f;
    float linked_x = 0.0f;
    float linked_z = 0.0f;
    unsigned linked_edge = 0U;
    unsigned wall_edge = 0U;
    uint16_t index;

#ifdef GE_TEST_PLAYER_GAIT_CHAIN
    assert(argc == 5);
#else
    assert(argc == 3);
#endif
    bytes = read_file(argv[1], &size);
    assert(ge_stan_collision_open(bytes, size, &surface)
           == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface, &native_size)
           == GE_STAN_COLLISION_OK);
    native_bytes = malloc(native_size);
    assert(native_bytes != NULL);
    assert(ge_stan_native_materialize(&surface, 0.23363999f, native_bytes,
           native_size, &native) == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_bind_original(&native) == GE_STAN_COLLISION_OK);
    spawn = native.spawn_tile;
    animation_bytes = read_file(argv[2], &animation_size);
    sprint_root = ge_original_animation_root_create(
        animation_bytes, animation_size,
        GE_ORIGINAL_BOND_ANIMATION_SPRINTING);
    assert(sprint_root != NULL);
#ifdef GE_TEST_PLAYER_GAIT_CHAIN
    walk_frames = read_file(argv[3], &walk_frames_size);
    sprint_frames = read_file(argv[4], &sprint_frames_size);
    walk_root = ge_original_animation_root_create(
        animation_bytes, animation_size,
        GE_ORIGINAL_BOND_ANIMATION_EYE_WALK);
    assert(walk_root != NULL);
    assert(ge_original_animation_root_bind_frames(
        walk_root, walk_frames, walk_frames_size));
    assert(ge_original_animation_root_bind_frames(
        sprint_root, sprint_frames, sprint_frames_size));
#endif

    for (index = 0U; index < ge_stan_native_point_count(spawn); ++index) {
        spawn_x += (float)spawn->points[index].x;
        spawn_z += (float)spawn->points[index].z;
        if (spawn->points[index].link != 0U) linked_edge = index;
        else wall_edge = index;
    }
    spawn_x /= (float)ge_stan_native_point_count(spawn) * native.level_scale;
    spawn_z /= (float)ge_stan_native_point_count(spawn) * native.level_scale;
    linked = (GeStanNativeTile *)(native.base
        + (size_t)spawn->points[linked_edge].link * GE_STAN_NATIVE_LINK_UNIT);
    for (index = 0U; index < ge_stan_native_point_count(linked); ++index) {
        linked_x += (float)linked->points[index].x;
        linked_z += (float)linked->points[index].z;
    }
    linked_x /= (float)ge_stan_native_point_count(linked) * native.level_scale;
    linked_z /= (float)ge_stan_native_point_count(linked) * native.level_scale;

    ge_original_player_spawn_bind(&spawn_providers, &harness.player);
    commit_spawn(&harness, &native, spawn, spawn_x, spawn_z);
    assert(harness.player.publication_generation == 1U);
    ge_original_animation_root_sample_set(
        &harness.root_sample, sprint_root, 7, 0);
    ge_original_bond_movement_bind(&movement_providers, &harness.movement);
    {
        const float start_x = harness.player.collision_position[0];
        const float start_z = harness.player.collision_position[2];
        assert(ge_original_bond_root_motion_tick(1, 1.0f));
        assert(harness.player.publication_generation == 2U);
        assert(harness.movement.root_motion_ticks == 1U);
        assert(harness.movement.root_motion_samples_missing == 0U);
        assert(fabsf(harness.movement.head_position[0] - -0.042f) < 0.001f);
        /* Sprint frame 7 decodes exact root Z 157, Bond model scale is 0.1,
         * then the original first-sample head damper applies (1 - 0.93). */
        assert(fabsf(harness.movement.head_position[2] - 1.099f) < 0.001f);
        assert(hypotf(harness.player.collision_position[0] - start_x,
                      harness.player.collision_position[2] - start_z) > 0.0f);
    }
    assert(ge_original_bond_collision_validate_position());
#ifdef GE_TEST_PLAYER_GAIT_CHAIN
    commit_spawn(&harness, &native, spawn, spawn_x, spawn_z);
    ge_original_player_gait_bind_bond_animations(walk_root, sprint_root);
    gait = ge_original_player_gait_create_current_player(
        walk_root, &gait_status);
    assert(gait != NULL && gait_status == GE_ORIGINAL_PLAYER_GAIT_OK);
    ge_original_player_gait_set_loop(gait, 9.5f, 27.0f, 0.0f);
    ge_original_spawn_player_get()->headanim = 0;
    ge_original_spawn_player_get()->animFlipFlag = 0;
    ge_original_spawn_player_get()->resetheadpos = TRUE;
    ge_original_spawn_player_get()->speedforwards = 1.0f;
    ge_original_spawn_player_get()->speedsideways = 0.25f;
    ge_original_spawn_player_get()->speedtheta = 0.0f;
    assert(ge_original_player_gait_current_player_movement_tick(
        gait, 1, 1.0f, &gait_tick));
    assert(fabsf(gait_tick.max_speed - 1.0f) < 0.000001f);
    assert(fabsf(gait_tick.percent_speed - 1.0f) < 0.000001f);
    assert(fabsf(gait_tick.sideways_motion - 2.355263f) < 0.001f);
    assert(isfinite(gait_tick.root_velocity[0]));
    assert(isfinite(gait_tick.root_velocity[1]));
    assert(isfinite(gait_tick.root_velocity[2]));
    assert(((Model *)(void *)&ge_original_spawn_player_get()->model)->anim ==
           ge_original_animation_root_native_abi(sprint_root));
    assert(((Model *)(void *)&ge_original_spawn_player_get()->model)->anim2 ==
           ge_original_animation_root_native_abi(walk_root));
    assert(fabsf(((Model *)(void *)&ge_original_spawn_player_get()->model)
                     ->unk84 - 11.0f / 12.0f) < 0.000001f);
    for (index = 1U; index < 12U; index++) {
        assert(ge_original_player_gait_current_player_movement_tick(
            gait, 1, 1.0f, &gait_tick));
    }
    assert(((Model *)(void *)&ge_original_spawn_player_get()->model)->anim2 ==
           NULL);
    assert(harness.movement.root_motion_ticks == 13U);
    assert(isfinite(harness.player.collision_position[0]));
    assert(isfinite(harness.player.collision_position[2]));
    ge_original_player_gait_destroy(gait);
#endif
    {
        const float offset[3] = {
            linked_x - spawn_x, 0.0f, linked_z - spawn_z
        };
        assert(ge_original_bond_collision_try_offset(offset, 1));
        assert(harness.player.current_stan == linked);
    }

    commit_spawn(&harness, &native, spawn, spawn_x, spawn_z);
    {
        const GeStanNativePoint *a = &spawn->points[wall_edge];
        const GeStanNativePoint *b = &spawn->points[
            (wall_edge + 1U) % ge_stan_native_point_count(spawn)];
        const float edge_x = ((float)a->x + (float)b->x) * 0.5f
            / native.level_scale;
        const float edge_z = ((float)a->z + (float)b->z) * 0.5f
            / native.level_scale;
        const float outside_x = edge_x * 2.0f - spawn_x;
        const float outside_z = edge_z * 2.0f - spawn_z;
        const float target[3] = {
            outside_x, harness.player.collision_position[1], outside_z
        };
        const float offset[3] = {
            (outside_x - spawn_x) * 0.5f, 0.0f,
            (outside_z - spawn_z) * 0.5f
        };
        assert(!ge_original_bond_collision_try_target_simple(target));
        assert(harness.player.current_stan == spawn);
        ge_original_bond_collision_try_offset(offset, 0);
        assert(harness.player.current_stan != NULL);
        assert(isfinite(harness.player.collision_position[0]));
        assert(isfinite(harness.player.collision_position[2]));
        assert(ge_original_stan_test_point_within_bounds(&native,
            (const GeStanNativeTile *)harness.player.current_stan,
            harness.player.collision_position[0],
            harness.player.collision_position[2]));
        assert(fabsf(harness.player.collision_position[0] - outside_x) > 1.0f
            || fabsf(harness.player.collision_position[2] - outside_z) > 1.0f);
    }
    assert(harness.movement.collision_checks >= 4U);
    puts("Original Bond collision/fallback and head-root diagnostic passed");
    ge_original_animation_root_destroy(sprint_root);
#ifdef GE_TEST_PLAYER_GAIT_CHAIN
    ge_original_animation_root_destroy(walk_root);
    free(walk_frames);
    free(sprint_frames);
#endif
    free(animation_bytes);
    free(native_bytes);
    free(bytes);
    return 0;
}

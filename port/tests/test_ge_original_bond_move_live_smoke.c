#include "ge_original_animation_root.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_bond_live.h"
#include "ge_original_bond_movement.h"
#include "ge_original_bond_move_state.h"
#include "ge_original_effect_buffers.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_input.h"
#include "ge_original_player_gait.h"
#include "ge_original_player_spawn.h"
#include "ge_original_player_spawn_internal.h"
#include "ge_original_prop_state.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "game/bg.h"
#include "game/explosion.h"

typedef struct LiveMoveHarness {
    GeOriginalPropState props;
    GeOriginalPlayerViewState publication;
    GeOriginalBondMovementStatus movement;
} LiveMoveHarness;

extern struct player_data *g_playerPerm;
extern void chrpropDeregisterRoom(PropRecord *prop, s16 room);
extern void ge_port_bond_movement_publish(struct player *player);
struct player *g_CurrentPlayer;
bg_portal_data_entry *g_BgPortals;

/* The smoke publishes only Bond's own PROP_TYPE_VIEWER record.  Preserve the
 * exact propIsOfCdType classification needed by the production dynamic STAN
 * volume body so field_AC, rather than a mask-free test path, proves the
 * canonical self-collision exclusion. */
s32 propIsOfCdType(PropRecord *prop, s32 cdtypes)
{
    assert(prop != NULL);
    assert(prop->type == PROP_TYPE_VIEWER);
    return (cdtypes & CDTYPE_PLAYERS) != 0;
}

/* Objective room-entry semantics have their own live-runtime sanitizer test;
 * this MoveBond smoke fixture retains only the original notification seam. */
void objectivestatusCheckRoomEntered(s32 room_id)
{
    (void)room_id;
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
    LiveMoveHarness *harness = context;
    return ge_original_prop_state_allocate_player(&harness->props);
}

static void prop_service(void *context, void *prop)
{
    LiveMoveHarness *harness = context;
    ge_original_prop_state_activate(&harness->props, prop);
}

static void enable_prop(void *context, void *prop)
{
    LiveMoveHarness *harness = context;
    ge_original_prop_state_enable(&harness->props, prop);
}

static void room_service(void *context, void *prop, int16_t room)
{
    LiveMoveHarness *harness = context;
    ge_original_prop_state_register_room(&harness->props, prop, room);
}

static void deregister_room(void *context, void *prop, int16_t room)
{
    (void)context;
    chrpropDeregisterRoom(prop, room);
}

int main(int argc, char **argv)
{
    LiveMoveHarness harness = {0};
    GeOriginalPlayerSpawnProviders spawn_providers = {
        .context = &harness,
        .allocate_prop = allocate_prop,
        .activate_prop = prop_service,
        .enable_prop = enable_prop,
        .deregister_room = deregister_room,
        .register_room = room_service,
    };
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
    GeStanNativeTile *spawn;
    GeOriginalAnimationRoot *walk_root;
    GeOriginalAnimationRoot *sprint_root;
    GeOriginalAnimationRoot *idle_root;
    GeOriginalPlayerGait *gait;
    GeOriginalPlayerGaitStatus gait_status;
    GeOriginalBondLiveState live = {0};
    GeOriginalInputSample input = {0};
    struct player *player;
    uint8_t *stan_bytes;
    uint8_t *native_bytes;
    uint8_t *animation_bytes;
    uint8_t *walk_frames;
    uint8_t *sprint_frames;
    uint8_t *idle_frames;
    size_t stan_size, native_size, animation_size;
    size_t walk_size, sprint_size, idle_size;
    float x = 0.0f, z = 0.0f, floor_y;
    float start_x, start_z;
    uint32_t generation;
    uint16_t index;

    assert(argc == 6);
    stan_bytes = read_file(argv[1], &stan_size);
    animation_bytes = read_file(argv[2], &animation_size);
    walk_frames = read_file(argv[3], &walk_size);
    sprint_frames = read_file(argv[4], &sprint_size);
    idle_frames = read_file(argv[5], &idle_size);
    assert(ge_stan_collision_open(stan_bytes, stan_size, &surface)
           == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface, &native_size)
           == GE_STAN_COLLISION_OK);
    native_bytes = malloc(native_size);
    assert(native_bytes != NULL);
    assert(ge_stan_native_materialize(&surface, 0.23363999f, native_bytes,
           native_size, &native) == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_bind_original(&native) == GE_STAN_COLLISION_OK);
    spawn = native.spawn_tile;
    for (index = 0U; index < ge_stan_native_point_count(spawn); index++) {
        x += (float)spawn->points[index].x;
        z += (float)spawn->points[index].z;
    }
    x /= (float)ge_stan_native_point_count(spawn) * native.level_scale;
    z /= (float)ge_stan_native_point_count(spawn) * native.level_scale;
    floor_y = ge_original_stan_get_position_y(&native, spawn, x, z);
    assert(ge_original_prop_state_reset(&harness.props, 256U));
    ge_original_player_spawn_bind(&spawn_providers, &harness.publication);
    {
        GeOriginalPlayerSpawnConfig config = {
            .position = {x, floor_y + 175.0f, z},
            .floor_y = floor_y,
            .eye_height = 175.0f,
            .look_angle_radians = 0.0f,
            .stan = spawn,
            .room = (int16_t)spawn->room,
        };
        assert(ge_original_player_spawn_commit(&config));
    }
    assert(ge_original_bond_move_state_initialize_single_player());
    player = ge_original_spawn_player_get();
    assert(player != NULL && player->prop != NULL && g_playerPerm != NULL);
    g_CurrentPlayer = player;
    {
        GeOriginalBondMovementProviders movement_providers = {
            .collision_types =
                ge_original_bond_movement_normal_collision_types,
            .set_prop_collision =
                ge_original_bond_movement_set_current_player_collision,
        };
        assert(ge_original_bond_movement_normal_collision_types(NULL)
               == (CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS
                   | CDTYPE_CHRS | CDTYPE_PATHBLOCKER));
        ge_original_bond_movement_set_current_player_collision(
            NULL, player->prop, 0);
        assert(player->field_AC == 0);
        ge_original_bond_movement_set_current_player_collision(
            NULL, player->prop, 1);
        assert(player->field_AC == 1);
        ge_original_bond_movement_bind(
            &movement_providers, &harness.movement);
    }
    ge_original_bond_input_bind_player(player, g_playerPerm);
    ge_original_bond_input_provider_reset_normal_dam();
    ge_original_effect_buffers_reset_single_player();
    assert(g_ExplosionBuffer != NULL);
    assert(g_SmokeBuffer != NULL);
    assert(g_ScorchBuffer != NULL);
    assert(g_BulletImpactBuffer != NULL);
    assert(g_FlyingParticlesBuffer != NULL);
    assert(g_NumExplosionEntries == 0);
    assert(g_NumSmokeEntries == 0);
    assert(g_NumScorchEntries == 0);
    assert(g_NumImpactEntries == 0);
    assert(g_NumParticleEntries == 0);
    assert(g_SpExplosionDamageMult == 1.0f);
    assert(g_ScorchBuffer[SCORCH_BUFFER_LEN - 1].roomid == -1);
    assert(g_BulletImpactBuffer[BULLET_IMPACT_BUFFER_LEN - 1].room == -1);
    assert(g_FlyingParticlesBuffer[MAX_FLYING_PARTICLES - 1].unk00 == 0);
    assert(max_particles == MAX_FLYING_PARTICLES);
    assert(ge_original_effect_explosion_capacity() == EXPLOSION_BUFFER_LEN);
    assert(ge_original_effect_smoke_capacity() == SMOKE_BUFFER_LEN);
    assert(ge_original_effect_particle_capacity() == MAX_FLYING_PARTICLES);
    explosionCreate(NULL, &player->prop->pos, spawn, 0, 0, 0,
                    player->prop->rooms, 0);
    assert(g_ExplosionBuffer[0].prop != NULL);
    assert(g_ExplosionBuffer[0].explosion_type == 0);
    assert(g_ExplosionBuffer[0].parts[0].frame == 1);
    g_SmokeBuffer[SMOKE_BUFFER_LEN - 1].prop = player->prop;
    g_SmokeBuffer[SMOKE_BUFFER_LEN - 1].parts[SMOKE_PARTS_LEN - 1].size = 1.0f;
    g_ScorchBuffer[SCORCH_BUFFER_LEN - 1].roomid = 1;
    g_BulletImpactBuffer[BULLET_IMPACT_BUFFER_LEN - 1].room = 1;
    g_FlyingParticlesBuffer[MAX_FLYING_PARTICLES - 1].unk00 = 1;
    g_NumExplosionEntries = 5;
    g_NumSmokeEntries = 5;
    g_NumScorchEntries = 5;
    g_NumImpactEntries = 5;
    g_NumParticleEntries = 5;
    g_SpExplosionDamageMult = 2.0f;
    ge_original_effect_buffers_reset_single_player();
    assert(g_ExplosionBuffer[0].prop == NULL);
    assert(g_ExplosionBuffer[0].parts[0].frame == 0);
    assert(g_SmokeBuffer[SMOKE_BUFFER_LEN - 1].prop == NULL);
    assert(g_SmokeBuffer[SMOKE_BUFFER_LEN - 1]
               .parts[SMOKE_PARTS_LEN - 1].size == 0.0f);
    assert(g_ScorchBuffer[SCORCH_BUFFER_LEN - 1].roomid == -1);
    assert(g_BulletImpactBuffer[BULLET_IMPACT_BUFFER_LEN - 1].room == -1);
    assert(g_FlyingParticlesBuffer[MAX_FLYING_PARTICLES - 1].unk00 == 0);
    assert(g_NumExplosionEntries == 0 && g_NumSmokeEntries == 0);
    assert(g_NumScorchEntries == 0 && g_NumImpactEntries == 0);
    assert(g_NumParticleEntries == 0);
    assert(g_SpExplosionDamageMult == 1.0f);
    ge_original_gameplay_services_reset();

    walk_root = ge_original_animation_root_create(animation_bytes,
        animation_size, GE_ORIGINAL_BOND_ANIMATION_EYE_WALK);
    sprint_root = ge_original_animation_root_create(animation_bytes,
        animation_size, GE_ORIGINAL_BOND_ANIMATION_SPRINTING);
    idle_root = ge_original_animation_root_create(animation_bytes,
        animation_size, GE_ORIGINAL_BOND_ANIMATION_IDLE);
    assert(walk_root != NULL && sprint_root != NULL && idle_root != NULL);
    assert(ge_original_animation_root_bind_frames(walk_root, walk_frames,
        walk_size));
    assert(ge_original_animation_root_bind_frames(sprint_root, sprint_frames,
        sprint_size));
    assert(ge_original_animation_root_bind_frames(idle_root, idle_frames,
        idle_size));
    ge_original_player_gait_bind_bond_animations(walk_root, sprint_root);
    gait = ge_original_player_gait_create_current_player(walk_root,
        &gait_status);
    assert(gait != NULL && gait_status == GE_ORIGINAL_PLAYER_GAIT_OK);
    assert(ge_original_player_gait_calibrate_current_player_standing(
        gait, idle_root, walk_root));
    player->headanim = 0;
    player->animFlipFlag = 0;
    player->resetheadpos = TRUE;
    player->bonddead = FALSE;

    ge_original_input_init();
    input.move_y = 1.0f;
    ge_original_input_tick(&input);
    live.initialized = 1;
    start_x = player->field_488.collision_position.f[0];
    start_z = player->field_488.collision_position.f[2];
    generation = harness.publication.publication_generation;
    assert(ge_original_bond_move_live_tick(&live, 1, 1, 1.0f));
    assert(live.input_tick_count == 0U);
    assert(live.move_tick_count == 1U);
    assert(harness.publication.publication_generation > generation);
    assert(player->field_488.current_tile_ptr != NULL);
    assert(player->prop->stan == player->field_488.current_tile_ptr);
    assert(player->registeredroom == (int16_t)player->prop->stan->room);
    assert(player->speedforwards > 0.0f);
    assert(fabsf(player->headpos.f[2]) > 0.0f);
    assert(hypotf(player->field_488.collision_position.f[0] - start_x,
                  player->field_488.collision_position.f[2] - start_z) > 0.0f);
    assert(player->field_AC == 1);
    assert(isfinite(harness.publication.camera_position[0]));
    assert(isfinite(harness.publication.camera_position[1]));
    assert(isfinite(harness.publication.camera_position[2]));

    /* Releasing movement enters unchanged bheadUpdate's stationary branch.
     * Its target must remain the authored idle calibration even when a slow
     * render delivers several original retraces in one gameplay dispatch. */
    {
        const float authored_standheight = player->standheight;
        uint32_t idle_tick;
        memset(&input, 0, sizeof(input));
        for (idle_tick = 0U; idle_tick < 90U; idle_tick++) {
            ge_original_input_tick(&input);
            assert(ge_original_bond_move_live_tick(
                &live, 4, 2 + (int32_t)(idle_tick * 4U), 4.0f));
            assert(isfinite(harness.publication.camera_position[1]));
        }
        assert(fabsf(player->headpos.y - authored_standheight) < 0.01f);
        assert(fabsf(harness.publication.camera_position[1]
                     - player->field_488.pos.f[1]) < 0.001f);
        assert(memcmp(harness.publication.camera_look,
                      player->field_488.applied_view.f,
                      sizeof(harness.publication.camera_look)) == 0);
        assert(memcmp(harness.publication.camera_up,
                      player->field_488.applied_view2.f,
                      sizeof(harness.publication.camera_up)) == 0);
        assert(player->eyeheight + player->ducking_height_offset > 0.0f);
    }

    /* A death animation can drive field_88/ducking far below the standing
     * eye offset.  MoveBond has already clamped its canonical camera pos to
     * 30 units above the STAN floor; the portable publication must consume
     * that result instead of reconstructing a below-floor standing camera. */
    player->bonddead = 2;
    player->stanHeight = -107.0f;
    player->eyeheight = 167.0f;
    player->ducking_height_offset = -247.0f;
    player->field_488.collision_position.f[1] = -77.0f;
    player->field_488.pos.f[1] = -77.0f;
    player->field_488.applied_view.f[0] = 0.25f;
    player->field_488.applied_view.f[1] = -0.5f;
    player->field_488.applied_view.f[2] = 0.75f;
    player->field_488.applied_view2.f[0] = 0.0f;
    player->field_488.applied_view2.f[1] = 0.75f;
    player->field_488.applied_view2.f[2] = 0.5f;
    ge_port_bond_movement_publish(player);
    assert(harness.publication.camera_position[1] == -77.0f);
    assert(harness.publication.camera_position[1]
           == harness.publication.floor_y + 30.0f);
    assert(harness.publication.camera_position[1]
           != player->stanHeight + player->eyeheight
                + player->ducking_height_offset);
    assert(player->prop->pos.f[1]
           == player->field_488.collision_position.f[1]);
    assert(harness.publication.camera_look[1] == -0.5f);
    assert(harness.publication.camera_up[1] == 0.75f);

    ge_original_player_gait_destroy(gait);
    ge_original_animation_root_destroy(idle_root);
    ge_original_animation_root_destroy(walk_root);
    ge_original_animation_root_destroy(sprint_root);
    free(native_bytes);
    free(stan_bytes);
    free(animation_bytes);
    free(walk_frames);
    free(sprint_frames);
    free(idle_frames);
    puts("unchanged MoveBond -> STAN/player/camera publication smoke passed");
    return 0;
}

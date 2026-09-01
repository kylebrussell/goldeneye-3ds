#include "ge_original_bond_movement.h"
#include "ge_original_bond_movement_internal.h"
#include "ge_original_player_spawn_internal.h"

#include <stddef.h>
#include <math.h>
#include <string.h>

extern s32 g_ClockTimer;
extern f32 g_GlobalTimerDelta;

static GeOriginalBondMovementProviders ge_movement_providers;
static GeOriginalBondMovementStatus *ge_movement_status;

void ge_original_bond_movement_bind(
    const GeOriginalBondMovementProviders *providers,
    GeOriginalBondMovementStatus *status)
{
    memset(&ge_movement_providers, 0, sizeof(ge_movement_providers));
    if (providers != NULL) {
        ge_movement_providers = *providers;
    }
    ge_movement_status = status;
    if (status != NULL) {
        memset(status, 0, sizeof(*status));
        status->room = -1;
    }
}

int ge_original_bond_movement_normal_collision_types(void *context)
{
    (void)context;
    return CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS
        | CDTYPE_PATHBLOCKER;
}

void ge_original_bond_movement_set_current_player_collision(
    void *context, void *opaque_prop, int enabled)
{
    struct player *player = ge_original_spawn_player_get();
    (void)context;
    /* Exact normal single-player result of
     * sub_GAME_7F03D058(viewer, enabled) ->
     * bondviewUpdateGuardTankFlagsRelated. The full tank branch remains
     * outside the non-tank MoveBond slice. */
    if (player != NULL && opaque_prop == player->prop)
        player->field_AC = enabled;
}

int ge_port_bond_movement_cdtypes(void)
{
    if (ge_movement_providers.collision_types != NULL) {
        return ge_movement_providers.collision_types(
            ge_movement_providers.context);
    }
    return 0;
}

void ge_port_bond_movement_collision_dimensions(float *radius,
                                                float *height,
                                                float *height_end)
{
    const struct player *player = ge_original_spawn_player_get();
    *radius = player->field_488.collision_radius;
    *height = (player->eyeheight + player->field_88
               + player->ducking_height_offset + 10.0f) - 30.0f;
    *height_end = 30.0f;
}

void ge_port_bond_movement_set_prop_collision(void *prop, int enabled)
{
    if (ge_movement_providers.set_prop_collision != NULL) {
        ge_movement_providers.set_prop_collision(
            ge_movement_providers.context, prop, enabled);
    }
}

void ge_port_bond_movement_record_canonical_collision(
    float prior_x, float prior_z)
{
    struct player *player = ge_original_spawn_player_get();
    if (ge_movement_status == NULL || player == NULL) return;
    ge_movement_status->collision_checks++;
    if (prior_x != player->field_488.collision_position.f[0]
            || prior_z != player->field_488.collision_position.f[2]) {
        ge_movement_status->accepted_checks++;
        return;
    }
    ge_movement_status->blocked_checks++;
    if (stanSavedColl_posData != NULL) {
        ge_movement_status->blocked_by_prop++;
        ge_movement_status->last_blocking_prop =
            (uintptr_t)stanSavedColl_posData;
        ge_movement_status->last_blocking_prop_type =
            stanSavedColl_posData->type;
        ge_movement_status->last_blocking_object_type =
            stanSavedColl_posData->obj != NULL
                ? ((PropDefHeaderRecord *)stanSavedColl_posData->obj)->type
                : -1;
    } else if (stanSavedColl_tile != NULL) {
        ge_movement_status->blocked_by_stan++;
        ge_movement_status->last_blocking_prop = 0U;
        ge_movement_status->last_blocking_prop_type = -1;
        ge_movement_status->last_blocking_object_type = -1;
    }
}

void ge_port_bond_movement_publish(struct player *player)
{
    float camera_position[3];
    float camera_look[3];
    float position_scale[3];
    int16_t room = -1;

    if (player == NULL || player->field_488.current_tile_ptr == NULL) {
        return;
    }
    room = (int16_t)player->field_488.current_tile_ptr->room;
    player->registeredroom = room;
    player->prop->stan = player->field_488.current_tile_ptr;
    player->prop->pos.f[0] = player->field_488.collision_position.f[0];
    player->prop->pos.f[1] = player->field_488.collision_position.f[1];
    player->prop->pos.f[2] = player->field_488.collision_position.f[2];

    /* MoveBond has already run the unchanged
     * bondviewUpdatePlayerCollisionPositionFields body.  Its pos/applied_view
     * pair is the canonical camera publication, including the minimum
     * 30-unit dead-player clearance and the authored death-head animation.
     * Reconstructing a standing camera from stanHeight/eyeheight here drops
     * field_88 from that equation and can place the camera below the STAN
     * floor while Bond dies. */
    memcpy(camera_position, player->field_488.pos.f,
           sizeof(camera_position));
    memcpy(camera_look, player->field_488.applied_view.f,
           sizeof(camera_look));
    position_scale[0] = camera_position[0] / 0.100000023842f;
    position_scale[1] = camera_position[1] / 0.100000023842f;
    position_scale[2] = camera_position[2] / 0.100000023842f;

    ge_port_player_spawn_publish(
        player->field_488.collision_position.f,
        camera_position,
        camera_look,
        player->field_488.applied_view2.f,
        player->field_488.theta_transform.f,
        player->bondprevpos.f,
        position_scale,
        player->stanHeight,
        player->eyeheight,
        player->vv_theta,
        player->field_488.collision_radius,
        player->field_488.current_tile_ptr,
        player->field_488.current_tile_ptr_for_portals,
        player->prop,
        room);

    if (ge_movement_status != NULL) {
        memcpy(ge_movement_status->position, camera_position,
               sizeof(camera_position));
        ge_movement_status->floor_y = player->stanHeight;
        ge_movement_status->yaw_degrees = player->vv_theta;
        ge_movement_status->room = room;
        ge_movement_status->initialized = 1;
    }
}

int ge_original_bond_collision_try_offset(const float offset[3],
                                          int allow_scoot)
{
    struct player *player = ge_original_spawn_player_get();
    struct coord3d original_offset;
    float prior_x;
    float prior_z;
    int accepted;

    if (ge_movement_status == NULL || offset == NULL || player == NULL
            || player->field_488.current_tile_ptr == NULL) {
        return 0;
    }
    original_offset.f[0] = offset[0];
    original_offset.f[1] = offset[1];
    original_offset.f[2] = offset[2];
    prior_x = player->field_488.collision_position.f[0];
    prior_z = player->field_488.collision_position.f[2];
    bondviewCalcUpdatePlayerCollision(&original_offset, allow_scoot);
    player->stanHeight = stanGetPositionYValue(
        player->field_488.current_tile_ptr,
        player->field_488.collision_position.f[0],
        player->field_488.collision_position.f[2]);
    ge_port_bond_movement_publish(player);
    accepted = prior_x != player->field_488.collision_position.f[0]
        || prior_z != player->field_488.collision_position.f[2]
        || (offset[0] == 0.0f && offset[2] == 0.0f);
    ge_movement_status->collision_checks++;
    if (accepted) {
        ge_movement_status->accepted_checks++;
    } else {
        ge_movement_status->blocked_checks++;
    }
    return accepted;
}

int ge_original_bond_collision_validate_position(void)
{
    const float zero[3] = {0.0f, 0.0f, 0.0f};
    return ge_original_bond_collision_try_offset(zero, 0);
}

int ge_original_bond_root_motion_tick(int32_t clock_timer,
                                      float global_timer_delta)
{
    struct player *player = ge_original_spawn_player_get();
    struct coord3d velocity;

    if (ge_movement_status == NULL || player == NULL
            || player->field_488.current_tile_ptr == NULL
            || clock_timer < 0 || !isfinite(global_timer_delta)
            || global_timer_delta < 0.0f
            || ge_movement_providers.sample_head_root_velocity == NULL) {
        if (ge_movement_status != NULL) {
            ge_movement_status->root_motion_samples_missing++;
        }
        return 0;
    }

    if (!ge_movement_providers.sample_head_root_velocity(
            ge_movement_providers.context,
            player->speedforwards,
            player->speedsideways,
            clock_timer,
            global_timer_delta,
            velocity.f)
            || !isfinite(velocity.f[0])
            || !isfinite(velocity.f[1])
            || !isfinite(velocity.f[2])) {
        ge_movement_status->root_motion_samples_missing++;
        return 0;
    }

    return ge_original_bond_root_motion_apply_current_player(
        clock_timer, global_timer_delta, velocity.f);
}

int ge_original_bond_root_motion_apply_current_player(
    int32_t clock_timer,
    float global_timer_delta,
    const float velocity_values[3])
{
    struct player *player = ge_original_spawn_player_get();
    struct coord3d velocity;
    float prior_x;
    float prior_z;

    if (ge_movement_status == NULL || player == NULL
            || player->field_488.current_tile_ptr == NULL
            || velocity_values == NULL || clock_timer < 0
            || !isfinite(global_timer_delta) || global_timer_delta < 0.0f
            || !isfinite(velocity_values[0])
            || !isfinite(velocity_values[1])
            || !isfinite(velocity_values[2])) {
        return 0;
    }
    velocity.f[0] = velocity_values[0];
    velocity.f[1] = velocity_values[1];
    velocity.f[2] = velocity_values[2];
    g_ClockTimer = clock_timer;
    g_GlobalTimerDelta = global_timer_delta;
    bheadUpdatePos(&velocity);
    memcpy(ge_movement_status->head_position, player->headpos.f,
           sizeof(ge_movement_status->head_position));
    prior_x = player->field_488.collision_position.f[0];
    prior_z = player->field_488.collision_position.f[2];
    ge_port_bond_movement_consume_head_root();
    ge_port_bond_movement_record_canonical_collision(prior_x, prior_z);
    ge_movement_status->root_motion_ticks++;
    return 1;
}

int ge_original_bond_collision_try_target_simple(const float position[3])
{
    struct coord3d target;
    struct coord3d collision_point_0 = {0};
    struct coord3d collision_point_1 = {0};

    if (position == NULL || ge_original_spawn_player_get() == NULL
            || ge_original_spawn_player_get()->field_488.current_tile_ptr
                == NULL) {
        return 0;
    }
    target.f[0] = position[0];
    target.f[1] = position[1];
    target.f[2] = position[2];
    return bondviewTrySimpleMovePlayerCollision(
        &target, &collision_point_0, &collision_point_1);
}

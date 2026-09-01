#include "ge_original_player_spawn.h"
#include "ge_original_player_spawn_internal.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static GeOriginalPlayerSpawnProviders ge_player_providers;
static GeOriginalPlayerViewState *ge_player_view;

void ge_original_player_spawn_bind(
    const GeOriginalPlayerSpawnProviders *providers,
    GeOriginalPlayerViewState *state)
{
    memset(&ge_player_providers, 0, sizeof(ge_player_providers));
    if (providers != NULL) {
        ge_player_providers = *providers;
    }
    ge_player_view = state;
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->room = -1;
    }
}

int ge_original_player_spawn_commit(
    const GeOriginalPlayerSpawnConfig *config)
{
    if (config == NULL || ge_player_view == NULL
            || !isfinite(config->position[0])
            || !isfinite(config->position[1])
            || !isfinite(config->position[2])
            || !isfinite(config->floor_y)
            || !isfinite(config->eye_height)
            || !isfinite(config->look_angle_radians)
            || config->stan == NULL || config->room < 0
            || ge_player_providers.allocate_prop == NULL) {
        return 0;
    }
    memset(ge_player_view, 0, sizeof(*ge_player_view));
    ge_player_view->room = -1;
    ge_original_spawn_player_reset(config->eye_height);
    return ge_original_commit_intro_player_spawn_slice(
        config->position, config->floor_y, config->look_angle_radians,
        config->stan, config->room);
}

void *ge_port_player_spawn_allocate_prop(void)
{
    void *prop = ge_player_providers.allocate_prop(
        ge_player_providers.context);
    if (prop != NULL && ge_player_view != NULL) {
        ge_player_view->allocation_calls++;
    }
    return prop;
}

void ge_port_player_spawn_activate_prop(void *prop)
{
    if (ge_player_providers.activate_prop != NULL) {
        ge_player_providers.activate_prop(ge_player_providers.context, prop);
        ge_player_view->activation_calls++;
    }
}

void ge_port_player_spawn_enable_prop(void *prop)
{
    if (ge_player_providers.enable_prop != NULL) {
        ge_player_providers.enable_prop(ge_player_providers.context, prop);
        ge_player_view->enable_calls++;
    }
}

void ge_port_player_spawn_deregister_room(void *prop, int16_t room)
{
    if (ge_player_providers.deregister_room != NULL) {
        ge_player_providers.deregister_room(ge_player_providers.context,
                                            prop, room);
        ge_player_view->deregistration_calls++;
    }
}

void ge_port_player_spawn_register_room(void *prop, int16_t room)
{
    if (ge_player_providers.register_room != NULL) {
        ge_player_providers.register_room(ge_player_providers.context,
                                          prop, room);
        ge_player_view->registration_calls++;
    }
}

void ge_port_player_spawn_publish(
    const float collision_position[3],
    const float camera_position[3],
    const float camera_look[3],
    const float camera_up[3],
    const float heading[3],
    const float previous_position[3],
    const float position_scale[3],
    float floor_y,
    float eye_height,
    float yaw_degrees,
    float collision_radius,
    void *current_stan,
    void *portal_stan,
    void *prop,
    int16_t room)
{
    uint64_t next_generation;

    if (ge_player_view == NULL) {
        return;
    }
    next_generation = ge_player_view->publication_generation + 1U;
    memcpy(ge_player_view->collision_position, collision_position,
           sizeof(ge_player_view->collision_position));
    memcpy(ge_player_view->camera_position, camera_position,
           sizeof(ge_player_view->camera_position));
    memcpy(ge_player_view->camera_look, camera_look,
           sizeof(ge_player_view->camera_look));
    memcpy(ge_player_view->camera_up, camera_up,
           sizeof(ge_player_view->camera_up));
    memcpy(ge_player_view->heading, heading,
           sizeof(ge_player_view->heading));
    memcpy(ge_player_view->previous_position, previous_position,
           sizeof(ge_player_view->previous_position));
    memcpy(ge_player_view->position_scale, position_scale,
           sizeof(ge_player_view->position_scale));
    ge_player_view->floor_y = floor_y;
    ge_player_view->eye_height = eye_height;
    ge_player_view->yaw_degrees = yaw_degrees;
    ge_player_view->collision_radius = collision_radius;
    ge_player_view->current_stan = current_stan;
    ge_player_view->portal_stan = portal_stan;
    ge_player_view->prop = prop;
    ge_player_view->room = room;
    ge_player_view->publication_generation = next_generation;
    ge_player_view->initialized = 1;
}

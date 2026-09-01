#ifndef GE_ORIGINAL_PLAYER_SPAWN_H
#define GE_ORIGINAL_PLAYER_SPAWN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GeOriginalPlayerSpawnProviders {
    void *context;
    void *(*allocate_prop)(void *context);
    void (*activate_prop)(void *context, void *prop);
    void (*enable_prop)(void *context, void *prop);
    void (*deregister_room)(void *context, void *prop, int16_t room);
    void (*register_room)(void *context, void *prop, int16_t room);
} GeOriginalPlayerSpawnProviders;

typedef struct GeOriginalPlayerSpawnConfig {
    float position[3];
    float floor_y;
    float eye_height;
    float look_angle_radians;
    void *stan;
    int16_t room;
} GeOriginalPlayerSpawnConfig;

/* Stable, portable view of the original player state.  Renderer, camera and
 * movement adapters consume this instead of reaching into struct player. */
typedef struct GeOriginalPlayerViewState {
    float collision_position[3];
    float camera_position[3];
    float camera_look[3];
    float camera_up[3];
    float heading[3];
    float previous_position[3];
    float position_scale[3];
    float floor_y;
    float eye_height;
    float yaw_degrees;
    float collision_radius;
    void *current_stan;
    void *portal_stan;
    void *prop;
    int16_t room;
    uint32_t allocation_calls;
    uint32_t activation_calls;
    uint32_t enable_calls;
    uint32_t deregistration_calls;
    uint32_t registration_calls;
    uint64_t publication_generation;
    int initialized;
} GeOriginalPlayerViewState;

void ge_original_player_spawn_bind(
    const GeOriginalPlayerSpawnProviders *providers,
    GeOriginalPlayerViewState *state);

/* Commits the intro result through the exact change_player_pos_to_target body
 * and the original intro/player semantic assignments. */
int ge_original_player_spawn_commit(
    const GeOriginalPlayerSpawnConfig *config);

#ifdef __cplusplus
}
#endif

#endif

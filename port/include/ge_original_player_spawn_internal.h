#ifndef GE_ORIGINAL_PLAYER_SPAWN_INTERNAL_H
#define GE_ORIGINAL_PLAYER_SPAWN_INTERNAL_H

#include <stdint.h>

struct player;

/* The bounded intro slice owns the real decompiled player object. Movement
 * and camera compatibility slices share it through this typed boundary. */
struct player *ge_original_spawn_player_get(void);

void ge_original_spawn_player_reset(float eye_height);
int ge_original_commit_intro_player_spawn_slice(const float position[3],
                                                float floor_y,
                                                float look_angle_radians,
                                                void *stan,
                                                int16_t room);

void *ge_port_player_spawn_allocate_prop(void);
void ge_port_player_spawn_activate_prop(void *prop);
void ge_port_player_spawn_enable_prop(void *prop);
void ge_port_player_spawn_deregister_room(void *prop, int16_t room);
void ge_port_player_spawn_register_room(void *prop, int16_t room);
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
    int16_t room);

#endif

#ifndef GE_ORIGINAL_BOND_MOVEMENT_H
#define GE_ORIGINAL_BOND_MOVEMENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GeOriginalBondMovementProviders {
    void *context;
    int (*collision_types)(void *context);
    void (*set_prop_collision)(void *context, void *prop, int enabled);
    /* Supplies the animation-derived root velocity that original bheadUpdate
     * normally obtains from the current Model's calculated root matrix. */
    int (*sample_head_root_velocity)(void *context,
                                     float speed_forwards,
                                     float speed_sideways,
                                     int32_t clock_timer,
                                     float global_timer_delta,
                                     float velocity[3]);
} GeOriginalBondMovementProviders;

typedef struct GeOriginalBondMovementStatus {
    float position[3];
    float head_position[3];
    float floor_y;
    float yaw_degrees;
    uint32_t collision_checks;
    uint32_t accepted_checks;
    uint32_t blocked_checks;
    uint32_t root_motion_ticks;
    uint32_t root_motion_samples_missing;
    uint32_t blocked_by_prop;
    uint32_t blocked_by_stan;
    uintptr_t last_blocking_prop;
    int32_t last_blocking_prop_type;
    int32_t last_blocking_object_type;
    int16_t room;
    int initialized;
} GeOriginalBondMovementStatus;

void ge_original_bond_movement_bind(
    const GeOriginalBondMovementProviders *providers,
    GeOriginalBondMovementStatus *status);

/* Canonical normal single-player provider pair. playerInit enables the full
 * dynamic collision mask; MoveBond temporarily removes only its own viewer
 * prop from those queries through player::field_AC. */
int ge_original_bond_movement_normal_collision_types(void *context);
void ge_original_bond_movement_set_current_player_collision(
    void *context, void *prop, int enabled);

/* Diagnostic compatibility boundary. It enters the exact original collision
 * and fallback bodies, but is deliberately not a gameplay controller. */
int ge_original_bond_collision_try_offset(const float offset[3],
                                          int allow_scoot);
int ge_original_bond_collision_try_target_simple(const float position[3]);
int ge_original_bond_collision_validate_position(void);

/* Advances the exact original bhead position damper and the bounded normal
 * MoveBond head-root consumer. Returns zero until an original-animation root
 * sample provider is bound; it never substitutes direct stick translation. */
int ge_original_bond_root_motion_tick(int32_t clock_timer,
                                      float global_timer_delta);
/* Applies an already-decoded original head-root velocity through the same
 * exact bheadUpdatePos and normal MoveBond/STAN consumer used above. */
int ge_original_bond_root_motion_apply_current_player(
    int32_t clock_timer,
    float global_timer_delta,
    const float velocity[3]);


#ifdef __cplusplus
}
#endif

#endif

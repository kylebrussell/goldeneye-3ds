#ifndef GE_ORIGINAL_PP7_FIRE_H
#define GE_ORIGINAL_PP7_FIRE_H

#include <stdint.h>

typedef enum GeOriginalPp7FireStatus {
    GE_ORIGINAL_PP7_FIRE_IDLE = 0,
    GE_ORIGINAL_PP7_FIRE_NO_PLAYER,
    GE_ORIGINAL_PP7_FIRE_VIEW_UNAVAILABLE,
    GE_ORIGINAL_PP7_FIRE_NO_STAN,
    GE_ORIGINAL_PP7_FIRE_ORIGIN_OUTSIDE_STAN,
    GE_ORIGINAL_PP7_FIRE_GUARD_HIT_REGISTERED,
    GE_ORIGINAL_PP7_FIRE_STAN_HIT,
    GE_ORIGINAL_PP7_FIRE_BACKGROUND_PROP_FRONTIER,
    GE_ORIGINAL_PP7_FIRE_GUARD_DAMAGE_APPLIED,
    GE_ORIGINAL_PP7_FIRE_OBJECT_HIT_REGISTERED,
    GE_ORIGINAL_PP7_FIRE_OBJECT_DAMAGE_APPLIED
} GeOriginalPp7FireStatus;

typedef struct GeOriginalPp7FireStats {
    uint32_t both_hands_ticks;
    uint32_t hand_dispatches;
    uint32_t pp7_shots;
    uint32_t stan_hits;
    uint32_t clear_stan_paths;
    uint32_t view_rejections;
    uint32_t guard_hits_registered;
    uint32_t guard_damage_applied;
    uint32_t guard_damage_frontiers;
    uint32_t object_hits_registered;
    uint32_t object_damage_applied;
    uint32_t object_destroyed;
    int32_t last_hand;
    int32_t last_weapon;
    int32_t last_ammo_after_hand_tick;
    uint16_t last_shot_sound;
    uint8_t last_beam_pose_ready;
    int8_t last_guard_hitpart;
    int8_t last_object_type;
    uint8_t last_object_destroyed_level;
    float last_origin[3];
    float last_direction[3];
    float last_endpoint[3];
} GeOriginalPp7FireStats;

typedef int (*GeOriginalPp7ObjectHitReady)(
    void *context, const void *model_instance);

/* Binds the native model-ownership/collision readiness service used before
 * the unchanged object Gfx ray traversal.  NULL is deliberately strict: no
 * ordinary object is traversed until its relocated geometry is proven live. */
void ge_original_pp7_fire_bind_object_hit_ready(
    void *context, GeOriginalPp7ObjectHitReady hit_ready);

void ge_original_pp7_fire_reset(void);

/* Executes the exact right-then-left chraiCheckUseHeldItems dispatch for the
 * PP7 pair, followed by the canonical chraiDefaultWeaponFireHandler prefix
 * through authored STAN traversal. gunTickHandState must run first: it owns
 * firing state, ammo consumption, volley state and shot SFX. */
GeOriginalPp7FireStatus ge_original_pp7_fire_tick(void);

void ge_original_pp7_fire_snapshot(GeOriginalPp7FireStats *stats);

#endif

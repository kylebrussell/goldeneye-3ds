#ifndef GE_ORIGINAL_GUARD_BULLET_HIT_H
#define GE_ORIGINAL_GUARD_BULLET_HIT_H

#include <stddef.h>
#include <stdint.h>

typedef size_t (*GeOriginalGuardBulletHitCount)(void *context);
typedef int (*GeOriginalGuardBulletHitActor)(
    void *context, size_t index, void **prop_record, void **chr_record);

typedef enum GeOriginalGuardBulletHitStatus {
    GE_ORIGINAL_GUARD_BULLET_IDLE = 0,
    GE_ORIGINAL_GUARD_BULLET_NO_GUARDS,
    GE_ORIGINAL_GUARD_BULLET_HIT_POOL_EXHAUSTED,
    GE_ORIGINAL_GUARD_BULLET_MISS,
    GE_ORIGINAL_GUARD_BULLET_HIT_REGISTERED,
    GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER,
    GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED
} GeOriginalGuardBulletHitStatus;

typedef struct GeOriginalGuardBulletHitStats {
    uint32_t pool_resets;
    uint32_t model_lists_built;
    uint32_t rays_tested;
    uint32_t guard_candidates;
    uint32_t onscreen_gate_passes;
    uint32_t depth_gate_passes;
    uint32_t sphere_gate_passes;
    uint32_t valid_room_lists;
    uint32_t rendered_room_gate_passes;
    uint32_t fog_gate_passes;
    uint32_t near_fog_gate_passes;
    uint32_t frustum_gate_passes;
    uint32_t registered_hits;
    uint32_t bounding_sphere_hits;
    uint32_t damage_attempts;
    uint32_t damage_applied;
    int32_t last_guard_index;
    int32_t last_hitpart;
    float last_distance;
    int32_t observed_guard_index;
    uint32_t observed_guard_samples;
    uint32_t observed_guard_onscreen_samples;
    int32_t observed_shared_current_room;
    int32_t observed_shared_rooms_drawn;
    uint32_t observed_prop_flags;
    float observed_prop_zdepth;
    uint8_t observed_prop_rooms[4];
    int32_t observed_chrai_rooms[4];
    uint8_t observed_room_rendered[4];
} GeOriginalGuardBulletHitStats;

void ge_original_guard_bullet_hit_reset(void);

/* Binds the campaign guard runtime whose canonical PropRecord/ChrRecord/model
 * instances are traversed by chrTestHit.  A null binding retains the focused
 * legacy Dam fixture used by host tests. */
void ge_original_guard_bullet_hit_bind_stage_guards(
    void *context, GeOriginalGuardBulletHitCount count,
    GeOriginalGuardBulletHitActor actor);

/* Selects one authored guard for read-only shot-time room ABI diagnostics.
 * This never changes hit testing or canonical visibility flags. */
void ge_original_guard_bullet_hit_observe_guard(int32_t guard_index);

/* Tests a camera-space shot against all currently constructed authored Dam
 * guards using the original ModelHitEntry/BBOX/ShotData registration chain. */
GeOriginalGuardBulletHitStatus ge_original_guard_bullet_hit_test(
    const float view_origin[3], const float view_direction[3],
    const float world_direction[3], int32_t weapon, float max_distance);

/* Appends the live authored guards to an existing canonical ShotData hit list.
 * This is the shared-list boundary used by the unchanged PP7 object/character
 * dispatch, where chrpropAddBulletHit retains the original depth ordering. */
GeOriginalGuardBulletHitStatus
ge_original_guard_bullet_hit_populate_shot(void *shot_record);

/* Applies one character entry from that shared canonical ShotData list through
 * the unchanged chrHandleBulletHit damage/reaction chain. */
GeOriginalGuardBulletHitStatus ge_original_guard_bullet_hit_apply_shot_hit(
    void *shot_record, int32_t hit_index);

/* Applies the most recently registered canonical ShotData through the exact
 * chrHandleBulletHit -> handles_shot_actors chain. */
GeOriginalGuardBulletHitStatus ge_original_guard_bullet_hit_apply_pending(void);

void ge_original_guard_bullet_hit_snapshot(
    GeOriginalGuardBulletHitStats *stats);

#endif

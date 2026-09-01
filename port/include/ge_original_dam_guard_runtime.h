#ifndef GE_ORIGINAL_DAM_GUARD_RUNTIME_H
#define GE_ORIGINAL_DAM_GUARD_RUNTIME_H

#include <stdint.h>

typedef enum GeOriginalDamGuardRuntimeStatus {
    GE_ORIGINAL_DAM_GUARD_RUNTIME_OK = 0,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_NOT_READY,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_PLAYER_STATE,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_TIMER_STATE,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_ACTIVE_LIST,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_ARENA_STATE,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_GUN_ORDER,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_ALREADY_TICKED,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_ARENA_OVERFLOW,
    GE_ORIGINAL_DAM_GUARD_RUNTIME_MATRIX_REFRESH
} GeOriginalDamGuardRuntimeStatus;

typedef struct GeOriginalDamGuardRuntimeStats {
    uint64_t ticks;
    uint64_t matrix_refreshes;
    uint64_t rejected_ticks;
    uint64_t weapon_fire_dispatches;
    uint64_t weapon_sound_starts;
    uint64_t player_damage_events;
    uint64_t last_arena_generation;
    float player_health_damage;
    float player_armour_damage;
    float last_player_health;
    float last_player_armour;
    uint32_t last_global_timer;
    uint32_t last_arena_bytes_before;
    uint32_t last_arena_bytes_after;
    uint8_t last_status;
} GeOriginalDamGuardRuntimeStats;

typedef struct GeOriginalPlayerCombatSnapshot {
    float health;
    float armour;
    float actual_health;
    float actual_armour;
    int32_t damage_show_time;
    uint8_t dead;
    uint8_t invincible;
} GeOriginalPlayerCombatSnapshot;

void ge_original_dam_guard_runtime_reset(void);

/* Production activation boundary. The 3DS build keeps the call compiled out
 * until GE_DAM_FULL_PROPS_LIVE selects the fully closed canonical support
 * slice. The caller must have published the canonical player/timers, completed
 * the original gun update, and kept the shared gun/graphics arena active for
 * this original tick. On success this invokes unchanged propsTick exactly
 * once, then rebuilds the durable guard matrices consumed by the 3DS renderer.
 * The bounded door runtime must not be called in the same activation path:
 * unchanged objTick owns both authored doors. */
GeOriginalDamGuardRuntimeStatus ge_original_dam_guard_runtime_tick(
    const float world_to_view[4][4]);

void ge_original_dam_guard_runtime_snapshot(
    GeOriginalDamGuardRuntimeStats *stats);
void ge_original_dam_guard_player_vitals_snapshot(float *health, float *armour);
void ge_original_dam_guard_player_combat_snapshot(
    GeOriginalPlayerCombatSnapshot *snapshot);

#endif

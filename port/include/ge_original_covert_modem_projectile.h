#ifndef GE_ORIGINAL_COVERT_MODEM_PROJECTILE_H
#define GE_ORIGINAL_COVERT_MODEM_PROJECTILE_H

#include <stdint.h>

typedef enum GeOriginalCovertModemProjectileStatus {
    GE_ORIGINAL_COVERT_MODEM_PROJECTILE_OK = 0,
    GE_ORIGINAL_COVERT_MODEM_PROJECTILE_INVALID_ARGUMENT,
    GE_ORIGINAL_COVERT_MODEM_PROJECTILE_NO_PLAYER,
    GE_ORIGINAL_COVERT_MODEM_PROJECTILE_SHADING_UNAVAILABLE,
    GE_ORIGINAL_COVERT_MODEM_PROJECTILE_ROOM_ABI_UNAVAILABLE,
    GE_ORIGINAL_COVERT_MODEM_PROJECTILE_POOL_EXHAUSTED
} GeOriginalCovertModemProjectileStatus;

typedef struct GeOriginalCovertModemProjectileStats {
    uint32_t launch_calls;
    uint32_t successful_launches;
    uint32_t fallback_launches;
    uint32_t pool_allocations;
    uint32_t pool_exhaustions;
    uint32_t sound_events;
} GeOriginalCovertModemProjectileStats;

void ge_original_covert_modem_projectile_reset(void);

/* Canonical gunInitProjectileFromPlayer boundary.  Unlike launch(), this
 * stops before generate_player_thrown_object's projectile flags and sound
 * tail so the unchanged original caller can own those side effects. */
GeOriginalCovertModemProjectileStatus
ge_original_covert_modem_projectile_init_from_player(
    void *object, const void *target, void *launch_matrix,
    const void *velocity, const void *orientation_matrix);

/* ABI-compatible void entrypoint used by the mechanically extracted original
 * generate_player_thrown_object body. */
void ge_original_gun_init_projectile_from_player_exact(
    void *object, void *target, void *launch_matrix,
    void *velocity, void *orientation_matrix);

/* Exact ITEM_BUG gunInitProjectileFromPlayer/gunInitProjectileObject path.
 * target, launch_matrix, velocity and orientation_matrix use the native
 * coord3d/Mtxf layouts and remain void here to keep the public port API free
 * of decomp-only include requirements. */
GeOriginalCovertModemProjectileStatus
ge_original_covert_modem_projectile_launch(
    void *object, const void *target, void *launch_matrix,
    const void *velocity, const void *orientation_matrix);

void ge_original_covert_modem_projectile_snapshot(
    GeOriginalCovertModemProjectileStats *stats);

#endif

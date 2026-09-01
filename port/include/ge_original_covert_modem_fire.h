#ifndef GE_ORIGINAL_COVERT_MODEM_FIRE_H
#define GE_ORIGINAL_COVERT_MODEM_FIRE_H

#include <stdint.h>

typedef enum GeOriginalCovertModemFireStatus {
    GE_ORIGINAL_COVERT_MODEM_FIRE_IDLE = 0,
    GE_ORIGINAL_COVERT_MODEM_FIRE_THROWN,
    GE_ORIGINAL_COVERT_MODEM_FIRE_NO_PLAYER,
    GE_ORIGINAL_COVERT_MODEM_FIRE_POSE_UNAVAILABLE,
    GE_ORIGINAL_COVERT_MODEM_FIRE_OBJECT_UNAVAILABLE,
    GE_ORIGINAL_COVERT_MODEM_FIRE_PROJECTILE_FAILED
} GeOriginalCovertModemFireStatus;

typedef struct GeOriginalCovertModemFireStats {
    uint32_t both_hands_ticks;
    uint32_t hand_dispatches;
    uint32_t throw_attempts;
    uint32_t successful_throws;
    uint32_t pose_rejections;
} GeOriginalCovertModemFireStats;

void ge_original_covert_modem_fire_reset(void);

/* Canonical right-then-left hand order. The ITEM_BUG branch now enters the
 * mechanically extracted generate_player_thrown_object body; the complete
 * gunUpdateAndFire pose/render dispatcher remains a separate closure. */
GeOriginalCovertModemFireStatus ge_original_covert_modem_fire_tick(void);

void ge_original_covert_modem_fire_snapshot(
    GeOriginalCovertModemFireStats *stats);

#endif

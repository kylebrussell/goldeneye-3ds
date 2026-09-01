#ifndef GE_ORIGINAL_GUN_LIVE_H
#define GE_ORIGINAL_GUN_LIVE_H

#include <stddef.h>
#include <stdint.h>
#include "ge_original_gun_frame_arena.h"

typedef struct GeOriginalGunLiveStats {
    uint64_t ticks;
    uint64_t last_frame_generation;
    size_t last_frame_bytes;
    size_t peak_frame_bytes;
} GeOriginalGunLiveStats;

typedef struct GeOriginalGunLiveHand {
    const void *model;
    const float (*matrices)[4][4];
    size_t matrix_count;
    uint64_t generation;
    int visible;
} GeOriginalGunLiveHand;

void ge_original_gun_live_reset(void);
int ge_original_gun_live_frame_begin(void);
int ge_original_gun_live_frame_finalize(GeOriginalDynFrameAudit *audit);
int ge_original_gun_live_tick(void);
void ge_original_gun_live_snapshot(GeOriginalGunLiveStats *stats);
int ge_original_gun_live_hand_snapshot(
    unsigned hand, GeOriginalGunLiveHand *publication);

#endif

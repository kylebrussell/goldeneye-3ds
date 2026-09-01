#ifndef GE_ORIGINAL_BOSS_H
#define GE_ORIGINAL_BOSS_H

#include <stdint.h>

typedef struct GeOriginalBossState {
    int32_t current_stage;
    int32_t requested_stage;
    int32_t debug_menu_open;
    int32_t show_memory_use;
    int32_t show_memory_bars;
} GeOriginalBossState;

/* Resets the original boss.c stage state to the title screen. */
void ge_original_boss_reset(void);

/* Requests a stage using boss.c's original deferred-transition function. */
void ge_original_boss_request_stage(int32_t stage);

/* Applies a pending request at the outer-loop boundary used by bossMainloop. */
int ge_original_boss_commit_requested_stage(void);

/* Copies the original boss.c globals without exposing them to frontends. */
void ge_original_boss_snapshot(GeOriginalBossState *state);

#endif

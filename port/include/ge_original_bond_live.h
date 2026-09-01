#ifndef GE_ORIGINAL_BOND_LIVE_H
#define GE_ORIGINAL_BOND_LIVE_H

#include <stdint.h>

#include "ge_original_dam_intro.h"
#include "ge_original_frontend_statistics.h"

typedef struct GeOriginalBondLiveState {
    uint32_t initialization_count;
    uint32_t input_tick_count;
    uint32_t move_tick_count;
    GeOriginalIntroLoadoutState loadout;
    int initialized;
} GeOriginalBondLiveState;

typedef struct GeOriginalBondAimSnapshot {
    float crosshair[2];
    float autoaim[2];
    float screen_left;
    float screen_top;
    float screen_width;
    float screen_height;
    void *target_x;
    void *target_y;
    int32_t target_time_x;
    int32_t target_time_y;
} GeOriginalBondAimSnapshot;

typedef struct GeOriginalBondMotionSnapshot {
    float speed_forwards;
    float speed_sideways;
    float speed_boost;
    float head_position[3];
    float stan_height;
    float eye_height;
    int32_t controls_locked;
    int32_t watch_animation_state;
    int32_t dead;
    int32_t camera_mode;
    int32_t in_tank;
} GeOriginalBondMotionSnapshot;

/* Binds the real player object committed by the intro spawn and runs the
 * canonical hand/player initialization with platform-owned stage buffers. */
int ge_original_bond_live_initialize(void *right_hand_buffer,
                                     void *left_hand_buffer,
                                     GeOriginalBondLiveState *state);

/* Runs the canonical bondviewProcessInput body against the controller sample
 * already published through ge_original_input_tick. */
int ge_original_bond_live_tick(GeOriginalBondLiveState *state,
                               int32_t clock_timer,
                               int32_t global_timer,
                               float global_timer_delta);

/* Runs unchanged canonical MoveBond exactly once for the current controller
 * frame, then publishes its resulting player/STAN/camera state.  This is the
 * replacement boundary for ge_original_bond_live_tick plus the bounded gait
 * consumer; callers must not invoke either of those in the same frame. */
int ge_original_bond_move_live_tick(GeOriginalBondLiveState *state,
                                    int32_t clock_timer,
                                    int32_t global_timer,
                                    float global_timer_delta);

int ge_original_bond_live_aim_snapshot(GeOriginalBondAimSnapshot *snapshot);
int ge_original_bond_live_motion_snapshot(
    GeOriginalBondMotionSnapshot *snapshot);
/* Exact watch page/subpage state for the 3DS GBI/text presentation adapter. */
int ge_original_bond_live_watch_objectives_visible(void);

/* Narrow exact snapshot of the player fields consumed by the unchanged
 * mission-complete statistic constructor. */
int ge_original_bond_live_statistics_state(
    int32_t shot_register[GE_ORIGINAL_FRONTEND_SHOT_REGISTER_COUNT],
    int32_t *kill_count,
    GeOriginalFrontendHeldWeapon
        held[GE_ORIGINAL_FRONTEND_HELD_WEAPON_COUNT]);
int ge_original_bond_live_weapon_choice_text(
    int32_t weapon, uint16_t *text_id);

#endif

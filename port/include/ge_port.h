#ifndef GE_PORT_H
#define GE_PORT_H

#include <stdbool.h>
#include <stdint.h>

#define GE_PORT_TICK_RATE 60.0

typedef enum GePortAction {
    GE_PORT_ACTION_FIRE = 1u << 0,
    GE_PORT_ACTION_USE = 1u << 1,
    GE_PORT_ACTION_RELOAD = 1u << 2,
    GE_PORT_ACTION_CROUCH = 1u << 3,
    GE_PORT_ACTION_AIM = 1u << 4,
    GE_PORT_ACTION_NEXT_WEAPON = 1u << 5,
    GE_PORT_ACTION_PREV_WEAPON = 1u << 6,
    GE_PORT_ACTION_PAUSE = 1u << 7,
} GePortAction;

typedef struct GePortInput {
    float move_x;
    float move_y;
    float look_x;
    float look_y;
    uint32_t held;
    uint32_t pressed;
} GePortInput;

typedef struct GePortState {
    uint64_t simulation_ticks;
    uint64_t dropped_simulation_ticks;
    double accumulator_seconds;
    GePortInput input;
    uint32_t random_sample;
    float view_yaw;
    float view_pitch;
    float view_orientation[4];
    float original_move_x;
    float original_move_y;
    float original_look_x;
    float original_look_y;
    uint16_t original_buttons;
    uint16_t original_buttons_pressed;
    int32_t original_stage;
    int32_t original_requested_stage;
    int32_t original_clock_timer;
    int32_t original_global_timer;
    int32_t original_active_frame_updates;
    float original_global_timer_delta;
    int32_t original_stage_frames;
    float original_stage_seconds;
    int32_t original_idle_frames;
    int32_t original_idle_latched;
} GePortState;

void ge_port_init(GePortState *state);
/* Uses boss.c's original deferred request/commit boundary, then selects the
 * corresponding original lvlManageMpGame branch. */
int ge_port_start_stage(GePortState *state, int32_t stage_id);
/* Canonical late-VI boundary: dispatch gameplay once and publish the elapsed
 * retrace count through speedgraphframes/g_ClockTimer, matching the original
 * waitForNextFrame -> updateFrameCounters ordering. */
unsigned ge_port_advance_retraces(GePortState *state,
                                  unsigned retrace_frames,
                                  const GePortInput *input);
unsigned ge_port_advance(GePortState *state, double elapsed_seconds, const GePortInput *input);
/*
 * Platform scheduling boundary for targets that cannot safely replay an
 * unbounded wall-clock backlog.  Every delivered tick still runs the exact
 * same original-game service chain as ge_port_advance; only excess elapsed
 * ticks are discarded after max_ticks have been delivered so a slow frame
 * cannot create a self-sustaining catch-up spiral.
 */
unsigned ge_port_advance_bounded(GePortState *state, double elapsed_seconds,
                                 const GePortInput *input,
                                 unsigned max_ticks);
double ge_port_frame_alpha(const GePortState *state);
void ge_port_view_vector(const GePortState *state, float *x, float *y, float *z);

#endif

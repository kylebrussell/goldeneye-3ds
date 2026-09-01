#include "ge_port.h"
#include "ge_original_input.h"
#include "ge_original_boss.h"
#include "ge_original_level.h"

#include <math.h>
#include <limits.h>
#include <string.h>

#include <random.h>
#include "game/frametiming.h"
#include "game/quaternion.h"

#define GE_PORT_LOOK_RADIANS_PER_TICK 0.035f
#define GE_PORT_MAX_PITCH 1.45f
#define GE_PORT_CSTICK_BUTTON_THRESHOLD 0.35f

static uint16_t ge_port_n64_buttons(const GePortInput *input)
{
    uint16_t buttons = 0U;
    const uint32_t actions = input->held | input->pressed;

    if ((actions & GE_PORT_ACTION_FIRE) != 0U) {
        buttons |= Z_TRIG;
    }
    if ((actions & (GE_PORT_ACTION_USE | GE_PORT_ACTION_RELOAD)) != 0U) {
        buttons |= B_BUTTON;
    }
    if ((actions & GE_PORT_ACTION_CROUCH) != 0U) {
        buttons |= R_TRIG;
    }
    if ((actions & GE_PORT_ACTION_AIM) != 0U) {
        buttons |= L_TRIG;
    }
    if ((actions & GE_PORT_ACTION_NEXT_WEAPON) != 0U) {
        buttons |= A_BUTTON;
    }
    if ((actions & GE_PORT_ACTION_PREV_WEAPON) != 0U) {
        buttons |= D_JPAD;
    }
    if ((actions & GE_PORT_ACTION_PAUSE) != 0U) {
        buttons |= START_BUTTON;
    }
    /* The stock 1.1 control body consumes N64 C-buttons for digital strafe
     * and vertical look. Quantizing the 3DS C-stick here keeps all resulting
     * camera/movement behavior inside bondviewProcessInput. */
    if (input->look_x < -GE_PORT_CSTICK_BUTTON_THRESHOLD) {
        buttons |= L_CBUTTONS;
    } else if (input->look_x > GE_PORT_CSTICK_BUTTON_THRESHOLD) {
        buttons |= R_CBUTTONS;
    }
    if (input->look_y < -GE_PORT_CSTICK_BUTTON_THRESHOLD) {
        buttons |= D_CBUTTONS;
    } else if (input->look_y > GE_PORT_CSTICK_BUTTON_THRESHOLD) {
        buttons |= U_CBUTTONS;
    }
    return buttons;
}

static uint16_t ge_port_level_buttons_pressed(void *context)
{
    (void)context;
    return ge_original_input_pressed();
}

static void ge_port_latch_input(GePortState *state, const GePortInput *input)
{
    state->input.move_x = input->move_x;
    state->input.move_y = input->move_y;
    state->input.look_x = input->look_x;
    state->input.look_y = input->look_y;
    state->input.held = input->held;
    state->input.pressed |= input->pressed;
}

static void ge_port_tick(GePortState *state, unsigned retrace_frames)
{
    GeOriginalBossState boss_state;
    GeOriginalLevelTimerState level_timer;
    GeOriginalInputSample original_input;
    vec3f view_angles;

    /*
     * These are the first original game services running in the portable tick:
     * frame accounting and the quaternion camera math. The MIPS RNG was assembly,
     * so port/src/random_port.c preserves that algorithm in portable C.
     */
    original_input.move_x = state->input.move_x;
    original_input.move_y = state->input.move_y;
    original_input.look_x = state->input.look_x;
    original_input.look_y = state->input.look_y;
    original_input.buttons = ge_port_n64_buttons(&state->input);
    ge_original_input_tick(&original_input);
    state->original_move_x = ge_original_input_move_x();
    state->original_move_y = ge_original_input_move_y();
    state->original_look_x = ge_original_input_look_x();
    state->original_look_y = ge_original_input_look_y();
    state->original_buttons = ge_original_input_buttons();
    state->original_buttons_pressed = ge_original_input_pressed();

    ge_original_boss_snapshot(&boss_state);
    state->original_stage = boss_state.current_stage;
    state->original_requested_stage = boss_state.requested_stage;
    ge_original_level_set_stage(boss_state.current_stage);

    updateFrameCounters((s32)retrace_frames);
    ge_original_level_tick(speedgraphframes);
    ge_original_level_timer_snapshot(&level_timer);
    state->original_clock_timer = level_timer.clock_timer;
    state->original_global_timer = level_timer.global_timer;
    state->original_active_frame_updates = level_timer.active_frame_updates;
    state->original_global_timer_delta = level_timer.global_timer_delta;
    state->original_stage_frames = level_timer.active_stage_frames;
    state->original_stage_seconds = level_timer.stage_seconds;
    state->original_idle_frames = level_timer.idle_frames;
    state->original_idle_latched = level_timer.idle_latched;
    /* Diagnostics must observe, never advance, the gameplay RNG. The former
     * sampling call inserted one non-authored draw per simulation tick and
     * desynchronised every original RAMROM attract recording immediately
     * after its first controller block. */
    state->random_sample = (uint32_t)g_randomSeed;

    state->view_yaw += state->original_look_x * GE_PORT_LOOK_RADIANS_PER_TICK
        * (float)retrace_frames;
    state->view_pitch += state->original_look_y * GE_PORT_LOOK_RADIANS_PER_TICK
        * (float)retrace_frames;
    if (state->view_pitch < -GE_PORT_MAX_PITCH) {
        state->view_pitch = -GE_PORT_MAX_PITCH;
    } else if (state->view_pitch > GE_PORT_MAX_PITCH) {
        state->view_pitch = GE_PORT_MAX_PITCH;
    }

    if (state->view_yaw > M_PI_F) {
        state->view_yaw -= M_TAU_F;
    } else if (state->view_yaw < -M_PI_F) {
        state->view_yaw += M_TAU_F;
    }

    view_angles[0] = state->view_pitch;
    view_angles[1] = state->view_yaw;
    view_angles[2] = 0.0f;
    quaternion_set_rotation_around_xyzf(view_angles, state->view_orientation);

    state->simulation_ticks++;
    state->input.pressed = 0;
}

void ge_port_init(GePortState *state)
{
    memset(state, 0, sizeof(*state));
    state->view_orientation[0] = 1.0f;
    ge_original_input_init();
    ge_original_boss_reset();
    {
        GeOriginalLevelProviders level_providers = {
            .buttons_pressed = ge_port_level_buttons_pressed,
        };
        ge_original_level_init(&level_providers);
    }
    {
        GeOriginalBossState boss_state;
        ge_original_boss_snapshot(&boss_state);
        state->original_stage = boss_state.current_stage;
        state->original_requested_stage = boss_state.requested_stage;
    }
    randomSetSeed(0x0073D5U);
    store_osgetcount();
}

int ge_port_start_stage(GePortState *state, int32_t stage_id)
{
    GeOriginalBossState boss_state;

    if (state == NULL) {
        return 0;
    }
    ge_original_boss_request_stage(stage_id);
    if (!ge_original_boss_commit_requested_stage()) {
        return 0;
    }
    ge_original_boss_snapshot(&boss_state);
    ge_original_level_set_stage(boss_state.current_stage);
    state->original_stage = boss_state.current_stage;
    state->original_requested_stage = boss_state.requested_stage;
    return boss_state.current_stage == stage_id;
}

static unsigned ge_port_advance_internal(GePortState *state,
                                         double elapsed_seconds,
                                         const GePortInput *input,
                                         unsigned max_ticks,
                                         int discard_excess)
{
    const double tick_seconds = 1.0 / GE_PORT_TICK_RATE;
    unsigned ticks = 0;

    if (elapsed_seconds < 0.0) {
        elapsed_seconds = 0.0;
    } else if (elapsed_seconds > 0.25) {
        elapsed_seconds = 0.25;
    }

    ge_port_latch_input(state, input);
    state->accumulator_seconds += elapsed_seconds;

    while (state->accumulator_seconds >= tick_seconds && ticks < max_ticks) {
        ge_port_tick(state, 1U);
        state->accumulator_seconds -= tick_seconds;
        ticks++;
    }

    if (discard_excess) {
        while (state->accumulator_seconds >= tick_seconds) {
            state->accumulator_seconds -= tick_seconds;
            state->dropped_simulation_ticks++;
        }
    }

    return ticks;
}

unsigned ge_port_advance_retraces(GePortState *state,
                                  unsigned retrace_frames,
                                  const GePortInput *input)
{
    ge_port_latch_input(state, input);
    if (retrace_frames == 0U) {
        return 0U;
    }
    if (retrace_frames > (unsigned)INT_MAX) {
        retrace_frames = (unsigned)INT_MAX;
    }
    /* The original frontend dispatches gameplay once after a late VI and
     * publishes the number of elapsed retraces through speedgraphframes.
     * Do not replay N expensive gameplay passes or silently discard time. */
    ge_port_tick(state, retrace_frames);
    return 1U;
}

unsigned ge_port_advance(GePortState *state, double elapsed_seconds,
                         const GePortInput *input)
{
    return ge_port_advance_internal(
        state, elapsed_seconds, input, UINT_MAX, 0);
}

unsigned ge_port_advance_bounded(GePortState *state, double elapsed_seconds,
                                 const GePortInput *input,
                                 unsigned max_ticks)
{
    return ge_port_advance_internal(
        state, elapsed_seconds, input, max_ticks, 1);
}

double ge_port_frame_alpha(const GePortState *state)
{
    return state->accumulator_seconds * GE_PORT_TICK_RATE;
}

void ge_port_view_vector(const GePortState *state, float *x, float *y, float *z)
{
    const float w = state->view_orientation[0];
    const float qx = state->view_orientation[1];
    const float qy = state->view_orientation[2];
    const float qz = state->view_orientation[3];

    *x = 2.0f * ((qx * qz) + (w * qy));
    *y = 2.0f * ((qy * qz) - (w * qx));
    *z = 1.0f - (2.0f * ((qx * qx) + (qy * qy)));
}

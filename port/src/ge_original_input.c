#include "ge_original_input.h"

#include <math.h>
#include <string.h>

#include <ultra64.h>
#include "joy.h"

#define GE_MOVE_CONTROLLER 0
#define GE_LOOK_CONTROLLER 1
#define GE_VIRTUAL_CONTROLLER_COUNT 2
#define GE_ORIGINAL_STICK_LIMIT 60.0f

static GeOriginalInputSample g_pending_sample;

static s8 ge_original_input_stick(float value)
{
    if (!isfinite(value)) {
        return 0;
    }
    if (value < -1.0f) {
        value = -1.0f;
    } else if (value > 1.0f) {
        value = 1.0f;
    }
    return (s8)lroundf(value * GE_ORIGINAL_STICK_LIMIT);
}

static s32 ge_original_input_playback(struct contsample *samples, s32 current)
{
    s32 next = (current + 1) % CONTSAMPLE_LEN;

    memset(&samples[next], 0, sizeof(samples[next]));
    samples[next].pads[GE_MOVE_CONTROLLER].stick_x =
        ge_original_input_stick(g_pending_sample.move_x);
    samples[next].pads[GE_MOVE_CONTROLLER].stick_y =
        ge_original_input_stick(g_pending_sample.move_y);
    samples[next].pads[GE_MOVE_CONTROLLER].button = g_pending_sample.buttons;
    samples[next].pads[GE_LOOK_CONTROLLER].stick_x =
        ge_original_input_stick(g_pending_sample.look_x);
    samples[next].pads[GE_LOOK_CONTROLLER].stick_y =
        ge_original_input_stick(g_pending_sample.look_y);
    return next;
}

void ge_original_input_init(void)
{
    memset(&g_pending_sample, 0, sizeof(g_pending_sample));
    joySetPlaybackFunc(
        ge_original_input_playback,
        GE_VIRTUAL_CONTROLLER_COUNT);
    joySetContDataIndex(1);

    /* Flush any held state when a portable runtime is restarted in-process. */
    joyConsumeSamplesWrapper();
    joyConsumeSamplesWrapper();
}

void ge_original_input_tick(const GeOriginalInputSample *sample)
{
    g_pending_sample = *sample;
    joyConsumeSamplesWrapper();
}

float ge_original_input_move_x(void)
{
    return joyGetStickXInRangef(GE_MOVE_CONTROLLER, -1.0f, 1.0f);
}

float ge_original_input_move_y(void)
{
    return joyGetStickYInRangef(GE_MOVE_CONTROLLER, -1.0f, 1.0f);
}

float ge_original_input_look_x(void)
{
    return joyGetStickXInRangef(GE_LOOK_CONTROLLER, -1.0f, 1.0f);
}

float ge_original_input_look_y(void)
{
    return joyGetStickYInRangef(GE_LOOK_CONTROLLER, -1.0f, 1.0f);
}

uint16_t ge_original_input_buttons(void)
{
    return joyGetButtons(GE_MOVE_CONTROLLER, ANY_BUTTON);
}

uint16_t ge_original_input_pressed(void)
{
    return joyGetButtonsPressedThisFrame(GE_MOVE_CONTROLLER, ANY_BUTTON);
}

void ge_original_input_read_bond_frame(
    uint16_t player_previous_buttons,
    GeOriginalBondInputFrame *frame)
{
    frame->stick_x = joyGetStickX(GE_MOVE_CONTROLLER);
    frame->stick_y = joyGetStickY(GE_MOVE_CONTROLLER);
    frame->buttons = joyGetButtons(GE_MOVE_CONTROLLER, ANY_BUTTON);
    frame->oldbuttons = player_previous_buttons;
}

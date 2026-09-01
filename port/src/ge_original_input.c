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
static const GeOriginalRamromReplay *g_ramrom_replay;
static const GeOriginalRamromBlock *g_ramrom_block;
static uint32_t g_ramrom_controller_count;

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

static s32 ge_original_input_ramrom_playback(
    struct contsample *samples, s32 current)
{
    size_t sample_index;
    if (samples == NULL || g_ramrom_replay == NULL
            || g_ramrom_block == NULL) return current;
    for (sample_index = 0U;
            sample_index < g_ramrom_block->sample_count;
            ++sample_index) {
        size_t controller;
        current = (current + 1) % CONTSAMPLE_LEN;
        memset(&samples[current], 0, sizeof(samples[current]));
        for (controller = 0U;
                controller < g_ramrom_controller_count;
                ++controller) {
            GeOriginalRamromPad pad;
            if (!ge_original_ramrom_block_pad(
                    g_ramrom_replay, g_ramrom_block,
                    sample_index, controller, &pad)) continue;
            samples[current].pads[controller].stick_x = pad.stick_x;
            samples[current].pads[controller].stick_y = pad.stick_y;
            samples[current].pads[controller].button = pad.buttons;
        }
    }
    g_ramrom_block = NULL;
    return current;
}

void ge_original_input_init(void)
{
    memset(&g_pending_sample, 0, sizeof(g_pending_sample));
    g_ramrom_replay = NULL;
    g_ramrom_block = NULL;
    g_ramrom_controller_count = 0U;
    joySetPlaybackFunc(
        ge_original_input_playback,
        GE_VIRTUAL_CONTROLLER_COUNT);
    joySetContDataIndex(1);

    /* Flush any held state when a portable runtime is restarted in-process. */
    joyConsumeSamplesWrapper();
    joyConsumeSamplesWrapper();
}

int ge_original_input_ramrom_bind(uint32_t controller_count)
{
    if (controller_count == 0U
            || controller_count > GE_ORIGINAL_RAMROM_MAX_CONTROLLERS)
        return 0;
    g_ramrom_replay = NULL;
    g_ramrom_block = NULL;
    g_ramrom_controller_count = controller_count;
    joySetPlaybackFunc(
        ge_original_input_ramrom_playback, (s32)controller_count);
    joySetContDataIndex(1);
    joyConsumeSamplesWrapper();
    return 1;
}

int ge_original_input_ramrom_queue(
    const GeOriginalRamromReplay *replay,
    const GeOriginalRamromBlock *block)
{
    if (replay == NULL || block == NULL || block->sample_bytes == NULL
            || replay->header.controller_count
                != g_ramrom_controller_count
            || block->sample_count == 0U) return 0;
    g_ramrom_replay = replay;
    g_ramrom_block = block;
    return 1;
}

void ge_original_input_ramrom_unbind(void)
{
    ge_original_input_init();
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

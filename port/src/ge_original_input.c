#include "ge_original_input.h"

#include <math.h>
#include <string.h>

#include <ultra64.h>
#include "joy.h"

#define GE_PRIMARY_CONTROLLER ((int)g_input_player)
#define GE_SECONDARY_CONTROLLER ((int)(g_input_player + g_input_count))
#define GE_VIRTUAL_CONTROLLER_COUNT 2

static GeOriginalInputSample g_pending_samples[4];
static GeOriginalInputPad g_pending_pads[4][2];
static int g_raw_pads;
static uint32_t g_input_count = 1;
static uint32_t g_input_player;
static int g_dual_analog;
static float g_stick_limit = 60.0f;

/* The original 2.x styles read walk/strafe from controller
 * (playernum + playerCount) and turn/pitch from the player's own controller,
 * the reverse of where the 1.x styles read the single stick. */
static int ge_original_input_move_controller(void)
{
    return g_dual_analog ? GE_SECONDARY_CONTROLLER : GE_PRIMARY_CONTROLLER;
}

static int ge_original_input_look_controller(void)
{
    return g_dual_analog ? GE_PRIMARY_CONTROLLER : GE_SECONDARY_CONTROLLER;
}
static const GeOriginalRamromReplay *g_ramrom_replay;
static const GeOriginalRamromBlock *g_ramrom_block;
static uint32_t g_ramrom_controller_count;
/* Optional metadata sink for controller-only links. The full ARM build
 * explicitly retains the strong rules owner and its canonical replay state. */
__attribute__((weak, noinline)) void ge_original_multiplayer_bind_intro_slot(
    int playback, int recording, uint32_t slot)
{
    (void)playback; (void)recording; (void)slot;
}

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
    return (s8)lroundf(value * g_stick_limit);
}

void ge_original_input_set_full_stick_range(int enabled)
{
    g_stick_limit = enabled ? 80.0f : 60.0f;
}

static s32 ge_original_input_playback(struct contsample *samples, s32 current)
{
    s32 next = (current + 1) % CONTSAMPLE_LEN;

    uint32_t p;
    memset(&samples[next], 0, sizeof(samples[next]));
    for (p = 0; p < g_input_count; ++p) {
        if (g_raw_pads) {
            unsigned pad;
            for (pad = 0; pad < 2; ++pad) {
                const GeOriginalInputPad *raw = &g_pending_pads[p][pad];
                samples[next].pads[p + pad * g_input_count].button = raw->buttons;
                samples[next].pads[p + pad * g_input_count].stick_x = raw->stick_x;
                samples[next].pads[p + pad * g_input_count].stick_y = raw->stick_y;
            }
            continue;
        }
        const GeOriginalInputSample *sample = &g_pending_samples[p];
        const int move = (int)(g_dual_analog ? p + g_input_count : p);
        const int look = (int)(g_dual_analog ? p : p + g_input_count);
        samples[next].pads[move].stick_x = ge_original_input_stick(sample->move_x);
        samples[next].pads[move].stick_y = ge_original_input_stick(sample->move_y);
        samples[next].pads[look].stick_x = ge_original_input_stick(sample->look_x);
        samples[next].pads[look].stick_y = ge_original_input_stick(sample->look_y);
        samples[next].pads[p].button = sample->buttons;
        samples[next].pads[p + g_input_count].button = sample->buttons_secondary;
    }
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
    ge_original_multiplayer_bind_intro_slot(0, 0, 0);
    memset(g_pending_samples, 0, sizeof(g_pending_samples));
    memset(g_pending_pads, 0, sizeof(g_pending_pads));
    g_raw_pads = 0;
    g_input_count = 1;
    g_input_player = 0;
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
    ge_original_multiplayer_bind_intro_slot(1, 0, replay->header.slot_number);
    return 1;
}

void ge_original_input_ramrom_unbind(void)
{
    ge_original_input_init();
}

void ge_original_input_tick(const GeOriginalInputSample *sample)
{
    if (sample != NULL && g_input_count == 1) {
        g_raw_pads = 0;
        g_pending_samples[0] = *sample;
        joyConsumeSamplesWrapper();
    }
}

int ge_original_input_configure_players(uint32_t count)
{
    if (count < 1 || count > 4 || count * 2 > JOY_MAX_CONTROLLERS) return 0;
    ge_original_input_init();
    g_input_count = count;
    joySetPlaybackFunc(ge_original_input_playback, (s32)(count * 2));
    /* Two neutral samples prevent held edges from an earlier roster. */
    joyConsumeSamplesWrapper();
    joyConsumeSamplesWrapper();
    return 1;
}

int ge_original_input_tick_players(const GeOriginalInputSample *samples,
                                  uint32_t count)
{
    if (samples == NULL || count != g_input_count || g_ramrom_controller_count)
        return 0;
    memcpy(g_pending_samples, samples, count * sizeof(*samples));
    g_raw_pads = 0;
    joyConsumeSamplesWrapper();
    return 1;
}

uint32_t ge_original_input_configured_player_count(void)
{
    return g_input_count;
}

uint32_t ge_original_input_selected_player(void)
{
    return g_input_player;
}

int ge_original_input_sample_pads(const GeOriginalInputSample *sample,
                                 GeOriginalInputPad pads[2])
{
    unsigned move = g_dual_analog ? 1 : 0;
    unsigned look = 1 - move;
    if (sample == NULL || pads == NULL) return 0;
    pads[0].buttons = sample->buttons;
    pads[1].buttons = sample->buttons_secondary;
    pads[move].stick_x = ge_original_input_stick(sample->move_x);
    pads[move].stick_y = ge_original_input_stick(sample->move_y);
    pads[look].stick_x = ge_original_input_stick(sample->look_x);
    pads[look].stick_y = ge_original_input_stick(sample->look_y);
    return 1;
}

int ge_original_input_tick_pad_pairs(const GeOriginalInputPad (*pads)[2],
                                    uint32_t count)
{
    if (pads == NULL || count != g_input_count || g_ramrom_controller_count)
        return 0;
    memcpy(g_pending_pads, pads, count * sizeof(*pads));
    g_raw_pads = 1;
    joyConsumeSamplesWrapper();
    return 1;
}

int ge_original_input_select_player(uint32_t slot)
{
    if (slot >= g_input_count) return 0;
    g_input_player = slot;
    return 1;
}

void ge_original_input_set_dual_analog(int enabled)
{
    g_dual_analog = enabled != 0;
}

int ge_original_input_dual_analog(void)
{
    return g_dual_analog;
}

float ge_original_input_move_x(void)
{
    return joyGetStickXInRangef(
        ge_original_input_move_controller(), -1.0f, 1.0f);
}

float ge_original_input_move_y(void)
{
    return joyGetStickYInRangef(
        ge_original_input_move_controller(), -1.0f, 1.0f);
}

float ge_original_input_look_x(void)
{
    return joyGetStickXInRangef(
        ge_original_input_look_controller(), -1.0f, 1.0f);
}

float ge_original_input_look_y(void)
{
    return joyGetStickYInRangef(
        ge_original_input_look_controller(), -1.0f, 1.0f);
}

uint16_t ge_original_input_buttons(void)
{
    return joyGetButtons(GE_PRIMARY_CONTROLLER, ANY_BUTTON);
}

uint16_t ge_original_input_pressed(void)
{
    return joyGetButtonsPressedThisFrame(GE_PRIMARY_CONTROLLER, ANY_BUTTON);
}

/* MoveBond's raw arguments are always the player's own controller; in the
 * 2.x styles that stick is turn/pitch and the body fetches walk/strafe from
 * controller (playernum + playerCount) itself. */
void ge_original_input_read_bond_frame(
    uint16_t player_previous_buttons,
    GeOriginalBondInputFrame *frame)
{
    frame->stick_x = joyGetStickX(GE_PRIMARY_CONTROLLER);
    frame->stick_y = joyGetStickY(GE_PRIMARY_CONTROLLER);
    frame->buttons = joyGetButtons(GE_PRIMARY_CONTROLLER, ANY_BUTTON);
    frame->oldbuttons = player_previous_buttons;
}

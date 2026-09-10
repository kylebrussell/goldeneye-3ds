#ifndef GE_ORIGINAL_INPUT_H
#define GE_ORIGINAL_INPUT_H

#include <stdint.h>

#include "ge_original_ramrom_replay.h"

typedef struct GeOriginalInputSample {
    float move_x;
    float move_y;
    float look_x;
    float look_y;
    uint16_t buttons;
    /* Buttons presented on the second virtual controller. Only the original
     * two-controller styles (2.1-2.4) read it; Z there is the aim trigger. */
    uint16_t buttons_secondary;
} GeOriginalInputSample;

typedef struct GeOriginalBondInputFrame {
    int8_t stick_x;
    int8_t stick_y;
    uint16_t buttons;
    uint16_t oldbuttons;
} GeOriginalBondInputFrame;
typedef struct GeOriginalInputPad {
    uint16_t buttons;
    int8_t stick_x;
    int8_t stick_y;
} GeOriginalInputPad;

/* A whole deterministic simulation tick is published at once. Controller p
 * belongs to slot p; controller p + count is its second pad. */
int ge_original_input_configure_players(uint32_t count);
int ge_original_input_tick_players(const GeOriginalInputSample *samples,
                                  uint32_t count);
int ge_original_input_select_player(uint32_t slot);
/* Network/replay input is already quantized. Never rescale received pads or
 * run receiver-local sensitivity settings over another player's sample. */
int ge_original_input_tick_pad_pairs(const GeOriginalInputPad (*pads)[2],
                                    uint32_t count);
int ge_original_input_sample_pads(const GeOriginalInputSample *sample,
                                 GeOriginalInputPad pads[2]);

/* Initializes the original joy.c playback path as two virtual controllers. */
void ge_original_input_init(void);

/* Queues and consumes one sample through joy.c's original 20-entry ring. */
void ge_original_input_tick(const GeOriginalInputSample *sample);

/* Selects the controller layout presented to the original control bodies.
 * Single stick (default): controller 0 carries the move stick and every
 * button, controller 1 carries the look stick, as GoldenEye's 1.x styles
 * expect. Dual analog: the original two-controller styles (2.1-2.4) read
 * turn/pitch from controller 0's stick and walk/strafe from controller 1's,
 * so the look stick is presented on controller 0 and the move stick on
 * controller 1, with buttons_secondary on controller 1. The move/look
 * accessors below always report by role, never by controller index. */
void ge_original_input_set_dual_analog(int enabled);
int ge_original_input_dual_analog(void);

/* Human gameplay needs the original +/-80 stick travel: manual aim begins
 * turning the camera beyond +/-60. Legacy scripted routes/frontends retain
 * their +/-60 calibration when disabled. RAMROM raw samples bypass this. */
void ge_original_input_set_full_stick_range(int enabled);

float ge_original_input_move_x(void);
float ge_original_input_move_y(void);
float ge_original_input_look_x(void);
float ge_original_input_look_y(void);
uint16_t ge_original_input_buttons(void);
uint16_t ge_original_input_pressed(void);

/* Reads MoveBond's raw arguments through canonical joy.c accessors. */
void ge_original_input_read_bond_frame(
    uint16_t player_previous_buttons,
    GeOriginalBondInputFrame *frame);

/* Installs the authored RAMROM controller-count/playback callback at joy.c's
 * original service boundary. Queue exactly one validated block before each
 * canonical gameplay tick; joyConsumeSamplesWrapper then preserves every
 * recorded sample and button edge in that block. */
int ge_original_input_ramrom_bind(uint32_t controller_count);
int ge_original_input_ramrom_queue(
    const GeOriginalRamromReplay *replay,
    const GeOriginalRamromBlock *block);
void ge_original_input_ramrom_unbind(void);

#endif

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
} GeOriginalInputSample;

typedef struct GeOriginalBondInputFrame {
    int8_t stick_x;
    int8_t stick_y;
    uint16_t buttons;
    uint16_t oldbuttons;
} GeOriginalBondInputFrame;

/* Initializes the original joy.c playback path as two virtual controllers. */
void ge_original_input_init(void);

/* Queues and consumes one sample through joy.c's original 20-entry ring. */
void ge_original_input_tick(const GeOriginalInputSample *sample);

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

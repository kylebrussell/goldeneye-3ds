#ifndef GE_ORIGINAL_INPUT_STATE_H
#define GE_ORIGINAL_INPUT_STATE_H

#include <stdint.h>

#define GE_ORIGINAL_INPUT_STATE_PLAYERS 4
#define GE_ORIGINAL_JOY_STATE_BANKS 2
#define GE_ORIGINAL_JOY_STATE_SAMPLES 20
#define GE_ORIGINAL_JOY_STATE_CONTROLLERS 8

enum {
    GE_ORIGINAL_INPUT_PLAYBACK_NONE = 0,
    GE_ORIGINAL_INPUT_PLAYBACK_PENDING = 1,
    GE_ORIGINAL_INPUT_PLAYBACK_RAMROM = 2,
    GE_ORIGINAL_INPUT_PLAYBACK_UNKNOWN = 255
};

typedef struct GeOriginalInputState {
    float move_x[4], move_y[4], look_x[4], look_y[4];
    uint16_t buttons[4], secondary_buttons[4];
    uint16_t raw_buttons[4][2];
    int8_t raw_x[4][2], raw_y[4][2];
    int32_t raw_pads, dual_analog;
    uint32_t player_count, selected_player;
    float stick_limit;
    uint32_t ramrom_controller_count;
    uint8_t playback_kind, ramrom_replay_present, ramrom_block_present;
} GeOriginalInputState;

typedef struct GeOriginalJoyState {
    uint16_t buttons[2][20][8];
    int8_t stick_x[2][20][8], stick_y[2][20][8];
    uint8_t errors[2][20][8];
    int32_t curlast[2], curstart[2], nextlast[2], nextsecondlast[2], playback_count[2];
    uint16_t pressed[2][8];
    int32_t selected_bank;
    uint8_t connected_controllers, controller_states, playback_present, record_present;
} GeOriginalJoyState;

/* Read only, including all inactive slots and ring storage in physical order.
 * Callback kind is validated by its owning implementation against the actual
 * registered function pointer. Unknown callbacks never acquire a guessed ID. */
int ge_original_input_state_snapshot(GeOriginalInputState *state);
int ge_original_joy_state_snapshot(GeOriginalJoyState *state);

#endif

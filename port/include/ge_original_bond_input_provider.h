#ifndef GE_ORIGINAL_BOND_INPUT_PROVIDER_H
#define GE_ORIGINAL_BOND_INPUT_PROVIDER_H

#include <stdint.h>
#include <stddef.h>

struct player;
struct player_data;
struct PropRecord;

/* External state read by the canonical bondviewProcessInput body. */
typedef struct GeOriginalBondInputProvider {
    struct player *current_player;
    struct player *player_pointers[4];
    struct player_data *player_permissions;
    int32_t player_count;
    int32_t player_number;
    int32_t controls_locked;
    int32_t player_actions_enabled;
    int32_t control_type;
    uint32_t aim_control;
    uint32_t look_vertical_inverted;
    int32_t stop_play_flag;
    int32_t game_over_flag;
    int32_t force_disarm;
    int32_t player_is_in_tank;
    struct PropRecord *player_tank_prop;
    float player_tank_y_offset;
    int32_t bond_can_enter_tank;
    int32_t clock_timer;
    int32_t global_timer;
    float global_timer_delta;
    float fov_y;
    int32_t speedgraph_frames;
} GeOriginalBondInputProvider;

/* Selects the original normal 1.1/Honey single-player Dam branches. */
void ge_original_bond_input_provider_reset_normal_dam(void);
GeOriginalBondInputProvider *ge_original_bond_input_provider(void);
void ge_original_bond_input_bind_player(
    struct player *player, struct player_data *permissions);
void ge_original_bond_input_set_fov_y(float fov_y);

#endif

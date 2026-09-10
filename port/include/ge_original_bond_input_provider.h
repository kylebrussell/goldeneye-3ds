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

/* Selects the original normal single-player Dam branches. The control style
 * is the one last chosen with ge_original_bond_input_set_control_style
 * (1.1/Honey until changed); like the original save-file option it survives
 * stage resets. */
void ge_original_bond_input_provider_reset_normal_dam(void);
/* CONTROLLER_CONFIG_* value read by the canonical bondviewProcessInput body
 * for every following reset, applied immediately as well. */
void ge_original_bond_input_set_control_style(int32_t control_type);
int32_t ge_original_bond_input_control_style(void);
GeOriginalBondInputProvider *ge_original_bond_input_provider(void);
void ge_original_bond_input_bind_player(
    struct player *player, struct player_data *permissions);
void ge_original_bond_input_set_fov_y(float fov_y);

/* Validates the entire roster before publication. Canonical set_cur_player
 * may then switch players (including temporary damage/score attribution).
 * The provider accessor follows that pointer and retains per-player options. */
int ge_original_bond_input_bind_roster(struct player *const *players,
    struct player_data *const *permissions, uint32_t count);
void ge_original_bond_input_unbind_roster(void);
void ge_original_bond_input_set_timers(int32_t clock_timer,
    int32_t global_timer, float global_timer_delta);

#endif

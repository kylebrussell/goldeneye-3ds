#include "ge_original_bond_input_provider.h"

#include <assert.h>

int main(void)
{
    GeOriginalBondInputProvider *provider;
    struct player *marker = (struct player *)(uintptr_t)0x1234;

    provider = ge_original_bond_input_provider();
    provider->current_player = marker;
    provider->player_count = 4;
    provider->controls_locked = 1;
    provider->player_is_in_tank = 1;

    ge_original_bond_input_provider_reset_normal_dam();
    provider = ge_original_bond_input_provider();
    assert(provider->current_player == marker);
    assert(provider->player_pointers[0] == marker);
    assert(provider->player_count == 1);
    assert(provider->player_number == 0);
    assert(provider->controls_locked == 0);
    assert(provider->player_actions_enabled == 1);
    assert(provider->control_type == 0);
    assert(provider->aim_control == 0);
    assert(provider->look_vertical_inverted == 0);
    assert(provider->stop_play_flag == 0);
    assert(provider->game_over_flag == 0);
    assert(provider->force_disarm == 0);
    assert(provider->player_is_in_tank == 0);
    assert(provider->player_tank_prop == 0);
    assert(provider->player_tank_y_offset == 0.0f);
    assert(provider->bond_can_enter_tank == 0);
    /* Runtime frame timing must be supplied by the original level clock. */
    assert(provider->clock_timer == 0);
    assert(provider->global_timer == 0);
    assert(provider->global_timer_delta == 0.0f);
    assert(provider->fov_y == 60.0f);
    assert(provider->speedgraph_frames == 1);
    return 0;
}

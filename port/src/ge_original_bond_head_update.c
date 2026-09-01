#include "ge_original_bond_head_update.h"
#include "ge_original_player_spawn_internal.h"

#include <ultra64.h>
#include <bondconstants.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"

void bheadUpdate(f32 percent_speed, f32 speedsideways);
f32 bheadGetBreathingValue(void);

float ge_original_bond_head_breathing_provider(void)
{
    return ge_original_spawn_player_get()->bondbreathing;
}

int ge_original_bond_head_update_tick(float percent_speed,
                                      float speed_sideways)
{
    if (ge_original_spawn_player_get() == 0) {
        return 0;
    }

    bheadUpdate(percent_speed, speed_sideways);
    return 1;
}

float ge_original_bond_head_breathing_value(void)
{
    if (ge_original_spawn_player_get() == 0) {
        return 0.0f;
    }

    return bheadGetBreathingValue();
}

#include <assert.h>
#include <math.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/gun.h"
#include "ge_original_bond_input_provider.h"

int main(void)
{
    struct player player;
    memset(&player, 0, sizeof(player));
    ge_original_bond_input_provider_reset_normal_dam();
    ge_original_bond_input_bind_player(&player, NULL);
    gunSetOffsetRelated((float)M_PI);
    assert(fabsf(player.hands[GUNRIGHT].gunofs2_z - 10.0f) < 0.0001f);
    assert(fabsf(player.hands[GUNLEFT].gunofs2_z - 10.0f) < 0.0001f);
    return 0;
}

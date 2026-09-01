#include "ge_original_bond_input_provider.h"

#include <assert.h>
#include <math.h>
#include <string.h>

typedef int PLAYERFLAG;
#include "game/bondview.h"

void bondviewUpdateSpeedSideways(s32 direction);
void bondviewUpdateSpeedForwards(s32 direction);
void bondviewCurrentPlayerUpdateSpeedVerta(f32 value);
void bondviewCurrentPlayerUpdateSpeedTheta(f32 value);

static void expect_near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.000001f);
}

int main(void)
{
    struct player player;
    GeOriginalBondInputProvider *provider;
    int i;

    memset(&player, 0, sizeof(player));
    provider = ge_original_bond_input_provider();
    provider->current_player = &player;
    ge_original_bond_input_provider_reset_normal_dam();
    provider->global_timer_delta = 0.25f;

    bondviewUpdateSpeedSideways(-1);
    expect_near(player.speedstrafe, -0.25f);
    expect_near(player.speedsideways, -0.25f);
    bondviewUpdateSpeedSideways(1);
    expect_near(player.speedsideways, 0.0f);
    for (i = 0; i < 5; i++) {
        bondviewUpdateSpeedSideways(1);
    }
    expect_near(player.speedsideways, 1.0f);

    bondviewUpdateSpeedForwards(1);
    expect_near(player.speedforwards, 0.25f);
    for (i = 0; i < 4; i++) {
        bondviewUpdateSpeedForwards(1);
    }
    expect_near(player.speedforwards, 1.0f);
    bondviewUpdateSpeedForwards(0);
    expect_near(player.speedforwards, 0.75f);

    bondviewCurrentPlayerUpdateSpeedVerta(1.0f);
    expect_near(player.speedverta, -0.003125f);
    bondviewCurrentPlayerUpdateSpeedVerta(0.0f);
    expect_near(player.speedverta, 0.0f);

    bondviewCurrentPlayerUpdateSpeedTheta(1.0f);
    expect_near(player.speedtheta, -0.003125f);
    bondviewCurrentPlayerUpdateSpeedTheta(0.0f);
    expect_near(player.speedtheta, 0.0f);
    bondviewCurrentPlayerUpdateSpeedTheta(-1.0f);
    expect_near(player.speedtheta, 0.003125f);

    return 0;
}

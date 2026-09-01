#include "ge_original_bond_input_provider.h"

#include <assert.h>
#include <math.h>
#include <string.h>

typedef int PLAYERFLAG;
#include "game/bondview.h"

void bondviewTriggerWatchZoom(f32 zoominfovy);
void bondviewUpdateWatchZoomIn(void);

/* Canonical bondview.c storage. The production input-state slice owns this
 * definition; this focused watch-helper link intentionally excludes that
 * larger object and therefore supplies the same authored initializer. */
f32 watch_transition_time = 0.90909088f;

static void expect_near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.00001f);
}

int main(void)
{
    struct player player;
    GeOriginalBondInputProvider *provider;

    memset(&player, 0, sizeof(player));
    provider = ge_original_bond_input_provider();
    provider->current_player = &player;
    ge_original_bond_input_provider_reset_normal_dam();
    expect_near(watch_transition_time, 0.90909088f);

    player.zoominfovy = 60.0f;
    player.watch_animation_state = WATCH_ANIMATION_0x0;
    bondviewTriggerWatchZoom(30.0f);
    expect_near(player.zoomintime, 0.0f);
    expect_near(player.zoomintimemax, 15.0f);
    expect_near(player.zoominfovyold, 60.0f);
    expect_near(player.zoominfovynew, 30.0f);

    bondviewUpdateWatchZoomIn();
    expect_near(player.zoomintime, 0.90909088f);
    expect_near(player.zoominfovy, 58.181816f);
    expect_near(player.fovy, player.zoominfovy);
    expect_near(provider->fov_y, player.zoominfovy);

    player.watch_animation_state = WATCH_ANIMATION_0x5;
    bondviewUpdateWatchZoomIn();
    expect_near(player.zoomintime, 1.90909088f);
    return 0;
}

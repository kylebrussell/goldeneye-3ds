#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chraction.h"

stagesetup g_CurrentSetup;
s32 g_SeenBondRecentlyGuardCount;

static s32 walk_calls;
static s32 route_calls;
static coord3d routed_position;
static StandTile *routed_stan;
static SPEED routed_speed;

s32 walkTilesBetweenPoints_NoCallback(StandTile **stan, f32 start_x,
        f32 start_z, f32 dest_x, f32 dest_z)
{
    (void)stan;
    assert(start_x == 100.0f && start_z == 300.0f);
    assert(dest_x == 122.0f && dest_z == 300.0f);
    walk_calls++;
    return TRUE;
}

s32 plot_course_for_actor(ChrRecord *self, coord3d *position,
        StandTile *stan, SPEED speed)
{
    (void)self;
    route_calls++;
    routed_position = *position;
    routed_stan = stan;
    routed_speed = speed;
    return TRUE;
}

int main(void)
{
    ChrRecord guard;
    PadRecord pads[7];
    StandTile tiles[2];

    memset(&guard, 0, sizeof(guard));
    memset(pads, 0, sizeof(pads));
    memset(tiles, 0, sizeof(tiles));
    guard.actiontype = ACT_STAND;
    guard.chrwidth = 20.0f;
    guard.padpreset1 = 6;
    g_CurrentSetup.pads = pads;

    assert(chrResolvePadId(&guard, PAD_PRESET1) == 6);
    assert(chrResolvePadId(&guard, 4) == 4);

    pads[4].stan = &tiles[0];
    pads[4].pos.x = 100.0f;
    pads[4].pos.y = 200.0f;
    pads[4].pos.z = 300.0f;
    pads[4].up.x = 1.0f;
    pads[4].up.y = 0.0f;
    assert(chrGoToPad(&guard, 4, SPEED_WALK));
    assert(walk_calls == 1 && route_calls == 1);
    assert(routed_position.x == 122.0f && routed_position.y == 200.0f
            && routed_position.z == 300.0f);
    assert(routed_stan == &tiles[0] && routed_speed == SPEED_WALK);

    pads[5].stan = &tiles[1];
    pads[5].pos.x = 400.0f;
    pads[5].pos.y = 500.0f;
    pads[5].pos.z = 600.0f;
    pads[5].up.y = 1.0f;
    assert(chrGoToPad(&guard, 5, SPEED_RUN));
    assert(walk_calls == 1 && route_calls == 2);
    assert(routed_position.x == 400.0f && routed_position.y == 500.0f
            && routed_position.z == 600.0f);
    assert(routed_stan == &tiles[1] && routed_speed == SPEED_RUN);

    g_SeenBondRecentlyGuardCount = 10;
    assert(!chrGoToPad(&guard, 5, SPEED_RUN));
    assert(route_calls == 2);

    puts("exact chrResolvePadId/chrGoToPad: preset, horizontal-pad STAN "
         "walk, vertical-pad route, and guard-count gate retained");
    return 0;
}

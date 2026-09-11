#include "ge_original_bond_live.h"
#include "ge_original_player_spawn_internal.h"
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct player fixture;
static struct player *selected;
struct player *ge_original_spawn_player_get(void) { return selected; }

int main(void)
{
    StandTile collision = {0}, portal = {0}, camera = {0};
    uint8_t room = 99;
    float reference[3] = {0};
    assert(!ge_original_bond_live_visibility_reference(&room, reference));
    selected = &fixture;
    assert(!ge_original_bond_live_visibility_reference(&room, reference));
    collision.room = 112;
    portal.room = 111;
    camera.room = 132;
    fixture.field_488.current_tile_ptr = &collision;
    fixture.field_488.current_tile_ptr_for_portals = &portal;
    fixture.cameratile = &camera;
    fixture.field_488.pos3 = (coord3d){{1, 2, 3}};
    fixture.pos3 = (coord3d){{4, 5, 6}};
    assert(ge_original_bond_live_visibility_reference(&room, reference));
    assert(room == 111);
    assert(memcmp(reference, fixture.field_488.pos3.f, sizeof(reference)) == 0);
    fixture.cameramode = 1;
    assert(ge_original_bond_live_visibility_reference(&room, reference));
    assert(room == 132);
    assert(memcmp(reference, fixture.pos3.f, sizeof(reference)) == 0);
    /* The original room getter falls back independently of the pos3 getter. */
    fixture.cameratile = NULL;
    assert(ge_original_bond_live_visibility_reference(&room, reference));
    assert(room == 111);
    assert(memcmp(reference, fixture.pos3.f, sizeof(reference)) == 0);
    assert(!ge_original_bond_live_visibility_reference(NULL, reference));
    assert(!ge_original_bond_live_visibility_reference(&room, NULL));
    puts("Live camera portal-room/floor reference selection passed");
    return 0;
}

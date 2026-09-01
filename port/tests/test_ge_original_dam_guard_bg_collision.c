#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ultra64.h>
#include <PR/gbi.h>
#include <bondtypes.h>
#include "bg.h"
#include "chrai.h"
#include "stan.h"

char list_visible_rooms_in_cur_global_vis_packet[0x98];
s32 num_visible_rooms_in_cur_global_vis_packet;
f32 room_data_float1 = 1.0f;
f32 room_data_float2 = 1.0f;
s_room_info g_BgRoomInfo[MAXROOMCOUNT];
bg_portal_data_entry *g_BgPortals;

StandTile *firststaninroom[139];
StanRoomBounds g_StanRoomBounds[139];
s32 dword_CODE_bss_8007B9DC;
f32 level_scale = 1.0f;
u8 list_of_tilesizes[] = {
    0x20, 0x20, 0x20, 0x20, 0x28, 0x30,
    0x38, 0x40, 0x48, 0x50, 0x58, 0x00
};

s32 g_OnScreenPropCount;
PropRecord *g_OnScreenPropList[ONSCREEN_PROP_LIST_LEN];
PropRecord **g_LastOnScreenProp = g_OnScreenPropList;
static PropRecord *g_test_active_tail;

static s32 g_intersection_calls;
static Gfx g_hit_gdl[1];

PropRecord *chrpropGetActiveTail(void)
{
    return g_test_active_tail;
}

s32 sub_GAME_7F0B9F14(s32 portalnum, coord3d *from, coord3d *to)
{
    (void)from;
    (void)to;
    return portalnum < 2 ? 1 : 0;
}

bool bgTestRayIntersectsBbox(coord3d *origin, coord3d *dir,
    s32 *bbox_min, s32 *bbox_max)
{
    (void)origin;
    (void)dir;
    (void)bbox_min;
    (void)bbox_max;
    return TRUE;
}

bool bgTestRayIntersectionInRoom(coord3d *from, coord3d *to, coord3d *dir,
    RoomVtxBatchBounds *point, s32 roomnum, struct HitThing *hit)
{
    (void)to;
    (void)dir;
    (void)point;
    (void)roomnum;
    ++g_intersection_calls;
    memset(hit, 0, offsetof(HitThing, tileformat));
    hit->hitpos.x = from->x + 3.0f;
    hit->hitpos.y = from->y + 4.0f;
    hit->hitpos.z = from->z;
    hit->normal.y = 1.0f;
    hit->texturenum = 17;
    hit->tricmd = &g_hit_gdl[0];
    return TRUE;
}

bool check_if_imageID_is_light(s32 image_id)
{
    (void)image_id;
    return FALSE;
}

f32 getShortest2dDispToInfTripleEdge(StandTile *tile, s32 edge,
    f32 x, f32 z)
{
    (void)tile;
    (void)edge;
    (void)x;
    (void)z;
    return 1.0f;
}

bool stanTileHasZeroArea(StandTile *tile)
{
    (void)tile;
    return FALSE;
}

void getTileMidPoint(StandTile *tile, coord3d *out)
{
    (void)tile;
    out->x = 1.0f;
    out->y = 0.0f;
    out->z = 1.0f;
}

s32 walkTilesBetweenPoints_NoCallback(StandTile **tile, f32 start_x,
    f32 start_z, f32 destination_x, f32 destination_z)
{
    (void)tile;
    (void)start_x;
    (void)start_z;
    (void)destination_x;
    (void)destination_z;
    return TRUE;
}

f32 stanGetPositionYValue(StandTile *tile, f32 x, f32 z)
{
    (void)tile;
    (void)x;
    (void)z;
    return 10.0f;
}

static void test_room_segment_and_visibility(void)
{
    bg_portal_data_entry portals[3];
    coord3d from = {.x = 0.0f, .y = 0.0f, .z = 0.0f};
    coord3d to = {.x = 100.0f, .y = 0.0f, .z = 0.0f};
    u8 initial[8] = {1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    u8 final_rooms[8] = {0};
    s32 traversed[20] = {0};
    s32 traversed_count = -1;
    s32 copied[4] = {-1, -1, -1, -1};

    memset(portals, 0, sizeof(portals));
    portals[0].offset_portal = (bg_portal_entry *)(uintptr_t)1;
    portals[0].connectedRoom1 = 1;
    portals[0].connectedRoom2 = 2;
    portals[1].offset_portal = (bg_portal_entry *)(uintptr_t)1;
    portals[1].connectedRoom1 = 2;
    portals[1].connectedRoom2 = 3;
    g_BgPortals = portals;

    bgFindRoomsAlongSegment(&from, &to, initial, final_rooms, traversed,
        &traversed_count, 20);
    assert(final_rooms[0] == 3 && final_rooms[1] == 0xff);
    assert(traversed_count == 3);
    assert(traversed[0] == 1 && traversed[1] == 2 && traversed[2] == 3);

    num_visible_rooms_in_cur_global_vis_packet = 3;
    list_visible_rooms_in_cur_global_vis_packet[0] = 4;
    list_visible_rooms_in_cur_global_vis_packet[1] = 7;
    list_visible_rooms_in_cur_global_vis_packet[2] = 9;
    assert(bgCopyVisibleRoomsToList(copied, 2) == 2);
    assert(copied[0] == 4 && copied[1] == 7 && copied[2] == -1);
}

static void test_background_hit_and_scale(void)
{
    RoomVtxBatchBounds bounds = {0};
    coord3d from = {.x = 2.0f, .y = 3.0f, .z = 4.0f};
    coord3d to = {.x = 8.0f, .y = 3.0f, .z = 4.0f};
    HitThing hit;

    memset(g_BgRoomInfo, 0, sizeof(g_BgRoomInfo));
    memset(&hit, 0, sizeof(hit));
    assert(!bgTestBulletHitBackground(&from, &to, 4, &hit));

    room_data_float1 = 2.0f;
    room_data_float2 = 0.5f;
    g_BgRoomInfo[4].vtx_batch_bounds = &bounds;
    g_BgRoomInfo[4].num_vtx_batch_bounds = 1;
    g_BgRoomInfo[4].ptr_expanded_mapping_info = &g_hit_gdl[0];
    memset(g_hit_gdl, 0, sizeof(g_hit_gdl));
    ((u8 *)&g_hit_gdl[0])[0] = G_SETTILE;
    g_intersection_calls = 0;
    assert(bgTestBulletHitBackground(&from, &to, 4, &hit));
    assert(g_intersection_calls == 1);
    assert(fabsf(hit.hitpos.x - 7.0f) < 0.001f);
    assert(fabsf(hit.hitpos.y - 10.0f) < 0.001f);
    assert(hit.tileformat == -1 && hit.tilesize == -1);
    assert(fabsf(get_room_data_float2() - 0.5f) < 0.001f);
}

static void test_tile_below_position(void)
{
    union {
        max_align_t align;
        u8 bytes[64];
    } storage;
    StandTile *tile = (StandTile *)storage.bytes;
    coord3d pos = {.x = 1.0f, .y = 20.0f, .z = 1.0f};
    u8 rooms[] = {1, 0xff};
    u8 other_rooms[] = {2, 0xff};
    f32 y = -1.0f;

    memset(&storage, 0, sizeof(storage));
    memset(firststaninroom, 0, sizeof(firststaninroom));
    memset(g_StanRoomBounds, 0, sizeof(g_StanRoomBounds));
    tile->id = 1;
    tile->room = 1;
    tile->tail.half = 0x3012;
    firststaninroom[1] = tile;
    g_StanRoomBounds[1].minX = -10;
    g_StanRoomBounds[1].minY = -10;
    g_StanRoomBounds[1].minZ = -10;
    g_StanRoomBounds[1].maxX = 10;
    g_StanRoomBounds[1].maxY = 30;
    g_StanRoomBounds[1].maxZ = 10;
    dword_CODE_bss_8007B9DC = 2;

    assert(stanFindTileBelowPos(&pos, rooms, &y) == tile);
    assert(fabsf(y - 10.0f) < 0.001f);
    assert(stanFindTileBelowPos(&pos, other_rooms, &y) == NULL);
    dword_CODE_bss_8007B9DC = 0;
    assert(stanFindTileBelowPos(&pos, NULL, &y) == NULL);
}

static void test_onscreen_prop_refresh(void)
{
    PropRecord props[3];
    size_t index;

    memset(props, 0, sizeof(props));
    for (index = 0; index < ONSCREEN_PROP_LIST_LEN; ++index) {
        g_OnScreenPropList[index] = &props[1];
    }
    props[0].flags = PROPFLAG_ENABLED | PROPFLAG_ONSCREEN;
    props[0].zDepth = 10.0f;
    props[1].flags = PROPFLAG_ONSCREEN;
    props[1].zDepth = 100.0f;
    props[2].flags = PROPFLAG_ENABLED | PROPFLAG_ONSCREEN;
    props[2].zDepth = 5.0f;
    props[2].prev = &props[1];
    props[1].prev = &props[0];
    props[0].prev = NULL;
    g_test_active_tail = &props[2];

    chraiUpdateOnscreenPropCount();
    assert(g_OnScreenPropCount == 2);
    assert(g_LastOnScreenProp == &g_OnScreenPropList[2]);
    assert(g_OnScreenPropList[0] == &props[0]);
    assert(g_OnScreenPropList[1] == &props[2]);
    assert(g_OnScreenPropList[2] == NULL);

    /* Removing and reusing a formerly visible record must remove its pointer
     * from the consumer-visible [begin, end) range on the next exact refresh. */
    props[0].flags = 0;
    props[2].prev = &props[1];
    props[1].prev = NULL;
    props[0].prev = (PropRecord *)(uintptr_t)0x1;
    chraiUpdateOnscreenPropCount();
    assert(g_OnScreenPropCount == 1);
    assert(g_LastOnScreenProp == &g_OnScreenPropList[1]);
    assert(g_OnScreenPropList[0] == &props[2]);
    assert(g_OnScreenPropList[1] == NULL);
    for (index = 0; index < (size_t)(g_LastOnScreenProp
            - g_OnScreenPropList); ++index) {
        assert(g_OnScreenPropList[index] != &props[0]);
    }

    props[2].flags = 0;
    chraiUpdateOnscreenPropCount();
    assert(g_OnScreenPropCount == 0);
    assert(g_LastOnScreenProp == g_OnScreenPropList);
    assert(g_OnScreenPropList[0] == NULL);
}

int main(void)
{
    test_room_segment_and_visibility();
    test_background_hit_and_scale();
    test_tile_below_position();
    test_onscreen_prop_refresh();
    return 0;
}

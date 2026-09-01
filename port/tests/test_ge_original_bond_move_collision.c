#include <assert.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/chrai.h"
#include "game/player.h"
#include "game/propobj.h"
#include "snd.h"
#include "ge_original_bond_input_provider.h"

static int close_enough(float actual, float expected)
{
    return fabsf(actual - expected) < 0.0001f;
}

static int captured_event_type;
static int captured_event_value;

void sndCreatePostEvent(ALSoundState *state, s16 event_type, s32 value)
{
    assert(state == (ALSoundState *)(uintptr_t)1);
    captured_event_type = event_type;
    captured_event_value = value;
}

int main(void)
{
    struct rect4f square;
    struct rect4f overlap;
    struct rect4f separate;
    struct coord3d point = {0};
    ModelRoData_BoundingBoxRecord bbox;
    PropRecord prop;
    StandTile stan;
    s32 rooms[8];
    float position;
    float speed;
    struct player player;
    PropRecord player_prop;
    coord3d sound_pos;

    memset(&square, 0, sizeof(square));
    memset(&overlap, 0, sizeof(overlap));
    memset(&separate, 0, sizeof(separate));
    square.points[0].x = -2.0f; square.points[0].y = -2.0f;
    square.points[1].x = -2.0f; square.points[1].y =  2.0f;
    square.points[2].x =  2.0f; square.points[2].y =  2.0f;
    square.points[3].x =  2.0f; square.points[3].y = -2.0f;
    overlap.points[0].x = 1.0f; overlap.points[0].y = -1.0f;
    overlap.points[1].x = 1.0f; overlap.points[1].y =  3.0f;
    overlap.points[2].x = 3.0f; overlap.points[2].y =  3.0f;
    overlap.points[3].x = 3.0f; overlap.points[3].y = -1.0f;
    separate.points[0].x = 5.0f; separate.points[0].y = 5.0f;
    separate.points[1].x = 5.0f; separate.points[1].y = 7.0f;
    separate.points[2].x = 7.0f; separate.points[2].y = 7.0f;
    separate.points[3].x = 7.0f; separate.points[3].y = 5.0f;

    assert(chrpropTestPointInPolygon(&point, &square, 4) == 1);
    point.x = 4.0f;
    assert(chrpropTestPointInPolygon(&point, &square, 4) == 0);
    assert(chrobjTestPolygonsTouchingOrOverlap2D(&square, 4, &overlap, 4) == 1);
    assert(chrobjTestPolygonsTouchingOrOverlap2D(&square, 4, &separate, 4) == 0);

    ge_original_bond_input_provider_reset_normal_dam();
    ge_original_bond_input_provider()->clock_timer = 3;
    position = 0.0f;
    speed = 0.0f;
    chrobjApplySpeed(&position, 1.0f, &speed, 0.1f, 0.1f, 0.25f);
    assert(close_enough(position, 0.55f));
    assert(close_enough(speed, 0.25f));

    memset(&prop, 0, sizeof(prop));
    memset(&stan, 0, sizeof(stan));
    prop.stan = &stan;
    prop.type = PROP_TYPE_VIEWER;
    stan.room = 23;
    chraiGetPropRoomIds(&prop, rooms);
    assert(rooms[0] == 23 && rooms[1] == -1);

    memset(&bbox, 0, sizeof(bbox));
    bbox.Bounds.ymin = -17.5f;
    assert(close_enough(chrpropBBOXGetYmin(&bbox), -17.5f));

    memset(&player, 0, sizeof(player));
    memset(&player_prop, 0, sizeof(player_prop));
    memset(&sound_pos, 0, sizeof(sound_pos));
    player.prop = &player_prop;
    g_playerPointers[0] = &player;
    g_playerPointers[1] = NULL;
    g_playerPointers[2] = NULL;
    g_playerPointers[3] = NULL;
    assert(getPlayerCount() == 1);
    chrobjSndCreatePostEventDefault((ALSoundState *)(uintptr_t)1, &sound_pos);
    assert(captured_event_type == 8);
    assert(captured_event_value == SHRT_MAX);

    return 0;
}

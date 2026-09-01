#include <assert.h>
#include <math.h>
#include <string.h>

#include <bondtypes.h>
#include "src/game/debugmenu_handler.h"
#include "src/game/lv.h"
#include "src/game/model.h"
#include "src/game/player.h"

extern void chrUpdateAimProperties(ChrRecord *chr);
extern void chrpropDelist(PropRecord *prop);

s32 g_ClockTimer;
f32 g_GlobalTimerDelta;
struct player *g_playerPointers[4];
extern PLAYER_ID array_PLAYER_IDs[4];
f32 g_ModelDistanceScale;
PropRecord *g_ActivePropsTail;
PropRecord *g_ActivePropsHead;

static const u32 shuffle_random[] = { 2U, 1U, 0U };
static size_t shuffle_random_index;

u32 randomGetNext(void)
{
    assert(shuffle_random_index
        < sizeof(shuffle_random) / sizeof(shuffle_random[0]));
    return shuffle_random[shuffle_random_index++];
}

static int close_enough(f32 first, f32 second)
{
    return fabsf(first - second) < 0.00001f;
}

static void test_aim_interpolation(void)
{
    ChrRecord chr;
    memset(&chr, 0, sizeof(chr));
    chr.aimendcount = 4;
    chr.aimendlshoulder = 2.0f;
    chr.aimendrshoulder = 4.0f;
    chr.aimendback = 6.0f;
    chr.aimendsideback = 8.0f;
    g_GlobalTimerDelta = 2.0f;
    g_ClockTimer = 1;
    chrUpdateAimProperties(&chr);
    assert(close_enough(chr.aimuplshoulder, 1.0f));
    assert(close_enough(chr.aimuprshoulder, 2.0f));
    assert(close_enough(chr.aimupback, 3.0f));
    assert(close_enough(chr.aimsideback, 4.0f));
    assert(chr.aimendcount == 3);

    chr.aimendcount = 1;
    chrUpdateAimProperties(&chr);
    assert(close_enough(chr.aimuplshoulder, 2.0f));
    assert(close_enough(chr.aimuprshoulder, 4.0f));
    assert(close_enough(chr.aimupback, 6.0f));
    assert(close_enough(chr.aimsideback, 8.0f));
}

static void test_player_shuffle(void)
{
    struct player players[4];
    memset(players, 0, sizeof(players));
    array_PLAYER_IDs[0] = PLAYER_3;
    array_PLAYER_IDs[1] = PLAYER_1;
    array_PLAYER_IDs[2] = PLAYER_4;
    array_PLAYER_IDs[3] = PLAYER_2;
    g_playerPointers[PLAYER_1] = &players[PLAYER_1];
    g_playerPointers[PLAYER_2] = NULL;
    g_playerPointers[PLAYER_3] = &players[PLAYER_3];
    g_playerPointers[PLAYER_4] = &players[PLAYER_4];
    assert(get_player_position_in_shuffled(PLAYER_3) == 0);
    assert(get_player_position_in_shuffled(PLAYER_1) == 1);
    assert(get_player_position_in_shuffled(PLAYER_4) == 2);
    assert(get_player_position_in_shuffled(PLAYER_2) == 3);

    shuffle_random_index = 0U;
    shuffle_player_ids();
    assert(shuffle_random_index == 3U);
    assert(array_PLAYER_IDs[0] == PLAYER_3);
    assert(array_PLAYER_IDs[1] == PLAYER_1);
    assert(array_PLAYER_IDs[2] == PLAYER_2);
    assert(array_PLAYER_IDs[3] == PLAYER_4);
}

static void test_active_list_delist(void)
{
    PropRecord props[3];
    memset(props, 0, sizeof(props));
    props[0].next = &props[1];
    props[1].prev = &props[0];
    props[1].next = &props[2];
    props[2].prev = &props[1];
    g_ActivePropsHead = &props[0];
    g_ActivePropsTail = &props[2];

    chrpropDelist(&props[1]);
    assert(props[0].next == &props[2]);
    assert(props[2].prev == &props[0]);
    assert(props[1].prev == NULL && props[1].next == NULL);
    chrpropDelist(&props[2]);
    assert(g_ActivePropsTail == &props[0]);
    chrpropDelist(&props[0]);
    assert(g_ActivePropsHead == NULL && g_ActivePropsTail == NULL);
}

int main(void)
{
    test_aim_interpolation();
    test_player_shuffle();
    test_active_list_delist();
    modelSetDistanceScale(0.3125f);
    assert(close_enough(g_ModelDistanceScale, 0.3125f));
    assert(get_debug_chrnum_flag() == 0);
    assert(get_debug_render_raster() == 2);
    return 0;
}

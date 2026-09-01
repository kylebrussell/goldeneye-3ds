#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondinv.h"
#include "game/bondview.h"
#include "game/gun.h"
#include "game/player.h"
#include "game/propobj.h"
#include "game/stan.h"
#include "ge_original_stage_pickup.h"
extern TICKOP ge_original_stage_obj_tick_player_exact(PropRecord *prop);

struct player *g_CurrentPlayer;
struct player_data *g_playerPerm;
bool g_PlayerInvincible;
s32 g_PlayerIsInTank;
s32 g_ClockTimer;
ALBank *g_musicSfxBufferPtr;
AmmoStats ammo_related[30];

static PropRecord *active_tail;
static PropRecord *player_prop;
static bool line_clear;
static unsigned free_calls;
static unsigned inventory_calls;
static TICKOP last_operation;
static s16 last_sound;
static char last_message[128];
static unsigned collectable_calls, safe_calls, armour_calls, player_calls,
    pitch_calls, stan_calls;

PropRecord *chrpropGetActiveTail(void) { return active_tail; }
PropRecord *getCurrentPlayerProp(void) { return player_prop; }
bool objIsCollectable(PropDefHeaderRecord *obj)
{
    ++collectable_calls;
    return obj != NULL && ((ObjectRecord *)obj)->type == PROPDEF_ARMOUR;
}
bool objCanPickupFromSafe(ObjectRecord *obj) { ++safe_calls; return obj != NULL; }
f32 currentPlayerGetArmor(void) { ++armour_calls; return g_CurrentPlayer->bondarmour; }
s32 getPlayerCount(void) { ++player_calls; return 1; }
f32 bondviewGetPlayerPitchRadians(void);
s32 stanTestLineUnobstructed(StandTile **stan, f32 x, f32 z,
                            f32 dest_x, f32 dest_z, int cdtypes,
                            f32 height, f32 a, f32 b, f32 c)
{
    ++stan_calls;
    (void)stan; (void)x; (void)z; (void)dest_x; (void)dest_z;
    (void)cdtypes; (void)height; (void)a; (void)b; (void)c;
    return line_clear ? 1 : 0;
}
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 sound,
                        ALSoundState *pending)
{
    (void)bank; (void)pending; last_sound = sound; return NULL;
}
textoverride *bondinvGetTextbyObj(ObjectRecord *obj)
{
    (void)obj; return NULL;
}
u8 *langGet(s32 id)
{
    (void)id; return (u8 *)"Picked up some body armor.\n";
}
void hudmsgBottomShow(char *message)
{
    snprintf(last_message, sizeof(last_message), "%s", message);
}
void objFree(ObjectRecord *obj, s32 free_prop, s32 can_regenerate)
{
    (void)obj; (void)free_prop; (void)can_regenerate; ++free_calls;
}
InvItem *bondinvGetNextAvailItem(void)
{
    ++inventory_calls; return NULL;
}
void propExecuteTickOperation(PropRecord *prop, TICKOP operation)
{
    (void)prop; last_operation = operation;
}

static void reset_observation(void)
{
    free_calls = 0U;
    inventory_calls = 0U;
    last_operation = TICKOP_NONE;
    last_sound = -1;
    last_message[0] = '\0';
    collectable_calls = safe_calls = armour_calls = player_calls = 0U;
    pitch_calls = stan_calls = 0U;
}

int main(void)
{
    struct player player = {0};
    struct player_data permanent = {0};
    PropRecord viewer = {0};
    PropRecord pickup = {0};
    StandTile stan = {0};
    BodyArmourRecord armour = {0};

    g_CurrentPlayer = &player;
    g_playerPerm = &permanent;
    g_ClockTimer = 1;
    g_PlayerInvincible = false;
    g_PlayerIsInTank = 0;
    player.prop = &viewer;
    player.vv_verta = 0.0f;
    player.magnetattracttime = -1;
    viewer.pos.x = pickup.pos.x = 100.0f;
    viewer.pos.y = pickup.pos.y = 20.0f;
    viewer.pos.z = pickup.pos.z = -40.0f;
    viewer.stan = pickup.stan = &stan;
    pickup.type = PROP_TYPE_OBJ;
    pickup.obj = (ObjectRecord *)&armour;
    armour.type = PROPDEF_ARMOUR;
    armour.amount = 1.0f;
    armour.prop = &pickup;
    armour.runtime_pos = pickup.pos;
    active_tail = &pickup;
    player_prop = &viewer;
    line_clear = true;

    reset_observation();
    ge_original_stage_props_tick_player_exact();
    assert(fabsf(player.bondarmour - 1.0f) < 0.00001f);
    assert(fabsf(permanent.body_armor_pickups - 1.0f) < 0.00001f);
    assert(last_sound == ARMOUR_COLLECT_SFX);
    assert(last_message[0] != '\0');
    assert(free_calls == 1U && inventory_calls == 0U);
    assert(last_operation == TICKOP_FREE);

    reset_observation();
    player.bondarmour = 0.0f;
    pickup.pos.x = 201.0f;
    armour.runtime_pos.x = pickup.pos.x;
    ge_original_stage_props_tick_player_exact();
    assert(player.bondarmour == 0.0f && free_calls == 0U);

    reset_observation();
    pickup.pos.x = viewer.pos.x;
    armour.runtime_pos.x = pickup.pos.x;
    line_clear = false;
    ge_original_stage_props_tick_player_exact();
    assert(player.bondarmour == 0.0f && free_calls == 0U);

    reset_observation();
    line_clear = true;
    g_PlayerInvincible = true;
    ge_original_stage_props_tick_player_exact();
    assert(player.bondarmour == 0.0f && free_calls == 0U);
    return 0;
}

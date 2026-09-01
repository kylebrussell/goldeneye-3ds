#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/bgfog.h"
#include "game/chraction.h"
#include "game/player.h"
#include "game/stan.h"

static struct player test_player;
static PropRecord guard_prop;
static PropRecord bond_prop;
static Model guard_model;
static ChrRecord guard;
static u8 guard_stan_storage[64];
static u8 bond_stan_storage[64];
static u8 other_stan_storage[64];
static s32 visible_to_guards;
static s32 line_result;
static StandTile *line_end_stan;
static u32 random_value;
static f32 guard_heading;
static char events[16];
static size_t event_count;
static s32 line_count;

struct player *g_CurrentPlayer = &test_player;
s32 g_GlobalTimer = 600;
f32 g_ScaledFarFogIntensity = FLT_MAX;
PropRecord *stanSavedColl_posData;
f32 stanSavedColl_someMin;

static StandTile *guard_stan(void)
{
    return (StandTile *)guard_stan_storage;
}

static StandTile *bond_stan(void)
{
    return (StandTile *)bond_stan_storage;
}

static StandTile *other_stan(void)
{
    return (StandTile *)other_stan_storage;
}

static void event(char value)
{
    assert(event_count < sizeof(events));
    events[event_count++] = value;
}

DIFFICULTY lvlGetSelectedDifficulty(void)
{
    return DIFFICULTY_AGENT;
}

u32 randomGetNext(void)
{
    return random_value;
}

f32 getsubroty(Model *model)
{
    assert(model == &guard_model);
    return guard_heading;
}

PropRecord *getCurrentPlayerProp(void)
{
    return g_CurrentPlayer->prop;
}

s32 bondviewGetVisibleToGuardsFlag(void)
{
    return visible_to_guards;
}

void chrSetMoving(ChrRecord *self, bool moving)
{
    assert(self == &guard);
    event(moving ? 'm' : 'M');
}

void bondviewUpdateGuardTankFlagsRelated(PropRecord *prop, s32 moving)
{
    assert(prop == &bond_prop);
    event(moving ? 'p' : 'P');
}

s32 stanTestLineUnobstructed(StandTile **stan, f32 x, f32 z,
        f32 dest_x, f32 dest_z, int cdtypes, f32 height,
        f32 height2, f32 dest_bottom, f32 dest_top)
{
    assert(stan != NULL && *stan == guard_stan());
    assert(fabsf(x - guard_prop.pos.x) < 0.001f);
    assert(fabsf(z - guard_prop.pos.z) < 0.001f);
    assert(fabsf(dest_x - bond_prop.pos.x) < 0.001f);
    assert(fabsf(dest_z - bond_prop.pos.z) < 0.001f);
    assert(cdtypes == (CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_CHRS
                       | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE));
    assert(fabsf(height - (guard.chrheight - 20.0f)) < 0.001f);
    assert(fabsf(height2 - height) < 0.001f);
    assert(dest_bottom == 0.0f && dest_top == 1.0f);
    event('L');
    line_count++;
    *stan = line_end_stan;
    stanSavedColl_posData = NULL;
    stanSavedColl_someMin = line_result ? 1.0f : 0.5f;
    return line_result;
}

static void reset_sight(void)
{
    memset(&test_player, 0, sizeof(test_player));
    memset(&guard_prop, 0, sizeof(guard_prop));
    memset(&bond_prop, 0, sizeof(bond_prop));
    memset(&guard_model, 0, sizeof(guard_model));
    memset(&guard, 0, sizeof(guard));
    memset(events, 0, sizeof(events));
    event_count = 0;
    line_count = 0;
    visible_to_guards = TRUE;
    line_result = TRUE;
    line_end_stan = bond_stan();
    random_value = 0;
    guard_heading = 0.0f;
    g_ScaledFarFogIntensity = 10000.0f;

    guard_prop.type = PROP_TYPE_CHR;
    guard_prop.chr = &guard;
    guard_prop.stan = guard_stan();
    guard_prop.pos.x = 0.0f;
    guard_prop.pos.y = 0.0f;
    guard_prop.pos.z = 0.0f;
    guard.prop = &guard_prop;
    guard.model = &guard_model;
    guard.chrheight = 180.0f;
    guard.visionrange = 10.0f;
    guard.speedrating = 100;

    bond_prop.type = PROP_TYPE_PLAYER;
    bond_prop.stan = bond_stan();
    bond_prop.pos.x = 0.0f;
    bond_prop.pos.y = 0.0f;
    bond_prop.pos.z = 100.0f;
    test_player.prop = &bond_prop;
}

static void assert_line_service_order(void)
{
    static const char expected[] = { 'M', 'P', 'L', 'm', 'p' };
    assert(event_count == sizeof(expected));
    assert(memcmp(events, expected, sizeof(expected)) == 0);
}

int main(void)
{
    reset_sight();
    assert(chrCheckTargetInSight(&guard) == TRUE);
    assert(line_count == 1);
    assert_line_service_order();
    assert(guard.seen_bond_time == g_GlobalTimer);
    assert(guard.lastseetarget60 == g_GlobalTimer);
    assert(guard.targetTile == bond_stan());
    assert(fabsf(guard.lastknowntargetpos.z - 100.0f) < 0.001f);

    reset_sight();
    bond_prop.pos.z = -500.0f;
    assert(chrCheckTargetInSight(&guard) == FALSE);
    assert(line_count == 0 && event_count == 0);

    reset_sight();
    bond_prop.pos.z = 1001.0f;
    assert(chrCheckTargetInSight(&guard) == FALSE);
    assert(line_count == 0 && event_count == 0);

    reset_sight();
    g_ScaledFarFogIntensity = 50.0f;
    assert(chrCheckTargetInSight(&guard) == FALSE);
    assert(line_count == 0 && event_count == 0);

    reset_sight();
    line_result = FALSE;
    assert(chrCheckTargetInSight(&guard) == FALSE);
    assert(line_count == 1);
    assert_line_service_order();
    assert(guard.lastseetarget60 == 0);

    reset_sight();
    line_end_stan = other_stan();
    assert(chrCheckTargetInSight(&guard) == FALSE);
    assert(line_count == 1);
    assert_line_service_order();

    reset_sight();
    guard.visionrange = 20.0f;
    bond_prop.pos.z = 1000.0f;
    guard.speedrating = 0;
    random_value = 1;
    assert(chrCheckTargetInSight(&guard) == FALSE);
    assert(line_count == 0 && event_count == 0);

    reset_sight();
    bond_prop.pos.z = -500.0f;
    assert(chrCanSeeBond(&guard) == TRUE);
    assert(line_count == 1);
    assert_line_service_order();

    reset_sight();
    visible_to_guards = FALSE;
    assert(chrCanSeeBond(&guard) == FALSE);
    assert(line_count == 0 && event_count == 0);

    puts("exact Dam guard sight: facing/range/fog/random gates and canonical "
         "player/STAN LOS accept/reject retained");
    return 0;
}

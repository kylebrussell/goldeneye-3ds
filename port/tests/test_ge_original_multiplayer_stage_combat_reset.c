/* Existing real canonical ray/BBOX/damage fixture, with explicit presentation
 * sinks. This regression begins at the real ray intersection, not at a damage
 * counter wrapper. Pickup checks retain the original dispatch loop/gate. */
#define main legacy_combat_main
#include "test_ge_original_multiplayer_combat.c"
#undef main
#include "ge_original_multiplayer_stage_reset.h"
#include "reset_globals.h"

static PropRecord pickup_prop;
static unsigned pickup_visits, pickup_lists, pickup_ops;
bool isBondInTank(void) { return g_PlayerIsInTank; }
PropRecord *chrpropGetActiveTail(void) { ++pickup_lists; return &pickup_prop; }
TICKOP objTickPlayer(PropRecord *prop)
{ assert(prop==&pickup_prop); ++pickup_visits; return TICKOP_NONE; }
TICKOP weaponTickPlayer(PropRecord *prop) { return objTickPlayer(prop); }
void propExecuteTickOperation(PropRecord *prop, TICKOP op)
{ assert(prop==&pickup_prop && op==TICKOP_NONE); ++pickup_ops; }
void propsTickPlayer(void);

static void carryover(size_t count, size_t attacker)
{
    size_t target=(attacker+1)%count;
    initialize(count); set_cur_player((s32)attacker);
    for(size_t slot=0;slot<count;++slot) {
        props[slot].pos.x=slot==target||slot==attacker ? 0 : 1000;
        props[slot].pos.z=slot==attacker ? -100 : -200;
        props[slot].zDepth=-props[slot].pos.z;
        matrices[slot].pos.m[3][0]=props[slot].pos.x;
        matrices[slot].pos.m[3][2]=props[slot].pos.z;
    }
    memset(&pickup_prop,0,sizeof(pickup_prop));pickup_prop.type=PROP_TYPE_WEAPON;
    pickup_visits=pickup_lists=pickup_ops=0;
    /* Authored campaign exit AI leaves this native scalar true. */
    g_PlayerInvincible=TRUE;
    ShotData shot=aimed_shot(target);
    assert(ge_original_guard_bullet_hit_apply_shot_hit(&shot,0)==GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED);
    assert(players[target].bondhealth==1);
    propsTickPlayer();
    assert(pickup_visits==0 && pickup_lists==0 && pickup_ops==0);
    ge_original_multiplayer_stage_intro_shared_reset_exact();
    assert(!g_PlayerInvincible);
    shot=aimed_shot(target);
    assert(ge_original_guard_bullet_hit_apply_shot_hit(&shot,0)==GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED);
    assert(players[target].bondhealth<1 && players[target].bondhealth>0);
    propsTickPlayer();
    assert(pickup_visits==1 && pickup_lists==1 && pickup_ops==1);
    assert(g_CurrentPlayer==&players[attacker]);
}
int main(void)
{
    for(size_t count=2;count<=4;count+=2)
        for(size_t attacker=0;attacker<count;++attacker)carryover(count,attacker);
    puts("stage carryover: original 2/4 ray-to-damage and pickup-dispatch gates recover from native invincibility");
    return 0;
}

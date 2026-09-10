#include "ge_original_multiplayer_stage_reset.h"
#include "ge_original_bond_input_provider.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "game/gun.h"
#include "game/bondinv.h"

/* Real original collision/view publication below; the single flat STAN
 * boundary only reports room connectivity and floor height. */
extern struct player *g_CurrentPlayer;
struct player_data *g_playerPerm;
enum CAMERAMODE g_CameraMode;
s32 g_ClockTimer=1, stanlinelog_flag=7;
static struct player players[4];
static struct player_data permissions[4];
static PropRecord props[4];
static Model bodies[4];
static StandTile tile;
static unsigned walks, floors, unequips, equips, items;
s32 g_bondviewForceDisarm, mission_timer, watch_time_0;
ITEM_IDS starting_weapon[2]={ITEM_FIST,ITEM_UNARMED};
void fixture_tick_original_force_disarm(void);
void bondviewPlayerBeginLife(void);
s32 getPlayerCount(void) { return ge_original_bond_input_provider()->player_count; }
ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand) { return g_CurrentPlayer->hands[hand].weaponnum; }
void currentPlayerUnEquipWeaponWrapper(GUNHAND hand, ITEM_IDS weapon)
{ (void)hand; (void)weapon; ++unequips; }
s32 currentPlayerEquipWeaponWrapper(GUNHAND hand, s32 weapon)
{ assert(weapon==starting_weapon[hand]); ++equips; return 1; }
int bondinvAddInvItem(ITEM_IDS item) { assert(item==ITEM_FIST); ++items; return 1; }
void solo_char_load(void) { assert(0); }
void bondviewUpdatePlayerCollisionPositionFields(void);
bool walkTilesBetweenPoints_NoCallback(StandTile **stan, f32 x1, f32 z1, f32 x2, f32 z2)
{
    assert(*stan==&tile && stanlinelog_flag==0);
    assert(x1==g_CurrentPlayer->field_488.collision_position.x);
    assert(z1==g_CurrentPlayer->field_488.collision_position.z);
    assert(x2==g_CurrentPlayer->field_488.pos.x && z2==g_CurrentPlayer->field_488.pos.z);
    ++walks; return TRUE;
}
f32 bondviewYPositionRelated(StandTile *stan, f32 x, f32 z)
{ assert(stan==&tile); (void)x; (void)z; ++floors; return 0; }

static void run(unsigned count, enum CAMERAMODE inherited)
{
    struct player *roster[4];
    struct player_data *perms[4];
    memset(players,0,sizeof(players)); memset(props,0,sizeof(props));
    memset(permissions,0,sizeof(permissions));
    for(unsigned slot=0;slot<count;++slot) {
        roster[slot]=&players[slot]; perms[slot]=&permissions[slot];
        players[slot].prop=&props[slot]; players[slot].bodyModel=&bodies[slot];
        players[slot].headpos.y=168; permissions[slot].player_perspective_height=1;
        players[slot].field_488.current_tile_ptr=&tile;
        players[slot].field_488.pos=(coord3d){-100,175,-200};
        players[slot].field_488.collision_position=(coord3d){100+slot,175,200+slot};
    }
    assert(ge_original_bond_input_bind_roster(roster,perms,count));
    /* This is the real defect: a previous native death/end mode keeps each
     * newly bodied player's view at spawn despite a changed collision point. */
    g_CameraMode=inherited;
    g_bondviewForceDisarm=1; mission_timer=12345; watch_time_0=45678;
    unsigned old_unequips=unequips;
    for(unsigned tick=0;tick<3;++tick) {
        g_CurrentPlayer=roster[tick%count];
        fixture_tick_original_force_disarm();
    }
    assert(unequips==old_unequips+2);
    g_bondviewForceDisarm=1;
    for(unsigned slot=0;slot<count;++slot) {
        g_CurrentPlayer=roster[slot];g_playerPerm=perms[slot];
        bondviewUpdatePlayerCollisionPositionFields();
        assert(players[slot].field_488.pos.x==-100);
        assert(props[slot].pos.x==100+slot);
    }
    ge_original_multiplayer_stage_intro_shared_reset_exact();
    assert(g_CameraMode==CAMERAMODE_NONE);
    assert(g_bondviewForceDisarm==0 && mission_timer==0 && watch_time_0==0);
    old_unequips=unequips;
    for(unsigned ordinal=0;ordinal<count;++ordinal) {
        unsigned slot=count-1-ordinal;
        g_CurrentPlayer=roster[slot];g_playerPerm=perms[slot];
        fixture_tick_original_force_disarm();
        bondviewPlayerBeginLife();
        assert(players[slot].lifestarttime60==0);
        assert(unequips==old_unequips);
        bondviewUpdatePlayerCollisionPositionFields();
        assert(!memcmp(&players[slot].field_488.pos,&players[slot].field_488.collision_position,sizeof(coord3d)));
        assert(players[slot].field_488.pos3.x==100+slot);
        assert(players[slot].field_488.current_tile_ptr_for_portals==&tile);
        assert(stanlinelog_flag==7);
    }
    ge_original_bond_input_unbind_roster();
}
int main(void)
{
    enum CAMERAMODE inherited[]={CAMERAMODE_DEATH_CAM_SP,CAMERAMODE_POSEND,CAMERAMODE_DEATH_CAM_MP};
    for(unsigned i=0;i<sizeof(inherited)/sizeof(*inherited);++i) { run(4,inherited[i]);run(2,inherited[i]); }
    assert(walks==36 && floors==walks && equips==36 && items==18);
    puts("stage intro subset: original 2/4 view/prop/portal and begin-life publication recovers from death/end modes, force-disarm and stale timers");
    return 0;
}

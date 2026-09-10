#include <assert.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/bondinv.h"
#include "game/gun.h"
#include "game/lv.h"
#include "ge_original_multiplayer_rules.h"

struct player *g_CurrentPlayer, *g_playerPointers[4];
struct player_data *g_playerPerm, g_playerPlayerData[4];
s32 player_num, scenario, g_stopPlayFlag, g_gameOverFlag, g_PlayerIsInTank;
s32 g_ExplodeTankOnDeathFlag, g_isBondKIA;
bool g_PlayerInvincible;
PropRecord *g_PlayerTankProp;
ALBank *g_musicSfxBufferPtr;
stagesetup g_CurrentSetup;
static struct player players[4];
static PropRecord props[4];
static ChrRecord actors[4];
static InvItem items[4][20];
static unsigned drop_count[4], equip_count[4];
static int num_players;
static uint32_t rng = 1;
extern s32 startpadcount;
extern PadRecord *g_Startpad[16];
extern void init_player_BONDdata(void);
extern s32 bondinvHasInvItem(ITEM_IDS);
extern void record_damage_kills(f32, f32, f32, s32, s32);
extern s32 bondviewGetRandomSpawnPadIndex(void);
extern void bondviewAddCurrentPlayerArmor(f32);
s32 getPlayerCount(void) { return num_players; }
s32 get_cur_playernum(void) { return player_num; }
MPSCENARIOS get_scenario(void) { return scenario; }
s32 getMissiontimer(void) { return 600; }
s32 get_player_control_style(s32 p) { (void)p; return 1; }
void cur_player_set_control_type(int p) { (void)p; }
s32 cur_player_get_control_type(void) { return 1; }
u32 randomGetNext(void) { rng = rng * 1664525u + 1013904223u; return rng; }
s32 check_ramrom_flags(void) { return 0; }
#ifdef GE_MP_FIXTURE_NATIVE_STAN
#include "ge_stan_native.h"
static const GeStanNativeMap *fixture_native_stan;
f32 bondviewYPositionRelated(StandTile *s, f32 x, f32 z)
{
    return ge_original_stan_get_position_y(fixture_native_stan, (const GeStanNativeTile *)s, x, z);
}
#else
f32 bondviewYPositionRelated(StandTile *s, f32 x, f32 z) { assert(s); (void)x; (void)z; return 40.0f; }
#endif
void hudmsgsSetOn(s32 x) { (void)x; }
void bondviewClearUpperTextDisplayFlag(s32 x) { (void)x; }
void hudMakeDamageSegments(void *p, s32 n, s32 a, f32 h) { (void)p;(void)n;(void)a;(void)h; }
void joyRumblePakStart(s32 p, f32 f) { (void)p;(void)f; }
void hudmsgBottomShow(char *p) { (void)p; }
u8 *langGet(s32 id) { (void)id;return (u8 *)"fixture"; }
ALSoundState *sndPlaySfx(struct ALBankAlt_s *b, s16 s, ALSoundState *p) { (void)b;(void)s;(void)p;return NULL; }
void trigger_solo_watch_menu(s32 x) { (void)x; }
f32 currentPlayerGetArmor(void) { return g_CurrentPlayer->bondarmour; }
f32 currentPlayerGetHealth(void) { return g_CurrentPlayer->bondhealth; }
s32 currentPlayerEquipWeaponWrapper(GUNHAND hand, s32 weapon) { (void)hand;(void)weapon;equip_count[player_num]++; return 1; }
void solo_char_load(void) { assert(!"fixture already owns a canonical body"); }
/* Resource boundary only: actual dropped actor materialization is covered by
 * runtime prop adapters; this fixture records that canonical damage requests it. */
void drop_inventory(void) { drop_count[player_num]++; }
ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand) { (void)hand;return ITEM_FIST; }
s32 get_ammo_type_for_weapon(ITEM_IDS item) { assert(item==ITEM_FIST);return AMMO_NONE; }
u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 flags) { (void)item;(void)flags;assert(!"fist uses no ammo");return false; }
WeaponStats *get_ptr_item_statistics(ITEM_IDS item) { (void)item;assert(!"fist uses no magazine");return NULL; }

static void setup(int count, int mode)
{
    int i;
    GeOriginalMultiplayerConfig c = {count, mode, 1, LEVELID_TEMPLE, {0,0,1,1}};
    memset(players,0,sizeof(players));memset(props,0,sizeof(props));
    memset(actors,0,sizeof(actors));memset(g_playerPlayerData,0,sizeof(g_playerPlayerData));
    memset(items,0,sizeof(items));memset(drop_count,0,sizeof(drop_count));
    memset(equip_count,0,sizeof(equip_count));
    num_players=count;
    for(i=0;i<count;i++) {
        g_playerPointers[i]=&players[i];players[i].prop=&props[i];props[i].chr=&actors[i];
        players[i].p_itemcur=items[i];players[i].equipmaxitems=20;
        g_playerPlayerData[i].handicap=1;g_playerPlayerData[i].player_perspective_height=1;
        g_playerPlayerData[i].shortest_inning=INT_MAX;
        set_cur_player(i);init_player_BONDdata();bondinvReinitInv();
        players[i].actual_health=players[i].actual_armor=1;
        players[i].bodyModel=(Model *)1;actors[i].fadealpha=255;
    }
    set_cur_player(0);
    assert(ge_original_multiplayer_configure(&c)==GE_ORIGINAL_MP_OK);
}

static void finish_death(int player)
{
    players[player].deathanimfinished=1;players[player].redbloodfinished=1;
    players[player].colourfadetimemax60=-1;
}

static void malformed_setup(void)
{
    PadRecord pads[2] = {0}; StandTile stan = {0};
    int32_t intro[] = {INTROTYPE_SPAWN, 0, 0, INTROTYPE_ITEM, ITEM_FIST, -1, 0,
        INTROTYPE_AMMO, AMMO_9MM, 10, 0, INTROTYPE_END};
    size_t n;
    pads[0].stan = &stan;
    assert(ge_original_multiplayer_publish_spawns(intro, 12, pads, 1, 0) == GE_ORIGINAL_MP_OK);
    for (n = 0; n < 12; ++n)
        assert(ge_original_multiplayer_publish_spawns(intro, n, pads, 1, 0) == GE_ORIGINAL_MP_INVALID_SETUP);
    intro[1] = 1;
    assert(ge_original_multiplayer_publish_spawns(intro, 12, pads, 1, 0) == GE_ORIGINAL_MP_INVALID_SETUP);
    intro[1] = 0; intro[8] = -1;
    assert(ge_original_multiplayer_publish_spawns(intro, 12, pads, 1, 0) == GE_ORIGINAL_MP_INVALID_SETUP);
    intro[8] = AMMO_9MM; intro[4] = ITEM_IDS_MAX;
    assert(ge_original_multiplayer_publish_spawns(intro, 12, pads, 1, 0) == GE_ORIGINAL_MP_INVALID_SETUP);
    intro[4] = ITEM_FIST; intro[2] = 1;
    assert(ge_original_multiplayer_publish_spawns(intro, 12, pads, 1, 0) == GE_ORIGINAL_MP_INVALID_SETUP);
    intro[2] = 0; pads[0].pos.x = NAN;
    assert(ge_original_multiplayer_publish_spawns(intro, 12, pads, 1, 0) == GE_ORIGINAL_MP_INVALID_SETUP);
    assert(startpadcount == 1 && g_Startpad[0] == &pads[0]);
}

static void end_rules(void)
{
    int i;
    setup(4, SCENARIO_NORMAL);
    assert(ge_original_multiplayer_end_tick(1, 0, 0, 5) == GE_ORIGINAL_MP_END_RUNNING);
    g_playerPlayerData[0].kill_counts[1] = 5;
    players[1].bonddead = 1;
    assert(ge_original_multiplayer_end_tick(0, 0, 0, 5) == GE_ORIGINAL_MP_END_RUNNING);
    assert(ge_original_multiplayer_end_tick(1, 0, 0, 5) == GE_ORIGINAL_MP_END_FREEZE);
    finish_death(1);
    assert(ge_original_multiplayer_end_tick(1, 0, 0, 5) == GE_ORIGINAL_MP_END_RESULTS);
    setup(2, SCENARIO_NORMAL);
    assert(ge_original_multiplayer_end_tick(1, 99, 100, 0) == GE_ORIGINAL_MP_END_RESULTS);
    assert(ge_original_multiplayer_end_tick(0, 99, 100, 0) == GE_ORIGINAL_MP_END_RUNNING);
    assert(ge_original_multiplayer_end_tick(1, INT_MAX, 0, 0) == GE_ORIGINAL_MP_END_INVALID);
    setup(4, SCENARIO_YOLT);
    for(i = 1; i < 4; ++i) {g_playerPlayerData[0].kill_counts[i] = 2; players[i].bonddead = 1;}
    assert(ge_original_multiplayer_end_tick(1, 0, 0, 0) == GE_ORIGINAL_MP_END_FREEZE);
    for(i = 1; i < 4; ++i) {assert(g_playerPlayerData[i].order_out_in_yolt == 2);finish_death(i);}
    assert(ge_original_multiplayer_end_tick(1, 0, 0, 0) == GE_ORIGINAL_MP_END_RESULTS);
}

static void combat_respawn(int count)
{
    PadRecord pads[4]={0};StandTile stan={0};
    int32_t intro[]={INTROTYPE_SPAWN,0,0,INTROTYPE_SPAWN,1,0,
        INTROTYPE_ITEM,ITEM_FIST,-1,0,INTROTYPE_AMMO,AMMO_9MM,12,0,INTROTYPE_END};
    int score, i;
    setup(count,SCENARIO_NORMAL);
    for(i=0;i<count;i++) props[i].pos.x=0;
    pads[0].stan=&stan;pads[1].stan=&stan;pads[1].pos.x=2000;
    assert(ge_original_multiplayer_publish_spawns(intro,15,pads,2,0)==GE_ORIGINAL_MP_OK);
    g_CurrentSetup.intro=(void *)intro;
    set_cur_player(1);
    bondviewAddCurrentPlayerArmor(0.5f);
    assert(players[1].bondarmour==0.5f && players[0].bondarmour==0);
    record_damage_kills(0.25f,1,0,0,1);
    assert(players[1].bondarmour==0.25f && players[1].bondhealth==1);
    record_damage_kills(2,1,0,0,1);
    assert(players[1].bonddead==1 && g_playerPlayerData[0].kill_counts[1]==1);
    assert(g_playerPlayerData[0].kill_count==1 && players[1].deathcount==1);
    assert(drop_count[1]==1 && get_cur_playernum()==1);
    assert(ge_original_multiplayer_score(0,&score)==GE_ORIGINAL_MP_OK && score==1);
    record_damage_kills(2,1,0,0,1);
    assert(g_playerPlayerData[0].kill_counts[1]==1);
    assert(!ge_original_multiplayer_can_respawn(1,0x8000));
    finish_death(1);
    assert(!ge_original_multiplayer_can_respawn(1,0));
    players[1].ammoheldarr[AMMO_9MM]=999;players[1].ammoheldarr[4]=99;
    set_cur_player(0);
    assert(ge_original_multiplayer_respawn(1,0x8000)==GE_ORIGINAL_MP_OK);
    assert(get_cur_playernum()==0 && g_playerPerm==&g_playerPlayerData[0]);
    assert(players[1].bonddead==0 && players[1].bondhealth==1 && players[1].bondarmour==0);
    assert(players[1].ammoheldarr[AMMO_9MM]==12 && players[1].ammoheldarr[4]==0);
    assert(props[1].pos.x==2000 && props[1].pos.y==215 && props[1].stan==&stan);
    assert(players[1].field_488.current_tile_ptr==&stan && players[1].eyeheight==175);
    assert(players[1].deathcount==1 && g_playerPlayerData[0].kill_counts[1]==1);
    set_cur_player(1);assert(bondinvHasInvItem(ITEM_FIST));
    record_damage_kills(2,1,0,1,1);
    assert(players[1].num_suicides==1 && g_playerPlayerData[1].kill_counts[1]==1);
    assert(ge_original_multiplayer_score(1,&score)==GE_ORIGINAL_MP_OK && score==-1);
}

int main(void)
{
    int score, i, slot;GeOriginalMultiplayerArena arena;GeOriginalMultiplayerWeapon weapon;
    GeOriginalMultiplayerConfig config={4,SCENARIO_NORMAL,0,LEVELID_TEMPLE,{0,0,1,1}};
    for(i=0;i<GE_ORIGINAL_MP_ARENAS;i++) {assert(ge_original_multiplayer_arena(i,&arena));assert(strncmp(arena.setup_key,"Ump_setup",9)==0);}
    assert(!ge_original_multiplayer_arena(GE_ORIGINAL_MP_ARENAS,&arena));
    config.level_id=LEVELID_EGYPT;assert(ge_original_multiplayer_validate(&config)==GE_ORIGINAL_MP_INVALID_CONFIG);
    config.level_id=LEVELID_TEMPLE;config.scenario=SCENARIO_2v1;assert(ge_original_multiplayer_validate(&config)==GE_ORIGINAL_MP_INVALID_CONFIG);
    malformed_setup();end_rules();combat_respawn(2);combat_respawn(4);
    setup(4,SCENARIO_2v2);
    g_playerPlayerData[0].kill_counts[2]=3;g_playerPlayerData[1].kill_counts[0]=1;
    assert(ge_original_multiplayer_score(0,&score)==GE_ORIGINAL_MP_OK && score==2);
    assert(ge_original_multiplayer_score(1,&score)==GE_ORIGINAL_MP_OK && score==2);
    setup(4,SCENARIO_YOLT);players[2].bonddead=1;finish_death(2);
    g_playerPlayerData[0].kill_counts[2]=1;assert(ge_original_multiplayer_can_respawn(2,0x1000));
    g_playerPlayerData[1].kill_counts[2]=1;assert(!ge_original_multiplayer_can_respawn(2,0x1000));
    g_playerPlayerData[2].order_out_in_yolt=3;assert(ge_original_multiplayer_score(2,&score)==GE_ORIGINAL_MP_OK && score==1);
    setup(2,SCENARIO_LTK);set_cur_player(1);players[1].bondarmour=1;
    record_damage_kills(0.001f,1,0,0,1);assert(players[1].bonddead);
    setup(4,SCENARIO_MWTGG);g_playerPlayerData[0].kill_counts[1]=1;g_playerPlayerData[0].killed_gg_owner_count=1;
    assert(ge_original_multiplayer_score(0,&score)==GE_ORIGINAL_MP_OK && score==3);
    setup(2,SCENARIO_TLD);g_playerPlayerData[1].flag_counter=125;
    assert(ge_original_multiplayer_score(1,&score)==GE_ORIGINAL_MP_OK && score==125);
    config.scenario=SCENARIO_NORMAL;config.player_count=2;
    for(i=0;i<14;i++) {config.weapon_set=i;assert(ge_original_multiplayer_configure(&config)==GE_ORIGINAL_MP_OK);for(slot=0;slot<8;slot++) assert(ge_original_multiplayer_weapon(slot,&weapon));}
    assert(!ge_original_multiplayer_weapon(8,&weapon));
    puts("canonical multiplayer rules: 2/4-player damage, death, respawn, ownership, scenario scores and weapon sets passed");
    return 0;
}

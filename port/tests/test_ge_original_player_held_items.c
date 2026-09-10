/* Reuse the canonical ray->player damage fixture's primitive models and
 * presentation sinks. Its direct-hit tests are separate; these start at the
 * real held-item dispatcher and real randomized screen-center bullet path. */
#define main legacy_combat_main
#define randomGetNext legacy_fixture_random
#define get_ptr_item_statistics legacy_fixture_statistics
#define inc_curplayer_hitcount_with_weapon legacy_fixture_counter
#define g_HitTypeSounds legacy_fixture_hit_sounds
#include "test_ge_original_multiplayer_combat.c"
#undef main
#undef randomGetNext
#undef get_ptr_item_statistics
#undef inc_curplayer_hitcount_with_weapon
#undef g_HitTypeSounds
#include "ge_original_player_held_items.h"
#include "ge_original_bond_input_provider.h"
#include "game/bg.h"
#include "game/stan.h"
#include <random.h>

extern WeaponStats fist_stats;
extern WeaponStats shotgun_stats, autoshot_stats;
ModelFileHeader cartshell_header; /* Shell rendering is outside held-item pass. */
extern u64 g_randomSeed;
PropRecord *g_OnScreenPropList[16];
PropRecord **g_LastOnScreenProp = g_OnScreenPropList;
s_room_info ge_bg_visibility_room_info[2];
s32 ge_bg_visibility_max_rooms = 2;
static bg_portal_data_entry portals[1];
bg_portal_data_entry *g_BgPortals = portals;
s32 g_RemoteMineOwnerTriggerFlag;
struct image_sound *g_HitTypeSounds[16];
static StandTile tile;
static unsigned photos, tanks, background_tests, light_calls, impact_explosions;
static int solo_sight, stan_blocked, bg_hit, melee_blocked;

/* This fixture world is one empty room and one unobstructed STAN. The actual
 * player models/BBOX intersections, damage, armor and score are original. */
bool walkTilesBetweenPoints_NoCallback(StandTile **from, f32 x1, f32 z1, f32 x2, f32 z2)
{ assert(*from == &tile); (void)x1; (void)z1; (void)x2; return !stan_blocked || z2 > -1000; }
void stanResetHits(void) {}
s32 getTileRoom(StandTile *from) { assert(from == &tile); return 1; }
f32 get_room_data_float1(void) { return 1; }
f32 get_room_data_float2(void) { return 1; }
f32 bgGetLevelVisibilityScale(void) { return 1; }
bool bgTestBulletHitBackground(coord3d *from, coord3d *to, s32 room, HitThing *hit)
{
    (void)from; (void)to; assert(room == 1); ++background_tests;
    if (bg_hit) { memset(hit, 0, sizeof(*hit)); hit->hitpos.z = -1000; hit->texturenum = 0; }
    return bg_hit;
}
bool bgTestRayIntersectsBbox(coord3d *p, coord3d *d, s32 *min, s32 *max)
{ (void)p; (void)d; (void)min; (void)max; abort(); }
s32 bgCopyVisibleRoomsToList(s32 *rooms, s32 cap) { assert(cap == 100); rooms[0] = 1; return 1; }
s32 bgGetConnectedRooms(s32 room, s32 *rooms, s32 cap)
{ (void)room; (void)rooms; (void)cap; abort(); }
bool check_if_imageID_is_light(s32 image) { return bg_hit && image == 0; }
void chrlvStanLineDirIntersection(coord3d *p, coord3d *d, coord3d *out)
{ (void)p; (void)d; *out = (coord3d){0, 0, -1000}; }
void sub_GAME_7F04E9BC(PropRecord *p, ShotData *shot) { (void)p; (void)shot; abort(); }
void objHit(ShotData *shot, BulletHit *hit) { (void)shot; (void)hit; abort(); }
void recall_joy2_hits_edit_flag(ITEM_IDS item, coord3d *pos, s32 texture)
{ (void)item; (void)pos; (void)texture; }
bool explosionCreate(PropRecord *owner, coord3d *pos, StandTile *stan, s32 type,
    bool makescorch, s32 player, u8 *rooms, PropRecord *source)
{ (void)owner; (void)pos; (void)stan; (void)makescorch; (void)player; (void)rooms; (void)source; assert(type == 1); ++impact_explosions; return TRUE; }
void bullet_spark_create(coord3d *pos, s32 a, f32 b, s32 room)
{ (void)pos; (void)a; (void)b; (void)room; }
bool stanTestLineUnobstructed(StandTile **from, f32 x1, f32 z1, f32 x2, f32 z2,
    int types, f32 ymax, f32 ymin, f32 a, f32 b)
{ (void)x1; (void)z1; (void)x2; (void)z2; (void)types; (void)ymax; (void)ymin; (void)a; (void)b; assert(*from == &tile); return !melee_blocked; }
void objectiveTakePictureHandler(void) { ++photos; }
void gunFireTankShell(s32 hand) { assert(hand == GUNRIGHT || hand == GUNLEFT); ++tanks; }
u32 cur_player_get_sight_onscreen_control(void) { return (u32)solo_sight; }
static int light_break(void *context, void *triangle, uint32_t type, int32_t room)
{ assert(context == &light_calls); (void)triangle; (void)type; assert(room == 1); ++light_calls; return 1; }

static void select_shooter(size_t slot)
{
    set_cur_player((s32)slot);
    assert(ge_original_bond_input_provider()->current_player == &players[slot]);
}
static void setup(size_t count, size_t shooter, size_t target)
{
    struct player_data *permissions[4];
    size_t i;
    initialize(count);
    for (i = 0; i < count; ++i) {
        permissions[i] = &g_playerPlayerData[i];
        players[i].c_screenwidth = 320; players[i].c_screenheight = 240;
        players[i].c_halfwidth = 160; players[i].c_halfheight = 120;
        players[i].c_scalex = players[i].c_scaley = 1.0f / 160;
        players[i].c_perspaspect = 4.0f / 3.0f;
        players[i].crosshair_angle = (coord2d){160, 120};
        players[i].field_10CC = &view;
        players[i].hands[0].weaponnum = players[i].hands[1].weaponnum = ITEM_WPPK;
        players[i].hands[0].weaponnum_watchmenu = players[i].hands[1].weaponnum_watchmenu = -1;
        props[i].stan = &tile;
        props[i].pos.x = i == target || i == shooter ? 0 : 1000;
        props[i].pos.z = i == shooter ? 0 : -200;
        props[i].zDepth = -props[i].pos.z;
        matrices[i].pos.m[3][0] = props[i].pos.x;
        matrices[i].pos.m[3][2] = props[i].pos.z;
        g_OnScreenPropList[i] = &props[i];
    }
    g_LastOnScreenProp = g_OnScreenPropList + count;
    assert(ge_original_bond_input_bind_roster(g_playerPointers, permissions, (uint32_t)count));
    select_shooter(shooter);
    gitem_structs[ITEM_FIST].item_weapon_stats = &fist_stats;
    gitem_structs[ITEM_KNIFE].item_weapon_stats = &fist_stats;
    gitem_structs[ITEM_SHOTGUN].item_weapon_stats = &shotgun_stats;
    gitem_structs[ITEM_AUTOSHOT].item_weapon_stats = &autoshot_stats;
    g_RemoteMineOwnerTriggerFlag = 0;
    photos = tanks = light_calls = background_tests = impact_explosions = 0;
    stan_blocked = bg_hit = melee_blocked = 0;
    ge_original_player_held_items_bind_lights(NULL, NULL);
    g_randomSeed = 0x123456789abcdef0ULL;
}

static void shooting(size_t count, size_t shooter, size_t target)
{
    f32 damage = wppk_stats.DestructionAmount * 2 * .125f;
    u64 expected;
    size_t i;
    setup(count, shooter, target);
    players[shooter].hands[GUNRIGHT].weapon_firing_status = 1;
    players[target].bondarmour = damage;
    expected = g_randomSeed;
    for (i = 0; i < 4; ++i) (void)randomGetNextFrom(&expected);
    assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
    assert(players[target].bondarmour == 0 && players[target].bondhealth == 1);
    assert(g_randomSeed == expected && background_tests == 1);
    assert(g_playerPlayerData[shooter].shot_count[SHOT_REGISTER_TOTAL] == 1);
    assert(g_playerPlayerData[shooter].shot_count[SHOT_REGISTER_BODY] == 1);
    assert(g_CurrentPlayer == &players[shooter]);
    players[shooter].hands[GUNRIGHT].weapon_firing_status = 0;
    assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
    assert(g_randomSeed == expected && background_tests == 1);
    players[target].bondhealth = damage;
    players[shooter].hands[GUNLEFT].weapon_firing_status = 1;
    assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
    assert(players[target].bonddead && players[target].deathcount == 1 && drops == 1);
    assert(g_playerPlayerData[shooter].kill_counts[target] == 1);
    assert(g_playerPlayerData[shooter].shot_count[SHOT_REGISTER_TOTAL] == 2);
    assert(g_CurrentPlayer == &players[shooter]);
    for (i = 0; i < count; ++i) if (i != target) assert(players[i].bondhealth == 1);
}

static void melee(size_t count, size_t shooter, size_t target)
{
    setup(count, shooter, target);
    players[shooter].hands[GUNRIGHT].weaponnum = ITEM_FIST;
    players[shooter].hands[GUNRIGHT].weapon_firing_status = 1;
    props[target].pos.z = matrices[target].pos.m[3][2] = -30;
    props[target].zDepth = 30;
    melee_blocked = 1;
    assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
    assert(players[target].bondhealth == 1);
    melee_blocked = 0;
    assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
    assert(players[target].bondhealth < 1 && players[shooter].bondhealth == 1);
    assert(g_CurrentPlayer == &players[shooter]);
}

int main(void)
{
    size_t count, shooter, target;
    static const ITEM_IDS deferred_items[] = {ITEM_GRENADELAUNCH, ITEM_ROCKETLAUNCH,
        ITEM_GRENADE, ITEM_THROWKNIFE, ITEM_REMOTEMINE, ITEM_PROXIMITYMINE,
        ITEM_TIMEDMINE, ITEM_FLAREPISTOL, ITEM_PITONGUN, ITEM_BOMBCASE, ITEM_BUG,
        ITEM_MICROCAMERA, ITEM_GOLDENEYEKEY, ITEM_TOKEN, ITEM_PLASTIQUE};
    for (count = 2; count <= 4; count += 2) {
        for (shooter = 0; shooter < count; ++shooter) {
            for (target = 0; target < count; ++target) if (shooter != target) {
                shooting(count, shooter, target);
                melee(count, shooter, target);
            }
            setup(count, shooter, (shooter + 1) % count);
            players[shooter].hands[0].weaponnum = ITEM_TRIGGER;
            players[shooter].hands[1].weaponnum = ITEM_WATCHMAGNETATTRACT;
            players[shooter].hands[0].weapon_firing_status = players[shooter].hands[1].weapon_firing_status = 1;
            players[shooter].magnetattracttime = 50;
            assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
            assert(g_RemoteMineOwnerTriggerFlag == (1 << shooter));
            assert(players[shooter].magnetattracttime == 0);
            players[shooter].hands[0].weaponnum = ITEM_CAMERA;
            players[shooter].hands[1].weaponnum = ITEM_TANKSHELLS;
            assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
            assert(photos == 1 && tanks == 1);
            solo_sight = 1; g_playerPlayerData[shooter].sight = 0;
            ge_original_player_apply_sight_option_exact();
            assert(players[shooter].gunsightmode & GUNSIGHTREASON_1);
            solo_sight = 0; g_playerPlayerData[shooter].sight = 1;
            ge_original_player_apply_sight_option_exact();
            assert(!(players[shooter].gunsightmode & GUNSIGHTREASON_1));
            /* Shotguns record one total shot but trace every original pellet
             * on each firing hand, consuming exactly four RNG draws each. */
            setup(count, shooter, (shooter + 1) % count);
            g_LastOnScreenProp = g_OnScreenPropList;
            players[shooter].hands[0].weaponnum = ITEM_SHOTGUN;
            players[shooter].hands[1].weaponnum = ITEM_AUTOSHOT;
            players[shooter].hands[0].weapon_firing_status = players[shooter].hands[1].weapon_firing_status = 1;
            u64 expected = g_randomSeed;
            for (size_t pellet = 0; pellet < 2U * NUMBER_SHOTGUN_BULLETS * 4U; ++pellet)
                (void)randomGetNextFrom(&expected);
            assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
            assert(g_randomSeed == expected && background_tests == 2U * NUMBER_SHOTGUN_BULLETS);
            assert(g_playerPlayerData[shooter].shot_count[SHOT_REGISTER_TOTAL] == 2);
            for (size_t item = 0; item < sizeof(deferred_items) / sizeof(*deferred_items); ++item) {
                players[shooter].hands[0].weaponnum = players[shooter].hands[1].weaponnum = deferred_items[item];
                assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
                assert(g_randomSeed == expected && background_tests == 2U * NUMBER_SHOTGUN_BULLETS);
            }
        }
    }
    setup(2, 0, 1);
    g_LastOnScreenProp = g_OnScreenPropList;
    players[0].hands[0].weapon_firing_status = 1;
    stan_blocked = 1;
    assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_STAN_TEXTURE_FRONTIER);
    stan_blocked = 0; bg_hit = 1;
    static struct image_sound sound;
    g_HitTypeSounds[0] = &sound; g_HitTypeSounds[5] = &sound;
    g_Textures[0].hitTexture = 0;
    assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_LIGHT_FRONTIER);
    assert(light_calls == 0 && impact_explosions == 1);
    /* Provider failure is explicit; this uses no fabricated light directory.
     * Distinct typed bitfields prove native endian-safe texture selection. */
    ge_original_player_held_items_bind_lights(&light_calls, light_break);
    g_Textures[0].hitSound = 7; g_Textures[0].hitTexture = 5;
    assert(ge_original_player_held_items_tick() == GE_ORIGINAL_PLAYER_HELD_ITEMS_OK);
    assert(light_calls == 1 && impact_explosions == 1);
    ge_original_bond_input_unbind_roster();
    ge_original_bond_input_bind_player(&players[0], &g_playerPlayerData[0]);
    ge_original_bond_input_provider()->player_count = 1;
    solo_sight = 0; g_playerPlayerData[0].sight = 1;
    ge_original_player_apply_sight_option_exact();
    assert(players[0].gunsightmode & GUNSIGHTREASON_1);
    solo_sight = 1; g_playerPlayerData[0].sight = 0;
    ge_original_player_apply_sight_option_exact();
    assert(!(players[0].gunsightmode & GUNSIGHTREASON_1));
    puts("held items: canonical 2/4-player randomized rays, armor/kill dispatch, melee LOS, gadget ownership and sight passed");
    return 0;
}

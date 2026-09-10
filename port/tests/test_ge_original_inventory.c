#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondinv.h"
#include "game/gun.h"
#include "snd.h"
#include "ge_original_inventory.h"
#include "ge_original_bond_input_provider.h"

static struct player player, other_player;
struct player *g_CurrentPlayer;
static InvItem slots[96];
static PropRecord props[4];
static ObjectRecord objects[4], rocket;
static textoverride overrides[4];
GunModelFileRecord gitem_structs[ITEM_JOYPAD + 1];
WeaponStats default_weaponstats;
static WeaponStats weaponstats[ITEM_JOYPAD + 1];
s32 g_bondviewForceDisarm;
int g_curWatchItemIndex, D_800409C4;
ALBank *g_musicSfxBufferPtr;
static int locked, model_calls, freed;
static char names[ITEM_JOYPAD + 1][32];

extern u16 *bondinvGetNameByIndex(s32 index);
extern int bondinvAddPropToInv(PropRecord *prop);
extern void sub_GAME_7F0A8378(void);
extern void advance_through_inventory(void);
extern void backstep_through_inventory(void);

s32 lvlGetControlsLockedFlag(void) { return locked; }
int getPlayerCount(void) { return 1; }
void osSyncPrintf(const char *format, ...) { (void)format; assert(0); }
u16 joyGetButtonsPressedThisFrame(s8 pad, u16 mask)
{ (void)pad; return mask & A_BUTTON; }
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 index, ALSoundState *state)
{ (void)bank; (void)index; (void)state; return NULL; }
void place_item_in_hand_swap_and_make_visible(GUNHAND hand, ITEM_IDS weapon)
{ assert(hand == GUNRIGHT || hand == GUNLEFT); assert(weapon >= 0); ++model_calls; }
void sub_GAME_7F09B368(GUNHAND hand) { (void)hand; assert(0); }
void objFreePermanently(ObjectRecord *object, s32 free_prop)
{ assert(object == &rocket && free_prop == 1); ++freed; }
s32 bondwalkItemHasAmmo(ITEM_IDS weapon) { (void)weapon; return 1; }
u32 bondwalkItemCheckBitflags(ITEM_IDS weapon, u32 mask)
{ (void)weapon; return mask & WEAPONSTATBITFLAG_CAN_DUAL_WIELD; }
const char *ge_original_language_text(uint16_t id)
{
    if (id == 1000) return "Mission decoder\n";
    if (id == 1001) return "Custom pistol\n";
    if (id == 1002) return "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
    if (id >= 1 && id <= ITEM_JOYPAD + 1) return names[id - 1];
    return NULL;
}
u8 *langGet(s32 id) { return (u8 *)ge_original_language_text((uint16_t)id); }
u16 *get_ptr_short_watch_text_for_item(ITEM_IDS weapon)
{ return (u16 *)langGet(gitem_structs[weapon].watch_equipment_text); }

static void reset(void)
{
    memset(&player, 0, sizeof(player));
    memset(slots, 0, sizeof(slots));
    memset(props, 0, sizeof(props));
    memset(objects, 0, sizeof(objects));
    memset(overrides, 0, sizeof(overrides));
    ge_original_bond_input_bind_player(&player, NULL);
    ge_original_bond_input_provider_reset_normal_dam();
    player.p_itemcur = slots;
    player.equipmaxitems = 96;
    player.outside_watch_menu = 1;
    player.hands[0].weaponnum_watchmenu = -1;
    player.hands[1].weaponnum_watchmenu = -1;
    bondinvReinitInv();
    ge_original_inventory_invalidate();
    g_bondviewForceDisarm = locked = model_calls = freed = 0;
    for (int i = 0; i <= ITEM_JOYPAD; ++i) {
        snprintf(names[i], sizeof(names[i]), "Weapon %d", i);
        gitem_structs[i].watch_equipment_text = (u16)(i + 1);
        gitem_structs[i].item_weapon_stats = &weaponstats[i];
        weaponstats[i].AmmoType = i == ITEM_WPPKSIL || i == ITEM_TT33 ? 1 : 0;
    }
}

static size_t find_weapon(const GeOriginalInventorySnapshot *snapshot, int weapon)
{
    for (size_t i = 0; i < snapshot->count; ++i)
        if (snapshot->entries[i].weapon == weapon) return i;
    assert(0); return 0;
}

static void add_prop(int slot, int visible, int override_weapon)
{
    props[slot].type = PROP_TYPE_OBJ;
    props[slot].obj = &objects[slot];
    objects[slot].flags2 = visible ? 0 : 0x40000;
    if (override_weapon >= 0) {
        overrides[slot].obj = &objects[slot];
        overrides[slot].weapon = override_weapon;
        overrides[slot].shorttext = 1000;
        overrides[slot].next = player.textoverrides;
        player.textoverrides = &overrides[slot];
    }
    assert(bondinvAddPropToInv(&props[slot]));
}

static void test_canonical_watch_and_gadget(void)
{
    GeOriginalInventorySnapshot snapshot;
    struct player before, after;
    reset();
    assert(bondinvAddInvItem(ITEM_FIST));
    assert(bondinvAddInvItem(ITEM_WPPKSIL));
    assert(bondinvAddInvItem(ITEM_TT33));
    assert(bondinvAddDoublesInvItem(ITEM_WPPKSIL, ITEM_TT33));
    assert(bondinvAddInvItem(ITEM_CAMERA));
    player.hands[0].weaponnum = ITEM_WPPKSIL;
    player.hands[1].weaponnum = ITEM_TT33;
    player.hands[0].weapon_ammo_in_magazine = 7;
    player.hands[1].weapon_ammo_in_magazine = 5;
    player.hands[0].rocket = &rocket;
    player.ammoheldarr[1] = 20;
    before = player;
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(memcmp(&before, &player, sizeof(player)) == 0);
    assert(snapshot.count == 4 && snapshot.selection_allowed);
    for (size_t i = 0; i < snapshot.count; ++i) {
        assert(snapshot.entries[i].item_identity);
        assert(strcmp(snapshot.entries[i].name,
            (char *)bondinvGetNameByIndex((s32)i)) == 0);
    }
    size_t camera = find_weapon(&snapshot, ITEM_CAMERA);
    assert(ge_original_inventory_select(&snapshot, camera) == GE_ORIGINAL_INVENTORY_OK);
    assert(model_calls == 2 && freed == 1);
    assert(player.ammoheldarr[1] == 32);
    assert(player.hands[0].weapon_ammo_in_magazine == 0);
    assert(player.hands[1].weapon_ammo_in_magazine == 0);
    assert(player.hands[0].previous_weapon == ITEM_WPPKSIL);
    assert(player.hands[1].previous_weapon == ITEM_TT33);
    assert(player.hands[0].weaponnum == ITEM_CAMERA);
    assert(player.hands[1].weaponnum == ITEM_UNARMED);
    after = player;
    player = before;
    g_curWatchItemIndex = (int)camera;
    sub_GAME_7F0A8378(); /* Actual canonical watch selection body. */
    assert(memcmp(&player, &after, sizeof(player)) == 0);
    advance_through_inventory();
    assert(player.hands[0].weapon_next_weapon == ITEM_WPPKSIL);
    assert(player.hands[1].weapon_next_weapon == ITEM_TT33);
    player = after;
    backstep_through_inventory();
    assert(player.hands[0].weapon_next_weapon == ITEM_WPPKSIL);
    assert(player.hands[1].weapon_next_weapon == ITEM_TT33);
    player = after;
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(snapshot.entries[camera].selected);
    before = player;
    assert(ge_original_inventory_select(&snapshot, camera) == GE_ORIGINAL_INVENTORY_NO_CHANGE);
    assert(memcmp(&player, &before, sizeof(player)) == 0);
}

static void test_names_visibility_and_stale(void)
{
    GeOriginalInventorySnapshot snapshot, refreshed;
    reset();
    assert(bondinvAddInvItem(ITEM_FIST));
    assert(bondinvAddInvItem(ITEM_WPPKSIL));
    overrides[3].weapon = ITEM_WPPKSIL;
    overrides[3].shorttext = 1001;
    player.textoverrides = &overrides[3];
    add_prop(0, 1, ITEM_DOORDECODER);
    add_prop(1, 0, -1);
    add_prop(2, 1, -1); /* Key/mission record without an equip mapping. */
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(snapshot.count == 4);
    assert(strcmp(snapshot.entries[1].name, "Custom pistol") == 0);
    assert(strcmp(snapshot.entries[2].name, "Mission decoder") == 0);
    for (size_t i = 0; i < snapshot.count; ++i) {
        const char *canonical_name = (char *)bondinvGetNameByIndex((s32)i);
        size_t length = strcspn(canonical_name, "\r\n");
        assert(strlen(snapshot.entries[i].name) == length);
        assert(strncmp(snapshot.entries[i].name, canonical_name, length) == 0);
    }
    assert(!snapshot.entries[3].selectable);
    assert(ge_original_inventory_select(&snapshot, 3) == GE_ORIGINAL_INVENTORY_NOT_SELECTABLE);
    assert(ge_original_inventory_select(&snapshot, 2) == GE_ORIGINAL_INVENTORY_OK);
    objects[0].flags2 = 0x40000;
    assert(ge_original_inventory_select(&snapshot, 2) == GE_ORIGINAL_INVENTORY_STALE);
    assert(ge_original_inventory_select(&snapshot, 3) == GE_ORIGINAL_INVENTORY_STALE);
    objects[0].flags2 = 0;
    overrides[0].weapon = ITEM_CAMERA;
    assert(ge_original_inventory_select(&snapshot, 2) == GE_ORIGINAL_INVENTORY_STALE);
    overrides[0].weapon = ITEM_DOORDECODER;
    overrides[0].shorttext = 1002;
    assert(ge_original_inventory_snapshot(&refreshed) == GE_ORIGINAL_INVENTORY_OK);
    assert(strlen(refreshed.entries[2].name) == GE_ORIGINAL_INVENTORY_NAME_CAPACITY - 1);
    assert(ge_original_inventory_select(&snapshot, 2) == GE_ORIGINAL_INVENTORY_STALE);
    bondinvRemoveItemByID(ITEM_FIST);
    assert(ge_original_inventory_select(&snapshot, 1) == GE_ORIGINAL_INVENTORY_STALE);
    assert(model_calls == 2);
}

static void test_prop_identity_and_weapon_visibility(void)
{
    GeOriginalInventorySnapshot snapshot;
    reset();
    add_prop(0, 1, ITEM_CAMERA);
    props[0].type = PROP_TYPE_WEAPON;
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(snapshot.count == 0); /* Canonical weapon props need bit 0x400. */
    objects[0].runtime_bitflags = 0x400;
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(snapshot.count == 1 && snapshot.entries[0].weapon == ITEM_CAMERA);
    objects[0].runtime_bitflags = 0;
    assert(ge_original_inventory_select(&snapshot, 0) == GE_ORIGINAL_INVENTORY_STALE);
    objects[0].runtime_bitflags = 0x400;
    props[1] = props[0];
    slots[0].type_inv_item.type_prop.prop = &props[1];
    assert(ge_original_inventory_select(&snapshot, 0) == GE_ORIGINAL_INVENTORY_STALE);
    slots[0].type_inv_item.type_prop.prop = &props[0];
    overrides[0].weapon = ITEM_JOYPAD + 1;
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(!snapshot.entries[0].selectable);
    assert(ge_original_inventory_select(&snapshot, 0) == GE_ORIGINAL_INVENTORY_NOT_SELECTABLE);
    overrides[0].weapon = ITEM_CAMERA;
    overrides[0].shorttext = 65000;
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(strcmp(snapshot.entries[0].name, "Item") == 0);
    assert(model_calls == 0);
}

static void test_state_gates_and_bounds(void)
{
    GeOriginalInventorySnapshot snapshot, blocked;
    GeOriginalBondInputProvider *provider = ge_original_bond_input_provider();
    reset();
    assert(bondinvAddInvItem(ITEM_FIST));
    assert(bondinvAddInvItem(ITEM_WPPKSIL));
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
#define GATE(field, value) do { \
    int old = (field); (field) = (value); \
    struct player before = player; \
    assert(ge_original_inventory_snapshot(&blocked) == GE_ORIGINAL_INVENTORY_OK); \
    assert(!blocked.selection_allowed); \
    assert(ge_original_inventory_select(&snapshot, 1) == GE_ORIGINAL_INVENTORY_BLOCKED); \
    assert(memcmp(&before, &player, sizeof(player)) == 0); \
    (field) = old; \
    assert(ge_original_inventory_select(&blocked, 1) == GE_ORIGINAL_INVENTORY_BLOCKED); \
} while (0)
    GATE(player.bonddead, 1);
    GATE(player.cameramode, 1);
    GATE(player.outside_watch_menu, 0);
    GATE(player.open_close_solo_watch_menu, 1);
    for (int state = 1; state <= 13; ++state) GATE(player.watch_animation_state, state);
    GATE(locked, 1);
    GATE(provider->controls_locked, 1);
    GATE(provider->player_actions_enabled, 0);
    GATE(provider->force_disarm, 1);
    GATE(g_bondviewForceDisarm, 1);
    GATE(provider->stop_play_flag, 1);
    GATE(provider->game_over_flag, 1);
    GATE(provider->player_is_in_tank, 1);
    for (int hand = 0; hand < 2; ++hand) {
        GATE(player.hands[hand].weaponnum_watchmenu, ITEM_FIST);
        GATE(player.hands[hand].weapon_current_animation, 5);
        for (int state = 1; state <= GUN_ANIM_STATE_USE_ITEM; ++state)
            GATE(player.hands[hand].weapon_action_state, state);
    }
#undef GATE
    assert(model_calls == 0);
    assert(ge_original_inventory_select(NULL, 0) == GE_ORIGINAL_INVENTORY_INVALID_ARGUMENT);
    assert(ge_original_inventory_snapshot(NULL) == GE_ORIGINAL_INVENTORY_INVALID_ARGUMENT);
    assert(ge_original_inventory_select(&snapshot, 2) == GE_ORIGINAL_INVENTORY_INVALID_ARGUMENT);
    snapshot.count = (size_t)-1;
    assert(ge_original_inventory_select(&snapshot, 64) == GE_ORIGINAL_INVENTORY_INVALID_ARGUMENT);
    snapshot.count = 2;
    snapshot.entries[1].index = -1;
    assert(ge_original_inventory_select(&snapshot, 1) == GE_ORIGINAL_INVENTORY_STALE);
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    ge_original_inventory_invalidate();
    assert(ge_original_inventory_select(&snapshot, 1) == GE_ORIGINAL_INVENTORY_STALE);
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    other_player = player;
    ge_original_bond_input_bind_player(&other_player, NULL);
    assert(ge_original_inventory_select(&snapshot, 1) == GE_ORIGINAL_INVENTORY_STALE);
    ge_original_bond_input_bind_player(&player, NULL);
    provider->player_pointers[0] = &other_player;
    assert(ge_original_inventory_select(&snapshot, 1) == GE_ORIGINAL_INVENTORY_UNAVAILABLE);
    provider->player_pointers[0] = &player;
    provider->player_count = 2;
    assert(ge_original_inventory_snapshot(&blocked) == GE_ORIGINAL_INVENTORY_UNAVAILABLE);
    provider->player_count = 1;
    slots[0].next = &slots[95];
    assert(ge_original_inventory_snapshot(&blocked) == GE_ORIGINAL_INVENTORY_UNAVAILABLE);
    slots[0].next = NULL;
    assert(ge_original_inventory_snapshot(&blocked) == GE_ORIGINAL_INVENTORY_UNAVAILABLE);
    slots[0].next = &slots[1];
    slots[1].next = &slots[1];
    assert(ge_original_inventory_snapshot(&blocked) == GE_ORIGINAL_INVENTORY_UNAVAILABLE);
    g_CurrentPlayer = NULL;
    assert(ge_original_inventory_snapshot(&blocked) == GE_ORIGINAL_INVENTORY_UNAVAILABLE);
}

static void test_virtual_and_capacity(void)
{
    GeOriginalInventorySnapshot snapshot;
    reset();
    player.equipallguns = 1;
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(snapshot.count == ITEM_TANKSHELLS);
    for (size_t i = 0; i < snapshot.count; ++i) {
        assert(snapshot.entries[i].weapon == (int)i + 1);
        assert(snapshot.entries[i].item_identity == 0);
        assert(snapshot.entries[i].selectable);
        assert(strcmp(snapshot.entries[i].name,
            (char *)bondinvGetNameByIndex((s32)i)) == 0);
    }
    assert(ge_original_inventory_select(&snapshot, ITEM_WPPKSIL - 1) == GE_ORIGINAL_INVENTORY_OK);
    player.equipallguns = 0;
    assert(ge_original_inventory_select(&snapshot, ITEM_TT33 - 1) == GE_ORIGINAL_INVENTORY_STALE);
    reset();
    for (int weapon = 0; weapon <= ITEM_JOYPAD; ++weapon)
        assert(bondinvAddInvItem((ITEM_IDS)weapon));
    assert(ge_original_inventory_snapshot(&snapshot) == GE_ORIGINAL_INVENTORY_OK);
    assert(snapshot.count == GE_ORIGINAL_INVENTORY_CAPACITY);
    assert(snapshot.total_count == ITEM_JOYPAD + 1);
    assert(ge_original_inventory_select(&snapshot, 63) == GE_ORIGINAL_INVENTORY_OK);
}

int main(void)
{
    test_canonical_watch_and_gadget();
    test_names_visibility_and_stale();
    test_prop_identity_and_weapon_visibility();
    test_state_gates_and_bounds();
    test_virtual_and_capacity();
    return 0;
}

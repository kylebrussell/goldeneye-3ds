#include "ge_original_bond_input_provider.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include <bondconstants.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "music.h"
#include "snd.h"

ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand);
s8 get_hands_firing_status(GUNHAND hand);
f32 getCurrentPlayerNoise(GUNHAND hand);
s32 get_ammo_in_hands_magazine(GUNHAND hand);
void gunSetAimType(s32 value);
void gunSetSightVisible(s32 reason, bool visible);
void currentPlayerSetSwayTarget(s32 value);
void currentPlayerAdjustCrouchPos(s32 value);
int bondviewGetIfCurrentPlayerDamageShowTime(void);
void bondviewSetVisibleToGuardsFlag(s32 value);
s32 bondviewGetVisibleToGuardsFlag(void);
InvItem *bondinvGetInvItem(ITEM_IDS item);
int bondinvHasInvItem(ITEM_IDS item);
bool bondinvIsAliveWithFlag(void);
void sub_GAME_7F067F58(f32 turn_x, f32 turn_y, f32 max_aim_lock_speed);
void give_cur_player_ammo(s32 ammo_type, s32 ammo_amount);
void add_ammo_to_weapon(ITEM_IDS weapon, s32 ammo);
int bondinvAddInvItem(ITEM_IDS item);
void bondinvRemoveItemByID(ITEM_IDS weaponnum);
void bondviewGetCollisionRadius(
    PropRecord *prop, f32 *collision_radius, f32 *height, f32 *always_30);
void trigger_remote_mine_detonation(void);
extern s32 g_RemoteMineOwnerTriggerFlag;
extern ChrRecord *g_ChrSlots;
extern s32 g_NumChrSlots;
void chrCheckGuardsHeardSound(f32 noise);
void trigger_solo_watch_menu(s32 arg0);

ALBank *g_musicSfxBufferPtr;
f32 watch_transition_time = 0.90909088f;
static s16 played_sfx = -1;
static unsigned watch_zoom_default_calls;
static unsigned watch_page_reset_calls;

void bondviewTriggerWatchZoomDefault(void)
{
    ++watch_zoom_default_calls;
}

void sub_GAME_7F0A69A8(void)
{
    ++watch_page_reset_calls;
}

/* The retained trigger also contains the fully-open close branch.  These
 * services are outside this focused transition test, but remain strict link
 * dependencies rather than weak production fallbacks. */
void deleteCurrentSelectedFolder(void)
{
}

void sub_GAME_7F0C1340(void)
{
}

ALSoundState *sndPlaySfx(
    struct ALBankAlt_s *sound_bank, s16 sound_index,
    ALSoundState *pending_state)
{
    assert(sound_bank == (struct ALBankAlt_s *)g_musicSfxBufferPtr);
    assert(pending_state == NULL);
    played_sfx = sound_index;
    return NULL;
}

int main(void)
{
    struct player player;
    GeOriginalBondInputProvider *provider;

    memset(&player, 0, sizeof(player));
    provider = ge_original_bond_input_provider();
    ge_original_bond_input_bind_player(&player, NULL);
    ge_original_bond_input_provider_reset_normal_dam();
    assert(provider->player_pointers[0] == &player);

    /* START reaches this unchanged body from bondviewProcessInput.  Exercise
     * the complete canonical open-state mutation and its authored watch
     * geometry construction, rather than merely proving that the symbol is
     * retained in the generated object. */
    player.hand_invisible[GUNLEFT] = 1;
    player.hand_invisible[GUNRIGHT] = 1;
    trigger_solo_watch_menu(0);
    assert(player.watch_animation_state == WATCH_ANIMATION_0x1);
    assert(player.watch_pause_time == 0);
    assert(player.timer_1C4 == 0);
    assert(player.pause_state == 0);
    assert(watch_zoom_default_calls == 1);
    assert(watch_page_reset_calls == 1);
    assert(fabsf(watch_transition_time - 1.0f) < 0.00001f);
    assert(player.buffer_for_watch_greenbackdrop_DL[0].words.w0 != 0);
    assert(player.buffer_for_watch_static_DL[0].words.w0 != 0);

    /* A second START while the watch is raising takes the exact close branch. */
    trigger_solo_watch_menu(0);
    assert(player.watch_animation_state == WATCH_ANIMATION_0x9);
    assert(watch_zoom_default_calls == 1);
    assert(watch_page_reset_calls == 1);

    memset(&player, 0, sizeof(player));
    watch_transition_time = 0.90909088f;
    ge_original_bond_input_bind_player(&player, NULL);
    ge_original_bond_input_provider_reset_normal_dam();

    player.hands[GUNRIGHT].weaponnum = ITEM_AK47;
    player.hands[GUNRIGHT].weapon_firing_status = 1;
    player.hands[GUNRIGHT].noise = 17.5f;
    player.hands[GUNRIGHT].weapon_ammo_in_magazine = 7;
    assert(getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_AK47);
    assert(get_hands_firing_status(GUNRIGHT) == 1);
    assert(getCurrentPlayerNoise(GUNRIGHT) == 17.5f);
    assert(get_ammo_in_hands_magazine(GUNRIGHT) == 7);

    player.hands[GUNRIGHT].weaponnum = ITEM_WPPKSIL;
    player.c_screenwidth = 320.0f;
    player.c_screenheight = 240.0f;
    player.c_halfwidth = 160.0f;
    player.c_halfheight = 120.0f;
    player.c_scalex = 1.0f;
    player.c_scaley = 1.0f;
    provider->clock_timer = 1;
    sub_GAME_7F067F58(0.1f, -0.1f, 0.8344f);
    assert(isfinite(player.crosshair_angle.f[0]));
    assert(isfinite(player.crosshair_angle.f[1]));
    assert(isfinite(player.hands[GUNRIGHT].field_A38));

    player.hands[GUNRIGHT].weaponnum = ITEM_WPPK;
    give_cur_player_ammo(AMMO_9MM, 100);
    assert(player.ammoheldarr[AMMO_9MM] == 100);
    add_ammo_to_weapon(ITEM_WPPK, 900);
    assert(player.ammoheldarr[AMMO_9MM] == 0x320);

    g_RemoteMineOwnerTriggerFlag = 0;
    provider->player_number = 2;
    trigger_remote_mine_detonation();
    assert(g_RemoteMineOwnerTriggerFlag == (1 << 2));
    assert(played_sfx == WATCH_DETONATE_MINE_SFX);
    provider->player_number = 0;

    {
        PropRecord player_prop;
        PropRecord guard_prop;
        ChrRecord guard;
        memset(&player_prop, 0, sizeof(player_prop));
        memset(&guard_prop, 0, sizeof(guard_prop));
        memset(&guard, 0, sizeof(guard));
        player.prop = &player_prop;
        player_prop.stan = (StandTile *)(uintptr_t)0x1234;
        guard.prop = &guard_prop;
        guard.model = (Model *)(uintptr_t)1;
        guard.hearingscale = 1.0f;
        guard_prop.pos.x = 3.0f;
        guard_prop.pos.y = 4.0f;
        g_ChrSlots = &guard;
        g_NumChrSlots = 1;
        provider->global_timer = 123;
        chrCheckGuardsHeardSound(0.04f);
        assert((guard.hidden & CHRHIDDEN_ALERT_GUARD_RELATED) == 0);
        chrCheckGuardsHeardSound(0.06f);
        assert((guard.hidden & CHRHIDDEN_ALERT_GUARD_RELATED) != 0);
        assert(guard.lastheartarget60 == 123);
        assert(guard.lastknowntargetpos.x == 0.0f);
        assert(guard.lastknowntargetpos.y == 0.0f);
        assert(guard.lastknowntargetpos.z == 0.0f);
        assert(guard.targetTile == player_prop.stan);
        g_ChrSlots = NULL;
        g_NumChrSlots = 0;
    }

    {
        PropRecord prop;
        f32 radius;
        f32 height;
        f32 lower;
        memset(&prop, 0, sizeof(prop));
        player.prop = &prop;
        player.field_488.collision_radius = 30.0f;
        player.eyeheight = 159.0f;
        player.field_88 = 2.0f;
        player.ducking_height_offset = -4.0f;
        bondviewGetCollisionRadius(&prop, &radius, &height, &lower);
        assert(radius == 30.0f);
        assert(height == 137.0f);
        assert(lower == 30.0f);
    }

    gunSetAimType(3);
    assert(player.aimtype == 3);
    gunSetSightVisible(4, FALSE);
    assert((player.gunsightmode & 4) != 0);
    gunSetSightVisible(4, TRUE);
    assert((player.gunsightmode & 4) == 0);

    currentPlayerSetSwayTarget(-1);
    assert(player.swaytarget == -75.0f);
    player.crouchpos = CROUCH_STAND;
    currentPlayerAdjustCrouchPos(-1);
    assert(player.crouchpos == CROUCH_HALF);
    currentPlayerAdjustCrouchPos(-1);
    assert(player.crouchpos == CROUCH_SQUAT);
    currentPlayerAdjustCrouchPos(8);
    assert(player.crouchpos == CROUCH_STAND);

    player.damageshowtime = -1;
    assert(!bondviewGetIfCurrentPlayerDamageShowTime());
    player.damageshowtime = 0;
    assert(bondviewGetIfCurrentPlayerDamageShowTime());
    bondviewSetVisibleToGuardsFlag(1);
    assert(bondviewGetVisibleToGuardsFlag() == 1);
    bondviewSetVisibleToGuardsFlag(0);
    assert(bondviewGetVisibleToGuardsFlag() == 0);

    {
        InvItem slots[3];
        InvItem token;
        memset(slots, 0, sizeof(slots));
        slots[0].type = -1;
        slots[1].type = -1;
        slots[2].type = -1;
        player.p_itemcur = slots;
        player.equipmaxitems = 3;
        player.ptr_inventory_first_in_cycle = NULL;
        assert(bondinvAddInvItem(ITEM_WPPK));
        assert(bondinvAddInvItem(ITEM_FIST));
        assert(bondinvHasInvItem(ITEM_WPPK));
        assert(bondinvHasInvItem(ITEM_FIST));
        assert(!bondinvAddInvItem(ITEM_WPPK));
        bondinvRemoveItemByID(ITEM_WPPK);
        assert(!bondinvHasInvItem(ITEM_WPPK));
        assert(bondinvHasInvItem(ITEM_FIST));

        memset(&token, 0, sizeof(token));
        token.type = INV_ITEM_WEAPON;
        token.type_inv_item.type_weap.weapon = ITEM_TOKEN;
        token.next = &token;
        player.ptr_inventory_first_in_cycle = &token;
        assert(bondinvGetInvItem(ITEM_TOKEN) == &token);
        assert(bondinvHasInvItem(ITEM_TOKEN));
        assert(bondinvIsAliveWithFlag());
        player.bonddead = TRUE;
        assert(!bondinvIsAliveWithFlag());
    }
    return 0;
}

#include "ge_original_multiplayer_players.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_player_spawn_internal.h"
#include "ge_original_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrObjRandom.h"
#include "joy.h"
#include <random.h>
extern struct player_data g_playerPlayerData[4];
extern struct player_data *g_playerPerm;
void init_player_BONDdata(void);
extern s32 random_byte;

static void run_roster(uint32_t count)
{
    GeOriginalInputSample inputs[4] = {{0}};
    struct player *first = NULL;
    const u64 seed = UINT64_C(0x91a2345678123456);
    const u64 chr_seed = UINT64_C(0x873456781234abcd);
    u64 expected_rng = seed;
    const s32 expected_byte = (s32)(randomGetNextFrom(&expected_rng) & 0xff);
    const u64 previous_rng = g_randomSeed, previous_chr_rng = g_chrObjRandomSeed;
    const int32_t previous_control = ge_original_bond_input_control_style();
    uint32_t p;
    assert(ge_original_multiplayer_players_create(count, count - 1, seed, chr_seed));
    assert(g_randomSeed == expected_rng && random_byte == expected_byte+(s32)count);
    assert(g_chrObjRandomSeed == chr_seed);
    assert(getPlayerCount() == (s32)count);
    assert(get_cur_playernum() == (s32)count - 1);
    assert(joyGetControllerCount() == (s8)(count * 2));
    assert(!ge_original_multiplayer_players_create(count, 0, 0, 0));
    {
        struct player_data *permissions[4];
        GeOriginalBondInputProvider *provider = ge_original_bond_input_provider();
        for (p = 0; p < count; ++p) permissions[p] = &g_playerPlayerData[p];
        assert(ge_original_bond_input_bind_roster(provider->player_pointers,
            permissions, count));
    }
    for (p = 0; p < count; ++p) {
        struct player *player = ge_original_multiplayer_player(p);
        assert(player && player != first);
        if (!p) first = player;
        assert(player->bondhealth == 1 && !player->bonddead);
        assert(player->damageshowtime == -1 && player->registeredroom == -1);
        assert(player->startnewbonddie && player->resetheadpos);
        assert(player->hands[0].weaponnum_watchmenu == -1);
        assert(player->hands[1].weaponnum_watchmenu == -1);
        assert(player->viewports[0].vp.vscale[0] == 0x280);
        assert(player->c_screenwidth == 320 && player->c_screenheight == 240);
        assert(player->last_kill_time[3] == -1);
        assert(player->field_29E0 == expected_byte+(s32)p);
        assert(g_playerPlayerData[p].handicap == 1);
        assert(ge_original_multiplayer_player_bind_inventory(p));
        if (p) assert(player->p_itemcur != first->p_itemcur);
        inputs[p].look_x = (float)(p + 1) / 4;
        inputs[p].move_y = -(float)(p + 1) / 4;
        inputs[p].buttons = (uint16_t)(A_BUTTON >> p);
        inputs[p].buttons_secondary = (uint16_t)(Z_TRIG >> p);
    }
    assert(ge_original_input_tick_players(inputs, count));
    ge_original_bond_input_set_timers(1, 47, 1.0f);
    for (p = 0; p < count; ++p) {
        GeOriginalBondInputFrame frame;
        assert(ge_original_multiplayer_select_player(p));
        assert(ge_original_spawn_player_get() == g_playerPointers[p]);
        assert(g_playerPerm == &g_playerPlayerData[p]);
        assert(ge_original_bond_input_provider()->player_number == (s32)p);
        assert(ge_original_bond_input_provider()->player_count == (s32)count);
        assert(ge_original_bond_input_provider()->global_timer == 47);
        ge_original_input_read_bond_frame(0x1234, &frame);
        assert(frame.stick_x == (s8)((p + 1) * 15));
        assert(frame.buttons == inputs[p].buttons && frame.oldbuttons == 0x1234);
        assert(joyGetStickY((s8)(p + count)) == -(s8)((p + 1) * 15));
        assert(joyGetButtons((s8)(p + count), 0xffff) == inputs[p].buttons_secondary);
        assert(ge_original_input_pressed() == inputs[p].buttons);
        ge_original_bond_input_provider()->fov_y = 30 + (float)p;
        g_CurrentPlayer->bondhealth = (float)p / 4;
        g_CurrentPlayer->hands[0].weapon_ammo_in_magazine = 5 + (s32)p;
    }
    /* Canonical damage attribution temporarily changes current player without
     * calling the native select API. All gameplay pointer accessors follow it. */
    for (p = 0; p < count; ++p) {
        set_cur_player((s32)p);
        assert(ge_original_spawn_player_get() == g_playerPointers[p]);
        assert(ge_original_bond_input_provider()->fov_y == 30 + (float)p);
        assert(g_CurrentPlayer->hands[0].weapon_ammo_in_magazine == 5 + (s32)p);
        assert(g_CurrentPlayer->bondhealth == (float)p / 4);
    }
    /* Held input has no new edge, independent of the chosen display player. */
    assert(ge_original_input_tick_players(inputs, count));
    for (p = 0; p < count; ++p) {
        assert(ge_original_multiplayer_select_player(p));
        assert(ge_original_input_pressed() == 0);
    }
    assert(!ge_original_multiplayer_select_player(count));
    assert(!ge_original_input_tick_players(inputs, count - 1));
    {
        GeOriginalInputPad pads[4][2] = {{{0}}};
        for (p = 0; p < count; ++p) {
            assert(ge_original_input_sample_pads(&inputs[p], pads[p]));
            assert(pads[p][0].stick_x == (s8)((p + 1) * 15));
            pads[p][0].stick_x = (int8_t)(80 + p);
            pads[p][1].stick_y = (int8_t)(-80 - (int)p);
        }
        assert(ge_original_input_tick_pad_pairs(pads, count));
        for (p = 0; p < count; ++p) {
            assert(ge_original_multiplayer_select_player(p));
            assert(joyGetStickX((s8)p) == (s8)(80 + p));
            assert(joyGetStickY((s8)(p + count)) == (s8)(-80 - (int)p));
        }
        assert(!ge_original_input_tick_pad_pairs(NULL, count));
        assert(!ge_original_input_tick_pad_pairs(pads, count - 1));
    }
    assert(ge_original_multiplayer_select_local_player());
    init_player_BONDdata();
    assert(g_CurrentPlayer->bondhealth == 1);
    assert(first->bondhealth == 0); /* reset of local life cannot reset slot 0 */
    ge_original_multiplayer_players_destroy();
    assert(g_randomSeed == previous_rng && g_chrObjRandomSeed == previous_chr_rng);
    assert(ge_original_bond_input_control_style() == previous_control);
    assert(ge_original_bond_input_provider()->control_type == previous_control);
    assert(!ge_original_multiplayer_player_count());
    assert(!ge_original_multiplayer_player(0));
}
int main(void)
{
    u64 seed;
    ge_original_input_init();
    ge_original_input_set_dual_analog(1);
    /* A match's forced two-pad style cannot overwrite the solo preference. */
    ge_original_bond_input_set_control_style(CONTROLLER_CONFIG_HONEY);
    randomSetSeed(42); seed = g_randomSeed;
    g_chrObjRandomSeed = UINT64_C(0xa765432189abcdef);
    assert(!ge_original_multiplayer_players_create(1, 0, 0, 0));
    assert(!ge_original_multiplayer_players_create(5, 0, 0, 0));
    assert(!ge_original_multiplayer_players_create(4, 4, 0, 0));
    run_roster(2); assert(g_randomSeed == seed);
    run_roster(4); assert(g_randomSeed == seed);
    run_roster(3); assert(g_randomSeed == seed);
    assert(!ge_original_input_configure_players(0));
    assert(!ge_original_input_configure_players(5));
    ge_original_multiplayer_players_destroy();
    puts("canonical multiplayer player allocation/input ownership tests passed (2/3/4 slots)");
    return 0;
}

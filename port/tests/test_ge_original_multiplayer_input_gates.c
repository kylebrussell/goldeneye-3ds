/* The live adapter and full original bondviewProcessInput/speed helpers run
 * here. MoveBond physics, input transport and gun/camera presentation are
 * explicit observing boundaries, not a simulated world or damage fixture. */
#include "ge_original_bond_live.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"

extern struct player *g_CurrentPlayer;
s32 g_stopPlayFlag, g_gameOverFlag, g_ClockTimer, g_GlobalTimer;
f32 g_GlobalTimerDelta;
s32 g_VisibleToGuardsFlag;
static s32 count, controls_locked;
static struct player players[4];
static struct player_data permissions[4];
static GeOriginalBondInputFrame frame;
static GeOriginalInputPad second_pad;
static int dual;
static unsigned move_calls, input_calls, published, gun_calls;
static s32 trigger;

s32 getPlayerCount(void) { return count; }
s32 lvlGetControlsLockedFlag(void) { return controls_locked; }
struct player *ge_original_spawn_player_get(void) { return g_CurrentPlayer; }
void ge_original_input_read_bond_frame(uint16_t previous, GeOriginalBondInputFrame *out)
{ ++input_calls; *out = frame; out->oldbuttons = previous; }
void bondviewProcessInput(s8, s8, u16, u16);
void MoveBond(s8 x, s8 y, u16 buttons, u16 previous)
{ ++move_calls; bondviewProcessInput(x, y, buttons, previous); }
void ge_port_bond_movement_publish(struct player *player)
{ assert(player == g_CurrentPlayer); ++published; }
void fixture_gun_trigger(s32 value) { ++gun_calls; trigger = value; }
s8 joyGetStickX(s32 controller)
{ assert(dual && controller==ge_original_bond_input_provider()->player_number+count); return second_pad.stick_x; }
s8 joyGetStickY(s32 controller)
{ assert(dual && controller==ge_original_bond_input_provider()->player_number+count); return second_pad.stick_y; }
u16 joyGetButtons(s32 controller, u32 mask)
{ assert(dual && controller==ge_original_bond_input_provider()->player_number+count); return second_pad.buttons & mask; }

static void run_roster(s32 n, int two_pads)
{
    struct player *roster[4];
    struct player_data *perms[4];
    GeOriginalBondLiveState states[4] = {0};
    count=n; dual=two_pads;
    memset(players,0,sizeof(players));
    memset(permissions,0,sizeof(permissions));
    for(s32 slot=0;slot<n;++slot) { roster[slot]=&players[slot]; perms[slot]=&permissions[slot]; states[slot].initialized=1; }
    if(n>1) assert(ge_original_bond_input_bind_roster(roster,perms,n));
    else { ge_original_bond_input_bind_player(roster[0],perms[0]); ge_original_bond_input_provider_reset_normal_dam(); }
    /* Run/release every gate with a different selected player order. Native
     * flags are deliberately changed after selection, without touching provider. */
    for(s32 phase=0;phase<9;++phase) {
        for(s32 position=0;position<n;++position) {
            s32 slot=n-1-position;
            g_CurrentPlayer=roster[slot];
            struct player *player=g_CurrentPlayer;
            GeOriginalBondInputProvider *provider=ge_original_bond_input_provider();
            g_stopPlayFlag=phase==1;
            g_gameOverFlag=phase==3 ? 300 : 0;
            controls_locked=phase==5;
            player->mpmenuon=phase==7 && slot==n-1;
            provider->control_type=dual ? CONTROLLER_CONFIG_PLENTY : CONTROLLER_CONFIG_HONEY;
            if(n==1) {
                /* Solo keeps its established explicit provider contract. */
                provider->controls_locked=controls_locked;
                provider->player_actions_enabled=1;
            }
            player->speedforwards=player->speedsideways=player->speedtheta=0;
            player->speedgo=player->speedstrafe=0;
            player->speedboost=1;
            player->damageshowtime=-1;
            player->hands[0].weaponnum=ITEM_WPPK;
            player->hands[1].weaponnum=ITEM_UNARMED;
            second_pad=(GeOriginalInputPad){0,65,75};
            frame=(GeOriginalBondInputFrame){35,75,Z_TRIG|B_BUTTON|R_CBUTTONS,0};
            unsigned old_moves=move_calls, old_published=published, old_gun=gun_calls, old_input=input_calls;
            assert(ge_original_bond_move_live_tick(&states[slot],1,100+phase,1.0f));
            assert(move_calls==old_moves+1 && published==old_published+1);
            assert(gun_calls==old_gun+1 && input_calls==old_input+1);
            assert(states[slot].move_tick_count==(unsigned)phase+1);
            assert(g_ClockTimer==1 && g_GlobalTimer==100+phase && g_GlobalTimerDelta==1);
            assert(provider->global_timer==100+phase && provider->clock_timer==1);
            int movement_allowed=n==1 ? !controls_locked : !(g_stopPlayFlag||g_gameOverFlag||controls_locked||player->mpmenuon);
            assert((player->speedforwards>0)==movement_allowed);
            assert((player->speedsideways>0)==movement_allowed);
            assert((player->field_D0!=0)==movement_allowed);
            /* Original input records raw trigger during menu/control lock.
             * Original gun hand-state code separately gates actual firing. */
            assert(trigger==(n==1 || (!g_stopPlayFlag && !g_gameOverFlag)));
            if(n>1) {
                assert(provider->stop_play_flag==g_stopPlayFlag);
                assert(provider->game_over_flag==g_gameOverFlag);
                assert(provider->controls_locked==controls_locked);
                assert(provider->player_actions_enabled==(!g_stopPlayFlag && !player->mpmenuon));
            }
        }
    }
    /* The input-only boundary must carry the same refreshed gates. */
    g_CurrentPlayer=roster[0]; g_stopPlayFlag=1; controls_locked=1;
    assert(ge_original_bond_live_tick(&states[0],1,1000,1));
    assert(states[0].input_tick_count==1 && trigger==(n==1));
    ge_original_bond_input_unbind_roster();
}
int main(void)
{
    run_roster(4,0);run_roster(2,0);run_roster(1,0);
    run_roster(4,1);run_roster(2,1);run_roster(1,1);
    puts("input gates: real full input and speed bodies, per-slot 2/4 stop/menu/lock/release, uninterrupted live ticks and solo passed");
    return 0;
}

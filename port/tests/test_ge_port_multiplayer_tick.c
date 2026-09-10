#include "ge_port.h"
#include "ge_original_level.h"
#include "ge_original_bond_input_provider.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/frametiming.h"
#include "joy.h"
#include <random.h>

struct player_data g_playerPlayerData[4], *g_playerPerm;
s32 player_num;
extern s32 currentFrameCounter;
static unsigned consumes, tlb_ticks, subsystems[GE_ORIGINAL_LEVEL_SUBSYSTEM_COUNT];
static int paused;
static void record(struct contsample *samples, s32 start, s32 end)
{
    (void)samples; (void)start; (void)end; ++consumes;
}
static int32_t is_paused(void *context) { (void)context; return paused; }
static void tlb(void *context) { (void)context; ++tlb_ticks; }
static void subsystem(GeOriginalLevelSubsystem which, void *context)
{
    (void)context; assert(which < GE_ORIGINAL_LEVEL_SUBSYSTEM_COUNT);
    ++subsystems[which];
}

static void conversion(void)
{
    GePortInput input = {.move_x=1,.move_y=-1,.look_x=-0.5f,.look_y=0.5f,
        .held=GE_PORT_ACTION_FIRE|GE_PORT_ACTION_USE|GE_PORT_ACTION_AIM,
        .pressed=GE_PORT_ACTION_PREV_WEAPON|GE_PORT_ACTION_PAUSE};
    GeOriginalInputPad pads[2], unchanged[2];
    unsigned before = consumes;
    ge_original_input_set_dual_analog(1);
    ge_original_input_set_full_stick_range(0);
    assert(ge_port_input_sample_pads(&input,pads));
    assert(pads[0].buttons==(Z_TRIG|B_BUTTON|A_BUTTON|START_BUTTON));
    assert(pads[1].buttons==Z_TRIG);
    assert(pads[0].stick_x==-30&&pads[0].stick_y==30);
    assert(pads[1].stick_x==60&&pads[1].stick_y==-60);
    ge_original_input_set_full_stick_range(1);
    assert(ge_port_input_sample_pads(&input,pads));
    assert(pads[0].stick_x==-40&&pads[1].stick_x==80);
    ge_original_input_set_dual_analog(0);
    input.held |= GE_PORT_ACTION_CROUCH;
    assert(ge_port_input_sample_pads(&input,pads));
    assert(pads[0].buttons==(Z_TRIG|B_BUTTON|A_BUTTON|START_BUTTON|L_TRIG|R_TRIG|L_CBUTTONS|U_CBUTTONS));
    assert(!pads[1].buttons&&pads[0].stick_x==80&&pads[1].stick_x==-40);
    memcpy(unchanged,pads,sizeof(pads));
    assert(!ge_port_input_sample_pads(NULL,pads));
    assert(!memcmp(unchanged,pads,sizeof(pads)));
    assert(!ge_port_input_sample_pads(&input,NULL));
    assert(consumes==before);
}

static void rejected(GePortState *state, const GeOriginalInputPad (*pads)[2], uint32_t count)
{
    GePortState before=*state;
    GeOriginalLevelTimerState timers_before, timers_after;
    unsigned before_consumes=consumes, before_tlb=tlb_ticks;
    s32 frame_before=currentFrameCounter;
    u64 rng=g_randomSeed;
    uint16_t buttons[8], edges[8];
    unsigned i;
    for(i=0;i<8;++i){buttons[i]=joyGetButtons((s8)i,0xffff);edges[i]=joyGetButtonsPressedThisFrame((s8)i,0xffff);}
    ge_original_level_timer_snapshot(&timers_before);
    assert(!ge_port_advance_multiplayer_tick(state,pads,count));
    ge_original_level_timer_snapshot(&timers_after);
    assert(!memcmp(state,&before,sizeof(before)));
    assert(!memcmp(&timers_before,&timers_after,sizeof(timers_before)));
    assert(consumes==before_consumes&&tlb_ticks==before_tlb);
    assert(currentFrameCounter==frame_before&&g_randomSeed==rng);
    for(i=0;i<8;++i){assert(joyGetButtons((s8)i,0xffff)==buttons[i]);assert(joyGetButtonsPressedThisFrame((s8)i,0xffff)==edges[i]);}
}

static void roster(unsigned count)
{
    GePortState state;
    struct player players[4]={{0}};
    struct player *pointers[4];
    struct player_data *permissions[4];
    GeOriginalInputPad pads[4][2]={{{0}}};
    uint16_t previous[4][2]={{0}};
    GeOriginalLevelProviders providers={.is_paused=is_paused,
        .reset_tlb_entries=tlb,.tick_subsystem=subsystem};
    unsigned slot,tick;
    s32 frame_before;
    const u64 rng=UINT64_C(0x8765432189abcdef);
    ge_port_init(&state);
    ge_original_level_init(&providers);
    assert(ge_port_start_stage(&state,LEVELID_DAM));
    for(slot=0;slot<count;++slot){pointers[slot]=&players[slot];permissions[slot]=&g_playerPlayerData[slot];}
    assert(ge_original_bond_input_bind_roster(pointers,permissions,count));
    assert(ge_original_input_configure_players(count));
    ge_original_input_set_dual_analog(1);
    ge_original_input_set_full_stick_range(0); /* Raw +/-80 must bypass this. */
    consumes=tlb_ticks=0;paused=0;memset(subsystems,0,sizeof(subsystems));
    joySetRecordFunc(record);
    g_randomSeed=rng;frame_before=currentFrameCounter;
    state.accumulator_seconds=0.009;
    state.dropped_simulation_ticks=7;
    for(tick=0;tick<64;++tick){
        for(slot=0;slot<count;++slot){
            GePortInput input={.held=GE_PORT_ACTION_USE|GE_PORT_ACTION_AIM};
            input.held |= tick%4<2?GE_PORT_ACTION_FIRE:0;
            input.pressed = tick%5==slot?GE_PORT_ACTION_PREV_WEAPON:0;
            assert(ge_port_input_sample_pads(&input,pads[slot]));
            pads[slot][0].stick_x=(int8_t)(80-(int)slot);
            pads[slot][1].stick_y=(int8_t)(-80+(int)slot);
        }
        slot=tick%count;
        set_cur_player((s32)slot);assert(ge_original_input_select_player(slot));
        assert(ge_port_advance_multiplayer_tick(&state,pads,count)==1);
        assert(consumes==tick+1&&tlb_ticks==tick+1);
        assert(state.simulation_ticks==tick+1&&currentFrameCounter==frame_before+(s32)tick+1);
        assert(state.original_clock_timer==1&&state.original_global_timer_delta==1);
        assert(state.original_global_timer==(s32)tick+1&&state.original_active_frame_updates==(s32)tick+1);
        assert(speedgraphframes==1&&state.original_stage==LEVELID_DAM);
        assert(state.accumulator_seconds==0.009&&state.dropped_simulation_ticks==7);
        assert(g_randomSeed==rng);
        assert(state.original_buttons==pads[slot][0].buttons);
        assert(state.original_buttons_pressed==(uint16_t)(pads[slot][0].buttons&~previous[slot][0]));
        for(slot=0;slot<count;++slot){
            unsigned hand;
            assert(joyGetStickX((s8)slot)==80-(s8)slot);
            assert(joyGetStickY((s8)(slot+count))==-80+(s8)slot);
            for(hand=0;hand<2;++hand){
                s8 controller=(s8)(slot+hand*count);
                assert(joyGetButtons(controller,0xffff)==pads[slot][hand].buttons);
                assert(joyGetButtonsPressedThisFrame(controller,0xffff)==(uint16_t)(pads[slot][hand].buttons&~previous[slot][hand]));
                previous[slot][hand]=pads[slot][hand].buttons;
            }
        }
    }
    assert(subsystems[GE_ORIGINAL_LEVEL_SUBSYSTEM_INITIAL_CHEATS]==1);
    assert(subsystems[GE_ORIGINAL_LEVEL_SUBSYSTEM_CHRPROP]==64);
    /* These are observed no-world dispatch boundaries, not fabricated props. */
    rejected(&state,pads,1);rejected(&state,pads,5);rejected(&state,NULL,count);
    for(unsigned reason=0;reason<2;++reason) {
        paused=reason==0;
        ge_original_level_set_controls_locked(reason==1);
        for(slot=0;slot<count;++slot) pads[slot][0].buttons=reason?A_BUTTON:START_BUTTON;
        assert(ge_port_advance_multiplayer_tick(&state,pads,count)==1);
        assert(consumes==65+reason&&tlb_ticks==65+reason&&state.simulation_ticks==65+reason);
        assert(currentFrameCounter==frame_before+65+(s32)reason&&speedgraphframes==1);
        /* Both native pause branches dispatch watch input at real cadence
         * while stopping only the gameplay clock, elapsed time and movement. */
        assert(state.original_clock_timer==0&&state.original_global_timer_delta==0);
        assert(state.original_global_timer==64&&state.original_active_frame_updates==64);
        GeOriginalLevelTimerState timers;ge_original_level_timer_snapshot(&timers);
        assert(timers.controls_locked==(int32_t)(reason==1));
        for(slot=0;slot<count;++slot)
            assert(joyGetButtonsPressedThisFrame((s8)slot,A_BUTTON|START_BUTTON)==(reason?A_BUTTON:START_BUTTON));
        assert(g_randomSeed==rng);
    }
    paused=0;ge_original_level_set_controls_locked(0);
    set_cur_player(0);assert(ge_original_input_select_player(1));rejected(&state,pads,count);
    assert(ge_original_input_select_player(0));
    g_playerPointers[1]=g_playerPointers[0];rejected(&state,pads,count);g_playerPointers[1]=pointers[1];
    state.simulation_ticks=UINT64_MAX;rejected(&state,pads,count);state.simulation_ticks=66;
    assert(ge_original_input_configure_players(1));rejected(&state,pads,count);
    assert(ge_original_input_configure_players(count));
    assert(ge_original_input_ramrom_bind(2));rejected(&state,pads,count);
    joySetRecordFunc(NULL);
    ge_original_bond_input_unbind_roster();
    memset(g_playerPointers,0,sizeof(pointers));g_CurrentPlayer=NULL;
}

int main(void)
{
    conversion();roster(2);roster(4);
    assert(!ge_port_advance_multiplayer_tick(NULL,NULL,2));
    puts("multiplayer port boundary: 2/4-player exact pad mapping, 64 ring ticks, one timer/sample per tick, held/edge ownership, paused/locked input dispatch, transactional rejection; no world simulation fixture");
    return 0;
}

#include "ge_mp_state_core.h"
#include "ge_original_level.h"
#include "ge_original_bond_input_provider.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "game/player.h"
#include "game/chrObjRandom.h"
#include <random.h>

/* Test-only inclusion exposes private owners for mutation sensitivity. These
 * are the actual unchanged live implementation bodies, compiled once here;
 * production offers only the read-only snapshot getters. */
#include "../../src/joy.c"
#include "../src/ge_original_input.c"

extern u64 g_randomSeed;
extern u64 g_chrObjRandomSeed;
extern s32 random_byte;
extern s32 lastFrameCounter;
extern s32 currentFrameCounter;
extern s32 speedgraphframes;
extern s32 previousFrameCounter;
extern s32 halfFrameCounter;
extern s32 isFrameCounterOdd;
extern s32 halfMinusPreviousCounter;
extern s32 frameDelay;
extern s32 g_ControlsLockedFlag;
extern s32 g_ClockTimer;
extern f32 g_GlobalTimerDelta;
extern s32 g_GlobalTimer;
extern s32 D_80048380;
extern s32 g_CurrentStageToLoad;
extern s32 D_80048388;
extern s32 D_8004838C;
extern s32 D_80048390;
extern s32 D_80048394;
extern s32 g_MpTime;
extern f32 g_CurrentMultiPlayerSec;
extern s32 D_800483A8;
extern f32 g_CurrentMultiPlayerMin;
extern s32 D_800483B0;
extern f32 g_StageTimeSec;
extern s32 D_800483B8;
extern f32 g_PowerOnTimeSec;
extern s32 g_StageNum;
extern s32 g_MainStageNum;
extern s32 g_BossIsDebugMenuOpen;
extern s32 g_DebugAndUpdateStageFlag;
extern s32 g_ShowMemUseFlag;
extern s32 g_ShowMemBarsFlag;
extern u32 g_CurentMMallocValue;
extern u32 g_CurentMaMallocValue;
extern s32 player_num;
extern s32 g_PlayerTickCount;
extern PLAYER_ID array_PLAYER_IDs[4];
extern u32 copy_of_osgetcount_value_0, copy_of_osgetcount_value_1;

static GePortState port_state;
static GeMpRegistryEntry entry;
static GeMpRegistry registry;
static uint64_t actor_id;
static unsigned checked[GE_MP_CORE_FIELD_COUNT];
static size_t mutation_count;

static size_t width(uint8_t type)
{
    if(type==GE_MP_STATE_U64) return 8;
    if(type==GE_MP_STATE_U32||type==GE_MP_STATE_I32||type==GE_MP_STATE_F32) return 4;
    if(type==GE_MP_STATE_U16) return 2;
    return 1;
}

static void capture(GeMpCoreState *state)
{
    assert(ge_mp_state_core_capture(state,&port_state,actor_id,
        GE_MP_CORE_BOUNDARY_COMMITTED_RAW)==GE_MP_STATE_INCOMPLETE);
    assert(state->diagnostics.omissions & GE_MP_CORE_OMIT_OTHER_NATIVE_DOMAINS);
}

/* Test-local diagnostic schema is explicitly scoped to these exported fields.
 * Production complete remains zero and is separately checked below. */
static size_t diagnostic_bytes(GeMpCoreState *state,uint8_t *bytes,size_t capacity,uint8_t hash[32])
{
    GeMpRecordSchema rs[2]={{0,GE_MP_CORE_SYSTEMS,NULL,0},*ge_mp_state_core_schema()};
    GeMpStateSchema schema={GE_MP_CORE_SCHEMA_VERSION,GE_MP_CORE_SYSTEMS,1,rs,2};
    GeMpStateRecord records[2]={{0,0,NULL,0},state->record};
    GeMpCanonicalSnapshot snapshot={.session=0xc0de0001,.tick=port_state.simulation_ticks,
        .enabled_systems=GE_MP_CORE_SYSTEMS,.schema=&schema,.registry=&registry,
        .records=records,.record_count=2,.coverage={GE_MP_CORE_SYSTEMS,0,0}};
    size_t size=0;
    const char build[]="CORE-DOMAIN-DIAGNOSTIC: not full native simulation";
    const char content[]="committed raw core transcripts v1";
    const char rules[]="diagnostic field coverage only";
    assert(ge_mp_sha256(build,sizeof(build)-1,snapshot.build_sha256)==GE_MP_STATE_OK);
    assert(ge_mp_sha256(content,sizeof(content)-1,snapshot.content_sha256)==GE_MP_STATE_OK);
    assert(ge_mp_sha256(rules,sizeof(rules)-1,snapshot.rules_sha256)==GE_MP_STATE_OK);
    assert(ge_mp_state_encode(&snapshot,bytes,capacity,&size,NULL)==GE_MP_STATE_OK);
    assert(ge_mp_state_sha256(&snapshot,hash,NULL)==GE_MP_STATE_OK);
    schema.complete=0;
    assert(ge_mp_state_sha256(&snapshot,hash,NULL)==GE_MP_STATE_INCOMPLETE);
    return size;
}

static void changed(uint16_t field_id,GeMpCoreState *before)
{
    GeMpCoreState after;
    uint8_t a[8192],b[8192],ha[32],hb[32];
    size_t i,as,bs;
    capture(&after);
    for(i=0;i<GE_MP_CORE_FIELD_COUNT;++i) if(before->fields[i].id==field_id) break;
    assert(i<GE_MP_CORE_FIELD_COUNT);
    assert(memcmp(before->fields[i].values,after.fields[i].values,
        before->fields[i].count*width(before->fields[i].type))!=0);
    as=diagnostic_bytes(before,a,sizeof(a),ha);bs=diagnostic_bytes(&after,b,sizeof(b),hb);
    assert(as==bs&&memcmp(a,b,as)!=0&&memcmp(ha,hb,32)!=0);
    checked[i]=1;++mutation_count;
}

#define CHECK_NATIVE_BYTE(owner, id, byte) do { \
    GeMpCoreState before; unsigned char saved[sizeof(owner)]; \
    capture(&before); memcpy(saved,&(owner),sizeof(owner)); \
    ((unsigned char *)&(owner))[byte] ^= 1U; changed(id,&before); \
    memcpy(&(owner),saved,sizeof(owner)); \
} while(0)
#define CHECK_NATIVE(owner, id) CHECK_NATIVE_BYTE(owner,id,0)

static s32 foreign_playback(struct contsample *samples,s32 current)
{ (void)samples;return current; }
static void foreign_record(struct contsample *samples,s32 start,s32 end)
{ (void)samples;(void)start;(void)end; }

static void sensitivity(void)
{
    unsigned p,h,bank,sample;
    GeMpCoreState before,after;
    GeOriginalRamromReplay replay={0};
    GeOriginalRamromBlock block={0};
    CHECK_NATIVE(g_randomSeed, GE_MP_CORE_FIELD_RNG);
    CHECK_NATIVE(g_chrObjRandomSeed, GE_MP_CORE_FIELD_CHR_RNG);
    CHECK_NATIVE(random_byte, GE_MP_CORE_FIELD_RANDOM_BYTE);
    CHECK_NATIVE(lastFrameCounter, GE_MP_CORE_FIELD_LASTFRAMECOUNTER);
    CHECK_NATIVE(currentFrameCounter, GE_MP_CORE_FIELD_CURRENTFRAMECOUNTER);
    CHECK_NATIVE(speedgraphframes, GE_MP_CORE_FIELD_SPEEDGRAPHFRAMES);
    CHECK_NATIVE(previousFrameCounter, GE_MP_CORE_FIELD_PREVIOUSFRAMECOUNTER);
    CHECK_NATIVE(halfFrameCounter, GE_MP_CORE_FIELD_HALFFRAMECOUNTER);
    CHECK_NATIVE(isFrameCounterOdd, GE_MP_CORE_FIELD_ISFRAMECOUNTERODD);
    CHECK_NATIVE(halfMinusPreviousCounter, GE_MP_CORE_FIELD_HALFMINUSPREVIOUSCOUNTER);
    CHECK_NATIVE(frameDelay, GE_MP_CORE_FIELD_FRAMEDELAY);
    CHECK_NATIVE(g_ControlsLockedFlag, GE_MP_CORE_FIELD_G_CONTROLSLOCKEDFLAG);
    CHECK_NATIVE(g_ClockTimer, GE_MP_CORE_FIELD_G_CLOCKTIMER);
    CHECK_NATIVE(g_GlobalTimerDelta, GE_MP_CORE_FIELD_G_GLOBALTIMERDELTA);
    CHECK_NATIVE(g_GlobalTimer, GE_MP_CORE_FIELD_G_GLOBALTIMER);
    CHECK_NATIVE(D_80048380, GE_MP_CORE_FIELD_D_80048380);
    CHECK_NATIVE(g_CurrentStageToLoad, GE_MP_CORE_FIELD_G_CURRENTSTAGETOLOAD);
    CHECK_NATIVE(D_80048388, GE_MP_CORE_FIELD_D_80048388);
    CHECK_NATIVE(D_8004838C, GE_MP_CORE_FIELD_D_8004838C);
    CHECK_NATIVE(D_80048390, GE_MP_CORE_FIELD_D_80048390);
    CHECK_NATIVE(D_80048394, GE_MP_CORE_FIELD_D_80048394);
    CHECK_NATIVE(g_MpTime, GE_MP_CORE_FIELD_G_MPTIME);
    CHECK_NATIVE(g_CurrentMultiPlayerSec, GE_MP_CORE_FIELD_G_CURRENTMULTIPLAYERSEC);
    CHECK_NATIVE(D_800483A8, GE_MP_CORE_FIELD_D_800483A8);
    CHECK_NATIVE(g_CurrentMultiPlayerMin, GE_MP_CORE_FIELD_G_CURRENTMULTIPLAYERMIN);
    CHECK_NATIVE(D_800483B0, GE_MP_CORE_FIELD_D_800483B0);
    CHECK_NATIVE(g_StageTimeSec, GE_MP_CORE_FIELD_G_STAGETIMESEC);
    CHECK_NATIVE(D_800483B8, GE_MP_CORE_FIELD_D_800483B8);
    CHECK_NATIVE(g_PowerOnTimeSec, GE_MP_CORE_FIELD_G_POWERONTIMESEC);
    CHECK_NATIVE(g_StageNum, GE_MP_CORE_FIELD_G_STAGENUM);
    CHECK_NATIVE(g_MainStageNum, GE_MP_CORE_FIELD_G_MAINSTAGENUM);
    CHECK_NATIVE(g_BossIsDebugMenuOpen, GE_MP_CORE_FIELD_G_BOSSISDEBUGMENUOPEN);
    CHECK_NATIVE(g_DebugAndUpdateStageFlag, GE_MP_CORE_FIELD_G_DEBUGANDUPDATESTAGEFLAG);
    CHECK_NATIVE(g_ShowMemUseFlag, GE_MP_CORE_FIELD_G_SHOWMEMUSEFLAG);
    CHECK_NATIVE(g_ShowMemBarsFlag, GE_MP_CORE_FIELD_G_SHOWMEMBARSFLAG);
    CHECK_NATIVE(g_CurentMMallocValue, GE_MP_CORE_FIELD_G_CURENTMMALLOCVALUE);
    CHECK_NATIVE(g_CurentMaMallocValue, GE_MP_CORE_FIELD_G_CURENTMAMALLOCVALUE);
    CHECK_NATIVE(player_num, GE_MP_CORE_FIELD_PLAYER_NUM);
    CHECK_NATIVE(g_PlayerTickCount, GE_MP_CORE_FIELD_PLAYER_TICK_COUNT);

    CHECK_NATIVE(port_state.simulation_ticks,GE_MP_CORE_FIELD_PORT_TICKS);
    CHECK_NATIVE_BYTE(g_randomSeed,GE_MP_CORE_FIELD_RNG,sizeof(g_randomSeed)-1);
    CHECK_NATIVE_BYTE(g_chrObjRandomSeed,GE_MP_CORE_FIELD_CHR_RNG,sizeof(g_chrObjRandomSeed)-1);
    CHECK_NATIVE_BYTE(port_state.simulation_ticks,GE_MP_CORE_FIELD_PORT_TICKS,sizeof(port_state.simulation_ticks)-1);
    for(p=0;p<4;++p) CHECK_NATIVE(array_PLAYER_IDs[p],GE_MP_CORE_FIELD_SHUFFLED_ORDER);
    { int32_t old=ge_original_bond_input_control_style();capture(&before);
      ge_original_bond_input_set_control_style(old^1);changed(GE_MP_CORE_FIELD_CONTROL_STYLE,&before);
      ge_original_bond_input_set_control_style(old); }
    for(p=0;p<4;++p) {
        CHECK_NATIVE(g_pending_samples[p].move_x,GE_MP_CORE_FIELD_INPUT_MOVE_X);
        CHECK_NATIVE(g_pending_samples[p].move_y,GE_MP_CORE_FIELD_INPUT_MOVE_Y);
        CHECK_NATIVE(g_pending_samples[p].look_x,GE_MP_CORE_FIELD_INPUT_LOOK_X);
        CHECK_NATIVE(g_pending_samples[p].look_y,GE_MP_CORE_FIELD_INPUT_LOOK_Y);
        CHECK_NATIVE(g_pending_samples[p].buttons,GE_MP_CORE_FIELD_INPUT_BUTTONS);
        CHECK_NATIVE(g_pending_samples[p].buttons_secondary,GE_MP_CORE_FIELD_INPUT_SECONDARY_BUTTONS);
        for(h=0;h<2;++h) {
            CHECK_NATIVE(g_pending_pads[p][h].buttons,GE_MP_CORE_FIELD_INPUT_RAW_BUTTONS);
            CHECK_NATIVE(g_pending_pads[p][h].stick_x,GE_MP_CORE_FIELD_INPUT_RAW_X);
            CHECK_NATIVE(g_pending_pads[p][h].stick_y,GE_MP_CORE_FIELD_INPUT_RAW_Y);
        }
    }
    CHECK_NATIVE(g_raw_pads,GE_MP_CORE_FIELD_INPUT_RAW_PADS);
    CHECK_NATIVE(g_dual_analog,GE_MP_CORE_FIELD_INPUT_DUAL_ANALOG);
    CHECK_NATIVE(g_input_count,GE_MP_CORE_FIELD_INPUT_PLAYER_COUNT);
    CHECK_NATIVE(g_input_player,GE_MP_CORE_FIELD_INPUT_SELECTED_PLAYER);
    CHECK_NATIVE(g_stick_limit,GE_MP_CORE_FIELD_INPUT_STICK_LIMIT);
    CHECK_NATIVE(g_ramrom_controller_count,GE_MP_CORE_FIELD_INPUT_RAMROM_CONTROLLER_COUNT);
    capture(&before);g_ramrom_replay=&replay;changed(GE_MP_CORE_FIELD_INPUT_RAMROM_REPLAY_PRESENT,&before);g_ramrom_replay=NULL;
    capture(&before);g_ramrom_block=&block;changed(GE_MP_CORE_FIELD_INPUT_RAMROM_BLOCK_PRESENT,&before);g_ramrom_block=NULL;
    for(bank=0;bank<2;++bank) {
        CHECK_NATIVE(g_ContData[bank].curlast,GE_MP_CORE_FIELD_JOY_CURLAST);
        CHECK_NATIVE(g_ContData[bank].curstart,GE_MP_CORE_FIELD_JOY_CURSTART);
        CHECK_NATIVE(g_ContData[bank].nextlast,GE_MP_CORE_FIELD_JOY_NEXTLAST);
        CHECK_NATIVE(g_ContData[bank].nextsecondlast,GE_MP_CORE_FIELD_JOY_NEXTSECONDLAST);
        CHECK_NATIVE(g_ContData[bank].playbackcontcount,GE_MP_CORE_FIELD_JOY_PLAYBACK_COUNT);
        for(p=0;p<8;++p) {
            CHECK_NATIVE(g_ContData[bank].buttonspressed[p],GE_MP_CORE_FIELD_JOY_PRESSED);
            for(sample=0;sample<20;++sample) {
                CHECK_NATIVE(g_ContData[bank].samples[sample].pads[p].button,GE_MP_CORE_FIELD_JOY_BUTTONS);
                CHECK_NATIVE(g_ContData[bank].samples[sample].pads[p].stick_x,GE_MP_CORE_FIELD_JOY_STICK_X);
                CHECK_NATIVE(g_ContData[bank].samples[sample].pads[p].stick_y,GE_MP_CORE_FIELD_JOY_STICK_Y);
                CHECK_NATIVE(g_ContData[bank].samples[sample].pads[p].errno,GE_MP_CORE_FIELD_JOY_ERRORS);
            }
        }
    }
    capture(&before);joySetContDataIndex(0);changed(GE_MP_CORE_FIELD_JOY_SELECTED_BANK,&before);joySetContDataIndex(1);
    CHECK_NATIVE(g_ConnectedControllers,GE_MP_CORE_FIELD_JOY_CONNECTED_CONTROLLERS);
    CHECK_NATIVE(g_ControllerStates,GE_MP_CORE_FIELD_JOY_CONTROLLER_STATES);
    capture(&before);joySetPlaybackFunc(NULL,(s32)g_input_count*2);
    changed(GE_MP_CORE_FIELD_INPUT_PLAYBACK_KIND,&before);changed(GE_MP_CORE_FIELD_JOY_PLAYBACK_PRESENT,&before);
    joySetPlaybackFunc(ge_original_input_playback,(s32)g_input_count*2);
    capture(&before);joySetRecordFunc(foreign_record);changed(GE_MP_CORE_FIELD_JOY_RECORD_PRESENT,&before);
    capture(&after);assert(after.diagnostics.omissions&GE_MP_CORE_OMIT_RECORDING_CALLBACK);joySetRecordFunc(NULL);
    joySetPlaybackFunc(foreign_playback,(s32)g_input_count*2);capture(&after);
    assert(after.values.input.playback_kind==GE_ORIGINAL_INPUT_PLAYBACK_UNKNOWN);
    assert(after.diagnostics.omissions&GE_MP_CORE_OMIT_UNKNOWN_PLAYBACK);
    joySetPlaybackFunc(ge_original_input_ramrom_playback,2);capture(&after);
    assert(after.values.input.playback_kind==GE_ORIGINAL_INPUT_PLAYBACK_RAMROM);
    assert(after.diagnostics.omissions&GE_MP_CORE_OMIT_RAMROM_CONTENT);
    joySetPlaybackFunc(ge_original_input_playback,(s32)g_input_count*2);
    g_ContDataPtr=NULL;capture(&after);assert(after.diagnostics.omissions&GE_MP_CORE_OMIT_UNKNOWN_JOY_BANK);joySetContDataIndex(1);
    assert(ge_mp_state_core_capture(&after,&port_state,actor_id,GE_MP_CORE_BOUNDARY_UNCLASSIFIED)==GE_MP_STATE_INCOMPLETE);
    assert(after.diagnostics.omissions&GE_MP_CORE_OMIT_NONCOMMITTED_CLOCK);
    { uint32_t old_x,old_y,negative_zero=UINT32_C(0x80000000);
      memcpy(&old_x,&g_pending_samples[3].look_y,4);memcpy(&old_y,&g_GlobalTimerDelta,4);
      g_pending_samples[3].look_y=0.0f;capture(&before);
      memcpy(&g_pending_samples[3].look_y,&negative_zero,4);changed(GE_MP_CORE_FIELD_INPUT_LOOK_Y,&before);
      g_GlobalTimerDelta=0.0f;capture(&before);
      memcpy(&g_GlobalTimerDelta,&negative_zero,4);changed(GE_MP_CORE_FIELD_G_GLOBALTIMERDELTA,&before);
      memcpy(&g_pending_samples[3].look_y,&old_x,4);memcpy(&g_GlobalTimerDelta,&old_y,4); }
    { uint32_t c0=copy_of_osgetcount_value_0,c1=copy_of_osgetcount_value_1;
      uint8_t bytes[8192],other[8192],a[32],b[32];size_t size,other_size;
      capture(&before);copy_of_osgetcount_value_0^=1;copy_of_osgetcount_value_1^=2;capture(&after);
      assert(after.diagnostics.os_count_previous==(c0^1)&&after.diagnostics.os_count_current==(c1^2));
      size=diagnostic_bytes(&before,bytes,sizeof(bytes),a);other_size=diagnostic_bytes(&after,other,sizeof(other),b);
      assert(size==other_size&&!memcmp(bytes,other,size)&&!memcmp(a,b,32));
      copy_of_osgetcount_value_0=c0;copy_of_osgetcount_value_1=c1; }
    { GeMpCoreState poisoned,copy;memset(&poisoned,0xa3,sizeof(poisoned));memcpy(&copy,&poisoned,sizeof(copy));
      assert(ge_mp_state_core_capture(&poisoned,&port_state,0,GE_MP_CORE_BOUNDARY_COMMITTED_RAW)==GE_MP_STATE_INVALID);
      assert(!memcmp(&poisoned,&copy,sizeof(copy)));
      assert(ge_mp_state_core_capture(&poisoned,NULL,actor_id,GE_MP_CORE_BOUNDARY_COMMITTED_RAW)==GE_MP_STATE_INVALID);
      assert(!memcmp(&poisoned,&copy,sizeof(copy)));
      assert(!ge_original_input_state_snapshot(NULL)&&!ge_original_joy_state_snapshot(NULL)); }
    for(p=0;p<GE_MP_CORE_FIELD_COUNT;++p) assert(checked[p]);
    printf("core native sensitivity: %u fields, %zu actual-owner mutations (including every ring element) passed\n",GE_MP_CORE_FIELD_COUNT,mutation_count);
}

static void transcript(const char *input_path,const char *output_path)
{
    FILE *input=fopen(input_path,"r"),*output=fopen(output_path,"wb");
    struct player players[4]={{0}};
    struct player *pointers[4];
    struct player_data *permissions[4];
    GeOriginalInputPad pads[4][2]={{{0}}};
    unsigned count,ticks,tick,p,h;
    uint16_t previous[4][2]={{0}};
    assert(input&&output&&fscanf(input,"%u %u",&count,&ticks)==2);
    assert((count==2||count==4)&&ticks==64);
    ge_port_init(&port_state);
    assert(ge_port_start_stage(&port_state,LEVELID_DAM));
    for(p=0;p<count;++p){pointers[p]=&players[p];permissions[p]=&g_playerPlayerData[p];}
    assert(ge_original_bond_input_bind_roster(pointers,permissions,count));
    assert(ge_original_input_configure_players(count));
    ge_original_input_set_dual_analog(1);ge_original_input_set_full_stick_range(0);
    g_randomSeed=UINT64_C(0x8765432189abcdef);
    g_chrObjRandomSeed=UINT64_C(0xfedcba9876543210);
    assert(ge_mp_registry_init(&registry,&entry,1)==GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&registry,ge_mp_registry_key(GE_MP_CORE_KIND,255,0))==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,entry.key,&actor_id)==GE_MP_STATE_OK);
    { GeMpCoreState initial;capture(&initial);
      assert(initial.values.rng==UINT64_C(0x8765432189abcdef));
      assert(initial.values.chr_rng==UINT64_C(0xfedcba9876543210)); }
    for(tick=0;tick<ticks;++tick) {
        GeMpCoreState core,again;
        uint8_t bytes[8192],hash[32],repeat[8192],rh[32];
        size_t size,rs;
        unsigned ordinal;
        assert(fscanf(input,"%u",&ordinal)==1&&ordinal==tick);
        for(p=0;p<count;++p) for(h=0;h<2;++h) {
            unsigned buttons;int x,y;
            assert(fscanf(input,"%u %d %d",&buttons,&x,&y)==3);
            assert(buttons<=UINT16_MAX&&x>=INT8_MIN&&x<=INT8_MAX&&y>=INT8_MIN&&y<=INT8_MAX);
            pads[p][h]=(GeOriginalInputPad){(uint16_t)buttons,(int8_t)x,(int8_t)y};
        }
        set_cur_player((s32)(tick%count));assert(ge_original_input_select_player(tick%count));
        assert(ge_port_advance_multiplayer_tick(&port_state,pads,count)==1);
        /* Explicit transcript services use the retained original shuffle and
         * both actual RNG algorithms. This is a core-domain test, no MoveBond/world simulation. */
        shuffle_player_ids();(void)chrObjRandomGetNext();
        capture(&core);capture(&again);
        assert(core.values.rng==g_randomSeed&&core.values.chr_rng==g_chrObjRandomSeed);
        assert(core.values.port_ticks==tick+1&&core.values.currentframecounter==(int32_t)tick+1);
        for(p=0;p<count;++p) for(h=0;h<2;++h) {
            unsigned controller=p+h*count;
            unsigned at=(unsigned)core.values.joy.curlast[1];
            assert(core.values.joy.buttons[1][at][controller]==pads[p][h].buttons);
            assert(core.values.joy.stick_x[1][at][controller]==pads[p][h].stick_x);
            assert(core.values.joy.stick_y[1][at][controller]==pads[p][h].stick_y);
            assert(core.values.joy.pressed[1][controller]==(uint16_t)(pads[p][h].buttons&~previous[p][h]));
            previous[p][h]=pads[p][h].buttons;
        }
        size=diagnostic_bytes(&core,bytes,sizeof(bytes),hash);rs=diagnostic_bytes(&again,repeat,sizeof(repeat),rh);
        assert(size==rs&&!memcmp(bytes,repeat,size)&&!memcmp(hash,rh,32));
        assert(fwrite(bytes,1,size,output)==size&&fwrite(hash,1,32,output)==32);
    }
    assert(fclose(input)==0&&fclose(output)==0);
    sensitivity();
    puts("core transcript: actual committed port/input/joy/timer/shuffle/RNG providers; full native coverage remains INCOMPLETE");
}

int main(int argc,char **argv)
{
    assert(argc==3);transcript(argv[1],argv[2]);return 0;
}

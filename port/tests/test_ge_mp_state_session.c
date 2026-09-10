#include "ge_mp_state_session.h"
#include "ge_mp_state_players.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_multiplayer_session.h"
#include "ge_original_gun_frame_arena.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/mp_weapon.h"
#include "game/ramromreplay.h"
#include "game/chrObjRandom.h"
#include <random.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern s32 fixture_intro[];
extern PadRecord fixture_pads[];
extern const size_t fixture_intro_words,fixture_pad_count;
extern s32 scenario,mp_weapon_set,startpadcount,g_MpPoint;
extern PadRecord *g_Startpad[16];
extern ITEM_IDS starting_weapon[2];
extern ramromfilestructure *ptr_active_demofile;
extern s32 is_ramrom_flag,recording_ramrom_flag;
extern void increment_num_kills_display_text_in_MP(void);
s32 g_stopPlayFlag,g_gameOverFlag,g_pausedFlag,g_MpTime,g_ControlsLockedFlag,g_ClockTimer;
f32 g_GlobalTimerDelta;
stagesetup g_CurrentSetup;
PropRecord *g_ActivePropsHead,*g_ActivePropsTail;
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots,g_GlobalTimer;
s32 fixture_mission_timer=600;
s32 getMissiontimer(void){return fixture_mission_timer;}
#ifndef GE_MP_WATCH_FIXTURE
u8 *langGet(s32 id){(void)id;return (u8 *)"fixture";}
#endif
void hudmsgBottomShow(char *text){assert(text);}

static GeMpRegistry registry;
static GeMpRegistryEntry entries[32];
static GeMpStatePlayersOwner players_owner;
static GeMpSessionSnapshot snapshot;
static GeOriginalMultiplayerDeathState death;
static GeOriginalBondLiveState lives[4];
static GeMpSessionSchedule schedule;
static PropRecord props[4];static ChrRecord chrs[4];static StandTile stand;
static _Alignas(16) uint8_t arena[0x28000];
static uint64_t intro_id,pads_id,demo_id;
static uint64_t alternate_intro_id;
static int32_t alternate_intro[64];
static size_t alternate_words;
static ramromfilestructure external_demo;
static uint16_t missing_field,wrong_player_field;
static unsigned mutations;
static uint64_t acquire(unsigned kind,unsigned slot)
{
    uint64_t id;uint32_t key=ge_mp_registry_key(kind,255,slot);
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,key,&id)==GE_MP_STATE_OK);return id;
}
static GeMpStateStatus reference(void *context,uint16_t field,unsigned index,
    GeMpSessionReferenceKind kind,const void *native,size_t extent,GeMpSessionReference *ref)
{
    (void)context;
    if(field==missing_field)return GE_MP_STATE_INCOMPLETE;
    ref->offset=0;
    if(kind==GE_MP_SESSION_REF_PLAYER) {
        assert(!extent);
        if(field==wrong_player_field){ref->actor_id=players_owner.ids[(index+1)%4];return GE_MP_STATE_OK;}
        return ge_mp_state_players_actor(&players_owner,native,&ref->actor_id);
    }
    if(kind==GE_MP_SESSION_REF_INTRO && native==fixture_intro && extent==fixture_intro_words*4) {
        ref->actor_id=intro_id;return GE_MP_STATE_OK;
    }
    if(kind==GE_MP_SESSION_REF_INTRO && native==alternate_intro && extent==alternate_words*4) {
        ref->actor_id=alternate_intro_id;return GE_MP_STATE_OK;
    }
    if(kind==GE_MP_SESSION_REF_PAD_ARRAY && native==fixture_pads && extent>0 && extent<=fixture_pad_count*sizeof(PadRecord)) {
        ref->actor_id=pads_id;return GE_MP_STATE_OK;
    }
    if(kind==GE_MP_SESSION_REF_PAD)for(size_t i=0;i<fixture_pad_count;++i)
        if(native==&fixture_pads[i] && extent==sizeof(PadRecord)) {
            ref->actor_id=pads_id;ref->offset=i*44;return GE_MP_STATE_OK;
        }
    if(kind==GE_MP_SESSION_REF_DEMO && native==&external_demo && extent==sizeof(external_demo)) {
        ref->actor_id=demo_id;return GE_MP_STATE_OK;
    }
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpSessionProviders providers={NULL,reference};
static void capture(int full)
{
    struct player *selected=g_CurrentPlayer;struct player_data *perm=g_playerPerm;
    uint64_t rng=g_randomSeed,chr_rng=g_chrObjRandomSeed;
    GeOriginalMultiplayerDeathState before=death;
    assert(ge_mp_state_session_capture(&registry,&providers,&schedule,&snapshot)==GE_MP_STATE_INCOMPLETE);
    assert(snapshot.record.actor_id==0 && snapshot.record.kind==0);
    assert(snapshot.record.field_count==GE_MP_SESSION_FIELD_COUNT);
    assert(snapshot.coverage.enumerated_systems==GE_MP_SESSION_SYSTEMS);
    assert(snapshot.total_gaps>=1 && snapshot.total_gaps==snapshot.gap_count);
    if(full) {
        assert(snapshot.total_gaps==5 && !snapshot.coverage.unresolved_references);
        const GeMpRecordSchema *schema=ge_mp_state_session_schema();
        for(size_t i=0;i<schema->field_count;++i) {
            assert(snapshot.fields[i].id==schema->fields[i].id && snapshot.fields[i].type==schema->fields[i].type);
            assert(snapshot.fields[i].count==((i>=299-256 && i<=321-256 && !schedule.active_props)
                || (i>=322-256 && i<=328-256 && !schedule.hud)?0:schema->fields[i].min_count));
            assert(ge_mp_state_session_field_path(snapshot.fields[i].id));
        }
        for(size_t i=0;i<snapshot.gap_count;++i)assert(ge_mp_state_session_gap_path(snapshot.gaps[i].reason));
    }
    assert(selected==g_CurrentPlayer && perm==g_playerPerm && rng==g_randomSeed && chr_rng==g_chrObjRandomSeed);
    assert(!memcmp(&before,&death,sizeof(death)));
}
/* Diagnostic typed payload only. This is deliberately not a complete native
 * state checkpoint; the public codec must refuse its digest below. */
static int legacy_projection;
static size_t payload(unsigned char *bytes)
{
    const unsigned width[]={0,1,2,4,8,1,2,4,8,4,1,8,4};size_t used=0;
    for(unsigned i=0;i<(legacy_projection?73:GE_MP_SESSION_FIELD_COUNT);++i) {
        const GeMpStateField *f=&snapshot.fields[i];
        bytes[used++]=f->id>>8;bytes[used++]=f->id;bytes[used++]=f->type;
        for(int shift=24;shift>=0;shift-=8)bytes[used++]=f->count>>shift;
        for(unsigned n=0;n<f->count;++n) {
            uint64_t value=0;memcpy(&value,(const unsigned char*)f->values+n*width[f->type],width[f->type]);
            for(int b=(int)width[f->type]-1;b>=0;--b)bytes[used++]=(uint8_t)(value>>(8*b));
        }
    }
    return used;
}
static void fingerprint(unsigned char digest[32],int full)
{unsigned char bytes[4096];capture(full);assert(ge_mp_sha256(bytes,payload(bytes),digest)==GE_MP_STATE_OK);}
static void configure(unsigned count,int mode,int weapons)
{
    GeOriginalMultiplayerConfig config={(int)count,mode,weapons,LEVELID_CAVES,{0,0,1,1}};
    assert(ge_original_multiplayer_configure(&config)==GE_ORIGINAL_MP_OK);
    ge_original_multiplayer_bind_intro_slot(0,0,0);
    g_CurrentSetup.intro=fixture_intro;g_CurrentSetup.pads=fixture_pads;
    assert(ge_original_multiplayer_publish_spawns(fixture_intro,fixture_intro_words,fixture_pads,fixture_pad_count,0)==GE_ORIGINAL_MP_OK);
    assert(ge_original_multiplayer_session_reset(0,20)==GE_ORIGINAL_MP_SESSION_OK);
}
static void setup(unsigned count,unsigned peer)
{
    assert(ge_original_multiplayer_players_create(count,peer%count,UINT64_C(0x123456789abcdef),UINT64_C(0x987654321fedcba)));
    assert(ge_mp_registry_init(&registry,entries,32)==GE_MP_STATE_OK);
    assert(ge_mp_state_players_begin(&players_owner,&registry)==GE_MP_STATE_OK);
    intro_id=acquire(0x21,0);pads_id=acquire(0x21,1);demo_id=acquire(0x21,2);alternate_intro_id=acquire(0x21,3);
    memset(props,0,sizeof(props));memset(chrs,0,sizeof(chrs));memset(lives,0,sizeof(lives));memset(&schedule,0,sizeof(schedule));
    for(size_t i=0;i<fixture_pad_count;++i)fixture_pads[i].stan=&stand;
    for(unsigned i=0;i<count;++i) {
        struct player *p=ge_original_multiplayer_player(i);
        p->prop=&props[i];props[i].chr=&chrs[i];props[i].type=PROP_TYPE_VIEWER;
        chrs[i].prop=&props[i];chrs[i].fadealpha=255;
        p->colourfadetimemax60=-1;p->bondfadetimemax60=-1;
        lives[i].initialized=1;schedule.lives[i]=&lives[i];
    }
    configure(count,SCENARIO_NORMAL,1);
    assert(ge_original_multiplayer_death_reset(&death));schedule.death=&death;
    capture(1);
    assert(snapshot.values.startpad_count==6);
    const unsigned authored[]={0,146,66,128,69,54};
    for(unsigned i=0;i<6;++i)assert(snapshot.values.startpad_offsets[i]==44*authored[i]);
    for(unsigned i=0;i<4;++i)assert(snapshot.values.players[i]==(i<count?players_owner.ids[i]:0));
}
static void teardown(void)
{
    assert(ge_mp_state_players_end(&players_owner)==GE_MP_STATE_OK);
    ge_original_multiplayer_players_destroy();
}
static void run_world(unsigned count)
{
    g_ClockTimer=1;g_GlobalTimerDelta=1;
    assert(ge_original_gun_frame_arena_begin(arena,sizeof(arena)));
    for(unsigned i=0;i<count;++i) {
        assert(ge_original_multiplayer_select_player(i));
        assert(ge_original_multiplayer_death_pre_move(&death,schedule.next_tick));
        assert(ge_original_multiplayer_death_post_gun(&death,schedule.next_tick));
    }
    assert(ge_original_gun_frame_arena_finalize(NULL));
    ++schedule.next_tick;++schedule.elapsed60;schedule.boundary=GE_MP_SESSION_WORLD_COMMITTED;
    capture(1);
}
#define MUTATE(expression,value) do { \
    unsigned char a[32],b[32];fingerprint(a,1); \
    __typeof__(expression) saved=(expression);(expression)=(value); \
    fingerprint(b,0);assert(memcmp(a,b,32));(expression)=saved;++mutations; \
} while(0)
static void mutations_and_negative(unsigned count)
{
    extern s32 who_paused,chevron_glow,chevron_glow_timer,alt_gameover_msg,alt_gameover_msg_timer,array_favweapon[4][2];
    MUTATE(who_paused,7);MUTATE(chevron_glow,17);MUTATE(chevron_glow_timer,19);
    MUTATE(alt_gameover_msg,23);MUTATE(alt_gameover_msg_timer,31);
    for(unsigned i=0;i<4;++i)for(unsigned h=0;h<2;++h)MUTATE(array_favweapon[i][h],93);
    MUTATE(g_MpPoint,-7);MUTATE(g_stopPlayFlag,3);MUTATE(g_gameOverFlag,300);MUTATE(g_pausedFlag,2);
    MUTATE(scenario,SCENARIO_LTK);MUTATE(mp_weapon_set,13);MUTATE(startpadcount,5);
    for(unsigned i=0;i<2;++i)MUTATE(starting_weapon[i],ITEM_GOLDENGUN);
    for(unsigned i=0;i<16;++i)MUTATE(g_Startpad[i],&fixture_pads[7]);
    MUTATE(is_ramrom_flag,2);MUTATE(recording_ramrom_flag,-1);
    MUTATE(schedule.next_tick,2);MUTATE(schedule.elapsed60,7);
    MUTATE(schedule.boundary,GE_MP_SESSION_INITIAL);MUTATE(death.count,3);
    MUTATE(death.pre_valid,1);MUTATE(death.post_valid,1);
    for(unsigned i=0;i<4;++i){MUTATE(death.pre_tick[i],19);MUTATE(death.post_tick[i],23);}
    for(unsigned i=0;i<count;++i)MUTATE(lives[i].initialized,2);
    unsigned char before[32],after[32];fingerprint(before,1);
    for(unsigned i=0;i<count;++i){lives[i].initialization_count+=9;lives[i].input_tick_count+=7;lives[i].move_tick_count+=5;}
    fingerprint(after,1);assert(!memcmp(before,after,32));
    const uint16_t references[]={265,270,272,275,279,281,293};
    for(unsigned i=0;i<sizeof(references)/sizeof(references[0]);++i) {
        missing_field=references[i];capture(0);
        assert(snapshot.coverage.unresolved_references && !snapshot.fields[missing_field-256].count);
    }
    missing_field=0;wrong_player_field=265;capture(0);assert(snapshot.coverage.unresolved_references);
    wrong_player_field=0;
    assert(ge_mp_registry_release(&registry,intro_id)==GE_MP_STATE_OK);capture(0);assert(snapshot.coverage.unresolved_references);
    assert(ge_mp_registry_acquire(&registry,ge_mp_registry_key(0x21,255,0),&intro_id)==GE_MP_STATE_OK);capture(1);
    GeOriginalMultiplayerDeathState saved=death;death.post_valid=0;capture(0);assert(snapshot.total_gaps>5);death=saved;
    const GeOriginalMultiplayerDeathState *old=schedule.death;schedule.death=NULL;capture(0);assert(!snapshot.fields[292-256].count);schedule.death=old;
    struct player *p=g_playerPointers[0];g_playerPointers[0]=NULL;capture(0);assert(snapshot.gaps[0].reason==GE_MP_SESSION_GAP_NATIVE_SESSION);g_playerPointers[0]=p;
    ramromfilestructure *owned=ptr_active_demofile;
    ptr_active_demofile=(ramromfilestructure*)(uintptr_t)0x1234;capture(0);assert(!snapshot.fields[286-256].count); /* never dereferenced */
    ptr_active_demofile=&external_demo;memset(&external_demo,0,sizeof(external_demo));capture(1);assert(snapshot.values.demo_kind==2 && snapshot.values.demo==demo_id);
    ptr_active_demofile=owned;ge_original_multiplayer_bind_intro_slot(0,0,0);capture(1);
    /* Mutable setup is content-owned. Ref mismatch and lost publication must
     * remain explicit gaps; capture must never call the mutating publisher. */
    s32 *oldintro=g_CurrentSetup.intro;g_CurrentSetup.intro=NULL;capture(0);assert(snapshot.total_gaps>5);g_CurrentSetup.intro=oldintro;
}
static void publication(unsigned count)
{
    unsigned char before[32],after[32];fingerprint(before,1);
    assert(ge_original_multiplayer_publish_spawns(fixture_intro,fixture_intro_words,
        fixture_pads,fixture_pad_count-1,0)==GE_ORIGINAL_MP_OK);
    fingerprint(after,1);assert(memcmp(before,after,32));assert(snapshot.values.pad_count==fixture_pad_count-1);
    assert(fixture_intro_words+2<64);
    memcpy(alternate_intro,fixture_intro,fixture_intro_words*4);
    alternate_intro[fixture_intro_words-1]=INTROTYPE_CUFF;
    alternate_intro[fixture_intro_words]=1;alternate_intro[fixture_intro_words+1]=INTROTYPE_END;
    alternate_words=fixture_intro_words+2;g_CurrentSetup.intro=alternate_intro;
    assert(ge_original_multiplayer_publish_spawns(alternate_intro,alternate_words,
        fixture_pads,fixture_pad_count,0)==GE_ORIGINAL_MP_OK);
    capture(1);assert(snapshot.values.intro_words==alternate_words && snapshot.values.validated_intro==alternate_intro_id);
    for(size_t i=0;i<alternate_words;) {
        if(alternate_intro[i]==INTROTYPE_SPAWN){alternate_intro[i+2]=3;i+=3;}
        else if(alternate_intro[i]==INTROTYPE_ITEM){alternate_intro[i+3]=3;i+=4;}
        else if(alternate_intro[i]==INTROTYPE_CUFF)i+=2;
        else {assert(alternate_intro[i]==INTROTYPE_END);break;}
    }
    ge_original_multiplayer_bind_intro_slot(1,0,3);
    assert(ge_original_multiplayer_publish_spawns(alternate_intro,alternate_words,
        fixture_pads,fixture_pad_count,3)==GE_ORIGINAL_MP_OK);
    capture(0);assert(snapshot.total_gaps==6 && snapshot.values.demo_slot==3 && snapshot.values.demo_selection==3);
    assert(snapshot.values.playback==1 && snapshot.values.demo_kind==1 && !snapshot.values.demo);
    for(int weapons=0;weapons<14;++weapons){configure(count,SCENARIO_NORMAL,weapons);capture(1);assert(snapshot.values.weapon_set==weapons);}
    configure(count,SCENARIO_NORMAL,1);
}
static void terminal(unsigned count)
{
    GeOriginalMultiplayerSessionState result;
    assert(ge_original_multiplayer_session_reset(0,1)==GE_ORIGINAL_MP_SESSION_OK);
    assert(ge_original_multiplayer_select_player(0));
    increment_num_kills_display_text_in_MP();g_playerPlayerData[0].kill_counts[1]=1;
    struct player *victim=g_playerPointers[1];victim->bonddead=1;
    assert(ge_original_multiplayer_session_tick(1,schedule.elapsed60,&result)==GE_ORIGINAL_MP_SESSION_OK);
    assert(result.decision==GE_ORIGINAL_MP_END_FREEZE);capture(1);
    assert(snapshot.values.freeze_causes==GE_ORIGINAL_MP_SESSION_CAUSE_POINT && snapshot.values.stop_play==1);
    /* Actual original death pre/post advances state, without substituting
     * completion. The model's end frame is a resource fixture input. */
    Model *model=(Model *)(void *)&victim->model;model->endframe=2;
    for(unsigned tick=0;tick<240 && !victim->redbloodfinished;++tick)run_world(count);
    assert(victim->redbloodfinished);
    model->animframe1=2;run_world(count);
    for(unsigned tick=0;tick<120 && victim->colourfadetimemax60>=0;++tick)run_world(count);
    assert(victim->deathanimfinished && victim->colourfadetimemax60<0);
    assert(ge_original_multiplayer_session_tick(1,schedule.elapsed60,&result)==GE_ORIGINAL_MP_SESSION_OK);
    schedule.boundary=GE_MP_SESSION_TERMINAL_BEFORE_WORLD;++schedule.elapsed60;capture(1);
    assert(snapshot.values.decision==GE_ORIGINAL_MP_END_RESULTS && snapshot.values.end_reason==GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    assert(snapshot.values.results_causes==GE_ORIGINAL_MP_SESSION_CAUSE_POINT && snapshot.values.game_over==1 && snapshot.values.paused==1);
    assert(ge_original_multiplayer_session_reset(schedule.elapsed60+1,1)==GE_ORIGINAL_MP_SESSION_OK);
    assert(ge_original_multiplayer_session_tick(1,schedule.elapsed60,&result)==GE_ORIGINAL_MP_SESSION_OK);++schedule.elapsed60;capture(1);
    assert(snapshot.values.results_causes==(GE_ORIGINAL_MP_SESSION_CAUSE_TIME|GE_ORIGINAL_MP_SESSION_CAUSE_POINT));
    assert(snapshot.values.end_reason==GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);
    assert(ge_original_multiplayer_session_reset(0,0)==GE_ORIGINAL_MP_SESSION_OK);
    assert(ge_original_multiplayer_session_abort(&result)==GE_ORIGINAL_MP_SESSION_OK);capture(1);
    assert(snapshot.values.results_causes==GE_ORIGINAL_MP_SESSION_CAUSE_ABORT && snapshot.values.end_reason==GE_ORIGINAL_MP_FRONTEND_END_ABORT);
}
static void incomplete_digest(void)
{
    GeMpStateSchema schema={GE_MP_SESSION_SCHEMA_VERSION,GE_MP_SESSION_SYSTEMS,0,ge_mp_state_session_schema(),1};
    GeMpCanonicalSnapshot aggregate={0};GeMpCoverageReport report;uint8_t digest[32];
    aggregate.session=42;aggregate.tick=schedule.next_tick;aggregate.enabled_systems=GE_MP_SESSION_SYSTEMS;
    memset(aggregate.build_sha256,1,32);memset(aggregate.content_sha256,2,32);memset(aggregate.rules_sha256,3,32);
    aggregate.schema=&schema;aggregate.registry=&registry;aggregate.records=&snapshot.record;aggregate.record_count=1;aggregate.coverage=snapshot.coverage;
    memset(digest,0xA5,32);assert(ge_mp_state_sha256(&aggregate,digest,&report)==GE_MP_STATE_INCOMPLETE);
    for(unsigned i=0;i<32;++i)assert(digest[i]==0xA5);
}
static void controls(unsigned count,unsigned peer)
{
    setup(count,peer);GeOriginalMultiplayerControlStorage view;
    assert(ge_original_multiplayer_players_control_storage(&view));
    assert(view.count==count && !view.explicit_selection);
    capture(1);assert(!snapshot.values.controls_explicit);
    int32_t styles[4]={0,0,0,0};unsigned char before[32],after[32];
    fingerprint(before,1);
    assert(ge_original_multiplayer_players_apply_control_styles(styles,count));
    fingerprint(after,1);assert(memcmp(before,after,32));
    assert(snapshot.values.controls_explicit==1);
    for(unsigned i=0;i<count;++i) {
        fingerprint(before,1);styles[i]=(int32_t)(i+1);
        assert(ge_original_multiplayer_players_apply_control_styles(styles,count));
        fingerprint(after,1);assert(memcmp(before,after,32));
        assert(snapshot.values.control_styles[i]==styles[i]);
    }
    assert(ge_original_multiplayer_players_control_storage(&view));
    styles[0]=8;assert(!ge_original_multiplayer_players_apply_control_styles(styles,count));
    GeOriginalMultiplayerControlStorage unchanged;
    assert(ge_original_multiplayer_players_control_storage(&unchanged));
    assert(view.count==unchanged.count && view.explicit_selection==unchanged.explicit_selection
        && !memcmp(view.styles,unchanged.styles,sizeof(view.styles)));
    teardown();assert(!ge_original_multiplayer_players_control_storage(&view));
    setup(count,peer);assert(ge_original_multiplayer_players_control_storage(&view));
    assert(!view.explicit_selection);for(unsigned i=0;i<4;++i)assert(!view.styles[i]);teardown();
}
int main(int argc,char **argv)
{
    assert(argc==4);unsigned count=(unsigned)atoi(argv[1]),peer=(unsigned)atoi(argv[2]);
    unsigned char initial[32],again[32],bytes[4096];
    /* Different menu/prior-match paths must converge with a fresh agreed
     * registry namespace, without erasing within-match generations. */
    if(peer){setup(4,peer);run_world(4);terminal(4);teardown();}
    setup(count,peer);legacy_projection=1;fingerprint(initial,1);run_world(count);
    capture(1);FILE *file=fopen(argv[3],"wb");assert(file);size_t n=payload(bytes);assert(fwrite(bytes,1,n,file)==n);assert(!fclose(file));
    legacy_projection=0;mutations_and_negative(count);publication(count);terminal(count);incomplete_digest();teardown();
    setup(4,peer);teardown();setup(count,peer);legacy_projection=1;fingerprint(again,1);assert(!memcmp(initial,again,32));teardown();
    legacy_projection=0;controls(count,peer);
    printf("kind0: %up peer%u, %u native mutations, authored Caves spawn order, exact death/limits, replay/ref/schedule rejection, prior-domain session convergence, launch controls and retained watch mutations passed\n",count,peer,mutations);
    return 0;
}

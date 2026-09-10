/* Composition-boundary test, deliberately NOT a match acceptance test. Every
 * gameplay producer below is an observing double. The real match fixture must
 * link the retained producers instead of this file. */
#include "ge_original_multiplayer_runtime.h"
#include "ge_original_door_interaction.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_gun_live.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_multiplayer_rules.h"
#include "ge_original_player_held_items.h"
#include "ge_original_stage_alarm_interaction.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/bondview.h"
#include "game/frametiming.h"

struct player *g_CurrentPlayer;
static struct player players[4];
static GeOriginalBondLiveState lives[4];
static uint32_t count, selected, local, failed_slot;
static uint32_t shuffle_rotation, move_ordinal;
static int mission_before, watch_before, passed_zero;
static unsigned events[256], event_slots[256], event_count;
static int arena_active, held_frontier, reload, object_present;
PropRecord *g_InteractProp, *g_OnScreenPropList[3], **g_LastOnScreenProp;
static PropRecord object_prop, door_prop;
enum Event {EFFECTS, BG_SEMANTICS, WORLD, REGEN, SHUFFLE, VIEWS_BEGIN, HUD_BEGIN, SELECT, FOV, PRE_DEATH, Y_AIM,
    X_AIM, LOOK, AMMO, SIGHT, MOVE, CAMERA, VIEW, PROPS, ONSCREEN, AUTOAIM,
    HELD, TEST_DOOR, DOOR, TEST_OBJECT, INTERACT, EXECUTE_OP, RELOAD_RIGHT, RELOAD_LEFT,
    PICKUPS, SYNC, GUN, RENDERER, MATRICES, FLASH, HUD_PREPARE, POST_DEATH,
    CAN_RESPAWN, RESPAWN, HUD_CAPTURE, CAPTURE, FINISH, SERVICES};
static void event(unsigned id) {assert(event_count<256);events[event_count]=id;event_slots[event_count++]=selected;}
uint32_t ge_original_multiplayer_player_count(void) {return count;}
struct player *ge_original_multiplayer_player(uint32_t slot) {return slot<count?&players[slot]:NULL;}
s32 get_cur_playernum(void) {return (s32)selected;}
s32 get_nth_player_from_shuffled(PLAYER_ID ordinal) {return (s32)((count-1-(uint32_t)ordinal+shuffle_rotation)%count);}
u32 osGetCount(void) {return 12345;} /* Wall-clock resource, not game timers. */
void shuffle_player_ids(void) {event(SHUFFLE);}
int ge_original_gun_frame_arena_active(void) {return arena_active;}
int ge_original_effect_runtime_pre_world(void) {event(EFFECTS);return 1;}
int ge_original_prop_render_semantics_prepare(GeOriginalStageGuardRuntime *r,uint64_t tick,unsigned slot)
{assert(r&&tick==17&&slot==selected);event(BG_SEMANTICS);return 1;}
GeOriginalStageActivePropStatus ge_original_stage_active_props_pre_tick_exact(GeOriginalStageActiveProps *s)
{assert(s);event(WORLD);return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;}
GeOriginalStageActivePropStatus ge_original_stage_active_props_tick_exact(GeOriginalStageActiveProps *s)
{assert(s);event(PROPS);return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;}
GeOriginalMultiplayerRegenStatus ge_original_multiplayer_regen_tick_all(int32_t tick,GeOriginalMultiplayerRegenBatch *batch)
{assert(tick==1&&batch);memset(batch,0,sizeof(*batch));event(REGEN);return GE_ORIGINAL_MP_REGEN_OK;}
void ge_original_bond_input_set_fov_y(float fovy) {assert(fovy==60&&g_CurrentPlayer->fovy==60);event(FOV);}
int ge_original_multiplayer_death_pre_move(GeOriginalMultiplayerDeathState *s,uint64_t tick)
{assert(s&&tick==17);event(PRE_DEATH);return 1;}
int ge_original_multiplayer_death_post_gun(GeOriginalMultiplayerDeathState *s,uint64_t tick)
{assert(s&&tick==17);event(POST_DEATH);return 1;}
void currentPlayerSetYAutoAimEnabled(s32 value) {assert(value==1);event(Y_AIM);}
void currentPlayerSetXAutoAimEnabled(s32 value) {assert(value==1);event(X_AIM);}
void currentPlayerSetLookAheadSetting(s32 value) {assert(value==1);event(LOOK);}
s32 cur_player_get_autoaim(void) {return 1;}
u32 cur_player_get_lookahead(void) {return 1;}
u32 cur_player_get_ammo_onscreen_setting(void) {return 1;}
void gunSetGunAmmoVisible(s32 reason,s32 value) {assert(reason==GUNAMMOREASON_OPTION&&value==1);event(AMMO);}
void ge_original_player_apply_sight_option_exact(void) {event(SIGHT);}
int ge_original_bond_move_live_tick(GeOriginalBondLiveState *s,int32_t clock,int32_t timer,float delta)
{assert(s==&lives[selected]&&clock==1&&timer==91&&delta==1);
    passed_zero|=selected==0;
    assert(getMissiontimer()==mission_before+passed_zero);
    assert(watch_time_0==watch_before+(int)move_ordinal*speedgraphframes);
    ++move_ordinal;g_CurrentPlayer->fovy=20+(float)selected;event(MOVE);return 1;}
void chraiUpdateOnscreenPropCount(void) {event(ONSCREEN);}
void chrpropUpdateAutoaimTarget(void) {event(AUTOAIM);}
GeOriginalPlayerHeldItemsStatus ge_original_player_held_items_tick(void)
{event(HELD);return held_frontier&&selected==failed_slot?GE_ORIGINAL_PLAYER_HELD_ITEMS_LIGHT_FRONTIER:GE_ORIGINAL_PLAYER_HELD_ITEMS_OK;}
int ge_original_door_interaction_test_native(void *prop,void **selected_prop)
{assert(prop==&door_prop&&!(*selected_prop));event(TEST_DOOR);*selected_prop=prop;return 0;}
GeOriginalDoorInteractionResult ge_original_door_interaction_activate_native(void *prop)
{assert(prop==&door_prop);event(DOOR);return GE_ORIGINAL_DOOR_INTERACTION_ACTIVATED;}
bool objTestForInteract(PropRecord *prop)
{assert(prop==&object_prop&&!g_InteractProp);event(TEST_OBJECT);g_InteractProp=prop;return FALSE;}
void propExecuteTickOperation(PropRecord *prop,TICKOP operation)
{assert((prop==&object_prop&&operation==TICKOP_FREE)||(prop==&door_prop&&operation==TICKOP_NONE));event(EXECUTE_OP);}
GeOriginalStageAlarmInteractionStatus ge_original_stage_object_interact_exact(void *p,const GeOriginalStageAlarmInteractionProviders *s,GeOriginalStageAlarmInteractionResult *r)
{assert(p==&object_prop&&s->play_sfx&&s->pickup_by_player&&r);r->tick_operation=TICKOP_FREE;event(INTERACT);return GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK;}
TICKOP ge_original_stage_prop_pickup_exact(PropRecord *p,bool immediate)
{(void)p;(void)immediate;assert(!"fixture never fabricates an item pickup");return TICKOP_NONE;}
void ge_original_gameplay_services_play_sfx(uint32_t sound) {(void)sound;assert(!"fixture never fabricates sound");}
void attempt_reload_item_in_hand(GUNHAND hand) {event(hand==GUNRIGHT?RELOAD_RIGHT:RELOAD_LEFT);}
void ge_original_stage_props_tick_player_exact(void) {event(PICKUPS);}
int ge_original_gun_live_tick(void) {event(GUN);return 1;}
GeOriginalStageGuardRuntimeStatus ge_original_stage_guard_runtime_update_matrices(GeOriginalStageGuardRuntime *r,const float m[4][4])
{assert(r&&m[0][0]==1);event(MATRICES);return GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;}
int ge_original_stage_guard_runtime_prepare_muzzle_flashes(GeOriginalStageGuardRuntime *r,uint64_t tick,unsigned slot)
{assert(r&&tick==17&&slot==selected);event(FLASH);return 1;}
int ge_original_multiplayer_hud_begin_frame(GeOriginalMultiplayerHud *h,uint64_t tick,uint32_t n)
{assert(h&&tick==17&&n==count);event(HUD_BEGIN);return 1;}
int ge_original_multiplayer_hud_prepare_shooter(GeOriginalMultiplayerHud *h,uint32_t slot)
{assert(h&&slot==selected);event(HUD_PREPARE);return 1;}
int ge_original_multiplayer_hud_capture_shooter(GeOriginalMultiplayerHud *h,uint32_t slot)
{assert(h&&slot==selected);event(HUD_CAPTURE);return 1;}
uint16_t ge_original_input_buttons(void) {return 0x8000;}
int ge_original_multiplayer_can_respawn(int32_t slot,uint16_t buttons)
{assert(slot==(int32_t)selected&&buttons==0x8000);event(CAN_RESPAWN);return 1;}
GeOriginalMultiplayerStatus ge_original_multiplayer_respawn(int32_t slot,uint16_t buttons)
{assert(slot==(int32_t)selected&&buttons==0x8000);event(RESPAWN);return GE_ORIGINAL_MP_OK;}
void ge_original_gameplay_services_tick(void) {event(SERVICES);}
GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_begin_frame(GeOriginalMultiplayerViews *v,uint32_t n,uint32_t display,const GeOriginalMultiplayerViewProviders *p)
{assert(v&&n==count&&display==local&&p);event(VIEWS_BEGIN);return GE_ORIGINAL_MULTIPLAYER_VIEW_OK;}
GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_begin_shooter(GeOriginalMultiplayerViews *v,uint32_t slot,const GeOriginalBondCameraConfig *c,const GeOriginalBondCameraResult *r)
{assert(v&&slot==selected&&c&&r->view[0][0]==1);event(VIEW);return GE_ORIGINAL_MULTIPLAYER_VIEW_OK;}
GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_capture_local(GeOriginalMultiplayerViews *v)
{assert(v&&selected==local);event(CAPTURE);return GE_ORIGINAL_MULTIPLAYER_VIEW_OK;}
GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_finish_local(GeOriginalMultiplayerViews *v)
{assert(v);event(FINISH);return GE_ORIGINAL_MULTIPLAYER_VIEW_OK;}
static int select_player(void *context,uint32_t slot)
{assert(context==players);selected=slot;g_CurrentPlayer=&players[slot];g_CurrentPlayer->field_D0=reload;event(SELECT);return 1;}
static int camera(void *context,uint32_t slot,GeOriginalBondCameraConfig *c,GeOriginalBondCameraResult *r)
{float fovy;assert(context==players&&slot==selected);assert(watch_time_0==watch_before+(int)move_ordinal*speedgraphframes);assert(ge_original_multiplayer_runtime_camera_fovy(&fovy)&&fovy==20+(float)slot);memset(c,0,sizeof(*c));memset(r,0,sizeof(*r));r->view[0][0]=1;event(CAMERA);return 1;}
static int sync_gun(void *context,uint32_t slot) {assert(context==players&&slot==selected);event(SYNC);return 1;}
static int renderer(void *context,uint32_t slot,const GeOriginalBondCameraConfig *c,const GeOriginalBondCameraResult *r)
{assert(context==players&&slot==selected&&slot==local&&c&&r);event(RENDERER);return 1;}
static void expect(unsigned *cursor,unsigned id,uint32_t slot)
{assert(*cursor<event_count);assert(events[*cursor]==id&&event_slots[*cursor]==slot);++*cursor;}

static void run(uint32_t n,uint32_t display,uint32_t rotation,int activation,int object)
{
    GeOriginalMultiplayerDeathState death={0};GeOriginalMultiplayerViews views={0};
    GeOriginalMultiplayerHud hud={0};GeOriginalStageActiveProps active={0};
    GeOriginalMultiplayerRegenBatch regen;
    int opaque_body;
    GeOriginalMultiplayerRuntimeFrame frame={.tick=17,.count=n,.local=display,
        .clock_timer=1,.global_timer=91,.global_timer_delta=1,.death=&death,.hud=&hud,
        .views=&views,.active_props=&active,.guards=(void *)&opaque_body,
        .context=players,.select_player=select_player,
        .produce_camera=camera,.sync_gun_resources=sync_gun,.prepare_local_renderer=renderer};
    count=n;local=selected=display;reload=activation;object_present=object;held_frontier=0;
    shuffle_rotation=rotation;move_ordinal=0;passed_zero=0;
    mission_timer=mission_before=57;watch_time_0=watch_before=101;
    g_CameraMode=CAMERAMODE_NONE;is_timer_active=1;
    /* Native updateFrameCounters must replace a stale prior-stage raw delta.
     * Mission uses the committed simulation clock; watch uses raw retraces. */
    speedgraphframes=99;updateFrameCounters(3);assert(speedgraphframes==3);
    object_prop.type=PROP_TYPE_OBJ;door_prop.type=PROP_TYPE_DOOR;
    g_OnScreenPropList[0]=object==1?&door_prop:&object_prop;
    g_OnScreenPropList[1]=object==1?&object_prop:&door_prop;
    g_LastOnScreenProp=g_OnScreenPropList+(object?2:0);
    arena_active=0;event_count=0;g_CurrentPlayer=&players[display];
    for(uint32_t i=0;i<n;++i){lives[i].initialized=1;frame.lives[i]=&lives[i];}
    assert(ge_original_multiplayer_runtime_tick(&frame)==GE_ORIGINAL_MP_RUNTIME_INVALID_ARGUMENT&&!event_count);
    arena_active=1;
    assert(ge_original_multiplayer_runtime_begin_world(&active,1,&regen)==GE_ORIGINAL_MP_RUNTIME_OK);
    assert(ge_original_multiplayer_runtime_tick(&frame)==GE_ORIGINAL_MP_RUNTIME_OK);
    unsigned cursor=0;expect(&cursor,EFFECTS,display);expect(&cursor,WORLD,display);expect(&cursor,REGEN,display);expect(&cursor,SHUFFLE,display);
    expect(&cursor,VIEWS_BEGIN,display);expect(&cursor,HUD_BEGIN,display);
    for(uint32_t ordinal=0;ordinal<n;++ordinal){
        uint32_t slot=(n-1-ordinal+shuffle_rotation)%n;
        unsigned prefix[]={SELECT,FOV,PRE_DEATH,Y_AIM,X_AIM,LOOK,AMMO,SIGHT,MOVE,CAMERA,
            VIEW,PROPS,ONSCREEN,AUTOAIM,HELD};
        for(size_t i=0;i<sizeof(prefix)/sizeof(*prefix);++i)expect(&cursor,prefix[i],slot);
        if(activation){
            if(object){expect(&cursor,object==1?TEST_OBJECT:TEST_DOOR,slot);
                expect(&cursor,object==1?INTERACT:DOOR,slot);expect(&cursor,EXECUTE_OP,slot);}
            else{expect(&cursor,RELOAD_RIGHT,slot);expect(&cursor,RELOAD_LEFT,slot);}}
        expect(&cursor,PICKUPS,slot);expect(&cursor,BG_SEMANTICS,slot);expect(&cursor,SYNC,slot);expect(&cursor,GUN,slot);
        if(slot==local)expect(&cursor,RENDERER,slot);
        unsigned suffix[]={MATRICES,FLASH,HUD_PREPARE,POST_DEATH,CAN_RESPAWN,RESPAWN,HUD_CAPTURE};
        for(size_t i=0;i<sizeof(suffix)/sizeof(*suffix);++i)expect(&cursor,suffix[i],slot);
        if(slot==local)expect(&cursor,CAPTURE,slot);
    }
    expect(&cursor,FINISH,rotation);expect(&cursor,SERVICES,rotation);
    assert(cursor==event_count&&frame.tick==17&&frame.last_slot==rotation);
    assert(getMissiontimer()==58&&watch_time_0==101+(int)(3*n));
    /* A real held-item frontier is fail-closed: no subsequent interaction,
     * pickup, gun, HUD or local draw can hide or retry partial gameplay. */
    held_frontier=1;failed_slot=(n-1+shuffle_rotation)%n;event_count=0;
    mission_before=mission_timer;watch_before=watch_time_0;move_ordinal=0;passed_zero=0;
    assert(ge_original_multiplayer_runtime_tick(&frame)==GE_ORIGINAL_MP_RUNTIME_HELD_ITEMS_FAILED);
    assert(events[event_count-1]==HELD&&frame.last_slot==failed_slot);
    for(unsigned i=0;i<event_count;++i)assert(events[i]!=GUN&&events[i]!=SERVICES&&events[i]!=CAPTURE);
    frame.count=5;event_count=0;
    assert(ge_original_multiplayer_runtime_tick(&frame)==GE_ORIGINAL_MP_RUNTIME_INVALID_ARGUMENT&&!event_count);
}

static void native_clock_modes(void)
{
    extern void ge_original_mp_runtime_mission_clock_exact(s32);
    extern void ge_original_mp_runtime_watch_clock_exact(void);
    for(int mode=CAMERAMODE_NONE;mode<CAMERAMODE_COUNT;++mode)
    for(int active=0;active<2;++active)
    for(selected=0;selected<4;++selected)
    for(int clock=0;clock<2;++clock){
        mission_timer=17;watch_time_0=29;g_CameraMode=(enum CAMERAMODE)mode;
        is_timer_active=active;updateFrameCounters(2);
        ge_original_mp_runtime_mission_clock_exact(clock);
        int advances=selected==0&&(mode==CAMERAMODE_NONE
            ||(mode==CAMERAMODE_FP&&active)||mode==CAMERAMODE_FADE_TO_TITLE);
        assert(getMissiontimer()==17+(advances?clock:0));
        assert(watch_time_0==29);
        ge_original_mp_runtime_watch_clock_exact();
        assert(watch_time_0==31);
    }
}

int main(void)
{
    for(uint32_t n=2;n<=4;n+=2)for(uint32_t display=0;display<n;++display)
        for(uint32_t rotation=0;rotation<n;++rotation)
            for(int activation=0;activation<2;++activation)for(int object=0;object<3;++object)
                run(n,display,rotation,activation,object);
    native_clock_modes();
    puts("shared multiplayer runtime: 2/4-slot rotated traversal, native mission/watch timers around movement, canonical mixed interaction/TICKOP, both-hand reload, FOV, local capture, HUD/respawn and frontier abort; other observing producer doubles, not match acceptance");
    return 0;
}

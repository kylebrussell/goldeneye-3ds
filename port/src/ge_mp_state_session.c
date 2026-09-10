#include "ge_mp_state_session.h"
#include "ge_original_multiplayer_session.h"
#include "ge_original_multiplayer_rules.h"
#include "ge_original_stage_setup.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrai.h"
#include "game/ramromreplay.h"
#include <string.h>

#define SHAPE(id,type,count,member,path) {id,GE_MP_STATE_##type,GE_MP_STATE_##type==GE_MP_STATE_ACTOR_REF?GE_MP_FIELD_NULLABLE:0,count,count},
static const GeMpFieldSchema fields[] = {GE_MP_SESSION_FIELDS(SHAPE)};
#undef SHAPE
_Static_assert(sizeof(fields)/sizeof(fields[0])==GE_MP_SESSION_FIELD_COUNT,"session field inventory");
static const GeMpRecordSchema schema = {0,GE_MP_SESSION_SYSTEMS,fields,GE_MP_SESSION_FIELD_COUNT};
const GeMpRecordSchema *ge_mp_state_session_schema(void) { return &schema; }
const char *ge_mp_state_session_field_path(uint16_t field)
{
#define PATH(id,type,count,member,path) case id:return path;
    switch(field){GE_MP_SESSION_FIELDS(PATH) default:return NULL;}
#undef PATH
}
const char *ge_mp_state_session_gap_path(GeMpSessionGapReason reason)
{
    switch(reason) {
    case GE_MP_SESSION_GAP_NATIVE_SESSION:return "session.bound_native_roster";
    case GE_MP_SESSION_GAP_PUBLICATION:return "rules.validated_setup_publication";
    case GE_MP_SESSION_GAP_REFERENCE:return "typed_owner_reference";
    case GE_MP_SESSION_GAP_SCHEDULE:return "runtime.committed_world_death_boundary";
    case GE_MP_SESSION_GAP_LIVE:return "runtime.lives.initialized";
    case GE_MP_SESSION_GAP_WATCH_OWNER:return "mpwatch.who_paused_and_watch_transition_owner";
    case GE_MP_SESSION_GAP_ACTIVE_PROPS_OWNER:return "runtime.active_props.owner_counters_and_provider_context";
    case GE_MP_SESSION_GAP_GUARD_SCHEDULE_OWNER:return "runtime.guards.matrix_muzzleflash_tick_guards";
    case GE_MP_SESSION_GAP_HUD_SCHEDULE_OWNER:return "runtime.hud.tick_initialized_prepared_captured_failed";
    case GE_MP_SESSION_GAP_RAMROM_OWNER:return "ramrom.replay_record_stream_cursors_and_header_owner";
    case GE_MP_SESSION_GAP_MUSIC_TRANSITION_OWNER:return "mp_music.mission_state_and_transition_rng_owner";
    case GE_MP_SESSION_GAP_RESOURCE_PROVIDERS:return "runtime.player_floor_gun_model_loader_providers";
    case GE_MP_SESSION_GAP_BUILD:return "native.US_nonDEBUG_layout";
    default:return NULL;
    }
}
static void gap(GeMpSessionSnapshot *out,uint16_t field,unsigned index,
    GeMpSessionGapReason reason,GeMpStateStatus status)
{
    if(out->gap_count<GE_MP_SESSION_MAX_GAPS)
        out->gaps[out->gap_count++]=(GeMpSessionGap){field,(uint16_t)index,reason,status};
    ++out->total_gaps;
    if(reason==GE_MP_SESSION_GAP_REFERENCE)++out->coverage.unresolved_references;
    else ++out->coverage.unclassified_fields;
}
static int resolve(const GeMpRegistry *registry,const GeMpSessionProviders *providers,
    GeMpSessionSnapshot *out,uint16_t field,unsigned index,GeMpSessionReferenceKind kind,
    const void *pointer,size_t extent,uint64_t *id,uint64_t *offset)
{
    GeMpSessionReference ref={0};
    GeMpStateStatus status=GE_MP_STATE_INCOMPLETE;
    if(!pointer)return 1;
    if(providers && providers->reference)
        status=providers->reference(providers->context,field,index,kind,pointer,extent,&ref);
    if(status==GE_MP_STATE_OK) {
        if(!ref.actor_id)status=GE_MP_STATE_INCOMPLETE;
        else status=ge_mp_registry_resolve(registry,ref.actor_id,NULL);
        if(status==GE_MP_STATE_OK && kind==GE_MP_SESSION_REF_PLAYER
                && (ref.offset || (ref.actor_id>>32)!=ge_mp_registry_key(0xE6,(uint8_t)index,0)))
            status=GE_MP_STATE_INCOMPLETE;
        if(status==GE_MP_STATE_OK && kind==GE_MP_SESSION_REF_PROP
                && (ref.offset || (ref.actor_id>>56)!=0xE3
                    || ((ref.actor_id>>48)&255)!=255 || ((ref.actor_id>>32)&65535)>=MAX_PROPS))
            status=GE_MP_STATE_INCOMPLETE;
    }
    if(status!=GE_MP_STATE_OK) {
        gap(out,field,index,GE_MP_SESSION_GAP_REFERENCE,status);
        out->fields[field-256].count=0;
        if(offset)out->fields[field-255].count=0;
        return 0;
    }
    *id=ref.actor_id;
    if(offset)*offset=ref.offset;
    return 1;
}
static void active_props(const GeMpRegistry *registry,const GeMpSessionProviders *providers,
    const GeMpSessionSchedule *schedule,const GeOriginalMultiplayerSessionNativeView *session,
    GeMpSessionSnapshot *out)
{
    const GeOriginalStageActiveProps *state=schedule->active_props;
    GeOriginalStageActivePropsNativeView native;
    GeMpSessionValues *v=&out->values;
    if(!state || !ge_original_stage_active_props_native_view(state,&native)) {
        gap(out,299,0,GE_MP_SESSION_GAP_ACTIVE_PROPS_OWNER,GE_MP_STATE_INCOMPLETE);
        for(unsigned i=299;i<=321;++i)out->fields[i-256].count=0;
        return;
    }
    v->active_pre_ticks=state->pre_ticks;v->active_ticks=state->ticks;
    v->active_paused_ticks=state->paused_ticks;v->active_count=state->count;
    v->active_chr_count=state->chr_count;v->active_bound=state->bound;
    v->active_pending=state->pre_tick_pending;v->active_player_count=state->player_count;
    v->active_player_mask=state->player_tick_mask;v->active_mismatch=state->last_binding_mismatch;
    v->active_ordered_present=state->ordered!=NULL;
    v->native_chr_count=native.current_chr_count;
    unsigned mask=(1U<<session->count)-1U;
    if(!state->bound || !state->ordered || state->ordered!=native.owned_ordered
            || state->player_count!=session->count || state->pre_tick_pending
            || (state->player_tick_mask&~mask) || state->chrs!=native.current_chrs
            || state->chr_count!=(uint64_t)(uint32_t)native.current_chr_count
            || native.current_chr_count<0 || !state->setup || !state->chrs)
        gap(out,299,0,GE_MP_SESSION_GAP_ACTIVE_PROPS_OWNER,GE_MP_STATE_INCOMPLETE);
    for(unsigned i=0;i<4;++i) {
        resolve(registry,providers,out,310,i,GE_MP_SESSION_REF_PROP,state->player_props[i],0,&v->active_players[i],NULL);
        if(state->player_props[i]!=(session->players[i]?session->players[i]->prop:NULL))
            gap(out,310,i,GE_MP_SESSION_GAP_ACTIVE_PROPS_OWNER,GE_MP_STATE_INCOMPLETE);
    }
    resolve(registry,providers,out,311,0,GE_MP_SESSION_REF_SETUP_OWNER,state->setup,sizeof(GeOriginalStageSetupRuntime),
        &v->active_setup,&v->active_setup_offset);
    size_t chr_extent=0;
    if(state->chr_count<=SIZE_MAX/sizeof(ChrRecord))chr_extent=state->chr_count*sizeof(ChrRecord);
    else gap(out,303,0,GE_MP_SESSION_GAP_ACTIVE_PROPS_OWNER,GE_MP_STATE_INCOMPLETE);
    resolve(registry,providers,out,313,0,GE_MP_SESSION_REF_CHR_POOL,state->chrs,chr_extent,
        &v->active_chrs,&v->active_chrs_offset);
    chr_extent=0;
    if(native.current_chr_count>=0 && (size_t)native.current_chr_count<=SIZE_MAX/sizeof(ChrRecord))
        chr_extent=(size_t)native.current_chr_count*sizeof(ChrRecord);
    else gap(out,317,0,GE_MP_SESSION_GAP_ACTIVE_PROPS_OWNER,GE_MP_STATE_INCOMPLETE);
    resolve(registry,providers,out,315,0,GE_MP_SESSION_REF_CHR_POOL,native.current_chrs,chr_extent,
        &v->native_chrs,&v->native_chrs_offset);
    resolve(registry,providers,out,318,0,GE_MP_SESSION_REF_PROPDEFS,native.current_propdefs,0,
        &v->native_propdefs,&v->native_propdefs_offset);
    resolve(registry,providers,out,320,0,GE_MP_SESSION_REF_AILISTS,native.current_ailists,0,
        &v->native_ailists,&v->native_ailists_offset);
}
static void hud_schedule(const GeMpSessionSchedule *schedule,unsigned count,GeMpSessionSnapshot *out)
{
    const GeOriginalMultiplayerHud *hud=schedule->hud;GeMpSessionValues *v=&out->values;
    if(!hud) {
        gap(out,322,0,GE_MP_SESSION_GAP_HUD_SCHEDULE_OWNER,GE_MP_STATE_INCOMPLETE);
        for(unsigned i=322;i<=328;++i)out->fields[i-256].count=0;
        return;
    }
    v->hud_tick=hud->tick;v->hud_player_count=hud->player_count;
    v->hud_initialized=hud->initialized;v->hud_prepared=hud->prepared;
    v->hud_captured=hud->captured;v->hud_failed=hud->failed;v->hud_intro_camera=hud->intro_camera;
    unsigned mask=(1U<<count)-1U;
    if(hud->failed || (!schedule->next_tick && hud->initialized)
            || (schedule->next_tick && (!hud->initialized || hud->tick!=schedule->next_tick-1
                || hud->player_count!=count || hud->prepared!=mask || hud->captured!=mask
                || (hud->intro_camera&~mask))))
        gap(out,322,0,GE_MP_SESSION_GAP_HUD_SCHEDULE_OWNER,GE_MP_STATE_INCOMPLETE);
}
GeMpStateStatus ge_mp_state_session_capture(const GeMpRegistry *registry,
    const GeMpSessionProviders *providers,const GeMpSessionSchedule *schedule,
    GeMpSessionSnapshot *out)
{
    GeOriginalMultiplayerSessionNativeView session;
    GeOriginalMultiplayerRulesNativeView rules;
    if(!registry || !registry->entries || registry->count>registry->capacity
            || registry->capacity>GE_MP_STATE_MAX_RECORDS || !schedule || !out)
        return GE_MP_STATE_INVALID;
    memset(out,0,sizeof(*out));
    GeMpSessionValues *v=&out->values;
#define FIELD(id,type,n,member,path) out->fields[id-256]=(GeMpStateField){id,GE_MP_STATE_##type,n,&v->member};
    GE_MP_SESSION_FIELDS(FIELD)
#undef FIELD
    out->record=(GeMpStateRecord){0,0,out->fields,GE_MP_SESSION_FIELD_COUNT};
    out->coverage.enumerated_systems=GE_MP_SESSION_SYSTEMS;
#if !defined(VERSION_US) || defined(DEBUG)
    gap(out,0,0,GE_MP_SESSION_GAP_BUILD,GE_MP_STATE_INCOMPLETE);
#endif
    if(ge_original_multiplayer_session_native_view(&session)!=GE_ORIGINAL_MP_SESSION_OK) {
        gap(out,256,0,GE_MP_SESSION_GAP_NATIVE_SESSION,GE_MP_STATE_INCOMPLETE);
        for(unsigned i=0;i<GE_MP_SESSION_FIELD_COUNT;++i)out->fields[i].count=0;
        return GE_MP_STATE_INCOMPLETE;
    }
    v->count=session.count;
    v->decision=session.state.decision;v->end_reason=session.state.end_reason;
    v->results_causes=session.state.results_causes;v->freeze_causes=session.state.freeze_causes;
    v->point_limit=session.state.point_limit;v->stop_play=session.state.stop_play;
    v->game_over=session.state.game_over;v->paused=session.state.paused;
    for(unsigned i=0;i<4;++i)
        resolve(registry,providers,out,265,i,GE_MP_SESSION_REF_PLAYER,session.players[i],0,&v->players[i],NULL);

    ge_original_multiplayer_rules_native_view(&rules);
    v->scenario=rules.scenario;v->weapon_set=rules.weapon_set;
    v->startpad_count=rules.startpad_count;
    for(unsigned i=0;i<2;++i)v->starting_weapon[i]=rules.starting_weapon[i];
    v->intro_words=rules.validated_intro_words;v->pad_count=rules.validated_pad_count;
    v->demo_selection=rules.validated_demo_playback;
    v->playback=rules.playback;v->recording=rules.recording;
    if(!rules.validated_intro || !rules.validated_pads || !rules.validated_intro_words
            || !rules.validated_pad_count || rules.startpad_count<1 || rules.startpad_count>16
            || rules.current_intro!=rules.validated_intro || rules.current_pads!=rules.validated_pads
            || rules.validated_intro_words>SIZE_MAX/sizeof(int32_t)
            || rules.validated_pad_count>SIZE_MAX/sizeof(PadRecord))
        gap(out,272,0,GE_MP_SESSION_GAP_PUBLICATION,GE_MP_STATE_INCOMPLETE);
    size_t intro_extent=rules.validated_intro_words<=SIZE_MAX/sizeof(int32_t)
        ?rules.validated_intro_words*sizeof(int32_t):0;
    size_t pad_extent=rules.validated_pad_count<=SIZE_MAX/sizeof(PadRecord)
        ?rules.validated_pad_count*sizeof(PadRecord):0;
    resolve(registry,providers,out,272,0,GE_MP_SESSION_REF_INTRO,rules.validated_intro,intro_extent,
        &v->validated_intro,&v->validated_intro_offset);
    resolve(registry,providers,out,275,0,GE_MP_SESSION_REF_PAD_ARRAY,rules.validated_pads,pad_extent,
        &v->validated_pads,&v->validated_pads_offset);
    resolve(registry,providers,out,279,0,GE_MP_SESSION_REF_INTRO,rules.current_intro,
        rules.current_intro==rules.validated_intro?intro_extent:0,&v->current_intro,&v->current_intro_offset);
    resolve(registry,providers,out,281,0,GE_MP_SESSION_REF_PAD_ARRAY,rules.current_pads,
        rules.current_pads==rules.validated_pads?pad_extent:0,&v->current_pads,&v->current_pads_offset);
    for(unsigned i=0;i<16;++i) {
        resolve(registry,providers,out,270,i,GE_MP_SESSION_REF_PAD,rules.startpads[i],sizeof(PadRecord),
            &v->startpads[i],&v->startpad_offsets[i]);
        if(i<(unsigned)rules.startpad_count && !rules.startpads[i])
            gap(out,270,i,GE_MP_SESSION_GAP_PUBLICATION,GE_MP_STATE_INCOMPLETE);
    }
    if(rules.demo_is_owned) {
        v->demo_kind=1;v->demo_slot=rules.owned_demo_slot;
    } else if(rules.active_demo) {
        v->demo_kind=2;
        if(resolve(registry,providers,out,287,0,GE_MP_SESSION_REF_DEMO,rules.active_demo,
                sizeof(ramromfilestructure),&v->demo,&v->demo_offset))
            v->demo_slot=((const ramromfilestructure *)rules.active_demo)->slotnum;
        else out->fields[286-256].count=0;
    }
    if((v->playback || v->recording) && !rules.active_demo)
        gap(out,287,0,GE_MP_SESSION_GAP_PUBLICATION,GE_MP_STATE_INCOMPLETE);
    if(v->playback || v->recording)
        gap(out,283,0,GE_MP_SESSION_GAP_RAMROM_OWNER,GE_MP_STATE_INCOMPLETE);
    if((uint32_t)v->demo_selection!=((v->playback || v->recording)?v->demo_slot:0))
        gap(out,278,0,GE_MP_SESSION_GAP_PUBLICATION,GE_MP_STATE_INCOMPLETE);

    v->next_tick=schedule->next_tick;v->elapsed60=schedule->elapsed60;
    v->boundary=(uint8_t)schedule->boundary;
    if(schedule->elapsed60<0 || (unsigned)schedule->boundary>GE_MP_SESSION_TERMINAL_BEFORE_WORLD
            || (schedule->boundary==GE_MP_SESSION_INITIAL && (schedule->next_tick || schedule->elapsed60))
            || (schedule->boundary==GE_MP_SESSION_WORLD_COMMITTED && !schedule->next_tick)
            || (schedule->boundary==GE_MP_SESSION_TERMINAL_BEFORE_WORLD
                && session.state.decision!=GE_ORIGINAL_MP_END_RESULTS))
        gap(out,289,0,GE_MP_SESSION_GAP_SCHEDULE,GE_MP_STATE_INCOMPLETE);
    const GeOriginalMultiplayerDeathState *death=schedule->death;
    if(!death) {
        gap(out,292,0,GE_MP_SESSION_GAP_SCHEDULE,GE_MP_STATE_INCOMPLETE);
        for(unsigned i=292;i<=297;++i)out->fields[i-256].count=0;
    } else {
        unsigned mask=(1U<<session.count)-1U,expected=schedule->next_tick?mask:0;
        v->death_count=death->count;v->death_pre_valid=death->pre_valid;v->death_post_valid=death->post_valid;
        if(death->count!=session.count || death->pre_valid!=expected || death->post_valid!=expected)
            gap(out,292,0,GE_MP_SESSION_GAP_SCHEDULE,GE_MP_STATE_INCOMPLETE);
        for(unsigned i=0;i<4;++i) {
            v->death_pre[i]=death->pre_tick[i];v->death_post[i]=death->post_tick[i];
            resolve(registry,providers,out,293,i,GE_MP_SESSION_REF_PLAYER,death->players[i],0,&v->death_players[i],NULL);
            if(death->players[i]!=session.players[i]
                    || (i<session.count && schedule->next_tick
                        && (death->pre_tick[i]!=schedule->next_tick-1 || death->post_tick[i]!=schedule->next_tick-1)))
                gap(out,293,i,GE_MP_SESSION_GAP_SCHEDULE,GE_MP_STATE_INCOMPLETE);
        }
    }
    for(unsigned i=0;i<session.count;++i) {
        if(schedule->lives[i])v->live_initialized[i]=schedule->lives[i]->initialized;
        if(!schedule->lives[i] || !schedule->lives[i]->initialized) {
            gap(out,298,i,GE_MP_SESSION_GAP_LIVE,GE_MP_STATE_INCOMPLETE);
            if(!schedule->lives[i])out->fields[298-256].count=0;
        }
    }
    active_props(registry,providers,schedule,&session,out);
    hud_schedule(schedule,session.count,out);
    /* These are native/wrapper owners, not nullable success defaults. They
     * need separate typed coverage before an aggregate can attest complete. */
    gap(out,0,0,GE_MP_SESSION_GAP_WATCH_OWNER,GE_MP_STATE_INCOMPLETE);
    gap(out,0,0,GE_MP_SESSION_GAP_GUARD_SCHEDULE_OWNER,GE_MP_STATE_INCOMPLETE);
    gap(out,0,0,GE_MP_SESSION_GAP_RESOURCE_PROVIDERS,GE_MP_STATE_INCOMPLETE);
    gap(out,0,0,GE_MP_SESSION_GAP_MUSIC_TRANSITION_OWNER,GE_MP_STATE_INCOMPLETE);
    return out->total_gaps?GE_MP_STATE_INCOMPLETE:GE_MP_STATE_OK;
}

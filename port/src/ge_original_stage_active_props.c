#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/player.h"
#include "game/chrai.h"

#include "ge_original_dam_guard_chr_scheduler.h"
#include "ge_original_stage_active_props.h"
#include "ge_original_stage_setup.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern PropRecord *g_ActivePropsHead;
extern PropRecord *g_ActivePropsTail;
extern ChrRecord *g_ChrSlots;
extern s32 g_NumChrSlots;
extern stagesetup g_CurrentSetup;
extern s32 g_ClockTimer;
extern s32 g_GlobalTimer;
extern f32 g_GlobalTimerDelta;
extern struct player *g_CurrentPlayer;
extern struct player *g_playerPointers[4];
/* Canonical RETICK can move the list head; ownership outlives that order. */
static GeOriginalStageActiveProps *g_active_owner;

static int input_before(const GeOriginalStageActivePropInput *left,
                        const GeOriginalStageActivePropInput *right)
{
    if(left->kind!=right->kind)return left->kind<right->kind;
    if(left->kind==GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED)
        return left->command_index<right->command_index;
    return 0;
}

static int chr_pointer_in_pool(const ChrRecord *chr,const ChrRecord *pool,
                               size_t count)
{
    uintptr_t value=(uintptr_t)chr,first=(uintptr_t)pool;
    return chr!=NULL&&pool!=NULL&&value>=first
        &&value<(uintptr_t)(pool+count)
        &&(value-first)%sizeof(*pool)==0U;
}

static uint32_t active_list_mismatch(const PropRecord *player)
{
    PropRecord *cursor=g_ActivePropsHead;
    PropRecord *previous=NULL;
    size_t visited=0U;
    int player_seen=0;
    uint32_t mismatch=0U;
    if(cursor==NULL)mismatch|=4U;
    while(cursor!=NULL&&visited<MAX_PROPS){
        if(cursor->prev!=previous){mismatch|=8U;break;}
        if(cursor==player)player_seen=1;
        previous=cursor;cursor=cursor->next;++visited;
    }
    if(cursor!=NULL)mismatch|=8U;
    if(!player_seen)mismatch|=16U;
    if(g_ActivePropsTail!=previous)mismatch|=32U;
    return mismatch;
}

GeOriginalStageActivePropStatus ge_original_stage_active_props_compose(
    GeOriginalStageActiveProps *state,
    const GeOriginalStageSetupRuntime *setup,void *opaque_player_prop,
    ChrRecord *chrs,size_t chr_count,
    const GeOriginalStageActivePropInput *inputs,size_t input_count)
{
    return ge_original_stage_active_props_compose_players(state, setup,
        &opaque_player_prop, 1, chrs, chr_count, inputs, input_count);
}

GeOriginalStageActivePropStatus ge_original_stage_active_props_compose_players(
    GeOriginalStageActiveProps *state,
    const GeOriginalStageSetupRuntime *setup,void *const *player_props,
    size_t player_count, ChrRecord *chrs,size_t chr_count,
    const GeOriginalStageActivePropInput *inputs,size_t input_count)
{
    GeOriginalStageActivePropInput *ordered;
    PropRecord *player;
    void *players[4];
    size_t index,prior;
    if(state==NULL||setup==NULL||setup->setup==NULL||player_props==NULL
            ||player_count<1U||player_count>4U||chrs==NULL||chr_count==0U
            ||(inputs==NULL&&input_count!=0U)||input_count>MAX_PROPS-player_count
            ||chr_count>(size_t)INT32_MAX)
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_ARGUMENT;
    for(index=0U;index<player_count;++index){
        player=player_props[index];
        if(player==NULL||player->type!=PROP_TYPE_VIEWER||player->parent!=NULL)
            return GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_PLAYER;
        for(prior=0U;prior<index;++prior)
            if(player_props[prior]==player)
                return GE_ORIGINAL_STAGE_ACTIVE_PROP_DUPLICATE;
    }
    memcpy(players,player_props,player_count*sizeof(*player_props));
    player_props=players;
    player=player_props[0];
    ordered=calloc(input_count?input_count:1U,sizeof(*ordered));
    if(ordered==NULL)return GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_ARGUMENT;
    if(input_count)memcpy(ordered,inputs,input_count*sizeof(*ordered));
    for(index=0U;index<input_count;++index){
        PropRecord *prop=ordered[index].prop;
        if((ordered[index].kind!=GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED
                &&ordered[index].kind!=GE_ORIGINAL_STAGE_ACTIVE_PROP_DYNAMIC)
                ||prop==NULL||prop==player||prop->parent!=NULL
                ||(ordered[index].kind==GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED
                    &&ordered[index].command_index>=setup->prop_record_count)){
            free(ordered);return GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_ACTOR;
        }
        for(prior=0U;prior<player_count;++prior)
            if(prop==player_props[prior]){
                free(ordered);return GE_ORIGINAL_STAGE_ACTIVE_PROP_DUPLICATE;
            }
        if(prop->type==PROP_TYPE_CHR
                &&(!chr_pointer_in_pool(prop->chr,chrs,chr_count)
                    ||prop->chr->prop!=prop||prop->chr->model==NULL
                    ||prop->chr->ailist==NULL)){
            free(ordered);return GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_ACTOR;
        }
        for(prior=0U;prior<index;++prior)
            if(ordered[prior].prop==prop
                    ||(ordered[index].kind==GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED
                        &&ordered[prior].kind==GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED
                        &&ordered[prior].command_index==ordered[index].command_index)){
                free(ordered);return GE_ORIGINAL_STAGE_ACTIVE_PROP_DUPLICATE;
            }
        for(prior=index;prior>0U
                &&input_before(&ordered[prior],&ordered[prior-1U]);--prior){
            GeOriginalStageActivePropInput swap=ordered[prior];
            ordered[prior]=ordered[prior-1U];ordered[prior-1U]=swap;
        }
    }
    ge_original_stage_active_props_close(state);
    state->ordered=ordered;state->setup=setup;state->chrs=chrs;
    state->player_prop=player;state->count=input_count;
    state->chr_count=chr_count;
    memcpy(state->player_props,player_props,player_count*sizeof(*player_props));
    state->player_count=(uint8_t)player_count;
    for(index=0U;index<player_count;++index){
        PropRecord *prop=player_props[index];
        prop->prev=index?player_props[index-1U]:NULL;
        prop->next=index+1U<player_count?player_props[index+1U]
            :input_count?ordered[0].prop:NULL;
    }
    for(index=0U;index<input_count;++index){
        PropRecord *prop=ordered[index].prop;
        prop->prev=index==0U?player_props[player_count-1U]
            :(PropRecord *)ordered[index-1U].prop;
        prop->next=index+1U<input_count
            ?(PropRecord *)ordered[index+1U].prop:NULL;
    }
    g_ActivePropsHead=player;
    g_ActivePropsTail=input_count?ordered[input_count-1U].prop
        :player_props[player_count-1U];
    g_ChrSlots=chrs;g_NumChrSlots=(s32)chr_count;
    g_CurrentSetup=*setup->setup;
    state->bound=1U;
    g_active_owner=state;
    return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
}

static GeOriginalStageActivePropStatus validate_live_binding(
    GeOriginalStageActiveProps *state, int count_pause)
{
    size_t slot;
    if(state==NULL||!state->bound||state->ordered==NULL||g_active_owner!=state)
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_NOT_BOUND;
    state->last_binding_mismatch=0U;
    if(g_ChrSlots!=state->chrs)state->last_binding_mismatch|=1U;
    if(g_NumChrSlots!=(s32)state->chr_count)
        state->last_binding_mismatch|=2U;
    for(slot=0U;slot<state->player_count;++slot)
        state->last_binding_mismatch|=active_list_mismatch(state->player_props[slot]);
    if(state->last_binding_mismatch!=0U)
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_NOT_BOUND;
    if(g_ClockTimer<0||g_GlobalTimer<0||!isfinite(g_GlobalTimerDelta)
            ||g_GlobalTimerDelta<0.0f)
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_TIMER_UNBOUND;
    /* A zero timer/delta is the original watch/cutscene pause state, not a
     * missing service. The platform still reaches this boundary once per
     * fixed simulation step; preserve the binding but do not advance AI or
     * props until canonical time resumes. */
    if(g_ClockTimer==0||g_GlobalTimerDelta==0.0f){
        if(count_pause)state->paused_ticks++;
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
    }
    for(slot=0U;slot<state->player_count;++slot)
        if(g_CurrentPlayer!=NULL&&g_playerPointers[slot]==g_CurrentPlayer
                &&g_CurrentPlayer->prop==state->player_props[slot])break;
    if(slot==state->player_count)
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_PLAYER_UNBOUND;
    if(state->setup==NULL||state->setup->setup==NULL
            ||g_CurrentSetup.propDefs!=state->setup->setup->propDefs
            ||g_CurrentSetup.ailists!=state->setup->setup->ailists)
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_SETUP_UNBOUND;
    return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
}

GeOriginalStageActivePropStatus ge_original_stage_active_props_pre_tick_exact(
    GeOriginalStageActiveProps *state)
{
    GeOriginalStageActivePropStatus status=validate_live_binding(state,1);
    if(status!=GE_ORIGINAL_STAGE_ACTIVE_PROP_OK)return status;
    if(g_ClockTimer==0||g_GlobalTimerDelta==0.0f){
        state->pre_tick_pending=0U;
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
    }
    /* A render-side service may transiently defer propsTick. Retain the
     * already-produced lvlManageMpGame state rather than executing canonical
     * background AI a second time on the next native frame. */
    if(state->pre_tick_pending)return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
    ge_original_dam_guard_all_chr_tick_exact();
    state->pre_tick_pending=1U;
    state->player_tick_mask=0U;
    state->pre_ticks++;
    return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
}

GeOriginalStageActivePropStatus ge_original_stage_active_props_tick_exact(
    GeOriginalStageActiveProps *state)
{
    GeOriginalStageActivePropStatus status=validate_live_binding(state,0);
    if(status!=GE_ORIGINAL_STAGE_ACTIVE_PROP_OK)return status;
    if(g_ClockTimer==0||g_GlobalTimerDelta==0.0f){
        state->pre_tick_pending=0U;
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
    }
    if(!state->pre_tick_pending)
        return GE_ORIGINAL_STAGE_ACTIVE_PROP_NOT_BOUND;
    {
        size_t slot;
        for(slot=0U;slot<state->player_count;++slot)
            if(g_CurrentPlayer->prop==state->player_props[slot])break;
        if(slot==state->player_count||(state->player_tick_mask&(1U<<slot)))
            return GE_ORIGINAL_STAGE_ACTIVE_PROP_PLAYER_UNBOUND;
        ge_original_dam_guard_props_tick_exact();
        state->player_tick_mask|=(uint8_t)(1U<<slot);
        if(state->player_tick_mask==(uint8_t)((1U<<state->player_count)-1U)){
            state->pre_tick_pending=0U;
            state->ticks++;
        }
    }
    return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
}

void ge_original_stage_active_props_close(GeOriginalStageActiveProps *state)
{
    if(state==NULL)return;
    if(state->bound&&g_active_owner==state){
        g_active_owner=NULL;
        g_ActivePropsHead=NULL;g_ActivePropsTail=NULL;
        if(g_ChrSlots==state->chrs){g_ChrSlots=NULL;g_NumChrSlots=0;}
        if(state->setup!=NULL&&state->setup->setup!=NULL
                &&g_CurrentSetup.propDefs==state->setup->setup->propDefs)
            memset(&g_CurrentSetup,0,sizeof(g_CurrentSetup));
    }
    free(state->ordered);memset(state,0,sizeof(*state));
}

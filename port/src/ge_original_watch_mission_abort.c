#include "ge_original_watch_mission_abort.h"

#include <string.h>

static uint32_t ge_watch_abort_hash_step(uint32_t hash,uint8_t service)
{
    hash^=service;
    return hash*UINT32_C(16777619);
}

static int ge_watch_abort_services_ready(
    const GeOriginalWatchMissionAbortServices *services)
{
    return services!=NULL
        &&services->set_mission_state_zero!=NULL
        &&services->request_title_stage!=NULL
        &&services->mark_mission_failed_or_aborted!=NULL
        &&services->persist_current_folder_settings!=NULL
        &&services->play_watch_beep!=NULL;
}

int ge_original_watch_mission_abort_reset(
    GeOriginalWatchMissionAbort *state,
    const GeOriginalWatchMissionAbortServices *services)
{
    if(state==NULL)return 0;
    memset(state,0,sizeof(*state));
    state->service_order_hash=UINT32_C(2166136261);
    if(!ge_watch_abort_services_ready(services)){
        state->service_frontiers=1U;
        return 0;
    }
    state->services=*services;
    state->bound=1U;
    return 1;
}

int ge_original_watch_mission_abort_navigation_tick(
    GeOriginalWatchMissionAbort *state,uint16_t buttons_pressed)
{
    if(state==NULL||!state->bound)return 0;
    ++state->navigation_ticks;

    /* Exact watch_screen0_navigation branch. D_800409A4 is cleared before
     * the four original side effects. */
    if(state->item_selected&&state->confirm_selected
            &&(buttons_pressed&GE_ORIGINAL_WATCH_ABORT_CONFIRM_BUTTONS)){
        state->confirm_selected=0U;
        state->services.set_mission_state_zero(state->services.context);
        state->service_order_hash=ge_watch_abort_hash_step(
            state->service_order_hash,1U);
        state->services.request_title_stage(state->services.context);
        state->service_order_hash=ge_watch_abort_hash_step(
            state->service_order_hash,2U);
        state->services.mark_mission_failed_or_aborted(
            state->services.context);
        state->service_order_hash=ge_watch_abort_hash_step(
            state->service_order_hash,3U);
        state->services.persist_current_folder_settings(
            state->services.context);
        state->service_order_hash=ge_watch_abort_hash_step(
            state->service_order_hash,4U);
        ++state->aborts;
        return 1;
    }
    return 0;
}

int ge_original_watch_mission_abort_presentation_tick(
    GeOriginalWatchMissionAbort *state,uint16_t buttons_pressed,
    uint16_t buttons_held,int32_t stick_x)
{
    if(state==NULL||!state->bound)return 0;
    ++state->presentation_ticks;

    /* draw_watch_current_page toggles focus before drawing the page. The
     * beep occurs only when focus becomes active. */
    if(buttons_pressed&GE_ORIGINAL_WATCH_ABORT_CONFIRM_BUTTONS){
        if(state->item_selected)state->item_selected=0U;
        else{
            state->item_selected=1U;
            state->services.play_watch_beep(state->services.context);
        }
    }

    /* Exact draw_abort_cancel_confirm thresholds and button masks. */
    if(state->item_selected){
        if(!state->confirm_selected){
            if(stick_x>=0x2e
                    ||(buttons_held&GE_ORIGINAL_WATCH_ABORT_SELECT_CONFIRM))
                state->confirm_selected=1U;
        }else if(stick_x<-0x2d
                ||(buttons_held&GE_ORIGINAL_WATCH_ABORT_SELECT_CANCEL))
            state->confirm_selected=0U;
    }
    return 1;
}

int ge_original_watch_mission_abort_frame_tick(
    GeOriginalWatchMissionAbort *state,uint16_t buttons_pressed,
    uint16_t buttons_held,int32_t stick_x)
{
    int aborted=ge_original_watch_mission_abort_navigation_tick(
        state,buttons_pressed);
    (void)ge_original_watch_mission_abort_presentation_tick(
        state,buttons_pressed,buttons_held,stick_x);
    return aborted;
}

void ge_original_watch_mission_abort_snapshot(
    const GeOriginalWatchMissionAbort *state,
    GeOriginalWatchMissionAbortSnapshot *snapshot)
{
    if(state==NULL||snapshot==NULL)return;
    snapshot->navigation_ticks=state->navigation_ticks;
    snapshot->presentation_ticks=state->presentation_ticks;
    snapshot->aborts=state->aborts;
    snapshot->service_frontiers=state->service_frontiers;
    snapshot->service_order_hash=state->service_order_hash;
    snapshot->item_selected=state->item_selected;
    snapshot->confirm_selected=state->confirm_selected;
    snapshot->bound=state->bound;
}

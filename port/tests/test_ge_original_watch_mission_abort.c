#include "ge_original_watch_mission_abort.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Harness {
    uint8_t order[4];
    size_t order_count;
    uint32_t beeps;
} Harness;

static void record(Harness *harness,uint8_t service)
{
    assert(harness->order_count<sizeof(harness->order));
    harness->order[harness->order_count++]=service;
}
static void mission_zero(void *context){record(context,1U);}
static void title(void *context){record(context,2U);}
static void aborted(void *context){record(context,3U);}
static void persist(void *context){record(context,4U);}
static void beep(void *context){++((Harness *)context)->beeps;}

int main(void)
{
    Harness harness={0};
    GeOriginalWatchMissionAbort state;
    GeOriginalWatchMissionAbortSnapshot snapshot;
    GeOriginalWatchMissionAbortServices services={
        &harness,mission_zero,title,aborted,persist,beep,
    };
    GeOriginalWatchMissionAbortServices missing=services;

    missing.persist_current_folder_settings=NULL;
    assert(!ge_original_watch_mission_abort_reset(&state,&missing));
    ge_original_watch_mission_abort_snapshot(&state,&snapshot);
    assert(!snapshot.bound&&snapshot.service_frontiers==1U);

    assert(ge_original_watch_mission_abort_reset(&state,&services));
    /* Navigation precedes presentation: the first A/Z edge selects Abort,
     * but cannot also confirm it during the earlier navigation phase. */
    assert(!ge_original_watch_mission_abort_navigation_tick(&state,0xa000));
    assert(ge_original_watch_mission_abort_presentation_tick(
        &state,0x8000,0x0100,0));
    ge_original_watch_mission_abort_snapshot(&state,&snapshot);
    assert(snapshot.item_selected&&snapshot.confirm_selected);
    assert(snapshot.aborts==0U&&harness.beeps==1U);

    assert(ge_original_watch_mission_abort_navigation_tick(&state,0x2000));
    assert(harness.order_count==4U
        &&memcmp(harness.order,(uint8_t[]){1,2,3,4},4U)==0);
    ge_original_watch_mission_abort_snapshot(&state,&snapshot);
    assert(snapshot.aborts==1U&&!snapshot.confirm_selected
        &&snapshot.navigation_ticks==2U
        &&snapshot.presentation_ticks==1U
        &&snapshot.service_frontiers==0U
        &&snapshot.service_order_hash==UINT32_C(1463068797));

    /* Strict source thresholds: +45 does not select, +46 does; -45 does not
     * cancel, -46 does. */
    assert(ge_original_watch_mission_abort_reset(&state,&services));
    assert(ge_original_watch_mission_abort_presentation_tick(
        &state,0x8000,0U,45));
    assert(state.item_selected&&!state.confirm_selected);
    assert(ge_original_watch_mission_abort_presentation_tick(
        &state,0U,0U,46));
    assert(state.confirm_selected);
    assert(ge_original_watch_mission_abort_presentation_tick(
        &state,0U,0U,-45));
    assert(state.confirm_selected);
    assert(ge_original_watch_mission_abort_presentation_tick(
        &state,0U,0U,-46));
    assert(!state.confirm_selected);
    assert(ge_original_watch_mission_abort_presentation_tick(
        &state,0U,0U,46));
    assert(state.confirm_selected);

    puts("Watch abort: exact select/confirm thresholds and service order retained");
    return 0;
}

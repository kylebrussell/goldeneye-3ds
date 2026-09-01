#include "ge_original_watch_mission_abort_services.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Harness Harness;

typedef struct OwnerContext {
    Harness *harness;
    uint8_t owner;
} OwnerContext;

struct Harness {
    uint8_t order[8];
    size_t order_count;
    int32_t mission_state;
    int32_t requested_stage;
    uint8_t mission_failed_or_aborted;
    uint32_t persisted;
    uint32_t beeps;
};

static void record(OwnerContext *context,uint8_t service,uint8_t owner)
{
    Harness *harness=context->harness;
    assert(context->owner==owner);
    assert(harness->order_count<sizeof(harness->order));
    harness->order[harness->order_count++]=service;
}

static void mission_zero(void *opaque)
{
    OwnerContext *context=opaque;
    record(context,1U,1U);
    context->harness->mission_state=0;
}

static void title(void *opaque)
{
    OwnerContext *context=opaque;
    record(context,2U,2U);
    context->harness->requested_stage=90;
}

static void aborted(void *opaque)
{
    OwnerContext *context=opaque;
    record(context,3U,2U);
    context->harness->mission_failed_or_aborted=1U;
}

static void persist(void *opaque)
{
    OwnerContext *context=opaque;
    record(context,4U,3U);
    ++context->harness->persisted;
}

static void beep(void *opaque)
{
    OwnerContext *context=opaque;
    assert(context->owner==4U);
    ++context->harness->beeps;
}

int main(void)
{
    Harness harness={0};
    OwnerContext mission={&harness,1U};
    OwnerContext frontend={&harness,2U};
    OwnerContext save={&harness,3U};
    OwnerContext audio={&harness,4U};
    GeOriginalWatchMissionAbortOwners owners={
        &mission,mission_zero,
        &frontend,title,aborted,
        &save,persist,
        &audio,beep,
    };
    GeOriginalWatchMissionAbortServiceAdapter adapter;
    GeOriginalWatchMissionAbortServiceSnapshot adapter_snapshot;
    GeOriginalWatchMissionAbort state;
    GeOriginalWatchMissionAbortSnapshot state_snapshot;
    GeOriginalWatchMissionAbortOwners missing=owners;

    missing.persist_current_folder_settings=NULL;
    assert(!ge_original_watch_mission_abort_services_bind(
        &adapter,&missing));
    assert(ge_original_watch_mission_abort_services(&adapter)==NULL);
    ge_original_watch_mission_abort_services_snapshot(
        &adapter,&adapter_snapshot);
    assert(!adapter_snapshot.bound&&adapter_snapshot.binds==1U
        &&adapter_snapshot.service_frontiers==1U);
    assert(!ge_original_watch_mission_abort_reset(
        &state,ge_original_watch_mission_abort_services(&adapter)));
    ge_original_watch_mission_abort_snapshot(&state,&state_snapshot);
    assert(!state_snapshot.bound&&state_snapshot.service_frontiers==1U);

    assert(ge_original_watch_mission_abort_services_bind(
        &adapter,&owners));
    assert(ge_original_watch_mission_abort_reset(
        &state,ge_original_watch_mission_abort_services(&adapter)));
    harness.mission_state=7;
    harness.requested_stage=-1;

    /* The edge first passes through navigation, where Abort is not selected
     * yet, and only then reaches the presentation-owned focus/confirm state. */
    assert(!ge_original_watch_mission_abort_frame_tick(
        &state,0x8000U,GE_ORIGINAL_WATCH_ABORT_SELECT_CONFIRM,0));
    assert(harness.order_count==0U&&harness.beeps==1U);
    ge_original_watch_mission_abort_snapshot(&state,&state_snapshot);
    assert(state_snapshot.navigation_ticks==1U
        &&state_snapshot.presentation_ticks==1U
        &&state_snapshot.item_selected&&state_snapshot.confirm_selected);

    /* On the next edge navigation owns the complete canonical side-effect
     * sequence. Presentation follows afterward and consumes the same edge. */
    assert(ge_original_watch_mission_abort_frame_tick(
        &state,0x2000U,0U,0));
    assert(harness.order_count==4U
        &&memcmp(harness.order,(uint8_t[]){1U,2U,3U,4U},4U)==0);
    assert(harness.mission_state==0&&harness.requested_stage==90
        &&harness.mission_failed_or_aborted==1U&&harness.persisted==1U);
    ge_original_watch_mission_abort_snapshot(&state,&state_snapshot);
    assert(state_snapshot.aborts==1U
        &&state_snapshot.navigation_ticks==2U
        &&state_snapshot.presentation_ticks==2U
        &&!state_snapshot.item_selected&&!state_snapshot.confirm_selected);
    ge_original_watch_mission_abort_services_snapshot(
        &adapter,&adapter_snapshot);
    assert(adapter_snapshot.bound&&adapter_snapshot.binds==1U
        &&adapter_snapshot.service_frontiers==0U
        &&adapter_snapshot.mission_resets==1U
        &&adapter_snapshot.title_requests==1U
        &&adapter_snapshot.aborted_marks==1U
        &&adapter_snapshot.settings_persists==1U
        &&adapter_snapshot.watch_beeps==1U);

    /* A new stage/watch reset reuses the same owners, clears only the two
     * canonical watch selection variables, and cannot replay an old edge. */
    assert(ge_original_watch_mission_abort_reset(
        &state,ge_original_watch_mission_abort_services(&adapter)));
    assert(!ge_original_watch_mission_abort_frame_tick(&state,0U,0U,0));
    assert(harness.order_count==4U&&harness.persisted==1U);

    puts("Watch abort services: original owners and frame ordering retained");
    return 0;
}

#include "ge_original_watch_mission_abort_services.h"

#include <string.h>

static int ge_watch_abort_owners_ready(
    const GeOriginalWatchMissionAbortOwners *owners)
{
    return owners != NULL
        && owners->set_mission_state_zero != NULL
        && owners->request_title_stage != NULL
        && owners->mark_mission_failed_or_aborted != NULL
        && owners->persist_current_folder_settings != NULL
        && owners->play_watch_beep != NULL;
}

static void ge_watch_abort_set_mission_state_zero(void *context)
{
    GeOriginalWatchMissionAbortServiceAdapter *adapter = context;
    adapter->owners.set_mission_state_zero(adapter->owners.mission_context);
    ++adapter->mission_resets;
}

static void ge_watch_abort_request_title_stage(void *context)
{
    GeOriginalWatchMissionAbortServiceAdapter *adapter = context;
    adapter->owners.request_title_stage(adapter->owners.frontend_context);
    ++adapter->title_requests;
}

static void ge_watch_abort_mark_failed_or_aborted(void *context)
{
    GeOriginalWatchMissionAbortServiceAdapter *adapter = context;
    adapter->owners.mark_mission_failed_or_aborted(
        adapter->owners.frontend_context);
    ++adapter->aborted_marks;
}

static void ge_watch_abort_persist_settings(void *context)
{
    GeOriginalWatchMissionAbortServiceAdapter *adapter = context;
    adapter->owners.persist_current_folder_settings(
        adapter->owners.save_context);
    ++adapter->settings_persists;
}

static void ge_watch_abort_play_beep(void *context)
{
    GeOriginalWatchMissionAbortServiceAdapter *adapter = context;
    adapter->owners.play_watch_beep(adapter->owners.audio_context);
    ++adapter->watch_beeps;
}

int ge_original_watch_mission_abort_services_bind(
    GeOriginalWatchMissionAbortServiceAdapter *adapter,
    const GeOriginalWatchMissionAbortOwners *owners)
{
    if (adapter == NULL) return 0;
    memset(adapter, 0, sizeof(*adapter));
    ++adapter->binds;
    if (!ge_watch_abort_owners_ready(owners)) {
        ++adapter->service_frontiers;
        return 0;
    }
    adapter->owners = *owners;
    adapter->services.context = adapter;
    adapter->services.set_mission_state_zero =
        ge_watch_abort_set_mission_state_zero;
    adapter->services.request_title_stage =
        ge_watch_abort_request_title_stage;
    adapter->services.mark_mission_failed_or_aborted =
        ge_watch_abort_mark_failed_or_aborted;
    adapter->services.persist_current_folder_settings =
        ge_watch_abort_persist_settings;
    adapter->services.play_watch_beep = ge_watch_abort_play_beep;
    adapter->bound = 1U;
    return 1;
}

const GeOriginalWatchMissionAbortServices *
ge_original_watch_mission_abort_services(
    const GeOriginalWatchMissionAbortServiceAdapter *adapter)
{
    if (adapter == NULL || !adapter->bound) return NULL;
    return &adapter->services;
}

void ge_original_watch_mission_abort_services_snapshot(
    const GeOriginalWatchMissionAbortServiceAdapter *adapter,
    GeOriginalWatchMissionAbortServiceSnapshot *snapshot)
{
    if (adapter == NULL || snapshot == NULL) return;
    snapshot->binds = adapter->binds;
    snapshot->service_frontiers = adapter->service_frontiers;
    snapshot->mission_resets = adapter->mission_resets;
    snapshot->title_requests = adapter->title_requests;
    snapshot->aborted_marks = adapter->aborted_marks;
    snapshot->settings_persists = adapter->settings_persists;
    snapshot->watch_beeps = adapter->watch_beeps;
    snapshot->bound = adapter->bound;
}

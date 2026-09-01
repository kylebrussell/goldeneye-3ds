#ifndef GE_ORIGINAL_WATCH_MISSION_ABORT_SERVICES_H
#define GE_ORIGINAL_WATCH_MISSION_ABORT_SERVICES_H

#include <stdint.h>

#include "ge_original_watch_mission_abort.h"

/* The four state-owning services used by watch_screen0_navigation live in
 * different original subsystems.  Keep their contexts separate so a native
 * platform can bind the existing mission, frontend, save and audio owners
 * without introducing a second watch or mission state machine. */
typedef struct GeOriginalWatchMissionAbortOwners {
    void *mission_context;
    void (*set_mission_state_zero)(void *context);

    void *frontend_context;
    void (*request_title_stage)(void *context);
    void (*mark_mission_failed_or_aborted)(void *context);

    void *save_context;
    void (*persist_current_folder_settings)(void *context);

    void *audio_context;
    void (*play_watch_beep)(void *context);
} GeOriginalWatchMissionAbortOwners;

typedef struct GeOriginalWatchMissionAbortServiceAdapter {
    GeOriginalWatchMissionAbortOwners owners;
    GeOriginalWatchMissionAbortServices services;
    uint32_t binds;
    uint32_t service_frontiers;
    uint32_t mission_resets;
    uint32_t title_requests;
    uint32_t aborted_marks;
    uint32_t settings_persists;
    uint32_t watch_beeps;
    uint8_t bound;
} GeOriginalWatchMissionAbortServiceAdapter;

typedef struct GeOriginalWatchMissionAbortServiceSnapshot {
    uint32_t binds;
    uint32_t service_frontiers;
    uint32_t mission_resets;
    uint32_t title_requests;
    uint32_t aborted_marks;
    uint32_t settings_persists;
    uint32_t watch_beeps;
    uint8_t bound;
} GeOriginalWatchMissionAbortServiceSnapshot;

/* Binds all original owners atomically.  A missing owner leaves the adapter
 * unbound, allowing ge_original_watch_mission_abort_reset to expose the
 * existing explicit frontier instead of performing a partial abort. */
int ge_original_watch_mission_abort_services_bind(
    GeOriginalWatchMissionAbortServiceAdapter *adapter,
    const GeOriginalWatchMissionAbortOwners *owners);

const GeOriginalWatchMissionAbortServices *
ge_original_watch_mission_abort_services(
    const GeOriginalWatchMissionAbortServiceAdapter *adapter);

void ge_original_watch_mission_abort_services_snapshot(
    const GeOriginalWatchMissionAbortServiceAdapter *adapter,
    GeOriginalWatchMissionAbortServiceSnapshot *snapshot);

#endif

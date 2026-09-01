#ifndef GE_ORIGINAL_LEVEL_H
#define GE_ORIGINAL_LEVEL_H

#include <stdint.h>

typedef int32_t (*GeOriginalLevelPausedProvider)(void *context);
typedef void (*GeOriginalLevelTlbResetProvider)(void *context);
typedef uint16_t (*GeOriginalLevelButtonsPressedProvider)(void *context);

typedef enum GeOriginalLevelSubsystem {
    GE_ORIGINAL_LEVEL_SUBSYSTEM_INITIAL_CHEATS = 0,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_VI_ZBUF,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_PLAYER_PRE_TICK,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_DIFFICULTY,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_ROOM_STATUS,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_ROOM_TRANSITION,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_SKY,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_BULLET_SPARKS,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_BULLET_CASINGS,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_BROKEN_WINDOWS,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_EXPLOSIONS,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_CHRPROP,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_MUSIC_SLOTS,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_LANGUAGE,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_TITLE_CHEATS,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_TITLE_MENU,
    GE_ORIGINAL_LEVEL_SUBSYSTEM_COUNT
} GeOriginalLevelSubsystem;

typedef void (*GeOriginalLevelSubsystemProvider)(
    GeOriginalLevelSubsystem subsystem,
    void *context);

typedef struct GeOriginalLevelProviders {
    GeOriginalLevelPausedProvider is_paused;
    GeOriginalLevelTlbResetProvider reset_tlb_entries;
    void *context;
    GeOriginalLevelButtonsPressedProvider buttons_pressed;
    GeOriginalLevelSubsystemProvider tick_subsystem;
} GeOriginalLevelProviders;

typedef struct GeOriginalLevelTimerState {
    int32_t controls_locked;
    int32_t clock_timer;
    int32_t global_timer;
    int32_t active_frame_updates;
    float global_timer_delta;
    int32_t stage_id;
    int32_t multiplayer_timer;
    float multiplayer_seconds;
    int32_t idle_frames;
    int32_t idle_latched;
    int32_t active_stage_frames;
    float stage_seconds;
    int32_t power_on_frames;
    float power_on_seconds;
} GeOriginalLevelTimerState;

/*
 * Initializes the isolated lvlManageMpGame spine. Missing providers use
 * deterministic no-op/not-paused defaults; no player or world state is made up.
 */
void ge_original_level_init(const GeOriginalLevelProviders *providers);

void ge_original_level_set_controls_locked(int32_t locked);

/* Selects the original stage branch. LEVELID_TITLE (90) suppresses world ticks. */
void ge_original_level_set_stage(int32_t stage_id);

/* Runs the timer-only or stage-spine build selected for the linked lv.c. */
void ge_original_level_timer_tick(int32_t retrace_frames);

/*
 * Runs the original single-player lvlManageMpGame frame spine when linked with
 * GE_PORT_LV_STAGE_TICK_SLICE. Subsystems not migrated yet are explicit
 * providers, invoked in their original order.
 */
void ge_original_level_tick(int32_t retrace_frames);

void ge_original_level_timer_snapshot(GeOriginalLevelTimerState *state);

#endif

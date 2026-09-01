#ifndef GE_ORIGINAL_WATCH_MISSION_ABORT_H
#define GE_ORIGINAL_WATCH_MISSION_ABORT_H

#include <stdint.h>

/* Raw N64 controller masks consumed by the unchanged options.c bodies. */
#define GE_ORIGINAL_WATCH_ABORT_CONFIRM_BUTTONS UINT16_C(0xa000)
#define GE_ORIGINAL_WATCH_ABORT_SELECT_CONFIRM UINT16_C(0x0111)
#define GE_ORIGINAL_WATCH_ABORT_SELECT_CANCEL UINT16_C(0x0222)
#define GE_ORIGINAL_WATCH_ABORT_MISSION_STATUS_PAGE UINT8_C(0)

typedef struct GeOriginalWatchMissionAbortServices {
    void *context;
    void (*set_mission_state_zero)(void *context);
    void (*request_title_stage)(void *context);
    void (*mark_mission_failed_or_aborted)(void *context);
    void (*persist_current_folder_settings)(void *context);
    void (*play_watch_beep)(void *context);
} GeOriginalWatchMissionAbortServices;

typedef struct GeOriginalWatchMissionAbort {
    GeOriginalWatchMissionAbortServices services;
    uint32_t navigation_ticks;
    uint32_t presentation_ticks;
    uint32_t aborts;
    uint32_t service_frontiers;
    uint32_t service_order_hash;
    uint8_t item_selected;
    uint8_t confirm_selected;
    uint8_t bound;
} GeOriginalWatchMissionAbort;

typedef struct GeOriginalWatchMissionAbortSnapshot {
    uint32_t navigation_ticks;
    uint32_t presentation_ticks;
    uint32_t aborts;
    uint32_t service_frontiers;
    uint32_t service_order_hash;
    uint8_t item_selected;
    uint8_t confirm_selected;
    uint8_t bound;
} GeOriginalWatchMissionAbortSnapshot;

/* Owns only the mission-status page's two state variables from options.c:
 * watch_item_is_actively_selected and D_800409A4. All services are required;
 * a missing original side effect is an explicit frontier, never a partial
 * title transition. */
int ge_original_watch_mission_abort_reset(
    GeOriginalWatchMissionAbort *state,
    const GeOriginalWatchMissionAbortServices *services);

/* Exact watch_screen0_navigation abort branch. Call during the canonical
 * watch update, before the presentation phase for the same displayed frame. */
int ge_original_watch_mission_abort_navigation_tick(
    GeOriginalWatchMissionAbort *state,uint16_t buttons_pressed);

/* Exact input-owned portions of draw_watch_current_page followed by
 * draw_abort_cancel_confirm. Call only while the fully opened watch is on
 * WATCH_INDEX_MISSION_STATUS. */
int ge_original_watch_mission_abort_presentation_tick(
    GeOriginalWatchMissionAbort *state,uint16_t buttons_pressed,
    uint16_t buttons_held,int32_t stick_x);

/* One fully-open mission-status watch frame in original order: optionsTick's
 * watch_screen0_navigation first, then draw_watch_current_page's input-owned
 * presentation work.  This is ordering glue only; both unchanged state
 * owners remain the functions above. */
int ge_original_watch_mission_abort_frame_tick(
    GeOriginalWatchMissionAbort *state,uint16_t buttons_pressed,
    uint16_t buttons_held,int32_t stick_x);

void ge_original_watch_mission_abort_snapshot(
    const GeOriginalWatchMissionAbort *state,
    GeOriginalWatchMissionAbortSnapshot *snapshot);

#endif

#ifndef GE_ORIGINAL_MULTIPLAYER_SESSION_H
#define GE_ORIGINAL_MULTIPLAYER_SESSION_H
#include "ge_original_multiplayer_frontend.h"

#define GE_ORIGINAL_MP_SESSION_CAUSE_TIME 1U
#define GE_ORIGINAL_MP_SESSION_CAUSE_POINT 2U
#define GE_ORIGINAL_MP_SESSION_CAUSE_ELIMINATION 4U
#define GE_ORIGINAL_MP_SESSION_CAUSE_ABORT 8U

typedef enum GeOriginalMultiplayerSessionStatus {
    GE_ORIGINAL_MP_SESSION_OK,
    GE_ORIGINAL_MP_SESSION_INVALID_ARGUMENT,
    GE_ORIGINAL_MP_SESSION_INVALID_ROSTER,
    GE_ORIGINAL_MP_SESSION_INVALID_STATS,
    GE_ORIGINAL_MP_SESSION_NOT_INITIALIZED,
    GE_ORIGINAL_MP_SESSION_RESOURCE_FAILED
} GeOriginalMultiplayerSessionStatus;

typedef struct GeOriginalMultiplayerSessionState {
    GeOriginalMultiplayerEndDecision decision;
    GeOriginalMultiplayerFrontendEndReason end_reason;
    uint32_t results_causes;
    uint32_t freeze_causes;
    int32_t time_limit60;
    int32_t point_limit;
    int32_t stop_play;
    int32_t game_over;
    int32_t paused;
    int32_t controls_locked;
} GeOriginalMultiplayerSessionState;

struct player;
typedef struct GeOriginalMultiplayerSessionNativeView {
    GeOriginalMultiplayerSessionState state;
    const struct player *players[4];
    uint32_t count;
} GeOriginalMultiplayerSessionNativeView;
/* Read-only bound roster and state, validated against the current native
 * roster. Entries beyond count are excluded: the owner never reads them. */
GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_native_view(
    GeOriginalMultiplayerSessionNativeView *view);

/* After player creation/configuration and level timer initialization, before
 * first simulation tick. Exact stage-load unlock/unpause and limit setters.
 * Does not reinitialize players, stats, inventory, RNG or level timers. */
GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_reset(
    int32_t time_limit60, int32_t point_limit);
/* Once per world tick, before incrementing elapsed60, chrlvAllChrTick/regen
 * and shuffled player passes. FREEZE sets the original stop flag only: keep
 * ticking deaths, blood/fade and the world. RESULTS uses the exact immediate
 * (gameoverdelay=FALSE) full awards, RNG, favorites, audio and control lock.
 * Keep running all player watch ticks through results; the native menu
 * requests TITLE only after every player selects MENU_FINISHED.
 * Original limits can invoke more than one terminal action in a tick: the
 * mask preserves all causes; end_reason identifies the FIRST source call
 * (time, then points, then elimination), a reporting convention only. */
GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_tick(
    int32_t tick60, int32_t elapsed60, GeOriginalMultiplayerSessionState *state);
/* Explicit caller cancellation, same immediate state entry as MP quit. */
GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_abort(
    GeOriginalMultiplayerSessionState *state);
GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_snapshot(
    GeOriginalMultiplayerSessionState *state);
#endif

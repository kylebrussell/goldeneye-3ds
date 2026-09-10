#ifndef GE_ORIGINAL_MULTIPLAYER_FRONTEND_H
#define GE_ORIGINAL_MULTIPLAYER_FRONTEND_H

#include "ge_original_multiplayer_rules.h"
#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_MP_FRONTEND_CHARACTERS 64
#define GE_ORIGINAL_MP_FRONTEND_LIMITS 8
#define GE_ORIGINAL_MP_FRONTEND_LABEL "Diagnostic local simulation"

typedef enum GeOriginalMultiplayerFrontendPhase {
    GE_ORIGINAL_MP_FRONTEND_SETUP,
    GE_ORIGINAL_MP_FRONTEND_START_PENDING,
    GE_ORIGINAL_MP_FRONTEND_LOADING,
    GE_ORIGINAL_MP_FRONTEND_RUNNING,
    GE_ORIGINAL_MP_FRONTEND_RESULTS
} GeOriginalMultiplayerFrontendPhase;

typedef enum GeOriginalMultiplayerFrontendStatus {
    GE_ORIGINAL_MP_FRONTEND_OK,
    GE_ORIGINAL_MP_FRONTEND_INVALID,
    GE_ORIGINAL_MP_FRONTEND_BUSY,
    GE_ORIGINAL_MP_FRONTEND_LOCKED,
    GE_ORIGINAL_MP_FRONTEND_UNAVAILABLE,
    GE_ORIGINAL_MP_FRONTEND_STALE,
    GE_ORIGINAL_MP_FRONTEND_START_FAILED
} GeOriginalMultiplayerFrontendStatus;

typedef enum GeOriginalMultiplayerFrontendOption {
    GE_ORIGINAL_MP_FRONTEND_PLAYERS,
    GE_ORIGINAL_MP_FRONTEND_ARENA,
    GE_ORIGINAL_MP_FRONTEND_SCENARIO,
    GE_ORIGINAL_MP_FRONTEND_WEAPONS,
    GE_ORIGINAL_MP_FRONTEND_CHARACTER,
    GE_ORIGINAL_MP_FRONTEND_LIMIT
} GeOriginalMultiplayerFrontendOption;

typedef struct GeOriginalMultiplayerFrontendCharacter {
    uint16_t text_id;
    uint8_t gender;
    int16_t body;
    int16_t head;
    float height;
} GeOriginalMultiplayerFrontendCharacter;

typedef struct GeOriginalMultiplayerFrontendLimit {
    uint16_t text_id;
    int32_t time_limit60;
    int32_t point_limit;
} GeOriginalMultiplayerFrontendLimit;

typedef struct GeOriginalMultiplayerFrontendSelection {
    GeOriginalMultiplayerConfig config;
    int32_t arena_index; /* Explicit ge_stage_asset_multiplayer index. */
    int32_t character_ids[GE_ORIGINAL_MP_PLAYERS];
    int32_t limit_index;
} GeOriginalMultiplayerFrontendSelection;

typedef struct GeOriginalMultiplayerFrontendStart {
    uint64_t request_id;
    GeOriginalMultiplayerFrontendSelection selection;
    GeOriginalMultiplayerFrontendCharacter characters[GE_ORIGINAL_MP_PLAYERS];
    int32_t time_limit60;
    int32_t point_limit;
    uint8_t diagnostic_local; /* Always 1; never authorizes network start. */
} GeOriginalMultiplayerFrontendStart;

typedef enum GeOriginalMultiplayerFrontendEndReason {
    GE_ORIGINAL_MP_FRONTEND_END_NONE,
    GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT,
    GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT,
    GE_ORIGINAL_MP_FRONTEND_END_ABORT,
    GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION
} GeOriginalMultiplayerFrontendEndReason;

/* Caller supplies canonical scores after runtime end/freeze; the UI neither
 * recomputes awards nor writes solo mission progress. Negative scores are
 * valid (e.g. suicides); kills/deaths are nonnegative. */
typedef struct GeOriginalMultiplayerFrontendResult {
    uint64_t request_id;
    int32_t player_count;
    int32_t scores[GE_ORIGINAL_MP_PLAYERS];
    int32_t kills[GE_ORIGINAL_MP_PLAYERS];
    int32_t deaths[GE_ORIGINAL_MP_PLAYERS];
    GeOriginalMultiplayerFrontendEndReason end_reason;
    uint8_t winner_mask;
    uint8_t aborted;
} GeOriginalMultiplayerFrontendResult;

typedef struct GeOriginalMultiplayerFrontendServices {
    void *context;
    /* Read-only original fileIsStageUnlockedAtDifficulty(..., Agent) result.
     * A missing callback keeps progression arenas locked. */
    int (*stage_unlocked)(void *context, int32_t mission);
    /* Set from canonical Cradle Agent completed / unlock_all_mp_chars state:
     * 8 default, 33 after Cradle, 64 only with the original all-char unlock. */
    uint8_t cradle_completed;
    uint8_t all_characters_unlocked;
} GeOriginalMultiplayerFrontendServices;

typedef struct GeOriginalMultiplayerFrontend {
    GeOriginalMultiplayerFrontendServices services;
    GeOriginalMultiplayerFrontendPhase phase;
    GeOriginalMultiplayerFrontendStatus status;
    GeOriginalMultiplayerFrontendSelection selection;
    GeOriginalMultiplayerFrontendStart start;
    GeOriginalMultiplayerFrontendResult result;
    uint64_t sequence;
    uint32_t previous_buttons;
    uint8_t input_armed;
    uint8_t character_count;
} GeOriginalMultiplayerFrontend;

typedef struct GeOriginalMultiplayerFrontendSnapshot {
    GeOriginalMultiplayerFrontendPhase phase;
    GeOriginalMultiplayerFrontendStatus status;
    GeOriginalMultiplayerFrontendSelection selection;
    GeOriginalMultiplayerFrontendResult result;
    uint8_t character_count;
    uint8_t input_armed;
    uint8_t can_start;
    uint8_t diagnostic_local;
    uint16_t arena_text_id;
    uint16_t scenario_text_id;
    uint16_t weapon_text_id;
    GeOriginalMultiplayerFrontendLimit limit;
    GeOriginalMultiplayerFrontendCharacter characters[GE_ORIGINAL_MP_PLAYERS];
} GeOriginalMultiplayerFrontendSnapshot;

int ge_original_multiplayer_frontend_init(GeOriginalMultiplayerFrontend *frontend,
    const GeOriginalMultiplayerFrontendServices *services);
int ge_original_multiplayer_frontend_character(size_t index,
    GeOriginalMultiplayerFrontendCharacter *out);
int ge_original_multiplayer_frontend_limit(size_t index,
    GeOriginalMultiplayerFrontendLimit *out);
/* Arena/scenario/player changes apply original compatible fallbacks; locked
 * arena/character choices and forced scenario options reject atomically.
 * player is used only for CHARACTER. Characters remain unique. */
GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_set(
    GeOriginalMultiplayerFrontend *frontend,
    GeOriginalMultiplayerFrontendOption option, int32_t player, int32_t value);
int ge_original_multiplayer_frontend_snapshot(
    const GeOriginalMultiplayerFrontend *frontend,
    GeOriginalMultiplayerFrontendSnapshot *snapshot);

/* Feed combined physical/touch held bits through this before dispatching UI
 * actions. Every phase change suppresses input until ALL held bits release;
 * then only rising edges pass. On entry/return also clear original controller
 * publications and invalidate the solo inventory snapshot in the root loop. */
uint32_t ge_original_multiplayer_frontend_input_edges(
    GeOriginalMultiplayerFrontend *frontend, uint32_t held_buttons);
GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_request_start(
    GeOriginalMultiplayerFrontend *frontend);
/* One-shot copied bootstrap request. Consume exactly once, then report load
 * success/failure with started. No player/config/save globals are applied. */
int ge_original_multiplayer_frontend_take_start(
    GeOriginalMultiplayerFrontend *frontend, GeOriginalMultiplayerFrontendStart *out);
GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_started(
    GeOriginalMultiplayerFrontend *frontend, uint64_t request_id, int succeeded);
GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_finish(
    GeOriginalMultiplayerFrontend *frontend,
    const GeOriginalMultiplayerFrontendResult *result);
/* Results/loading failure/pending cancel -> setup. A running match must first
 * finish with an aborted result so its root-owned resources are accounted for. */
GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_return_to_setup(
    GeOriginalMultiplayerFrontend *frontend);

#endif

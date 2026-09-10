#ifndef GE_MP_STATE_SESSION_H
#define GE_MP_STATE_SESSION_H
#include "ge_mp_state.h"
#include "ge_original_multiplayer_death.h"
#include "ge_original_bond_live.h"

#define GE_MP_SESSION_SCHEMA_VERSION 1U
#define GE_MP_SESSION_MAX_GAPS 64U
#define GE_MP_SESSION_SYSTEMS (GE_MP_SYSTEM_SESSION|GE_MP_SYSTEM_RULES|GE_MP_SYSTEM_RESPAWN)
/* Typed paths and stable field IDs. No pointer or native ABI width is wire state. */
#define GE_MP_SESSION_FIELDS(X) \
 X(256,U32,1,count,"session.count") \
 X(257,I32,1,decision,"session.decision") \
 X(258,I32,1,end_reason,"session.end_reason") \
 X(259,U32,1,results_causes,"session.results_causes") \
 X(260,U32,1,freeze_causes,"session.freeze_causes") \
 X(261,I32,1,point_limit,"g_MpPoint") \
 X(262,I32,1,stop_play,"g_stopPlayFlag") \
 X(263,I32,1,game_over,"g_gameOverFlag") \
 X(264,I32,1,paused,"g_pausedFlag") \
 X(265,ACTOR_REF,4,players,"session.players") \
 X(266,I32,1,scenario,"scenario") \
 X(267,I32,1,weapon_set,"mp_weapon_set") \
 X(268,I32,1,startpad_count,"startpadcount") \
 X(269,I32,2,starting_weapon,"starting_weapon") \
 X(270,ACTOR_REF,16,startpads,"g_Startpad") \
 X(271,U64,16,startpad_offsets,"g_Startpad.owner_offset") \
 X(272,ACTOR_REF,1,validated_intro,"rules.validated_intro") \
 X(273,U64,1,validated_intro_offset,"rules.validated_intro.owner_offset") \
 X(274,U64,1,intro_words,"rules.validated_intro_words") \
 X(275,ACTOR_REF,1,validated_pads,"rules.validated_pads") \
 X(276,U64,1,validated_pads_offset,"rules.validated_pads.owner_offset") \
 X(277,U64,1,pad_count,"rules.validated_pad_count") \
 X(278,I32,1,demo_selection,"rules.validated_demo_playback") \
 X(279,ACTOR_REF,1,current_intro,"g_CurrentSetup.intro") \
 X(280,U64,1,current_intro_offset,"g_CurrentSetup.intro.owner_offset") \
 X(281,ACTOR_REF,1,current_pads,"g_CurrentSetup.pads") \
 X(282,U64,1,current_pads_offset,"g_CurrentSetup.pads.owner_offset") \
 X(283,I32,1,playback,"is_ramrom_flag") \
 X(284,I32,1,recording,"recording_ramrom_flag") \
 X(285,U8,1,demo_kind,"ptr_active_demofile.kind") \
 X(286,U32,1,demo_slot,"ptr_active_demofile.slotnum") \
 X(287,ACTOR_REF,1,demo,"ptr_active_demofile.external_owner") \
 X(288,U64,1,demo_offset,"ptr_active_demofile.owner_offset") \
 X(289,U64,1,next_tick,"schedule.next_tick") \
 X(290,I32,1,elapsed60,"schedule.elapsed60") \
 X(291,U8,1,boundary,"schedule.boundary") \
 X(292,U8,1,death_count,"death.count") \
 X(293,ACTOR_REF,4,death_players,"death.players") \
 X(294,U64,4,death_pre,"death.pre_tick") \
 X(295,U64,4,death_post,"death.post_tick") \
 X(296,U8,1,death_pre_valid,"death.pre_valid") \
 X(297,U8,1,death_post_valid,"death.post_valid") \
 X(298,I32,4,live_initialized,"lives.initialized")
#define GE_MP_SESSION_FIELD_COUNT 43U
typedef enum GeMpSessionBoundary {
    GE_MP_SESSION_INITIAL,
    GE_MP_SESSION_WORLD_COMMITTED,
    GE_MP_SESSION_TERMINAL_BEFORE_WORLD
} GeMpSessionBoundary;
typedef struct GeMpSessionSchedule {
    /* Next tick equals last completed world tick + 1, matching the shared
     * runtime caller. INITIAL has no completed tick. Terminal decisions may
     * increment elapsed60 without executing another world/death pass. */
    uint64_t next_tick;
    int32_t elapsed60;
    GeMpSessionBoundary boundary;
    const GeOriginalMultiplayerDeathState *death;
    const GeOriginalBondLiveState *lives[4];
} GeMpSessionSchedule;
typedef enum GeMpSessionReferenceKind {
    GE_MP_SESSION_REF_PLAYER,
    GE_MP_SESSION_REF_INTRO,
    GE_MP_SESSION_REF_PAD_ARRAY,
    GE_MP_SESSION_REF_PAD,
    GE_MP_SESSION_REF_DEMO
} GeMpSessionReferenceKind;
typedef struct GeMpSessionReference { uint64_t actor_id, offset; } GeMpSessionReference;
typedef struct GeMpSessionProviders {
    void *context;
    /* Validate native range/lifetime and return its live typed owner + stable
     * offset (not an ABI-dependent host byte offset). Content owners must
     * enumerate mutable intro/pad fields and nested STAN/name refs separately.
     * For PLAYER, map through the E6 physical-lifetime owner; extent is zero.
     * DEMO success warrants readable sizeof(ramromfilestructure) storage. */
    GeMpStateStatus (*reference)(void *, uint16_t field, unsigned index,
        GeMpSessionReferenceKind, const void *, size_t extent, GeMpSessionReference *);
} GeMpSessionProviders;
typedef enum GeMpSessionGapReason {
    GE_MP_SESSION_GAP_NATIVE_SESSION,
    GE_MP_SESSION_GAP_PUBLICATION,
    GE_MP_SESSION_GAP_REFERENCE,
    GE_MP_SESSION_GAP_SCHEDULE,
    GE_MP_SESSION_GAP_LIVE,
    GE_MP_SESSION_GAP_WATCH_OWNER,
    GE_MP_SESSION_GAP_ACTIVE_PROPS_OWNER,
    GE_MP_SESSION_GAP_GUARD_SCHEDULE_OWNER,
    GE_MP_SESSION_GAP_HUD_SCHEDULE_OWNER,
    GE_MP_SESSION_GAP_RAMROM_OWNER,
    GE_MP_SESSION_GAP_MUSIC_TRANSITION_OWNER,
    GE_MP_SESSION_GAP_RESOURCE_PROVIDERS,
    GE_MP_SESSION_GAP_BUILD
} GeMpSessionGapReason;
typedef struct GeMpSessionGap {
    uint16_t field, index;
    GeMpSessionGapReason reason;
    GeMpStateStatus status;
} GeMpSessionGap;
typedef struct GeMpSessionValues {
    uint32_t count, results_causes, freeze_causes, demo_slot;
    int32_t decision,end_reason,point_limit,stop_play,game_over,paused;
    int32_t scenario,weapon_set,startpad_count,starting_weapon[2],demo_selection;
    int32_t playback,recording,elapsed60,live_initialized[4];
    uint64_t players[4],startpads[16],startpad_offsets[16];
    uint64_t validated_intro,validated_intro_offset,intro_words;
    uint64_t validated_pads,validated_pads_offset,pad_count;
    uint64_t current_intro,current_intro_offset,current_pads,current_pads_offset;
    uint64_t demo,demo_offset,next_tick,death_players[4],death_pre[4],death_post[4];
    uint8_t demo_kind,boundary,death_count,death_pre_valid,death_post_valid;
} GeMpSessionValues;
typedef struct GeMpSessionSnapshot {
    GeMpStateRecord record;
    GeMpStateField fields[GE_MP_SESSION_FIELD_COUNT];
    GeMpSessionValues values;
    GeMpSessionGap gaps[GE_MP_SESSION_MAX_GAPS];
    size_t gap_count,total_gaps;
    GeMpStateCoverage coverage;
} GeMpSessionSnapshot;
/* Read-only capture: no selection, setup republication, clock advancement or
 * registry mutations. Owns mandatory kind0/actor0, never creates another ID.
 * Snapshot must stay at the same address through aggregate encoding. Known
 * wrapper-owner gaps currently return INCOMPLETE even with all fields present;
 * this function never sets an aggregate schema's complete attestation. */
GeMpStateStatus ge_mp_state_session_capture(const GeMpRegistry *,
    const GeMpSessionProviders *, const GeMpSessionSchedule *, GeMpSessionSnapshot *);
const GeMpRecordSchema *ge_mp_state_session_schema(void);
const char *ge_mp_state_session_field_path(uint16_t);
const char *ge_mp_state_session_gap_path(GeMpSessionGapReason);
#endif

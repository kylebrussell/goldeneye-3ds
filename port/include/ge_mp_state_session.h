#ifndef GE_MP_STATE_SESSION_H
#define GE_MP_STATE_SESSION_H
#include "ge_mp_state.h"
#include "ge_original_multiplayer_death.h"
#include "ge_original_bond_live.h"
#include "ge_original_stage_active_props.h"
#include "ge_original_multiplayer_hud.h"

#define GE_MP_SESSION_SCHEMA_VERSION 2U
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
 X(298,I32,4,live_initialized,"lives.initialized") \
 X(299,U64,1,active_pre_ticks,"active_props.pre_ticks") \
 X(300,U64,1,active_ticks,"active_props.ticks") \
 X(301,U64,1,active_paused_ticks,"active_props.paused_ticks") \
 X(302,U64,1,active_count,"active_props.count") \
 X(303,U64,1,active_chr_count,"active_props.chr_count") \
 X(304,U8,1,active_bound,"active_props.bound") \
 X(305,U8,1,active_pending,"active_props.pre_tick_pending") \
 X(306,U8,1,active_player_count,"active_props.player_count") \
 X(307,U8,1,active_player_mask,"active_props.player_tick_mask") \
 X(308,U32,1,active_mismatch,"active_props.last_binding_mismatch") \
 X(309,BOOL,1,active_ordered_present,"active_props.ordered.present") \
 X(310,ACTOR_REF,4,active_players,"active_props.player_props") \
 X(311,ACTOR_REF,1,active_setup,"active_props.setup") \
 X(312,U64,1,active_setup_offset,"active_props.setup.owner_offset") \
 X(313,ACTOR_REF,1,active_chrs,"active_props.chrs") \
 X(314,U64,1,active_chrs_offset,"active_props.chrs.owner_offset") \
 X(315,ACTOR_REF,1,native_chrs,"g_ChrSlots") \
 X(316,U64,1,native_chrs_offset,"g_ChrSlots.owner_offset") \
 X(317,I32,1,native_chr_count,"g_NumChrSlots") \
 X(318,ACTOR_REF,1,native_propdefs,"g_CurrentSetup.propDefs") \
 X(319,U64,1,native_propdefs_offset,"g_CurrentSetup.propDefs.owner_offset") \
 X(320,ACTOR_REF,1,native_ailists,"g_CurrentSetup.ailists") \
 X(321,U64,1,native_ailists_offset,"g_CurrentSetup.ailists.owner_offset") \
 X(322,U64,1,hud_tick,"hud.tick") \
 X(323,U32,1,hud_player_count,"hud.player_count") \
 X(324,U8,1,hud_initialized,"hud.initialized") \
 X(325,U8,1,hud_prepared,"hud.prepared") \
 X(326,U8,1,hud_captured,"hud.captured") \
 X(327,U8,1,hud_failed,"hud.failed") \
 X(328,U8,1,hud_intro_camera,"hud.intro_camera")
#define GE_MP_SESSION_FIELD_COUNT 73U
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
    const GeOriginalStageActiveProps *active_props;
    const GeOriginalMultiplayerHud *hud;
} GeMpSessionSchedule;
typedef enum GeMpSessionReferenceKind {
    GE_MP_SESSION_REF_PLAYER,
    GE_MP_SESSION_REF_INTRO,
    GE_MP_SESSION_REF_PAD_ARRAY,
    GE_MP_SESSION_REF_PAD,
    GE_MP_SESSION_REF_DEMO,
    GE_MP_SESSION_REF_PROP,
    GE_MP_SESSION_REF_SETUP_OWNER,
    GE_MP_SESSION_REF_CHR_POOL,
    GE_MP_SESSION_REF_PROPDEFS,
    GE_MP_SESSION_REF_AILISTS
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
    uint64_t active_pre_ticks,active_ticks,active_paused_ticks,active_count,active_chr_count;
    uint64_t active_players[4],active_setup,active_setup_offset,active_chrs,active_chrs_offset;
    uint64_t native_chrs,native_chrs_offset,native_propdefs,native_propdefs_offset,native_ailists,native_ailists_offset;
    uint32_t active_mismatch,hud_player_count;
    int32_t native_chr_count;
    uint8_t active_bound,active_pending,active_player_count,active_player_mask,active_ordered_present;
    uint64_t hud_tick;
    uint8_t hud_initialized,hud_prepared,hud_captured,hud_failed,hud_intro_camera;
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

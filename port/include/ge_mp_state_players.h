#ifndef GE_MP_STATE_PLAYERS_H
#define GE_MP_STATE_PLAYERS_H
#include "ge_mp_state.h"
#include "ge_mp_state_players_fields.inc"
#define GE_MP_PLAYERS_KIND 0xE6U
#define GE_MP_PLAYER_SCHEMA_VERSION 2U
#define GE_MP_PLAYER_MAX_GAPS 256U
struct player;

typedef enum GeMpPlayerReferenceKind {
    GE_MP_PLAYER_REF_PROP, GE_MP_PLAYER_REF_HAND, GE_MP_PLAYER_REF_INVENTORY_OWNER,
    GE_MP_PLAYER_REF_STAN, GE_MP_PLAYER_REF_MODEL, GE_MP_PLAYER_REF_GAIT_MODEL,
    GE_MP_PLAYER_REF_WATCH_MODEL, GE_MP_PLAYER_REF_DISPLAY_LIST,
    GE_MP_PLAYER_REF_FIXED_MATRIX, GE_MP_PLAYER_REF_FLOAT_MATRIX,
    GE_MP_PLAYER_REF_IMAGE, GE_MP_PLAYER_REF_HAND_BUFFER, GE_MP_PLAYER_REF_MODEL_NODE,
    GE_MP_PLAYER_REF_SKELETON, GE_MP_PLAYER_REF_MODEL_SWITCHES,
    GE_MP_PLAYER_REF_MODEL_TEXTURES, GE_MP_PLAYER_REF_TEXTURE_POOL, GE_MP_PLAYER_REF_TEXT,
    GE_MP_PLAYER_REF_LOOKAT, GE_MP_PLAYER_REF_UNKNOWN,
    GE_MP_PLAYER_REF_ANIMATION
} GeMpPlayerReferenceKind;
typedef struct GeMpPlayerReference {
    uint64_t actor_id, offset;
} GeMpPlayerReference;
typedef struct GeMpStatePlayersProviders {
    void *context;
    /* Return a live owner identity plus stable byte offset. A nonzero extent
     * identifies an embedded storage range requiring that owner's typed export;
     * it never authorizes copying the range as bytes. No native address is wire
     * state. Null pointers bypass the resolver as explicit nullable refs.
     * ANIMATION receives the exact legacy uint32 address token held in
     * players_cur_animation. The owning provider must validate its identity;
     * an unrecoverable/truncated host pointer must return INCOMPLETE. */
    GeMpStateStatus (*reference)(void *,uint32_t player,uint16_t field,
        GeMpPlayerReferenceKind,const void *native,size_t extent,GeMpPlayerReference *);
} GeMpStatePlayersProviders;
typedef struct GeMpStatePlayersOwner {
    GeMpRegistry *registry;
    const struct player *players[4];
    uint64_t ids[4],lifetime;
    uint32_t active_count;
    uint8_t live;
} GeMpStatePlayersOwner;
typedef enum GeMpPlayerGapReason {
    GE_MP_PLAYER_GAP_RESOLVER, GE_MP_PLAYER_GAP_REFERENCE,
    GE_MP_PLAYER_GAP_NONFINITE, GE_MP_PLAYER_GAP_BUILD, GE_MP_PLAYER_GAP_UNKNOWN
} GeMpPlayerGapReason;
typedef struct GeMpPlayerGap {
    uint16_t field,index;
    uint8_t player,reference_kind,reason;
    GeMpStateStatus status;
} GeMpPlayerGap;
typedef struct GeMpPlayerValues {
    int32_t i32[GE_MP_PLAYER_I32_COUNT];
    int16_t i16[GE_MP_PLAYER_I16_COUNT];
    uint32_t u32[GE_MP_PLAYER_U32_COUNT];
    uint16_t u16[GE_MP_PLAYER_U16_COUNT];
    uint8_t u8[GE_MP_PLAYER_U8_COUNT];
    float f32[GE_MP_PLAYER_F32_COUNT];
    uint64_t refs[GE_MP_PLAYER_REF_COUNT],offsets[GE_MP_PLAYER_REF_COUNT];
} GeMpPlayerValues;
typedef struct GeMpStatePlayersSnapshot {
    GeMpStateRecord records[4];
    GeMpStateField fields[4][GE_MP_PLAYER_FIELD_COUNT];
    GeMpPlayerValues values[4];
    GeMpPlayerGap gaps[GE_MP_PLAYER_MAX_GAPS];
    size_t record_count,gap_count,total_gaps;
    GeMpStateCoverage coverage;
} GeMpStatePlayersSnapshot;
/* All four physical rows exist from create to destroy; inactive rows are
 * uninitialized calloc storage, not active players or fabricated models.
 * Zero-init owner, begin after create, end before destroy; missed lifetime
 * teardown/ABA is rejected. E2/E4 owners retain separate lifetimes. */
GeMpStateStatus ge_mp_state_players_begin(GeMpStatePlayersOwner *,GeMpRegistry *);
GeMpStateStatus ge_mp_state_players_end(GeMpStatePlayersOwner *);
GeMpStateStatus ge_mp_state_players_actor(void *owner,const void *player,uint64_t *);
/* Incomplete field/reference coverage retains four diagnostic records and gaps, with
 * zero elements for unresolved values. Only OK is a successful domain export.
 * Keep output at the same address and immutable through aggregate encoding. */
GeMpStateStatus ge_mp_state_players_export(const GeMpStatePlayersOwner *,
    const GeMpStatePlayersProviders *,GeMpStatePlayersSnapshot *);
const GeMpRecordSchema *ge_mp_state_players_schema(void);
const char *ge_mp_state_players_field_path(uint16_t field);
#endif

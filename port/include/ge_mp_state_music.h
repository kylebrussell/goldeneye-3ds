#ifndef GE_MP_STATE_MUSIC_H
#define GE_MP_STATE_MUSIC_H
#include "ge_mp_state.h"

#define GE_MP_MUSIC_KIND 0xEFU
#define GE_MP_MUSIC_NAMESPACE 255U
#define GE_MP_MUSIC_SLOT 0U
#define GE_MP_MUSIC_SETUP_ROWS 24U
#define GE_MP_MUSIC_RANDOM_TRACKS 44U
#define GE_MP_MUSIC_FIELD_COUNT 8U
typedef struct GeMpMusicValues {
    int32_t stage_id, mission_state, saved_mission_state;
    int32_t slot_active[4], slot_minimum[4], slot_duration[4];
    int16_t setup[GE_MP_MUSIC_SETUP_ROWS][4];
    int16_t random_tracks[GE_MP_MUSIC_RANDOM_TRACKS];
} GeMpMusicValues;
typedef struct GeMpStateMusicSnapshot {
    GeMpMusicValues values;
    GeMpStateField fields[GE_MP_MUSIC_FIELD_COUNT];
    GeMpStateRecord record;
    size_t record_count;
    GeMpStateCoverage coverage;
} GeMpStateMusicSnapshot;
/* Native mp_music globals have program lifetime. The caller owns this exact
 * EF/255/0 registry ID for its match; capture does not reset native history,
 * advance music, invoke audio, consume RNG, or mutate registry ownership.
 * This covers transition control only, not sequence/DSP/device presentation.
 * Actual ordered source tables, including sentinels, are captured too. The
 * content/build manifest must cover their interpretation, and the
 * shared gameplay random stream belongs to E0. Aggregate completeness stays 0. */
GeMpStateStatus ge_mp_state_music_capture(const GeMpRegistry *, uint64_t actor_id,
    GeMpStateMusicSnapshot *);
const GeMpRecordSchema *ge_mp_state_music_schema(void);
#endif

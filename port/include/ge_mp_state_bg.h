#ifndef GE_MP_STATE_BG_H
#define GE_MP_STATE_BG_H

#include "ge_mp_state.h"
#include "ge_original_bg_geometry.h"

#define GE_MP_BG_KIND 0xe7U
#define GE_MP_BG_NAMESPACE 255U
#define GE_MP_BG_SLOT 0U
#define GE_MP_BG_FIELDS 21U
#define GE_MP_BG_ROOMS GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS
#define GE_MP_BG_FIXTURES 100U
#define GE_MP_BG_DARKENED 512U

enum GeMpBgField {
    GE_MP_BG_STATUS=256, GE_MP_BG_ROOM_COUNT, GE_MP_BG_POINT_COUNT, GE_MP_BG_SCALES,
    GE_MP_BG_FIXTURE_SLOT, GE_MP_BG_FIXTURE_ROOM, GE_MP_BG_DARKENED_CURSOR,
    GE_MP_BG_FIXTURE_ROOMS, GE_MP_BG_FIXTURE_LISTS, GE_MP_BG_FIXTURE_STARTS,
    GE_MP_BG_FIXTURE_ENDS, GE_MP_BG_DARKENED_ROOMS, GE_MP_BG_DARKENED_POINTS,
    GE_MP_BG_POINT_OFFSETS, GE_MP_BG_PRIMARY_COMMANDS, GE_MP_BG_SECONDARY_COMMANDS,
    GE_MP_BG_VERTEX_BATCHES, GE_MP_BG_ORIGINS, GE_MP_BG_MINIMA, GE_MP_BG_MAXIMA,
    GE_MP_BG_RGBA
};

typedef struct GeMpBgValues {
    uint32_t status, room_count, point_count;
    float scales[2];
    int16_t fixture_slot, fixture_room;
    int32_t darkened_cursor;
    int16_t fixture_rooms[GE_MP_BG_FIXTURES];
    uint8_t fixture_lists[GE_MP_BG_FIXTURES];
    uint32_t fixture_starts[GE_MP_BG_FIXTURES], fixture_ends[GE_MP_BG_FIXTURES];
    uint16_t darkened_rooms[GE_MP_BG_DARKENED], darkened_points[GE_MP_BG_DARKENED];
    uint32_t point_offsets[GE_MP_BG_ROOMS + 1U];
    uint32_t primary_commands[GE_MP_BG_ROOMS], secondary_commands[GE_MP_BG_ROOMS];
    uint32_t vertex_batches[GE_MP_BG_ROOMS];
    float origins[GE_MP_BG_ROOMS * 3U], minima[GE_MP_BG_ROOMS * 3U], maxima[GE_MP_BG_ROOMS * 3U];
} GeMpBgValues;

typedef struct GeMpStateBgSnapshot {
    GeMpStateRecord record;
    GeMpStateField fields[GE_MP_BG_FIELDS];
    GeMpBgValues values;
    /* One explicit 0xRRGGBBAA numeric value per authored point, in room then
     * point order. This is a typed colour sequence, never a memory blob. */
    uint32_t *rgba;
    size_t rgba_capacity, record_count;
    GeMpStateCoverage coverage;
    uint8_t initialized;
} GeMpStateBgSnapshot;

/* Zero-init snapshot. Reserve owned capture storage after geometry load/bind,
 * before ticks; close after its last encode. Capture performs no allocation.
 * The caller owns E7/ns255/slot0's registry lifetime: acquire after native load,
 * release before native close, and acquire a new generation for a new stage.
 * These functions never create a gameplay owner or change its lifetime. */
GeMpStateStatus ge_mp_state_bg_init(GeMpStateBgSnapshot *, const GeOriginalBgGeometry *);
void ge_mp_state_bg_close(GeMpStateBgSnapshot *);
GeMpStateStatus ge_mp_state_bg_capture(GeMpStateBgSnapshot *, const GeOriginalBgGeometry *,
    const GeMpRegistry *, uint64_t actor_id);
const GeMpRecordSchema *ge_mp_state_bg_schema(void);

/* Capture at a quiescent canonical boundary. Result owns all captured values;
 * do not move it before encoding because field pointers refer into it.
 * Failure clears record_count and coverage; no state digest is authorized.
 * This covers only native BG/light state. Immutable original geometry must be
 * identified by the complete content digest. E0 owns RNG; glass, shards and
 * other effects require their separate domain. This domain never sets
 * schema.complete or attests those downstream effects. */
#endif

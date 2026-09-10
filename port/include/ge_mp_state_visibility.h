#ifndef GE_MP_STATE_VISIBILITY_H
#define GE_MP_STATE_VISIBILITY_H
#include "ge_mp_state.h"
#include "ge_dam_visibility_runtime.h"

#define GE_MP_VISIBILITY_KIND 0xe9U
#define GE_MP_VISIBILITY_NAMESPACE 255U
#define GE_MP_VISIBILITY_SLOT 0U
#define GE_MP_VISIBILITY_FIELDS 51U

enum GeMpVisibilityField {
    GE_MP_VISIBILITY_ROOM_COUNT = 256,
    GE_MP_VISIBILITY_PORTAL_COUNT = 257,
    GE_MP_VISIBILITY_DRAW_COUNT = 258,
    GE_MP_VISIBILITY_ALPHA_COUNT = 259,
    GE_MP_VISIBILITY_COMMAND_COUNT = 260,
    GE_MP_VISIBILITY_POINT_COUNT = 261,
    GE_MP_VISIBILITY_CACHE_COUNT = 262,
    GE_MP_VISIBILITY_LEVEL_INDEX = 263,
    GE_MP_VISIBILITY_CURRENT_ROOM = 264,
    GE_MP_VISIBILITY_ROOM_CYCLE = 265,
    GE_MP_VISIBILITY_VIEW_RELATED = 266,
    GE_MP_VISIBILITY_SCALES = 267,
    GE_MP_VISIBILITY_CLIP_RANGE = 268,
    GE_MP_VISIBILITY_VIEWPORT = 269,
    GE_MP_VISIBILITY_PLAYER_POSITION = 270,
    GE_MP_VISIBILITY_WORLD_TO_SCREEN = 271,
    GE_MP_VISIBILITY_PROJECTION = 272,
    GE_MP_VISIBILITY_SCREEN_BOX = 273,
    GE_MP_VISIBILITY_PROJECTION_OFFSETS = 274,
    GE_MP_VISIBILITY_ROOM_RENDERED = 275,
    GE_MP_VISIBILITY_ROOM_NEIGHBOR = 276,
    GE_MP_VISIBILITY_ROOM_LOADED_MASK = 277,
    GE_MP_VISIBILITY_ROOM_VISITS = 278,
    GE_MP_VISIBILITY_ROOM_MINIMA = 279,
    GE_MP_VISIBILITY_ROOM_MAXIMA = 280,
    GE_MP_VISIBILITY_DRAW_ROOMS = 281,
    GE_MP_VISIBILITY_DRAW_DEPTHS = 282,
    GE_MP_VISIBILITY_DRAW_FLAGS = 283,
    GE_MP_VISIBILITY_DRAW_BOXES = 284,
    GE_MP_VISIBILITY_ALPHA_ORDER = 285,
    GE_MP_VISIBILITY_PORTAL_ROOMS = 286,
    GE_MP_VISIBILITY_PORTAL_CONTROLS = 287,
    GE_MP_VISIBILITY_AUTHORITATIVE_CONTROLS = 288,
    GE_MP_VISIBILITY_PORTAL_POINT_OFFSETS = 289,
    GE_MP_VISIBILITY_PORTAL_POINTS = 290,
    GE_MP_VISIBILITY_COMMAND_TYPES = 291,
    GE_MP_VISIBILITY_COMMAND_LENGTHS = 292,
    GE_MP_VISIBILITY_COMMAND_ARGUMENTS = 293,
    GE_MP_VISIBILITY_STREAM_HASH = 294,
    GE_MP_VISIBILITY_STACK = 295,
    GE_MP_VISIBILITY_STACK_CURSOR = 296,
    GE_MP_VISIBILITY_CURRENT_VISIBILITY = 297,
    GE_MP_VISIBILITY_INTERPRETER_BOX = 298,
    GE_MP_VISIBILITY_INTERPRETER_STOP = 299,
    GE_MP_VISIBILITY_DESCENT_COUNT = 300,
    GE_MP_VISIBILITY_MAX_DEPTH = 301,
    GE_MP_VISIBILITY_QUEUE_CURSORS = 302,
    GE_MP_VISIBILITY_PORTAL_DEPTHS = 303,
    GE_MP_VISIBILITY_CACHE_FLAGS = 304,
    GE_MP_VISIBILITY_CACHE_PORTALS = 305,
    GE_MP_VISIBILITY_CACHE_BOXES = 306
};

typedef struct GeMpVisibilityValues {
    uint32_t room_count;
    uint32_t portal_count;
    uint32_t draw_count;
    uint32_t alpha_count;
    uint32_t command_count;
    uint32_t point_count;
    uint32_t cache_count;
    int32_t level_index;
    int32_t current_room;
    int32_t room_cycle;
    int32_t view_related[4];
    float scales[3];
    float clip_range[2];
    int16_t viewport[4];
    float player_position[3];
    float world_to_screen[16];
    float projection[6];
    float screen_box[4];
    float projection_offsets[2];
    uint8_t room_rendered[139];
    uint8_t room_neighbor[139];
    uint8_t room_loaded_mask[139];
    uint8_t room_visits[139];
    float room_minima[417];
    float room_maxima[417];
    int32_t draw_rooms[204];
    int32_t draw_depths[204];
    int32_t draw_flags[204];
    float draw_boxes[816];
    uint8_t alpha_order[152];
    uint8_t portal_rooms[400];
    uint8_t portal_controls[400];
    uint8_t authoritative_controls[200];
    uint32_t portal_point_offsets[201];
    float portal_points[5400];
    uint8_t command_types[512];
    uint8_t command_lengths[512];
    int32_t command_arguments[512];
    uint64_t stream_hash;
    int32_t stack[20];
    int32_t stack_cursor;
    int32_t interpreter_visibility;
    float interpreter_box[4];
    int32_t interpreter_stop;
    int32_t descent_count;
    int32_t max_depth;
    int32_t queue_cursors[2];
    uint8_t portal_depths[200];
    int32_t cache_flags[200];
    uint16_t cache_portals[200];
    float cache_boxes[800];
} GeMpVisibilityValues;

typedef struct GeMpStateVisibilitySnapshot {
    GeMpStateRecord record;
    GeMpStateField fields[GE_MP_VISIBILITY_FIELDS];
    GeMpVisibilityValues values;
    GeMpStateCoverage coverage;
    size_t record_count;
} GeMpStateVisibilitySnapshot;

/* Caller acquires E9/ns255/slot0 after the stage visibility runtime loads and
 * releases before it closes. Recreated owners within a registry advance the
 * generation. Fresh match sessions have fresh registries scoped by session ID.
 * Capture after a successful last canonical pass with an empty traversal
 * queue; supply the live authoritative door controls (exact portal count).
 * No allocation or mutable renderer residency input is accepted. Failure
 * clears publication/coverage and cannot authorize an encoded state digest.
 * Snapshot owns all values; do not move it until encoding finishes.
 * This is only the canonical visibility subdomain of OBJECTS and never sets
 * schema.complete or attests fog, geometry, effects or full object coverage. */
GeMpStateStatus ge_mp_state_visibility_capture(GeMpStateVisibilitySnapshot *,
    const GeDamVisibilityRuntime *, const uint8_t *portal_controls,
    size_t portal_control_count, const GeMpRegistry *, uint64_t actor_id);
const GeMpRecordSchema *ge_mp_state_visibility_schema(void);
#endif

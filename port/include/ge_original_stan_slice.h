#ifndef GE_ORIGINAL_STAN_SLICE_H
#define GE_ORIGINAL_STAN_SLICE_H

/* Minimal ABI surface used when compiling the original stan.c geometry slice. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#ifndef M_U32_MAX_VALUE_F
#define M_U32_MAX_VALUE_F 4294967296.0f
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef s32 bool;

typedef struct coord2d {
    union {
        struct {
            f32 x;
            f32 y;
        };
        f32 f[2];
    };
} coord2d;

typedef struct coord3d {
    union {
        struct {
            f32 x;
            f32 y;
            f32 z;
        };
        f32 f[3];
    };
} coord3d;

_Static_assert(sizeof(coord3d) == 12U,
    "original coord3d ABI must remain three floats");
_Static_assert(offsetof(coord3d, z) == 8U,
    "original coord3d z offset must remain eight bytes");

struct PropRecord;

#if defined(GE_PORT_STAN_DYNAMIC_PROP_COLLISION)
typedef struct rect4f {
    union {
        coord2d points[4];
        f32 f[8];
    };
} rect4f;

extern s16 *ptr_list_object_lookup_indices;
void roomGetProps(s32 *rooms);
s32 propIsOfCdType(struct PropRecord *prop, s32 cdtypes);
void chraiGetCollisionBounds(struct PropRecord *prop,
    struct rect4f **polygon, s32 *edges, f32 *top, f32 *bottom);
struct PropRecord *ge_port_stan_prop_at_index(s16 index);
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef struct StandTilePoint {
    s16 x;
    s16 y;
    s16 z;
    u16 link;
} StandTilePoint;

typedef union GeOriginalStanMid {
    s16 half;
} GeOriginalStanMid;

typedef union GeOriginalStanTail {
    s16 half;
    struct {
        s16 pointCount : 4;
        s16 headerC : 4;
        s16 headerD : 4;
        s16 headerE : 4;
    } hdrTail;
} GeOriginalStanTail;

typedef struct StandTile {
    u32 id : 24;
    u8 room;
    GeOriginalStanMid mid;
    GeOriginalStanTail tail;
    StandTilePoint points[];
} StandTile;

_Static_assert(sizeof(StandTilePoint) == 8U,
    "original STAN point ABI must remain eight bytes");
_Static_assert(sizeof(StandTile) == 8U,
    "original STAN header ABI must remain eight bytes");
_Static_assert(offsetof(StandTile, room) == 3U,
    "original STAN room byte must follow the 24-bit id");
_Static_assert(offsetof(StandTile, points) == 8U,
    "original STAN points must immediately follow the header");

struct StandTileWalkCallbackRecord {
    s32 *roomBuf;
    s32 count;
    s32 bufMax;
    s32 lastRoom;
};

struct StandTileLocusCallbackRecord {
    s32 *rooms;
    s32 count;
    s32 bufMax;
    s32 nearEdgeCount;
};

typedef s32 (*standTileLocusCallback_A_t)(StandTile *,
    struct StandTileLocusCallbackRecord *);
typedef s32 (*standTileLocusCallback_B_t)(StandTile *, s32, f32, f32, f32,
    struct StandTileLocusCallbackRecord *);
typedef s32 (*standTileLocusCallback_C_t)(StandTile **, s32,
    struct StandTileLocusCallbackRecord *);

typedef enum StanCollisionResult {
    STAN_COLLISION_NONE = -2,
    STAN_COLLISION_FOUND = 2,
    STAN_COLLISION_TRAVERSAL_LIMIT = 5
} StanCollisionResult;

enum {
    STANTILEFLAG_FORCECROUCH = 0x02,
    STANTILEFLAG_LADDER = 0x40
};

typedef void (*standTileWalkCallback_t)(StandTile *, StandTile *,
    struct StandTileWalkCallbackRecord *);

void noteTileRoomIfDifferentToPrev(StandTile *tile, StandTile *unused,
    struct StandTileWalkCallbackRecord *data);

#define STAN_TAIL_POINT_COUNT(tile) ((tile)->tail.half & 0xf)
#define STAN_TAIL_C(tile) (((tile)->tail.half >> 4) & 0xf)
#define STAN_TAIL_D(tile) (((tile)->tail.half >> 8) & 0xf)
#define STAN_TAIL_E(tile) (((tile)->tail.half >> 12) & 0xf)

struct StanPrefixRecord {
    s32 stanfile;
    StandTile *ptr_firstroom;
};

extern struct StanPrefixRecord *stan_prefix;
extern StandTile *standTileStart;
extern f32 level_scale;
extern f32 inv_level_scale;

void setLevelScale(f32 scale);
f32 stanGetPositionYValue(StandTile *tile, f32 x, f32 z);
s32 stanTestPointWithinTileBoundsMaybe(StandTile *tile, f32 x, f32 z);
s32 isPointInsideTriStandTileUnscaled_Maybe(StandTile *tile, f32 x, f32 z);
StandTile *sub_GAME_7F0AFB78(f32 *x, f32 *y, f32 *z, f32 arg3);
s32 walkTilesBetweenPoints_NoCallback(StandTile **tile, f32 start_x,
    f32 start_z, f32 destination_x, f32 destination_z);
StandTilePoint *stanMatchTileName(char *id);
s32 sub_GAME_7F0B20D0(StandTile **tile, f32 target_x, f32 target_z,
    f32 radius);
s32 stanTileDistanceRelated(StandTile **tile, f32 target_x, f32 target_z,
    f32 radius, struct StandTileLocusCallbackRecord *record);
s32 stanGetLocusField0(struct StandTileLocusCallbackRecord *record);
s32 stanGetLocusCount(struct StandTileLocusCallbackRecord *record);
s32 stanTestLocusEdgeAboveY(StandTile **tile, f32 target_x, f32 target_z,
    f32 radius, f32 y_threshold);
s32 sub_GAME_7F0B21B0(StandTile **tile, f32 target_x, f32 target_z,
    f32 radius, s32 *rooms, s32 *count_rtn, s32 buf_max);
s32 stanTestLineUnobstructed(StandTile **tile, f32 start_x, f32 start_z,
    f32 destination_x, f32 destination_z, s32 cdtypes, f32 height,
    f32 height_end, f32 slope_start, f32 slope_end);
s32 stanTestVolume(StandTile **tile, f32 x, f32 z, f32 radius, s32 cdtypes,
    f32 height, f32 height_end);
f32 calculateSegmentIntersectionFraction(coord2d *start1, coord2d *end1,
    coord2d *start2, coord2d *end2);

#endif

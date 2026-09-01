#ifndef GE_STAN_COLLISION_H
#define GE_STAN_COLLISION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_STAN_COLLISION_ASSET_PATH \
    "converted/levels/dam/collision/collision.gestan"
#define GE_STAN_COLLISION_FORMAT_VERSION 1U

typedef enum GeStanCollisionStatus {
    GE_STAN_COLLISION_OK = 0,
    GE_STAN_COLLISION_INVALID_ARGUMENT,
    GE_STAN_COLLISION_INVALID_BLOB,
    GE_STAN_COLLISION_NO_GROUND
} GeStanCollisionStatus;

typedef struct GeStanCollisionSurface {
    const uint8_t *blob;
    size_t blob_size;
    uint32_t tile_count;
    uint32_t point_count;
    uint32_t spawn_tile;
    uint32_t spawn_room;
    uint32_t points_offset;
} GeStanCollisionSurface;

typedef struct GeStanCollisionHit {
    float y;
    float normal[3];
    uint32_t tile_index;
    uint32_t source_tile_index;
    uint32_t tile_id;
    uint8_t room;
    uint8_t special;
} GeStanCollisionHit;

typedef struct GeStanCollisionTile {
    uint32_t source_tile_index;
    uint32_t tile_id;
    uint32_t first_point;
    uint16_t point_count;
    uint8_t room;
    uint8_t special;
    uint16_t mid;
    uint16_t tail;
} GeStanCollisionTile;

typedef struct GeStanCollisionPoint {
    int16_t x;
    int16_t y;
    int16_t z;
    uint16_t source_link;
} GeStanCollisionPoint;

/* Opens a validated, zero-copy GESTAN01 collision blob. */
GeStanCollisionStatus ge_stan_collision_open(
    const uint8_t *blob,
    size_t blob_size,
    GeStanCollisionSurface *surface);

GeStanCollisionStatus ge_stan_collision_get_tile(
    const GeStanCollisionSurface *surface,
    uint32_t tile_index,
    GeStanCollisionTile *tile);

GeStanCollisionStatus ge_stan_collision_get_point(
    const GeStanCollisionSurface *surface,
    uint32_t point_index,
    GeStanCollisionPoint *point);

/* Finds the highest walkable polygon within the caller's vertical window. */
GeStanCollisionStatus ge_stan_collision_ground(
    const GeStanCollisionSurface *surface,
    float x,
    float reference_y,
    float z,
    float max_step_up,
    float max_drop,
    float min_floor_normal_y,
    GeStanCollisionHit *hit);

const char *ge_stan_collision_status_name(GeStanCollisionStatus status);

#ifdef __cplusplus
}
#endif

#endif

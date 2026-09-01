#include "ge_stan_collision.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define GE_STAN_HEADER_SIZE 32U
#define GE_STAN_TILE_SIZE 20U
#define GE_STAN_POINT_SIZE 8U
#define GE_STAN_MAX_POLYGON_POINTS 15U

static uint16_t ge_stan_read_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
}

static int16_t ge_stan_read_s16(const uint8_t *data)
{
    return (int16_t)ge_stan_read_u16(data);
}

static uint32_t ge_stan_read_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) | ((uint32_t)data[1] << 16U)
        | ((uint32_t)data[2] << 8U) | (uint32_t)data[3];
}

static int ge_stan_size_multiply(size_t left, size_t right, size_t *result)
{
    if (left != 0U && right > SIZE_MAX / left) {
        return 0;
    }
    *result = left * right;
    return 1;
}

static int ge_stan_size_add(size_t left, size_t right, size_t *result)
{
    if (right > SIZE_MAX - left) {
        return 0;
    }
    *result = left + right;
    return 1;
}

static const uint8_t *ge_stan_tile(const GeStanCollisionSurface *surface,
                                   uint32_t tile_index)
{
    return surface->blob + GE_STAN_HEADER_SIZE
        + (size_t)tile_index * GE_STAN_TILE_SIZE;
}

static const uint8_t *ge_stan_point(const GeStanCollisionSurface *surface,
                                    uint32_t point_index)
{
    return surface->blob + surface->points_offset
        + (size_t)point_index * GE_STAN_POINT_SIZE;
}

GeStanCollisionStatus ge_stan_collision_open(
    const uint8_t *blob,
    size_t blob_size,
    GeStanCollisionSurface *surface)
{
    static const uint8_t magic[8] = {'G', 'E', 'S', 'T', 'A', 'N', '0', '1'};
    uint32_t tile_count;
    uint32_t point_count;
    uint32_t spawn_tile;
    uint32_t spawn_room;
    uint32_t points_offset;
    size_t tile_bytes;
    size_t expected_points_offset;
    size_t point_bytes;
    size_t expected_size;
    uint32_t tile_index;

    if (surface == NULL) {
        return GE_STAN_COLLISION_INVALID_ARGUMENT;
    }
    memset(surface, 0, sizeof(*surface));
    if (blob == NULL || blob_size < GE_STAN_HEADER_SIZE
            || memcmp(blob, magic, sizeof(magic)) != 0
            || ge_stan_read_u32(blob + 8U)
                != GE_STAN_COLLISION_FORMAT_VERSION) {
        return GE_STAN_COLLISION_INVALID_BLOB;
    }
    tile_count = ge_stan_read_u32(blob + 12U);
    point_count = ge_stan_read_u32(blob + 16U);
    spawn_tile = ge_stan_read_u32(blob + 20U);
    spawn_room = ge_stan_read_u32(blob + 24U);
    points_offset = ge_stan_read_u32(blob + 28U);
    if (tile_count == 0U || point_count == 0U || spawn_tile >= tile_count
            || spawn_room > UINT8_MAX
            || !ge_stan_size_multiply((size_t)tile_count,
                GE_STAN_TILE_SIZE, &tile_bytes)
            || !ge_stan_size_add(GE_STAN_HEADER_SIZE, tile_bytes,
                &expected_points_offset)
            || points_offset != expected_points_offset
            || !ge_stan_size_multiply((size_t)point_count,
                GE_STAN_POINT_SIZE, &point_bytes)
            || !ge_stan_size_add((size_t)points_offset, point_bytes,
                &expected_size)
            || expected_size != blob_size) {
        return GE_STAN_COLLISION_INVALID_BLOB;
    }
    for (tile_index = 0U; tile_index < tile_count; ++tile_index) {
        const uint8_t *tile = blob + GE_STAN_HEADER_SIZE
            + (size_t)tile_index * GE_STAN_TILE_SIZE;
        const uint32_t first = ge_stan_read_u32(tile + 4U);
        const uint16_t count = ge_stan_read_u16(tile + 8U);

        if (count < 3U || count > GE_STAN_MAX_POLYGON_POINTS
                || first > point_count || count > point_count - first
                || ge_stan_read_u32(tile + 12U) > UINT32_C(0xFFFFFF)
                || (ge_stan_read_u16(tile + 16U) >> 12U) != tile[11]
                || (ge_stan_read_u16(tile + 18U) >> 12U) != count) {
            return GE_STAN_COLLISION_INVALID_BLOB;
        }
        if (tile_index == spawn_tile && tile[10] != (uint8_t)spawn_room) {
            return GE_STAN_COLLISION_INVALID_BLOB;
        }
    }
    surface->blob = blob;
    surface->blob_size = blob_size;
    surface->tile_count = tile_count;
    surface->point_count = point_count;
    surface->spawn_tile = spawn_tile;
    surface->spawn_room = spawn_room;
    surface->points_offset = points_offset;
    return GE_STAN_COLLISION_OK;
}

GeStanCollisionStatus ge_stan_collision_get_tile(
    const GeStanCollisionSurface *surface,
    uint32_t tile_index,
    GeStanCollisionTile *tile)
{
    const uint8_t *source;

    if (surface == NULL || surface->blob == NULL || tile == NULL) {
        return GE_STAN_COLLISION_INVALID_ARGUMENT;
    }
    if (tile_index >= surface->tile_count) {
        return GE_STAN_COLLISION_INVALID_BLOB;
    }
    source = ge_stan_tile(surface, tile_index);
    tile->source_tile_index = ge_stan_read_u32(source);
    tile->first_point = ge_stan_read_u32(source + 4U);
    tile->point_count = ge_stan_read_u16(source + 8U);
    tile->room = source[10];
    tile->special = source[11];
    tile->tile_id = ge_stan_read_u32(source + 12U);
    tile->mid = ge_stan_read_u16(source + 16U);
    tile->tail = ge_stan_read_u16(source + 18U);
    return GE_STAN_COLLISION_OK;
}

GeStanCollisionStatus ge_stan_collision_get_point(
    const GeStanCollisionSurface *surface,
    uint32_t point_index,
    GeStanCollisionPoint *point)
{
    const uint8_t *source;

    if (surface == NULL || surface->blob == NULL || point == NULL) {
        return GE_STAN_COLLISION_INVALID_ARGUMENT;
    }
    if (point_index >= surface->point_count) {
        return GE_STAN_COLLISION_INVALID_BLOB;
    }
    source = ge_stan_point(surface, point_index);
    point->x = ge_stan_read_s16(source);
    point->y = ge_stan_read_s16(source + 2U);
    point->z = ge_stan_read_s16(source + 4U);
    point->source_link = ge_stan_read_u16(source + 6U);
    return GE_STAN_COLLISION_OK;
}

static int ge_stan_point_in_polygon(const GeStanCollisionSurface *surface,
                                    const uint8_t *tile, double x, double z)
{
    const uint32_t first = ge_stan_read_u32(tile + 4U);
    const uint16_t count = ge_stan_read_u16(tile + 8U);
    int inside = 0;
    uint16_t index;
    uint16_t previous = (uint16_t)(count - 1U);

    for (index = 0U; index < count; ++index) {
        const uint8_t *a = ge_stan_point(surface, first + index);
        const uint8_t *b = ge_stan_point(surface, first + previous);
        const double ax = (double)ge_stan_read_s16(a);
        const double az = (double)ge_stan_read_s16(a + 4U);
        const double bx = (double)ge_stan_read_s16(b);
        const double bz = (double)ge_stan_read_s16(b + 4U);
        const double edge_x = ax - bx;
        const double edge_z = az - bz;
        const double cross = edge_x * (z - bz) - edge_z * (x - bx);
        const double length_squared = edge_x * edge_x + edge_z * edge_z;

        if (length_squared > 0.0 && fabs(cross) <= 0.000001 * length_squared
                && (x - bx) * (x - ax) <= 0.0
                && (z - bz) * (z - az) <= 0.0) {
            return 1;
        }
        if ((az > z) != (bz > z)
                && x < (bx - ax) * (z - az) / (bz - az) + ax) {
            inside = !inside;
        }
        previous = index;
    }
    return inside;
}

static int ge_stan_tile_plane(const GeStanCollisionSurface *surface,
                              const uint8_t *tile, double x, double z,
                              double *y, float normal[3])
{
    const uint32_t first = ge_stan_read_u32(tile + 4U);
    const uint16_t count = ge_stan_read_u16(tile + 8U);
    const uint8_t *a = ge_stan_point(surface, first);
    uint16_t middle;

    for (middle = 1U; middle + 1U < count; ++middle) {
        const uint8_t *b = ge_stan_point(surface, first + middle);
        const uint8_t *c = ge_stan_point(surface, first + middle + 1U);
        const double abx = (double)ge_stan_read_s16(b)
            - (double)ge_stan_read_s16(a);
        const double aby = (double)ge_stan_read_s16(b + 2U)
            - (double)ge_stan_read_s16(a + 2U);
        const double abz = (double)ge_stan_read_s16(b + 4U)
            - (double)ge_stan_read_s16(a + 4U);
        const double acx = (double)ge_stan_read_s16(c)
            - (double)ge_stan_read_s16(a);
        const double acy = (double)ge_stan_read_s16(c + 2U)
            - (double)ge_stan_read_s16(a + 2U);
        const double acz = (double)ge_stan_read_s16(c + 4U)
            - (double)ge_stan_read_s16(a + 4U);
        double nx = aby * acz - abz * acy;
        double ny = abz * acx - abx * acz;
        double nz = abx * acy - aby * acx;
        const double length = sqrt(nx * nx + ny * ny + nz * nz);

        if (fabs(ny) < 0.000001 || length < 0.000001) {
            continue;
        }
        if (ny < 0.0) {
            nx = -nx;
            ny = -ny;
            nz = -nz;
        }
        normal[0] = (float)(nx / length);
        normal[1] = (float)(ny / length);
        normal[2] = (float)(nz / length);
        *y = (double)ge_stan_read_s16(a + 2U)
            - (nx * (x - (double)ge_stan_read_s16(a))
            + nz * (z - (double)ge_stan_read_s16(a + 4U))) / ny;
        return 1;
    }
    return 0;
}

GeStanCollisionStatus ge_stan_collision_ground(
    const GeStanCollisionSurface *surface,
    float x,
    float reference_y,
    float z,
    float max_step_up,
    float max_drop,
    float min_floor_normal_y,
    GeStanCollisionHit *hit)
{
    double best_y = -DBL_MAX;
    GeStanCollisionHit best;
    uint32_t tile_index;
    int found = 0;

    if (surface == NULL || surface->blob == NULL || hit == NULL
            || !isfinite(x) || !isfinite(reference_y) || !isfinite(z)
            || !isfinite(max_step_up) || max_step_up < 0.0f
            || !isfinite(max_drop) || max_drop < 0.0f
            || !isfinite(min_floor_normal_y)
            || min_floor_normal_y < 0.0f || min_floor_normal_y > 1.0f) {
        return GE_STAN_COLLISION_INVALID_ARGUMENT;
    }
    memset(&best, 0, sizeof(best));
    for (tile_index = 0U; tile_index < surface->tile_count; ++tile_index) {
        const uint8_t *tile = ge_stan_tile(surface, tile_index);
        float normal[3];
        double floor_y;

        if (!ge_stan_point_in_polygon(surface, tile, (double)x, (double)z)
                || !ge_stan_tile_plane(surface, tile, (double)x, (double)z,
                    &floor_y, normal)
                || normal[1] < min_floor_normal_y
                || floor_y > (double)reference_y + (double)max_step_up
                || floor_y < (double)reference_y - (double)max_drop
                || (found && floor_y <= best_y)) {
            continue;
        }
        found = 1;
        best_y = floor_y;
        best.y = (float)floor_y;
        best.normal[0] = normal[0];
        best.normal[1] = normal[1];
        best.normal[2] = normal[2];
        best.tile_index = tile_index;
        best.source_tile_index = ge_stan_read_u32(tile);
        best.room = tile[10];
        best.special = tile[11];
        best.tile_id = ge_stan_read_u32(tile + 12U);
    }
    if (!found) {
        memset(hit, 0, sizeof(*hit));
        return GE_STAN_COLLISION_NO_GROUND;
    }
    *hit = best;
    return GE_STAN_COLLISION_OK;
}

const char *ge_stan_collision_status_name(GeStanCollisionStatus status)
{
    switch (status) {
    case GE_STAN_COLLISION_OK: return "ok";
    case GE_STAN_COLLISION_INVALID_ARGUMENT: return "invalid argument";
    case GE_STAN_COLLISION_INVALID_BLOB: return "invalid blob";
    case GE_STAN_COLLISION_NO_GROUND: return "no ground";
    default: return "unknown";
    }
}

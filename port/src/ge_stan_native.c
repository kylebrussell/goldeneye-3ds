#include "ge_stan_native.h"
#include "ge_original_stan_slice.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GE_STAN_NATIVE_TILE_HEADER_SIZE 8U
#define GE_STAN_NATIVE_TERMINATOR_SIZE 8U
#define GE_STAN_NATIVE_MAX_WALK_ITERATIONS 0x1f5U

_Static_assert(sizeof(GeStanNativePoint) == GE_STAN_NATIVE_LINK_UNIT,
    "native STAN point must retain the original eight-byte stride");
_Static_assert(offsetof(GeStanNativeTile, points)
        == GE_STAN_NATIVE_TILE_HEADER_SIZE,
    "native STAN tile header must retain the original eight-byte stride");

static size_t ge_stan_native_tile_size(uint16_t point_count)
{
    return GE_STAN_NATIVE_TILE_HEADER_SIZE
        + (size_t)point_count * sizeof(GeStanNativePoint);
}

static int ge_stan_native_pointer_is_tile(const GeStanNativeMap *map,
                                          const GeStanNativeTile *candidate)
{
    const GeStanNativeTile *tile;
    uint32_t index;

    if (map == NULL || map->base == NULL || candidate == NULL) {
        return 0;
    }
    tile = map->first_tile;
    for (index = 0U; index < map->tile_count; ++index) {
        if (tile == candidate) {
            return 1;
        }
        tile = (const GeStanNativeTile *)((const uint8_t *)tile
            + ge_stan_native_tile_size(ge_stan_native_point_count(tile)));
    }
    return 0;
}

static const GeStanNativeMap *ge_stan_native_active_map;
static GeStanNativeRouteSearchStats ge_stan_native_route_search_stats;

int ge_stan_native_contains_tile(
    const GeStanNativeMap *map, const void *candidate)
{
    return ge_stan_native_pointer_is_tile(
        map, (const GeStanNativeTile *)candidate);
}

int ge_stan_native_original_binding_matches(const GeStanNativeMap *map)
{
    return map != NULL && map->base != NULL
        && standTileStart == (StandTile *)map->base;
}

int ge_stan_native_route_search_start(const void *candidate)
{
    ge_stan_native_route_search_stats.calls++;
    if (!ge_stan_native_pointer_is_tile(ge_stan_native_active_map,
            (const GeStanNativeTile *)candidate)) {
        ge_stan_native_route_search_stats.rejected_start_tiles++;
        return 0;
    }
    return 1;
}

int ge_stan_native_route_search_result(const void *candidate)
{
    if (candidate != NULL
            && !ge_stan_native_pointer_is_tile(ge_stan_native_active_map,
                (const GeStanNativeTile *)candidate)) {
        ge_stan_native_route_search_stats.rejected_result_tiles++;
        return 0;
    }
    return 1;
}

void ge_stan_native_route_search_snapshot(
    GeStanNativeRouteSearchStats *stats)
{
    if (stats != NULL) *stats = ge_stan_native_route_search_stats;
}

GeStanCollisionStatus ge_stan_native_required_size(
    const GeStanCollisionSurface *surface,
    size_t *required_size)
{
    size_t size = GE_STAN_NATIVE_PREFIX_SIZE + GE_STAN_NATIVE_TERMINATOR_SIZE;
    uint32_t tile_index;

    if (surface == NULL || surface->blob == NULL || required_size == NULL) {
        return GE_STAN_COLLISION_INVALID_ARGUMENT;
    }
    for (tile_index = 0U; tile_index < surface->tile_count; ++tile_index) {
        GeStanCollisionTile tile;
        size_t tile_size;

        if (ge_stan_collision_get_tile(surface, tile_index, &tile)
                != GE_STAN_COLLISION_OK) {
            return GE_STAN_COLLISION_INVALID_BLOB;
        }
        tile_size = ge_stan_native_tile_size(tile.point_count);
        if (tile_size > SIZE_MAX - size) {
            return GE_STAN_COLLISION_INVALID_BLOB;
        }
        size += tile_size;
    }
    *required_size = size;
    return GE_STAN_COLLISION_OK;
}

static void ge_stan_native_write_id(GeStanNativeTile *tile, uint32_t id)
{
    tile->id_bytes[0] = (uint8_t)id;
    tile->id_bytes[1] = (uint8_t)(id >> 8U);
    tile->id_bytes[2] = (uint8_t)(id >> 16U);
}

uint32_t ge_stan_native_tile_id(const GeStanNativeTile *tile)
{
    if (tile == NULL) {
        return 0U;
    }
    return (uint32_t)tile->id_bytes[0]
        | ((uint32_t)tile->id_bytes[1] << 8U)
        | ((uint32_t)tile->id_bytes[2] << 16U);
}

uint16_t ge_stan_native_point_count(const GeStanNativeTile *tile)
{
    return tile == NULL ? 0U : (uint16_t)(((uint16_t)tile->tail >> 12U) & 0xfU);
}

GeStanNativeTile *ge_stan_native_tile_at(
    const GeStanNativeMap *map,
    uint32_t tile_index)
{
    GeStanNativeTile *tile;
    uint32_t index;

    if (map == NULL || map->base == NULL || tile_index >= map->tile_count) {
        return NULL;
    }
    tile = map->first_tile;
    for (index = 0U; index < tile_index; ++index) {
        tile = (GeStanNativeTile *)((uint8_t *)tile
            + ge_stan_native_tile_size(ge_stan_native_point_count(tile)));
    }
    return tile;
}

GeStanCollisionStatus ge_stan_native_materialize(
    const GeStanCollisionSurface *surface,
    float scale,
    void *storage,
    size_t storage_size,
    GeStanNativeMap *map)
{
    size_t required_size;
    uint8_t *cursor;
    uint32_t tile_index;

    if (map == NULL) {
        return GE_STAN_COLLISION_INVALID_ARGUMENT;
    }
    memset(map, 0, sizeof(*map));
    if (storage == NULL || !isfinite(scale) || scale <= 0.0f
            || ge_stan_native_required_size(surface, &required_size)
                != GE_STAN_COLLISION_OK
            || storage_size < required_size) {
        return GE_STAN_COLLISION_INVALID_ARGUMENT;
    }
    memset(storage, 0, required_size);
    map->base = storage;
    map->size = required_size;
    map->first_tile = (GeStanNativeTile *)(map->base
        + GE_STAN_NATIVE_PREFIX_SIZE);
    map->tile_count = surface->tile_count;
    map->level_scale = scale;
    map->inverse_level_scale = 1.0f / scale;
    cursor = (uint8_t *)map->first_tile;

    for (tile_index = 0U; tile_index < surface->tile_count; ++tile_index) {
        GeStanCollisionTile source_tile;
        GeStanNativeTile *tile = (GeStanNativeTile *)cursor;
        uint16_t point_index;

        if (ge_stan_collision_get_tile(surface, tile_index, &source_tile)
                != GE_STAN_COLLISION_OK) {
            memset(map, 0, sizeof(*map));
            return GE_STAN_COLLISION_INVALID_BLOB;
        }
        ge_stan_native_write_id(tile, source_tile.tile_id);
        tile->room = source_tile.room;
        tile->mid = (int16_t)source_tile.mid;
        tile->tail = (int16_t)source_tile.tail;
        for (point_index = 0U; point_index < source_tile.point_count;
                ++point_index) {
            GeStanCollisionPoint source_point;

            if (ge_stan_collision_get_point(surface,
                    source_tile.first_point + point_index, &source_point)
                    != GE_STAN_COLLISION_OK) {
                memset(map, 0, sizeof(*map));
                return GE_STAN_COLLISION_INVALID_BLOB;
            }
            tile->points[point_index].x = source_point.x;
            tile->points[point_index].y = source_point.y;
            tile->points[point_index].z = source_point.z;
            tile->points[point_index].link = source_point.source_link;
        }
        if (tile_index == surface->spawn_tile) {
            map->spawn_tile = tile;
        }
        cursor += ge_stan_native_tile_size(source_tile.point_count);
    }
    map->terminator = (GeStanNativeTile *)cursor;

    /* Every nonzero original link must land on a native tile boundary. */
    for (tile_index = 0U; tile_index < map->tile_count; ++tile_index) {
        GeStanNativeTile *tile = ge_stan_native_tile_at(map, tile_index);
        uint16_t point_index;

        for (point_index = 0U; point_index < ge_stan_native_point_count(tile);
                ++point_index) {
            const uint16_t link = tile->points[point_index].link;
            const size_t byte_offset = (size_t)link * GE_STAN_NATIVE_LINK_UNIT;
            GeStanNativeTile *linked;

            if (link == 0U) {
                continue;
            }
            if (byte_offset < GE_STAN_NATIVE_PREFIX_SIZE
                    || byte_offset >= (size_t)((uint8_t *)map->terminator
                        - map->base)) {
                memset(map, 0, sizeof(*map));
                return GE_STAN_COLLISION_INVALID_BLOB;
            }
            linked = (GeStanNativeTile *)(map->base + byte_offset);
            if (!ge_stan_native_pointer_is_tile(map, linked)) {
                memset(map, 0, sizeof(*map));
                return GE_STAN_COLLISION_INVALID_BLOB;
            }
        }
    }
    return GE_STAN_COLLISION_OK;
}

static struct StanPrefixRecord ge_stan_native_original_prefix;

GeStanCollisionStatus ge_stan_native_bind_original(const GeStanNativeMap *map)
{
    if (map == NULL || map->base == NULL || map->first_tile == NULL
            || map->spawn_tile == NULL || map->tile_count == 0U
            || !isfinite(map->level_scale) || map->level_scale <= 0.0f) {
        return GE_STAN_COLLISION_INVALID_ARGUMENT;
    }
    if (ge_stan_native_active_map != map) {
        memset(&ge_stan_native_route_search_stats, 0,
            sizeof(ge_stan_native_route_search_stats));
    }
    ge_stan_native_active_map = map;
    ge_stan_native_original_prefix.stanfile = 0;
    ge_stan_native_original_prefix.ptr_firstroom = (StandTile *)map->first_tile;
    stan_prefix = &ge_stan_native_original_prefix;
    standTileStart = (StandTile *)map->base;
    setLevelScale(map->level_scale);
    return GE_STAN_COLLISION_OK;
}

GeStanNativeTile *ge_original_stan_match_tile_name(
    const GeStanNativeMap *map,
    const char *name)
{
    if (map == NULL || map->base == NULL || name == NULL) {
        return NULL;
    }
    if (ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return NULL;
    }
    return (GeStanNativeTile *)stanMatchTileName((char *)name);
}

int ge_original_stan_resolve_pad(
    const GeStanNativeMap *map,
    const char *name,
    float x,
    float y,
    float z,
    GeStanNativeTile **tile_io)
{
    StandTile *tile;
    coord3d nearest;
    float stan_x;
    float stan_y;
    float stan_z;

    if (map == NULL || map->base == NULL || name == NULL || tile_io == NULL
            || !isfinite(x) || !isfinite(y) || !isfinite(z)) {
        if (tile_io != NULL) {
            *tile_io = NULL;
        }
        return 0;
    }
    if (ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        *tile_io = NULL;
        return 0;
    }

    /* proplvreset2 has already multiplied every setup pad by the room-scale
     * reciprocal before entering init_pathtable_something. */
    stan_x = x;
    stan_y = y;
    stan_z = z;

    tile = (StandTile *)stanMatchTileName((char *)name);
    if (tile != NULL
            && isPointInsideTriStandTileUnscaled_Maybe(tile, stan_x,
                stan_z) != 0) {
        *tile_io = (GeStanNativeTile *)tile;
        return 1;
    }

    nearest.x = stan_x;
    nearest.y = stan_y;
    nearest.z = stan_z;
    tile = sub_GAME_7F0AFB78(&nearest.x, &nearest.y, &nearest.z, 0.0f);
    if (tile != NULL
            && walkTilesBetweenPoints_NoCallback(&tile, nearest.x, nearest.z,
                stan_x, stan_z) != 0) {
        *tile_io = (GeStanNativeTile *)tile;
        return 2;
    }

    *tile_io = NULL;
    return 0;
}

float ge_original_stan_get_position_y(
    const GeStanNativeMap *map,
    const GeStanNativeTile *tile,
    float x,
    float z)
{
    if (map == NULL || map->base == NULL || tile == NULL) {
        return 0.0f;
    }
    if (ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return 0.0f;
    }
    return stanGetPositionYValue((StandTile *)tile, x, z);
}

int ge_original_stan_test_point_within_bounds(
    const GeStanNativeMap *map,
    const GeStanNativeTile *tile,
    float x,
    float z)
{
    if (map == NULL || map->base == NULL || tile == NULL) {
        return 0;
    }
    if (ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return 0;
    }
    return stanTestPointWithinTileBoundsMaybe((StandTile *)tile, x, z);
}

int ge_original_stan_walk_tiles_between_points(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile_io,
    float start_x,
    float start_z,
    float destination_x,
    float destination_z)
{
    StandTile *original_tile;
    int result;

    if (map == NULL || tile_io == NULL
            || !ge_stan_native_pointer_is_tile(map, *tile_io)) {
        return 0;
    }
    if (ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return 0;
    }
    original_tile = (StandTile *)*tile_io;
    result = walkTilesBetweenPoints_NoCallback(&original_tile, start_x, start_z,
        destination_x, destination_z);
    *tile_io = (GeStanNativeTile *)original_tile;
    return result;
}

int ge_original_stan_test_radius(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile_io,
    float x,
    float z,
    float radius)
{
    StandTile *original_tile;
    int result;

    if (map == NULL || tile_io == NULL
            || !ge_stan_native_pointer_is_tile(map, *tile_io)
            || !isfinite(x) || !isfinite(z) || !isfinite(radius)
            || radius < 0.0f
            || ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return STAN_COLLISION_TRAVERSAL_LIMIT;
    }
    original_tile = (StandTile *)*tile_io;
    result = sub_GAME_7F0B20D0(&original_tile, x, z, radius);
    *tile_io = (GeStanNativeTile *)original_tile;
    return result;
}

int ge_original_stan_test_locus(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile_io,
    float x,
    float z,
    float radius,
    GeOriginalStanLocusResult *result_out)
{
    union {
        max_align_t align;
        uint8_t bytes[64];
    } record_storage;
    struct StandTileLocusCallbackRecord *record;
    StandTile *original_tile;
    int collision_result;

    if (map == NULL || tile_io == NULL || result_out == NULL
            || !ge_stan_native_pointer_is_tile(map, *tile_io)
            || !isfinite(x) || !isfinite(z) || !isfinite(radius)
            || radius < 0.0f
            || ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return 0;
    }

    memset(&record_storage, 0, sizeof(record_storage));
    record = (struct StandTileLocusCallbackRecord *)record_storage.bytes;
    original_tile = (StandTile *)*tile_io;
    collision_result = stanTileDistanceRelated(&original_tile, x, z, radius,
        record);
    result_out->collision_result = collision_result;
    result_out->force_crouch = stanGetLocusField0(record) != 0;
    result_out->linked_ladder = stanGetLocusCount(record) != 0;
    *tile_io = (GeStanNativeTile *)original_tile;
    return 1;
}

int ge_original_stan_test_locus_edge_above_y(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile_io,
    float x,
    float z,
    float radius,
    float y_threshold)
{
    StandTile *original_tile;
    int result;

    if (map == NULL || tile_io == NULL
            || !ge_stan_native_pointer_is_tile(map, *tile_io)
            || !isfinite(x) || !isfinite(z) || !isfinite(radius)
            || !isfinite(y_threshold) || radius < 0.0f
            || ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return STAN_COLLISION_TRAVERSAL_LIMIT;
    }
    original_tile = (StandTile *)*tile_io;
    result = stanTestLocusEdgeAboveY(&original_tile, x, z, radius,
        y_threshold);
    *tile_io = (GeStanNativeTile *)original_tile;
    return result;
}

int ge_original_stan_test_line_static(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile_io,
    float start_x,
    float start_z,
    float destination_x,
    float destination_z,
    float height,
    float height_end,
    float slope_start,
    float slope_end)
{
    StandTile *original_tile;
    int result;

    if (map == NULL || tile_io == NULL
            || !ge_stan_native_pointer_is_tile(map, *tile_io)
            || !isfinite(start_x) || !isfinite(start_z)
            || !isfinite(destination_x) || !isfinite(destination_z)
            || !isfinite(height) || !isfinite(height_end)
            || !isfinite(slope_start) || !isfinite(slope_end)
            || ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return 0;
    }
    original_tile = (StandTile *)*tile_io;
    result = stanTestLineUnobstructed(&original_tile, start_x, start_z,
        destination_x, destination_z, 0, height, height_end, slope_start,
        slope_end);
    *tile_io = (GeStanNativeTile *)original_tile;
    return result;
}

int ge_original_stan_test_volume_static(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile_io,
    float x,
    float z,
    float radius,
    float height,
    float height_end)
{
    StandTile *original_tile;
    int result;

    if (map == NULL || tile_io == NULL
            || !ge_stan_native_pointer_is_tile(map, *tile_io)
            || !isfinite(x) || !isfinite(z) || !isfinite(radius)
            || !isfinite(height) || !isfinite(height_end)
            || radius < 0.0f
            || ge_stan_native_bind_original(map) != GE_STAN_COLLISION_OK) {
        return STAN_COLLISION_TRAVERSAL_LIMIT;
    }
    original_tile = (StandTile *)*tile_io;
    result = stanTestVolume(&original_tile, x, z, radius, 0, height,
        height_end);
    *tile_io = (GeStanNativeTile *)original_tile;
    return result;
}

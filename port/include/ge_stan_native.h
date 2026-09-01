#ifndef GE_STAN_NATIVE_H
#define GE_STAN_NATIVE_H

#include "ge_stan_collision.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A loaded N64 STAN uses link values as eight-byte offsets from a virtual
 * base 0x80 bytes before its first tile.  These types describe the native
 * ARM representation of that arena.  They deliberately retain the original
 * eight-byte tile header and eight-byte point records so original stan.c
 * geometry can consume it through a narrow compatibility boundary.
 */
#define GE_STAN_NATIVE_PREFIX_SIZE 0x80U
#define GE_STAN_NATIVE_LINK_UNIT 8U

typedef struct GeStanNativePoint {
    int16_t x;
    int16_t y;
    int16_t z;
    uint16_t link;
} GeStanNativePoint;

typedef struct GeStanNativeTile {
    /* ARM's original u32 id:24 bitfield occupies these low-order bytes. */
    uint8_t id_bytes[3];
    uint8_t room;
    int16_t mid;
    int16_t tail;
    GeStanNativePoint points[];
} GeStanNativeTile;

typedef struct GeStanNativeMap {
    uint8_t *base;
    size_t size;
    GeStanNativeTile *first_tile;
    GeStanNativeTile *terminator;
    GeStanNativeTile *spawn_tile;
    uint32_t tile_count;
    float level_scale;
    float inverse_level_scale;
} GeStanNativeMap;

typedef struct GeStanNativeRouteSearchStats {
    uint32_t calls;
    uint32_t rejected_start_tiles;
    uint32_t rejected_result_tiles;
} GeStanNativeRouteSearchStats;

/* Returns the caller-owned arena size needed by materialize. */
GeStanCollisionStatus ge_stan_native_required_size(
    const GeStanCollisionSurface *surface,
    size_t *required_size);

/*
 * Rebuilds the complete contiguous native STAN.  Links are validated against
 * native tile starts and copied unchanged; subsets cannot pass validation.
 */
GeStanCollisionStatus ge_stan_native_materialize(
    const GeStanCollisionSurface *surface,
    float level_scale,
    void *storage,
    size_t storage_size,
    GeStanNativeMap *map);

GeStanNativeTile *ge_stan_native_tile_at(
    const GeStanNativeMap *map,
    uint32_t tile_index);

uint32_t ge_stan_native_tile_id(const GeStanNativeTile *tile);
uint16_t ge_stan_native_point_count(const GeStanNativeTile *tile);
int ge_stan_native_contains_tile(
    const GeStanNativeMap *map, const void *candidate);
int ge_stan_native_original_binding_matches(const GeStanNativeMap *map);

/*
 * Compatibility audit used at the single original stanFillSearch caller.
 * The canonical search body remains unchanged; these calls prevent an invalid
 * native pointer from becoming an emulator-dependent zero-filled read.
 */
int ge_stan_native_route_search_start(const void *candidate);
int ge_stan_native_route_search_result(const void *candidate);
void ge_stan_native_route_search_snapshot(
    GeStanNativeRouteSearchStats *stats);

/*
 * Installs this arena in the original stan.c globals.  After this succeeds,
 * original gameplay code can call the compiled STAN symbols directly.
 * The map and caller-owned storage must remain alive until another bind.
 */
GeStanCollisionStatus ge_stan_native_bind_original(
    const GeStanNativeMap *map);

/* Port-safe form of the original stanPackId/stanMatchTileName pair. */
GeStanNativeTile *ge_original_stan_match_tile_name(
    const GeStanNativeMap *map,
    const char *name);

/*
 * Exact init_pathtable_something resolution semantics.  A named tile is
 * accepted only when the authored pad point lies inside it.  Missing, empty,
 * or mismatched names take the original nearest-navigable-tile plus walk
 * fallback.  Returns the original body's 1 (name), 2 (fallback), or 0
 * (unresolved), and writes NULL on failure.
 */
int ge_original_stan_resolve_pad(
    const GeStanNativeMap *map,
    const char *name,
    float x,
    float y,
    float z,
    GeStanNativeTile **tile);

/* Thin wrappers around the exact bodies compiled from src/game/stan.c. */
float ge_original_stan_get_position_y(
    const GeStanNativeMap *map,
    const GeStanNativeTile *tile,
    float x,
    float z);

int ge_original_stan_test_point_within_bounds(
    const GeStanNativeMap *map,
    const GeStanNativeTile *tile,
    float x,
    float z);

int ge_original_stan_walk_tiles_between_points(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile,
    float start_x,
    float start_z,
    float destination_x,
    float destination_z);

typedef enum GeOriginalStanCollisionResult {
    GE_ORIGINAL_STAN_COLLISION_NONE = -2,
    GE_ORIGINAL_STAN_COLLISION_FOUND = 2,
    GE_ORIGINAL_STAN_COLLISION_TRAVERSAL_LIMIT = 5
} GeOriginalStanCollisionResult;

/* Exact original radius traversal used by bondviewTryMoveToStan. */
int ge_original_stan_test_radius(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile,
    float x,
    float z,
    float radius);

typedef struct GeOriginalStanLocusResult {
    int collision_result;
    int force_crouch;
    int linked_ladder;
} GeOriginalStanLocusResult;

/*
 * Exact stanTileDistanceRelated body behind a 64-byte compatibility record.
 * The record size contains the decomp's deliberate four-record clear and the
 * wrapper prevents that clear from touching adjacent player stack state.
 */
int ge_original_stan_test_locus(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile,
    float x,
    float z,
    float radius,
    GeOriginalStanLocusResult *result);

/* Exact tall-edge locus test used by bondviewTryMoveToStan. */
int ge_original_stan_test_locus_edge_above_y(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile,
    float x,
    float z,
    float radius,
    float y_threshold);

/*
 * Exact static-STAN portions of the original movement collision pair.  Object
 * cdtypes remain zero until the typed prop collision provider is connected.
 */
int ge_original_stan_test_line_static(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile,
    float start_x,
    float start_z,
    float destination_x,
    float destination_z,
    float height,
    float height_end,
    float slope_start,
    float slope_end);

int ge_original_stan_test_volume_static(
    const GeStanNativeMap *map,
    GeStanNativeTile **tile,
    float x,
    float z,
    float radius,
    float height,
    float height_end);

#ifdef __cplusplus
}
#endif

#endif

#ifndef GE_ORIGINAL_BG_GEOMETRY_H
#define GE_ORIGINAL_BG_GEOMETRY_H

#include "ge_asset_pack.h"
#include "ge_dam_room.h"
#include "ge_dam_world.h"
#include "ge_original_bg_visibility.h"
#include "ge_stage_assets.h"

typedef struct GeOriginalBgGeometry GeOriginalBgGeometry;

typedef enum GeOriginalBgGeometryStatus {
    GE_ORIGINAL_BG_GEOMETRY_OK = 0,
    GE_ORIGINAL_BG_GEOMETRY_INVALID_ARGUMENT,
    GE_ORIGINAL_BG_GEOMETRY_INVALID_ASSET,
    GE_ORIGINAL_BG_GEOMETRY_NO_MEMORY,
    GE_ORIGINAL_BG_GEOMETRY_CAPACITY,
    GE_ORIGINAL_BG_GEOMETRY_NOT_BOUND
} GeOriginalBgGeometryStatus;

typedef struct GeOriginalBgGeometryReport {
    GeOriginalBgGeometryStatus status;
    size_t rooms, vertices, commands, batches, fixtures;
    uint32_t failed_room;
} GeOriginalBgGeometryReport;

typedef struct GeOriginalBgGeometryHit {
    float position[3], normal[3];
    int16_t texture, tile_format, tile_size;
    uint32_t triangle_type;
    void *triangle;
} GeOriginalBgGeometryHit;

/* Owns copies of every authored room, irrespective of renderer residency.
 * Rooms 1..room_count-1 must appear exactly once; room0 is the original void
 * room. Original bounds use authored room coordinates, origins included. */
GeOriginalBgGeometry *ge_original_bg_geometry_open_rooms(
    const GeDamRoomBlobDescriptor *rooms, size_t descriptor_count,
    const GeOriginalBgRoomBounds *bounds, size_t room_count, float level_scale,
    GeOriginalBgGeometryReport *report);
GeOriginalBgGeometry *ge_original_bg_geometry_load(
    GeAssetPack *pack, const GeStageAssetDescriptor *stage,
    const GeDamWorld *world, const GeOriginalBgRoomBounds *bounds,
    GeOriginalBgGeometryReport *report);
void ge_original_bg_geometry_close(GeOriginalBgGeometry *geometry);
void ge_original_bg_geometry_bind(GeOriginalBgGeometry *geometry);
/* Bound canonical room count, including room0; zero when unbound. */
size_t ge_original_bg_geometry_room_count(void);
/* Exact authored origin owned by the currently bound native geometry,
 * including void room0. No level-scale conversion or renderer dependency.
 * Returns NULL when unbound, invalid, or outside the authored room range.
 * Borrow read-only while this owner remains bound/alive; never retain across
 * rebind/close. The forward declaration avoids importing native ABI headers. */
struct coord3d;
const struct coord3d *ge_original_bg_geometry_room_origin(int32_t room);
GeOriginalBgGeometryStatus ge_original_bg_geometry_status(
    const GeOriginalBgGeometry *geometry);

/* Same exact original background algorithm used by the gameplay symbol
 * bgTestBulletHitBackground. Inputs are world coordinates, output remains
 * scaled as in the original caller contract. No triangles are approximated. */
int ge_original_bg_geometry_hit(GeOriginalBgGeometry *geometry,
    const float from[3], const float to[3], int32_t room,
    GeOriginalBgGeometryHit *hit);

/* GeOriginalPlayerLightBreak callback. Triangle must be a live primary-list
 * token returned by this owner. Calls the retained lightFixtureBreak body,
 * including actual glassCreateShard, and persists mutated vertex colours. */
int ge_original_bg_geometry_light_break(void *context, void *triangle,
    uint32_t triangle_type, int32_t room);

/* Local renderer boundary only: reapplies this owner's persistent authored
 * vertex colours to retained/imported room triangles using exact GBI point
 * provenance. Pass only the room batch prefix, excluding actor/object
 * overlays. Returns0 on mismatched geometry.
 * Ranges must be ordered and nonoverlapping as emitted by ge_dam_rooms_build;
 * authored world coordinates must match this owner's room origins. Rejection
 * leaves all vertices unchanged. Neither canonical collision nor visibility
 * depends on this call. */
int ge_original_bg_geometry_apply_scene(GeOriginalBgGeometry *geometry,
    GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batches, size_t batch_count,
    size_t *changed_vertices);

#endif

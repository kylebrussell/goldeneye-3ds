#ifndef GE_STAGE_ASSETS_H
#define GE_STAGE_ASSETS_H

#include "ge_asset_pack.h"
#include "ge_dam_world.h"
#include "ge_stan_collision.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_STAGE_ASSET_PATH_CAPACITY 160U

typedef enum GeStageId {
    GE_STAGE_DAM = 0,
    GE_STAGE_FACILITY = 1,
#define GE_SOLO_STAGE(symbol, ...) symbol,
#include "ge_solo_stage_registry.inc"
#undef GE_SOLO_STAGE
    /* Authored post-Cradle credits stage. This is not a selectable solo
     * mission and therefore intentionally lives outside the generated
     * 20-entry mission-folder registry. */
    GE_STAGE_CUBA,
    GE_STAGE_COUNT
} GeStageId;

typedef enum GeStageRoomStream {
    GE_STAGE_ROOM_POINTS = 0,
    GE_STAGE_ROOM_PRIMARY_GDL,
    GE_STAGE_ROOM_SECONDARY_GDL
} GeStageRoomStream;

typedef struct GeStageAssetDescriptor {
    GeStageId stage;
    int32_t level_id;
    int32_t level_index;
    float level_scale;
    float visibility_scale;
    float visibility_distance;
    const char *key;
    const char *decomp_key;
    const char *setup_key;
    const char *stan_key;
    const char *background_path;
    const char *rooms_prefix;
    const char *room_bounds_path;
    const char *collision_path;
    const char *setup_path;
    size_t expected_background_size;
    size_t expected_collision_size;
    size_t expected_room_bounds_size;
    size_t expected_setup_size;
    uint64_t expected_background_fnv1a64;
    uint64_t expected_collision_fnv1a64;
    uint64_t expected_room_bounds_fnv1a64;
    uint64_t expected_setup_fnv1a64;
    size_t expected_room_count;
    size_t expected_portal_count;
    uint32_t expected_stan_tiles;
    uint32_t expected_stan_points;
    uint32_t expected_spawn_tile;
    uint8_t expected_spawn_room;
} GeStageAssetDescriptor;

typedef struct GeStageResolvedAssets {
    const GeStageAssetDescriptor *descriptor;
    uint8_t *background;
    size_t background_size;
    uint8_t *collision;
    size_t collision_size;
    GeDamWorld world;
    GeStanCollisionSurface stan;
    const GeAssetPackEntry *room_bounds_entry;
    const GeAssetPackEntry *setup_entry;
    uint8_t loaded;
} GeStageResolvedAssets;

typedef enum GeStageAssetStatus {
    GE_STAGE_ASSET_OK = 0,
    GE_STAGE_ASSET_INVALID_ARGUMENT,
    GE_STAGE_ASSET_NOT_FOUND,
    GE_STAGE_ASSET_INVALID,
    GE_STAGE_ASSET_NO_MEMORY,
    GE_STAGE_ASSET_PATH_TOO_LONG
} GeStageAssetStatus;

const GeStageAssetDescriptor *ge_stage_asset_descriptor(GeStageId stage);
const GeStageAssetDescriptor *ge_stage_asset_descriptor_by_key(const char *key);
const GeStageAssetDescriptor *ge_stage_asset_descriptor_by_level_id(
    int32_t level_id);
const GeStageAssetDescriptor *ge_stage_asset_dam(void);
const GeStageAssetDescriptor *ge_stage_asset_facility(void);

GeStageAssetStatus ge_stage_asset_room_path(
    const GeStageAssetDescriptor *descriptor, uint8_t room,
    GeStageRoomStream stream, char *path, size_t path_capacity);

GeStageAssetStatus ge_stage_assets_resolve(
    GeAssetPack *pack, const GeStageAssetDescriptor *descriptor,
    GeStageResolvedAssets *resolved);
void ge_stage_assets_close(GeStageResolvedAssets *resolved);
const char *ge_stage_asset_status_name(GeStageAssetStatus status);

#ifdef __cplusplus
}
#endif

#endif

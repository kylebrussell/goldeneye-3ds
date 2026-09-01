#ifndef GE_DAM_VISIBILITY_RUNTIME_H
#define GE_DAM_VISIBILITY_RUNTIME_H

#include "ge_asset_pack.h"
#include "ge_original_bg_visibility.h"
#include "ge_original_bond_camera.h"
#include "ge_stage_assets.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_DAM_VISIBILITY_BACKGROUND_PATH \
    "converted/levels/dam/background.bin"
#define GE_DAM_VISIBILITY_BOUNDS_PATH \
    "converted/levels/dam/room_bounds.gebounds"
#define GE_DAM_VISIBILITY_BOUNDS_VERSION 1U
#define GE_DAM_VISIBILITY_BOUNDS_HEADER_SIZE 80U
#define GE_DAM_VISIBILITY_BOUNDS_RECORD_SIZE 24U

typedef struct GeDamVisibilityRuntime {
    uint8_t *background;
    size_t background_size;
    GeOriginalBgRoomBounds room_bounds[
        GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS];
    size_t room_count;
    int32_t level_index;
    float level_scale;
    float visibility_scale;
    float near_distance;
    float far_distance;
    uint8_t source_sha256[32];
    uint64_t source_fnv1a64;
    uint64_t payload_fnv1a64;
    GeOriginalBgVisibilityStatus last_original_status;
    uint8_t loaded;
} GeDamVisibilityRuntime;

typedef enum GeDamVisibilityRuntimeStatus {
    GE_DAM_VISIBILITY_RUNTIME_OK = 0,
    GE_DAM_VISIBILITY_RUNTIME_INVALID_ARGUMENT,
    GE_DAM_VISIBILITY_RUNTIME_ASSET_NOT_FOUND,
    GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET,
    GE_DAM_VISIBILITY_RUNTIME_NO_MEMORY,
    GE_DAM_VISIBILITY_RUNTIME_CAMERA_INVALID,
    GE_DAM_VISIBILITY_RUNTIME_VISIBILITY_FAILED
} GeDamVisibilityRuntimeStatus;

/* Buffer entry point used by asset-pack loading and deterministic tests. */
GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_init(
    const uint8_t *background, size_t background_size,
    const uint8_t *bounds_asset, size_t bounds_asset_size,
    GeDamVisibilityRuntime *runtime);

GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_load(
    GeAssetPack *pack, GeDamVisibilityRuntime *runtime);

GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_load_for_stage(
    GeAssetPack *pack, const GeStageAssetDescriptor *stage,
    GeDamVisibilityRuntime *runtime);

void ge_dam_visibility_runtime_close(GeDamVisibilityRuntime *runtime);

/* Installs the viSetZRange values selected by the unchanged stage
 * fogLoadLevelEnvironment record. */
int ge_dam_visibility_runtime_set_zrange(
    GeDamVisibilityRuntime *runtime, float near_distance, float far_distance);

/* Feeds the exact live bondview camera view matrix and room into the exact
 * original bgDetermineVisibleRooms slice. Dam's original level scales and
 * camera near/far contract remain fixed at their source values. */
GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_run(
    GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera,
    const float player_position[3],
    GeOriginalBgVisibilityResult *result);

GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_run_with_providers(
    GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera,
    const float player_position[3],
    const GeOriginalBgVisibilityProviders *providers,
    GeOriginalBgVisibilityResult *result);

const char *ge_dam_visibility_runtime_status_name(
    GeDamVisibilityRuntimeStatus status);

#ifdef __cplusplus
}
#endif

#endif

#include "ge_dam_visibility_runtime.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GE_DAM_BACKGROUND_MAX_SIZE (512U * 1024U)
#define GE_DAM_LEVEL_SCALE 0.23363999f
#define GE_DAM_VISIBILITY_SCALE 0.2f
#define GE_DAM_NEAR_DISTANCE 100.0f
#define GE_DAM_FAR_DISTANCE 10000.0f

static const uint8_t ge_bounds_magic[8] = {
    'G', 'E', 'D', 'M', 'B', 'N', 'D', 0,
};

static uint32_t ge_read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8)
        | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint64_t ge_read_u64_le(const uint8_t *data)
{
    return (uint64_t)ge_read_u32_le(data)
        | ((uint64_t)ge_read_u32_le(data + 4U) << 32);
}

static float ge_read_float_le(const uint8_t *data)
{
    const uint32_t bits = ge_read_u32_le(data);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t ge_fnv1a64(const uint8_t *data, size_t size)
{
    uint64_t value = UINT64_C(14695981039346656037);
    size_t index;

    for (index = 0U; index < size; ++index) {
        value ^= data[index];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

void ge_dam_visibility_runtime_close(GeDamVisibilityRuntime *runtime)
{
    if (runtime == NULL) return;
    ge_original_bg_visibility_program_close(runtime->program);
    free(runtime->background);
    memset(runtime, 0, sizeof(*runtime));
}

int ge_dam_visibility_runtime_set_zrange(
    GeDamVisibilityRuntime *runtime, float near_distance, float far_distance)
{
    if (runtime == NULL || runtime->loaded == 0U
            || !isfinite(near_distance) || !isfinite(far_distance)
            || near_distance <= 0.0f || far_distance <= near_distance)
        return 0;
    runtime->near_distance = near_distance;
    runtime->far_distance = far_distance;
    return 1;
}

GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_init(
    const uint8_t *background, size_t background_size,
    const uint8_t *bounds_asset, size_t bounds_asset_size,
    GeDamVisibilityRuntime *runtime)
{
    size_t room_count;
    size_t payload_size;
    const uint8_t *payload;
    uint64_t source_hash;
    uint64_t payload_hash;
    size_t room;

    if (background == NULL || bounds_asset == NULL || runtime == NULL) {
        return GE_DAM_VISIBILITY_RUNTIME_INVALID_ARGUMENT;
    }
    memset(runtime, 0, sizeof(*runtime));
    if (background_size == 0U || background_size > GE_DAM_BACKGROUND_MAX_SIZE
            || bounds_asset_size < GE_DAM_VISIBILITY_BOUNDS_HEADER_SIZE
            || memcmp(bounds_asset, ge_bounds_magic,
                      sizeof(ge_bounds_magic)) != 0
            || ge_read_u32_le(bounds_asset + 8U)
                != GE_DAM_VISIBILITY_BOUNDS_VERSION
            || ge_read_u32_le(bounds_asset + 12U)
                != GE_DAM_VISIBILITY_BOUNDS_HEADER_SIZE
            || ge_read_u32_le(bounds_asset + 20U)
                != GE_DAM_VISIBILITY_BOUNDS_RECORD_SIZE) {
        return GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET;
    }
    room_count = ge_read_u32_le(bounds_asset + 16U);
    payload_size = ge_read_u32_le(bounds_asset + 24U);
    if (room_count == 0U
            || room_count > GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS
            || payload_size != room_count
                * GE_DAM_VISIBILITY_BOUNDS_RECORD_SIZE
            || bounds_asset_size
                != GE_DAM_VISIBILITY_BOUNDS_HEADER_SIZE + payload_size) {
        return GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET;
    }
    payload = bounds_asset + GE_DAM_VISIBILITY_BOUNDS_HEADER_SIZE;
    source_hash = ge_fnv1a64(background, background_size);
    payload_hash = ge_fnv1a64(payload, payload_size);
    if (source_hash != ge_read_u64_le(bounds_asset + 64U)
            || payload_hash != ge_read_u64_le(bounds_asset + 72U)) {
        return GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET;
    }
    for (room = 0U; room < room_count; ++room) {
        const uint8_t *record = payload
            + room * GE_DAM_VISIBILITY_BOUNDS_RECORD_SIZE;
        size_t axis;

        for (axis = 0U; axis < 3U; ++axis) {
            const float minimum = ge_read_float_le(record + axis * 4U);
            const float maximum = ge_read_float_le(record + 12U + axis * 4U);

            if (!isfinite(minimum) || !isfinite(maximum)
                    || minimum > maximum) {
                return GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET;
            }
            runtime->room_bounds[room].minimum[axis] = minimum;
            runtime->room_bounds[room].maximum[axis] = maximum;
        }
    }
    runtime->background = malloc(background_size);
    if (runtime->background == NULL) {
        memset(runtime, 0, sizeof(*runtime));
        return GE_DAM_VISIBILITY_RUNTIME_NO_MEMORY;
    }
    memcpy(runtime->background, background, background_size);
    runtime->program = ge_original_bg_visibility_program_create(
        runtime->background, background_size, room_count, &runtime->last_original_status);
    if (runtime->program == NULL) {
        GeDamVisibilityRuntimeStatus status = runtime->last_original_status
                == GE_ORIGINAL_BG_VISIBILITY_NO_MEMORY
            ? GE_DAM_VISIBILITY_RUNTIME_NO_MEMORY : GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET;
        ge_dam_visibility_runtime_close(runtime);
        return status;
    }
    runtime->background_size = background_size;
    runtime->room_count = room_count;
    /* Buffer-only callers historically consume Dam assets. Stage-aware loads
     * replace these with the exact levelinfotable values below. */
    runtime->level_index = 13;
    runtime->level_scale = GE_DAM_LEVEL_SCALE;
    runtime->visibility_scale = GE_DAM_VISIBILITY_SCALE;
    runtime->near_distance = GE_DAM_NEAR_DISTANCE;
    runtime->far_distance = GE_DAM_FAR_DISTANCE;
    memcpy(runtime->source_sha256, bounds_asset + 32U,
           sizeof(runtime->source_sha256));
    runtime->source_fnv1a64 = source_hash;
    runtime->payload_fnv1a64 = payload_hash;
    runtime->loaded = 1U;
    return GE_DAM_VISIBILITY_RUNTIME_OK;
}

GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_load(
    GeAssetPack *pack, GeDamVisibilityRuntime *runtime)
{
    return ge_dam_visibility_runtime_load_for_stage(
        pack, ge_stage_asset_dam(), runtime);
}

GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_load_for_stage(
    GeAssetPack *pack, const GeStageAssetDescriptor *stage,
    GeDamVisibilityRuntime *runtime)
{
    const GeAssetPackEntry *background_entry;
    const GeAssetPackEntry *bounds_entry;
    uint8_t *background;
    uint8_t *bounds;
    size_t bytes_read;
    GeDamVisibilityRuntimeStatus status;

    if (pack == NULL || stage == NULL || runtime == NULL) {
        return GE_DAM_VISIBILITY_RUNTIME_INVALID_ARGUMENT;
    }
    background_entry = ge_asset_pack_find(pack, stage->background_path);
    bounds_entry = ge_asset_pack_find(pack, stage->room_bounds_path);
    if (background_entry == NULL || bounds_entry == NULL) {
        return GE_DAM_VISIBILITY_RUNTIME_ASSET_NOT_FOUND;
    }
    if (background_entry->data_size == 0U
            || background_entry->data_size > GE_DAM_BACKGROUND_MAX_SIZE
            || background_entry->data_size > SIZE_MAX
            || bounds_entry->data_size < GE_DAM_VISIBILITY_BOUNDS_HEADER_SIZE
            || bounds_entry->data_size
                > GE_DAM_VISIBILITY_BOUNDS_HEADER_SIZE
                    + GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS
                        * GE_DAM_VISIBILITY_BOUNDS_RECORD_SIZE
            || bounds_entry->data_size > SIZE_MAX) {
        return GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET;
    }
    background = malloc((size_t)background_entry->data_size);
    bounds = malloc((size_t)bounds_entry->data_size);
    if (background == NULL || bounds == NULL) {
        free(bounds);
        free(background);
        return GE_DAM_VISIBILITY_RUNTIME_NO_MEMORY;
    }
    if (ge_asset_pack_read(pack, stage->background_path,
            background, (size_t)background_entry->data_size, &bytes_read)
            != GE_ASSET_PACK_OK
            || bytes_read != (size_t)background_entry->data_size
            || ge_asset_pack_read(pack, stage->room_bounds_path,
                bounds, (size_t)bounds_entry->data_size, &bytes_read)
                != GE_ASSET_PACK_OK
            || bytes_read != (size_t)bounds_entry->data_size) {
        free(bounds);
        free(background);
        return GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET;
    }
    status = ge_dam_visibility_runtime_init(background,
        (size_t)background_entry->data_size, bounds,
        (size_t)bounds_entry->data_size, runtime);
    if (status == GE_DAM_VISIBILITY_RUNTIME_OK) {
        runtime->level_index = stage->level_index;
        runtime->level_scale = stage->level_scale;
        runtime->visibility_scale = stage->visibility_scale;
    }
    free(bounds);
    free(background);
    return status;
}

GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_run(
    GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera,
    const float player_position[3],
    GeOriginalBgVisibilityResult *result)
{
    return ge_dam_visibility_runtime_run_with_providers(runtime, camera,
        player_position, NULL, result);
}

GeDamVisibilityRuntimeStatus ge_dam_visibility_runtime_run_with_providers(
    GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera,
    const float player_position[3],
    const GeOriginalBgVisibilityProviders *providers,
    GeOriginalBgVisibilityResult *result)
{
    GeOriginalBgVisibilityInput input;
    GeOriginalBgVisibilityStatus visibility_status;
    int width;
    int height;

    if (runtime == NULL || camera == NULL || player_position == NULL
            || result == NULL || runtime->loaded == 0U) {
        return GE_DAM_VISIBILITY_RUNTIME_INVALID_ARGUMENT;
    }
    width = abs(camera->viewport_scale[0]) / 2;
    height = abs(camera->viewport_scale[1]) / 2;
    if (camera->room >= runtime->room_count || width <= 0 || height <= 0
            || width > INT16_MAX || height > INT16_MAX) {
        return GE_DAM_VISIBILITY_RUNTIME_CAMERA_INVALID;
    }
    memset(&input, 0, sizeof(input));
    input.background = runtime->background;
    input.background_size = runtime->background_size;
    input.room_bounds = runtime->room_bounds;
    input.room_count = runtime->room_count;
    input.level_index = runtime->level_index;
    input.current_room = camera->room;
    memcpy(input.player_position, player_position,
           sizeof(input.player_position));
    memcpy(input.world_to_screen, camera->view,
           sizeof(input.world_to_screen));
    input.level_scale = runtime->level_scale;
    input.visibility_scale = runtime->visibility_scale;
    input.near_distance = runtime->near_distance;
    input.far_distance = runtime->far_distance;
    input.vertical_fov_degrees = 60.0f;
    input.aspect_ratio = (float)width / (float)height;
    input.view_width = (int16_t)width;
    input.view_height = (int16_t)height;
    input.providers = providers;
    input.program = runtime->program;
    visibility_status = ge_original_bg_visibility_run(&input, result);
    runtime->last_original_status = visibility_status;
    return visibility_status == GE_ORIGINAL_BG_VISIBILITY_OK
        ? GE_DAM_VISIBILITY_RUNTIME_OK
        : GE_DAM_VISIBILITY_RUNTIME_VISIBILITY_FAILED;
}

const char *ge_dam_visibility_runtime_status_name(
    GeDamVisibilityRuntimeStatus status)
{
    switch (status) {
    case GE_DAM_VISIBILITY_RUNTIME_OK: return "ok";
    case GE_DAM_VISIBILITY_RUNTIME_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_DAM_VISIBILITY_RUNTIME_ASSET_NOT_FOUND:
        return "asset not found";
    case GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET:
        return "invalid asset";
    case GE_DAM_VISIBILITY_RUNTIME_NO_MEMORY: return "no memory";
    case GE_DAM_VISIBILITY_RUNTIME_CAMERA_INVALID:
        return "camera invalid";
    case GE_DAM_VISIBILITY_RUNTIME_VISIBILITY_FAILED:
        return "visibility failed";
    default: return "unknown";
    }
}

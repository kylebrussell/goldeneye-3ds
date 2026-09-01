#include "ge_stage_assets.h"

#include <stdlib.h>
#include <string.h>

static uint64_t ge_stage_fnv1a64(const uint8_t *data, size_t size);

static GeStageAssetStatus ge_stage_read(
    GeAssetPack *pack, const char *path, size_t expected_size,
    uint64_t expected_fnv1a64, uint8_t **data)
{
    const GeAssetPackEntry *entry = ge_asset_pack_find(pack, path);

    *data = NULL;
    if (entry == NULL) return GE_STAGE_ASSET_NOT_FOUND;
    if (entry->data_size != expected_size || entry->data_size > SIZE_MAX) {
        return GE_STAGE_ASSET_INVALID;
    }
    *data = malloc(expected_size);
    if (*data == NULL) return GE_STAGE_ASSET_NO_MEMORY;
    if (ge_asset_pack_read(pack, path, *data, expected_size, NULL)
            != GE_ASSET_PACK_OK) {
        free(*data);
        *data = NULL;
        return GE_STAGE_ASSET_INVALID;
    }
    if (ge_stage_fnv1a64(*data, expected_size) != expected_fnv1a64) {
        free(*data);
        *data = NULL;
        return GE_STAGE_ASSET_INVALID;
    }
    return GE_STAGE_ASSET_OK;
}

static uint64_t ge_stage_fnv1a64(const uint8_t *data, size_t size)
{
    uint64_t value = UINT64_C(14695981039346656037);
    size_t index;

    for (index = 0U; index < size; ++index) {
        value ^= data[index];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

void ge_stage_assets_close(GeStageResolvedAssets *resolved)
{
    if (resolved == NULL) return;
    free(resolved->collision);
    free(resolved->background);
    memset(resolved, 0, sizeof(*resolved));
}

GeStageAssetStatus ge_stage_assets_resolve(
    GeAssetPack *pack, const GeStageAssetDescriptor *descriptor,
    GeStageResolvedAssets *resolved)
{
    GeStageResolvedAssets next;
    GeStageAssetStatus status;
    const GeAssetPackEntry *entry;
    uint8_t *bounds = NULL;
    uint8_t *setup = NULL;

    if (pack == NULL || descriptor == NULL || resolved == NULL
            || descriptor != ge_stage_asset_descriptor(descriptor->stage)) {
        return GE_STAGE_ASSET_INVALID_ARGUMENT;
    }
    memset(&next, 0, sizeof(next));
    next.descriptor = descriptor;
    status = ge_stage_read(pack, descriptor->background_path,
                           descriptor->expected_background_size,
                           descriptor->expected_background_fnv1a64,
                           &next.background);
    if (status != GE_STAGE_ASSET_OK) goto fail;
    next.background_size = descriptor->expected_background_size;
    if (ge_dam_world_parse(next.background, next.background_size, &next.world)
            != GE_DAM_WORLD_OK
            || next.world.room_count != descriptor->expected_room_count
            || next.world.portal_count != descriptor->expected_portal_count) {
        status = GE_STAGE_ASSET_INVALID;
        goto fail;
    }
    status = ge_stage_read(pack, descriptor->collision_path,
                           descriptor->expected_collision_size,
                           descriptor->expected_collision_fnv1a64,
                           &next.collision);
    if (status != GE_STAGE_ASSET_OK) goto fail;
    next.collision_size = descriptor->expected_collision_size;
    if (ge_stan_collision_open(next.collision, next.collision_size, &next.stan)
            != GE_STAN_COLLISION_OK
            || next.stan.tile_count != descriptor->expected_stan_tiles
            || next.stan.point_count != descriptor->expected_stan_points
            || next.stan.spawn_tile != descriptor->expected_spawn_tile
            || next.stan.spawn_room != descriptor->expected_spawn_room) {
        status = GE_STAGE_ASSET_INVALID;
        goto fail;
    }
    entry = ge_asset_pack_find(pack, descriptor->room_bounds_path);
    if (entry == NULL) {
        status = GE_STAGE_ASSET_NOT_FOUND;
        goto fail;
    }
    if (entry->data_size != descriptor->expected_room_bounds_size) {
        status = GE_STAGE_ASSET_INVALID;
        goto fail;
    }
    status = ge_stage_read(pack, descriptor->room_bounds_path,
                           descriptor->expected_room_bounds_size,
                           descriptor->expected_room_bounds_fnv1a64, &bounds);
    if (status != GE_STAGE_ASSET_OK) goto fail;
    free(bounds);
    bounds = NULL;
    next.room_bounds_entry = entry;
    entry = ge_asset_pack_find(pack, descriptor->setup_path);
    if (entry == NULL) {
        status = GE_STAGE_ASSET_NOT_FOUND;
        goto fail;
    }
    if (entry->data_size != descriptor->expected_setup_size) {
        status = GE_STAGE_ASSET_INVALID;
        goto fail;
    }
    status = ge_stage_read(pack, descriptor->setup_path,
                           descriptor->expected_setup_size,
                           descriptor->expected_setup_fnv1a64, &setup);
    if (status != GE_STAGE_ASSET_OK) goto fail;
    free(setup);
    setup = NULL;
    next.setup_entry = entry;
    next.loaded = 1U;
    *resolved = next;
    return GE_STAGE_ASSET_OK;

fail:
    free(bounds);
    free(setup);
    ge_stage_assets_close(&next);
    return status;
}

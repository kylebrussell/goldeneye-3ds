#include "ge_stage_assets.h"

#include <stdio.h>
#include <string.h>

static const GeStageAssetDescriptor ge_stage_descriptors[GE_STAGE_COUNT] = {
    {
        GE_STAGE_DAM, 33, 13, 0.23363999f, 0.2f, 100.0f,
        "dam", "dam", "UsetupdamZ",
        "Tbg_dam_all_p_stanZ",
        "converted/levels/dam/background.bin",
        "converted/levels/dam/rooms",
        "converted/levels/dam/room_bounds.gebounds",
        "converted/levels/dam/collision/collision.gestan",
        "converted/levels/dam/setup/setup.bin",
        197024U, 122060U, 3368U, 81808U,
        UINT64_C(0x548dab6894be896c),
        UINT64_C(0x65b801e392ac6640),
        UINT64_C(0xeda9cbae2f757d29),
        UINT64_C(0x7b8e848fedee57ff),
        137U, 194U, 2755U, 8366U, 171U, 135U,
    },
    {
        GE_STAGE_FACILITY, 34, 14, 1.20648f, 1.0f, 64.102562f,
        "facility", "ark", "UsetuparkZ",
        "Tbg_ark_all_p_stanZ",
        "converted/levels/facility/background.bin",
        "converted/levels/facility/rooms",
        "converted/levels/facility/room_bounds.gebounds",
        "converted/levels/facility/collision/collision.gestan",
        "converted/levels/facility/collision/setup.bin",
        200576U, 115276U, 1952U, 96160U,
        UINT64_C(0x4d61300bdc56e68d),
        UINT64_C(0xf371ed616821ff1f),
        UINT64_C(0x24a7bf757530fd4f),
        UINT64_C(0xcb7dcf0c0e6d5f55),
        78U, 109U, 2599U, 7908U, 2325U, 13U,
    },
#define GE_SOLO_STAGE(symbol, level, level_index_value, level_scale_value,  \
                      visibility_value, distance_value, key_value, bg_key,  \
                      setup_key_value,                                      \
                      stan_key_value, bg_size, bg_fnv, collision_size,       \
                      collision_fnv, bounds_size, bounds_fnv, setup_size,   \
                      setup_fnv, rooms, portals, tiles, points, spawn_tile, \
                      spawn_room)                                           \
    {                                                                        \
        symbol, level, level_index_value, level_scale_value,                 \
        visibility_value, distance_value, key_value, bg_key,                \
        setup_key_value, stan_key_value,                                    \
        "converted/levels/" key_value "/background.bin",                  \
        "converted/levels/" key_value "/rooms",                           \
        "converted/levels/" key_value "/room_bounds.gebounds",            \
        "converted/levels/" key_value "/collision/collision.gestan",      \
        "converted/levels/" key_value "/collision/setup.bin",             \
        bg_size, collision_size, bounds_size, setup_size,                    \
        bg_fnv, collision_fnv, bounds_fnv, setup_fnv,                        \
        rooms, portals, tiles, points, spawn_tile, spawn_room,               \
    },
#include "ge_solo_stage_registry.inc"
#undef GE_SOLO_STAGE
    {
        GE_STAGE_CUBA, 54, 34, 0.094662853f, 1.0f, 6.6844921f,
        "cuba", "len", "UsetuplenZ", "Tbg_len_all_p_stanZ",
        "converted/levels/cuba/background.bin",
        "converted/levels/cuba/rooms",
        "converted/levels/cuba/room_bounds.gebounds",
        "converted/levels/cuba/collision/collision.gestan",
        "converted/levels/cuba/collision/setup.bin",
        4000U, 9096U, 128U, 9328U,
        UINT64_C(0x01ff3651d304ad98),
        UINT64_C(0x4bdb5808ac06041c),
        UINT64_C(0x519d24a908b00531),
        UINT64_C(0xad4e3f13ae9aa9b9),
        2U, 0U, 206U, 618U, 89U, 1U,
    },
};

const GeStageAssetDescriptor *ge_stage_asset_descriptor(GeStageId stage)
{
    return (unsigned)stage < (unsigned)GE_STAGE_COUNT
        ? &ge_stage_descriptors[(size_t)stage] : NULL;
}

const GeStageAssetDescriptor *ge_stage_asset_descriptor_by_key(const char *key)
{
    size_t index;

    if (key == NULL) return NULL;
    for (index = 0U; index < GE_STAGE_COUNT; ++index) {
        if (strcmp(ge_stage_descriptors[index].key, key) == 0)
            return &ge_stage_descriptors[index];
    }
    return NULL;
}

const GeStageAssetDescriptor *ge_stage_asset_descriptor_by_level_id(
    int32_t level_id)
{
    size_t index;

    for (index = 0U; index < GE_STAGE_COUNT; ++index) {
        if (ge_stage_descriptors[index].level_id == level_id)
            return &ge_stage_descriptors[index];
    }
    return NULL;
}

const GeStageAssetDescriptor *ge_stage_asset_dam(void)
{
    return ge_stage_asset_descriptor(GE_STAGE_DAM);
}

const GeStageAssetDescriptor *ge_stage_asset_facility(void)
{
    return ge_stage_asset_descriptor(GE_STAGE_FACILITY);
}

GeStageAssetStatus ge_stage_asset_room_path(
    const GeStageAssetDescriptor *descriptor, uint8_t room,
    GeStageRoomStream stream, char *path, size_t path_capacity)
{
    const char *filename;
    int length;

    if (descriptor == NULL || path == NULL || path_capacity == 0U
            || (size_t)room >= descriptor->expected_room_count) {
        return GE_STAGE_ASSET_INVALID_ARGUMENT;
    }
    switch (stream) {
    case GE_STAGE_ROOM_POINTS: filename = "point_table.bin"; break;
    case GE_STAGE_ROOM_PRIMARY_GDL: filename = "primary_gdl.bin"; break;
    case GE_STAGE_ROOM_SECONDARY_GDL: filename = "secondary_gdl.bin"; break;
    default: return GE_STAGE_ASSET_INVALID_ARGUMENT;
    }
    length = snprintf(path, path_capacity, "%s/room%03u/%s",
                      descriptor->rooms_prefix, (unsigned)room, filename);
    return length < 0 || (size_t)length >= path_capacity
        ? GE_STAGE_ASSET_PATH_TOO_LONG : GE_STAGE_ASSET_OK;
}

const char *ge_stage_asset_status_name(GeStageAssetStatus status)
{
    switch (status) {
    case GE_STAGE_ASSET_OK: return "ok";
    case GE_STAGE_ASSET_INVALID_ARGUMENT: return "invalid argument";
    case GE_STAGE_ASSET_NOT_FOUND: return "asset not found";
    case GE_STAGE_ASSET_INVALID: return "invalid asset";
    case GE_STAGE_ASSET_NO_MEMORY: return "no memory";
    case GE_STAGE_ASSET_PATH_TOO_LONG: return "path too long";
    default: return "unknown";
    }
}

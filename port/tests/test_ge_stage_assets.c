#include "ge_stage_assets.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check_stage(GeAssetPack *pack,
                        const GeStageAssetDescriptor *descriptor,
                        size_t rooms, size_t portals,
                        uint32_t tiles, uint32_t points,
                        uint32_t spawn_tile, uint8_t spawn_room)
{
    GeStageResolvedAssets resolved;
    GeStageAssetStatus status = ge_stage_assets_resolve(
        pack, descriptor, &resolved);

    if (status != GE_STAGE_ASSET_OK) {
        fprintf(stderr, "%s: %s\n", descriptor->key,
                ge_stage_asset_status_name(status));
    }
    assert(status == GE_STAGE_ASSET_OK);
    assert(resolved.loaded != 0U);
    assert(resolved.world.room_count == rooms);
    assert(resolved.world.portal_count == portals);
    assert(resolved.stan.tile_count == tiles);
    assert(resolved.stan.point_count == points);
    assert(resolved.stan.spawn_tile == spawn_tile);
    assert(resolved.stan.spawn_room == spawn_room);
    assert(resolved.setup_entry != NULL);
    assert(resolved.setup_entry->data_size == descriptor->expected_setup_size);
    assert(resolved.room_bounds_entry != NULL);
    ge_stage_assets_close(&resolved);
    assert(resolved.loaded == 0U && resolved.background == NULL
           && resolved.collision == NULL);
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    const GeStageAssetDescriptor *dam = ge_stage_asset_dam();
    const GeStageAssetDescriptor *facility = ge_stage_asset_facility();
    const GeStageAssetDescriptor *surface1;
    const GeStageAssetDescriptor *surface2;
    char path[GE_STAGE_ASSET_PATH_CAPACITY];
    size_t stage;

    assert(argc == 2);
    assert(dam != NULL && dam->stage == GE_STAGE_DAM);
    assert(dam->level_id == 33 && strcmp(dam->key, "dam") == 0
           && strcmp(dam->decomp_key, "dam") == 0);
    assert(dam->level_index == 13 && dam->level_scale == 0.23363999f);
    assert(dam->visibility_scale == 0.2f
           && dam->visibility_distance == 100.0f);
    assert(facility != NULL && facility->stage == GE_STAGE_FACILITY);
    assert(facility->level_id == 34 && strcmp(facility->key, "facility") == 0
           && strcmp(facility->decomp_key, "ark") == 0);
    assert(facility->level_index == 14
           && facility->level_scale == 1.20648f);
    assert(facility->visibility_scale == 1.0f
           && facility->visibility_distance == 64.102562f);
    assert(GE_STAGE_COUNT == 21);
    assert(ge_stage_asset_descriptor(GE_STAGE_COUNT) == NULL);
    assert(ge_stage_asset_descriptor((GeStageId)-1) == NULL);
    assert(ge_stage_asset_descriptor_by_key("dam") == dam);
    assert(ge_stage_asset_descriptor_by_key("not-a-stage") == NULL);
    assert(ge_stage_asset_descriptor_by_level_id(34) == facility);
    {
        const GeStageAssetDescriptor *cuba =
            ge_stage_asset_descriptor_by_level_id(54);
        assert(cuba != NULL && cuba->stage == GE_STAGE_CUBA);
        assert(strcmp(cuba->key, "cuba") == 0
            && strcmp(cuba->decomp_key, "len") == 0
            && strcmp(cuba->setup_key, "UsetuplenZ") == 0
            && strcmp(cuba->stan_key, "Tbg_len_all_p_stanZ") == 0);
        assert(cuba->expected_room_count == 2U
            && cuba->expected_portal_count == 0U
            && cuba->expected_stan_tiles == 206U
            && cuba->expected_stan_points == 618U
            && cuba->expected_spawn_tile == 89U
            && cuba->expected_spawn_room == 1U);
    }
    assert(ge_stage_asset_descriptor_by_level_id(-1) == NULL);
    surface1 = ge_stage_asset_descriptor_by_key("surface1");
    surface2 = ge_stage_asset_descriptor_by_key("surface2");
    assert(surface1 != NULL && surface2 != NULL);
    assert(surface1->level_id == 36 && surface2->level_id == 43);
    assert(strcmp(surface1->decomp_key, "sevx") == 0
           && strcmp(surface2->decomp_key, "sevx") == 0);
    assert(strcmp(surface1->stan_key, surface2->stan_key) == 0);
    assert(strcmp(surface1->setup_key, surface2->setup_key) != 0);
    assert(surface1->expected_background_fnv1a64
           == surface2->expected_background_fnv1a64);
    assert(surface1->expected_collision_fnv1a64
           == surface2->expected_collision_fnv1a64);
    assert(surface1->expected_setup_fnv1a64
           != surface2->expected_setup_fnv1a64);
    assert(ge_stage_asset_room_path(dam, 135U, GE_STAGE_ROOM_POINTS,
        path, sizeof(path)) == GE_STAGE_ASSET_OK);
    assert(strcmp(path,
        "converted/levels/dam/rooms/room135/point_table.bin") == 0);
    assert(ge_stage_asset_room_path(facility, 13U,
        GE_STAGE_ROOM_PRIMARY_GDL, path, sizeof(path)) == GE_STAGE_ASSET_OK);
    assert(strcmp(path,
        "converted/levels/facility/rooms/room013/primary_gdl.bin") == 0);
    assert(ge_stage_asset_room_path(facility, 78U, GE_STAGE_ROOM_POINTS,
        path, sizeof(path)) == GE_STAGE_ASSET_INVALID_ARGUMENT);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    for (stage = 0U; stage < GE_STAGE_COUNT; ++stage) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage);
        assert(descriptor != NULL);
        assert(descriptor->level_index >= 0
               && descriptor->level_scale > 0.0f
               && descriptor->visibility_scale > 0.0f
               && descriptor->visibility_distance > 0.0f);
        assert(ge_stage_asset_descriptor_by_key(descriptor->key) == descriptor);
        assert(ge_stage_asset_descriptor_by_level_id(descriptor->level_id)
               == descriptor);
        check_stage(&pack, descriptor, descriptor->expected_room_count,
                    descriptor->expected_portal_count,
                    descriptor->expected_stan_tiles,
                    descriptor->expected_stan_points,
                    descriptor->expected_spawn_tile,
                    descriptor->expected_spawn_room);
    }
    ge_asset_pack_close(&pack);
    puts("all 20 solo missions plus authored Cuba credits stage resolved");
    return 0;
}

#include "ge_asset_pack.h"
#include "ge_dam_dynamic_scene.h"
#include "ge_dam_preload_queue.h"
#include "ge_dam_world.h"
#include "ge_stage_assets.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    static const GeDamDynamicSceneLimits limits = {10U, 65536U, 65536U};
    const GeStageAssetDescriptor *stage;
    GeAssetPack pack;
    GeStageResolvedAssets assets;
    GeDamDynamicScene scene;
    GeDamPreloadQueue queue;
    uint8_t initial[10];
    uint8_t rendered[1];
    uint8_t requested = 0U;
    size_t initial_count = 0U;
    size_t room;
    size_t tick;
    uint64_t generation;

    assert(argc == 2);
    stage = ge_stage_asset_descriptor_by_key("silo");
    assert(stage != NULL && stage->level_id == 20);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    assert(ge_stage_assets_resolve(&pack, stage, &assets)
           == GE_STAGE_ASSET_OK);
    assert(ge_dam_world_collect_connected(
        &assets.world, stage->expected_spawn_room, initial, sizeof(initial),
        &initial_count) == GE_DAM_WORLD_OK);
    assert(initial_count == 10U && initial[0] == 26U);
    assert(ge_dam_dynamic_scene_init_for_stage(
        &scene, &pack, stage, &assets.world, initial, initial_count, &limits)
        == GE_DAM_DYNAMIC_SCENE_OK);
    assert(ge_dam_preload_queue_init(
        &queue, assets.world.room_count, GE_DAM_PRELOAD_MAX_ROOMS,
        initial, initial_count) == GE_DAM_PRELOAD_OK);

    rendered[0] = initial[0];
    for (tick = 0U; tick < 3U; ++tick) {
        assert(ge_dam_dynamic_scene_tick_visibility(
            &scene, &queue, rendered, 1U) == GE_DAM_DYNAMIC_SCENE_OK);
    }
    assert(scene.room_count == 10U && scene.room_age[rendered[0]] == 1U);
    assert(scene.room_age[initial[1]] == 4U);
    for (room = 1U; room < assets.world.room_count; ++room) {
        if (!ge_dam_dynamic_scene_is_resident(&scene, (uint8_t)room)) {
            requested = (uint8_t)room;
            break;
        }
    }
    assert(requested != 0U);
    assert(ge_dam_preload_queue_request(&queue, requested) != 0U);
    generation = scene.generation;
    assert(ge_dam_dynamic_scene_tick_visibility(
        &scene, &queue, rendered, 1U) == GE_DAM_DYNAMIC_SCENE_OK);
    assert(scene.generation == generation + 1U);
    assert(scene.room_count == 2U && scene.scene.room_count == 2U);
    assert(scene.scene.vertex_count < limits.vertex_capacity);
    assert(scene.scene.batch_count < limits.batch_capacity);
    assert(ge_dam_dynamic_scene_is_resident(&scene, rendered[0]));
    assert(ge_dam_dynamic_scene_is_resident(&scene, requested));
    assert(!ge_dam_dynamic_scene_is_resident(&scene, initial[1]));
    assert(scene.eviction_attempts == 1U && scene.eviction_successes == 1U
           && scene.eviction_failures == 0U && scene.rooms_evicted == 9U);
    assert(queue.eviction_count == 9U && queue.pending_count == 0U
           && queue.loading_count == 0U);
    assert(ge_dam_preload_queue_room_state(&queue, requested)
           == GE_DAM_PRELOAD_ROOM_RESIDENT);
    assert(ge_dam_preload_queue_room_state(&queue, initial[1])
           == GE_DAM_PRELOAD_ROOM_UNLOADED);

    ge_dam_dynamic_scene_close(&scene);
    ge_stage_assets_close(&assets);

    /* Exact initial Streets resident-object expansion from the canonical
     * setup/Pitem provider is 1,044 vertices and 99 batches. Exercise a real
     * room replacement with that retained overlay: the publication stays
     * inside the live cap and its rebuild source aliases the combined scene
     * tail instead of retaining a second multi-megabyte vertex copy. */
    stage = ge_stage_asset_descriptor_by_key("streets");
    initial_count = 0U;
    assert(stage != NULL
           && ge_stage_assets_resolve(&pack, stage, &assets)
                == GE_STAGE_ASSET_OK
           && ge_dam_world_collect_connected(
                &assets.world, stage->expected_spawn_room,
                initial, sizeof(initial), &initial_count) == GE_DAM_WORLD_OK
           && initial_count == 10U && initial[0] == 7U
           && ge_dam_dynamic_scene_init_for_stage(
                &scene, &pack, stage, &assets.world,
                initial, initial_count, &limits)
                == GE_DAM_DYNAMIC_SCENE_OK);
    {
        GeDamRoomWorldVertex *overlay_vertices = calloc(
            1044U, sizeof(*overlay_vertices));
        GeDamRoomDrawBatch *overlay_batches = calloc(
            99U, sizeof(*overlay_batches));
        assert(overlay_vertices != NULL && overlay_batches != NULL);
        overlay_batches[0].room_id = 7U;
        overlay_batches[0].vertex_count = 1044U;
        overlay_batches[0].triangle_count = 348U;
        overlay_batches[0].coordinate_space =
            GE_DAM_ROOM_COORDINATE_RUNTIME;
        assert(ge_dam_dynamic_scene_set_overlay(
            &scene, overlay_vertices, 1044U, overlay_batches, 99U)
            == GE_DAM_DYNAMIC_SCENE_OK);
        free(overlay_batches);
        free(overlay_vertices);
    }
    assert(scene.scene.vertex_count == 11241U
           && scene.scene.batch_count == 1043U
           && scene.overlay_vertices
                == scene.vertices + scene.scene.vertex_count
                    - scene.overlay_vertex_count);
    assert(ge_dam_preload_queue_init(
        &queue, assets.world.room_count, GE_DAM_PRELOAD_MAX_ROOMS,
        initial, initial_count) == GE_DAM_PRELOAD_OK);
    rendered[0] = initial[0];
    for (tick = 0U; tick < 4U; ++tick)
        assert(ge_dam_dynamic_scene_age_visibility(
            &scene, rendered, 1U) == GE_DAM_DYNAMIC_SCENE_OK);
    requested = 0U;
    for (room = 1U; room < assets.world.room_count; ++room)
        if (!ge_dam_dynamic_scene_is_resident(&scene, (uint8_t)room)) {
            requested = (uint8_t)room;
            break;
        }
    assert(requested != 0U
           && ge_dam_preload_queue_request(&queue, requested) != 0U
           && ge_dam_dynamic_scene_install_next(&scene, &queue)
                == GE_DAM_DYNAMIC_SCENE_OK
           && scene.overlay_vertex_count == 1044U
           && scene.overlay_batch_count == 99U
           && scene.overlay_vertices
                == scene.vertices + scene.scene.vertex_count
                    - scene.overlay_vertex_count);
    ge_dam_dynamic_scene_close(&scene);
    ge_stage_assets_close(&assets);
    ge_asset_pack_close(&pack);
    puts("Silo eviction and Streets exact resident overlay streaming passed");
    return 0;
}

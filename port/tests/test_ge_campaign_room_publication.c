#include "ge_original_bg_geometry_internal.h"
#include "ge_dam_dynamic_scene.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The retained native light-break slice links this provider. This test never
 * invokes light effects; it audits renderer publication against that owner. */
void glassCreateShard(coord3d *position, float minimum, float maximum)
{ (void)position; (void)minimum; (void)maximum; assert(!"unexpected shard"); }

static void *read_asset(GeAssetPack *pack, const char *path, size_t *size)
{
    const GeAssetPackEntry *entry = ge_asset_pack_find(pack, path);
    assert(entry && entry->data_size <= SIZE_MAX);
    *size = (size_t)entry->data_size;
    void *bytes = malloc(*size);
    assert(bytes && ge_asset_pack_read(pack, path, bytes, *size, NULL)
        == GE_ASSET_PACK_OK);
    return bytes;
}

static size_t publications;
static void verify(GeOriginalBgGeometry *owner, GeDamDynamicScene *scene)
{
    size_t changed = 0;
    assert(scene->scene.vertex_count >= scene->overlay_vertex_count);
    assert(scene->scene.batch_count >= scene->overlay_batch_count);
    assert(ge_original_bg_geometry_apply_scene(owner, scene->vertices,
        scene->scene.vertex_count - scene->overlay_vertex_count, scene->batches,
        scene->scene.batch_count - scene->overlay_batch_count, &changed));
    assert(changed == 0);
    ++publications;
}

static void overlay(GeOriginalBgGeometry *owner, GeDamDynamicScene *scene,
    size_t iteration)
{
    GeDamRoomWorldVertex vertices[24] = {0};
    GeDamRoomDrawBatch batches[8] = {0};
    size_t count = iteration % 2 ? 24 : 6;
    for (size_t i = 0; i < 8; ++i) {
        batches[i].first_vertex = i * 3;
        batches[i].vertex_count = 3;
        batches[i].triangle_count = 1;
        batches[i].coordinate_space = GE_DAM_ROOM_COORDINATE_RUNTIME;
        batches[i].room_id = scene->room_ids[0];
    }
    for (size_t i = 0; i < 24; ++i) vertices[i].world[0] = (float)i;
    if (!scene->overlay_vertex_count)
        assert(ge_dam_dynamic_scene_set_overlay(scene, vertices, count,
            batches, count / 3) == GE_DAM_DYNAMIC_SCENE_OK);
    else
        assert(ge_dam_dynamic_scene_replace_overlay_segment(scene, 0,
            scene->overlay_vertex_count, vertices, count, 0,
            scene->overlay_batch_count, batches, count / 3)
                == GE_DAM_DYNAMIC_SCENE_OK);
    verify(owner, scene);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    GeAssetPack pack;
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    size_t total_rooms = 0, total_evictions = 0;
    for (size_t stage_id = 0; stage_id < GE_STAGE_COUNT; ++stage_id) {
        const GeStageAssetDescriptor *stage = ge_stage_asset_descriptor((GeStageId)stage_id);
        size_t size;
        void *bytes = read_asset(&pack, stage->background_path, &size);
        GeDamWorld world;
        assert(ge_dam_world_parse(bytes, size, &world) == GE_DAM_WORLD_OK);
        free(bytes);
        bytes = read_asset(&pack, stage->room_bounds_path, &size);
        assert(size == 80 + world.room_count * 24);
        GeOriginalBgRoomBounds bounds[GE_BG_GEOMETRY_MAX_ROOMS];
        memcpy(bounds, (unsigned char *)bytes + 80, world.room_count * 24);
        free(bytes);
        GeOriginalBgGeometryReport report;
        GeOriginalBgGeometry *owner = ge_original_bg_geometry_load(
            &pack, stage, &world, bounds, &report);
        assert(owner);
        ge_original_bg_geometry_bind(owner);
        uint8_t first = 1;
        const GeDamDynamicSceneLimits limits = {10, 65536, 65536};
        GeDamDynamicScene scene;
        GeDamPreloadQueue queue;
        assert(ge_dam_dynamic_scene_init_for_stage(&scene, &pack, stage,
            &world, &first, 1, &limits) == GE_DAM_DYNAMIC_SCENE_OK);
        assert(ge_dam_preload_queue_init(&queue, world.room_count,
            GE_DAM_PRELOAD_MAX_ROOMS, &first, 1) == GE_DAM_PRELOAD_OK);
        verify(owner, &scene);
        if (scene.scene.vertex_count != 0) {
            /* A changed authored position must fail provenance validation,
             * even when batch offsets/counts and room identity still match. */
            const float saved_x = scene.vertices[0].world[0];
            size_t changed = 0;
            scene.vertices[0].world[0] += 1.0f;
            assert(!ge_original_bg_geometry_apply_scene(owner, scene.vertices,
                scene.scene.vertex_count, scene.batches, scene.scene.batch_count,
                &changed));
            scene.vertices[0].world[0] = saved_x;
            verify(owner, &scene);
        }
        /* Deliberately visit every room, including disconnected and empty
         * logic rooms. This tests publication, not an invented player route.
         * Four exact unload ticks retain one peer and delete older rooms;
         * the requested room commits with those evictions atomically. */
        for (size_t room = 2; room < world.room_count; ++room) {
            uint8_t keep = scene.room_ids[scene.room_count - 1];
            ge_dam_preload_queue_begin_visibility(&queue);
            for (size_t tick = 0; tick < 3; ++tick) {
                assert(ge_dam_dynamic_scene_age_visibility(&scene, &keep, 1)
                    == GE_DAM_DYNAMIC_SCENE_OK);
                verify(owner, &scene);
            }
            assert(ge_dam_preload_queue_request(&queue, (uint8_t)room));
            assert(ge_dam_dynamic_scene_tick_visibility(&scene, &queue, &keep, 1)
                == GE_DAM_DYNAMIC_SCENE_OK);
            assert(ge_dam_dynamic_scene_is_resident(&scene, (uint8_t)room));
            verify(owner, &scene);
            overlay(owner, &scene, room);
        }
        printf("%s: %zu authored rooms, %llu evictions, %llu room reuses, %llu overlay replacements\n",
            stage->key, world.room_count - 1,
            (unsigned long long)scene.rooms_evicted,
            (unsigned long long)scene.room_geometry_reuses,
            (unsigned long long)(scene.overlay_inplace_replacements
                + scene.overlay_allocating_replacements));
        total_rooms += world.room_count - 1;
        total_evictions += scene.rooms_evicted;
        ge_dam_dynamic_scene_close(&scene);
        ge_original_bg_geometry_close(owner);
    }
    printf("PASS campaign stages=%u rooms=%zu evictions=%zu publication_checks=%zu\n",
        GE_STAGE_COUNT, total_rooms, total_evictions, publications);
    ge_asset_pack_close(&pack);
    return 0;
}

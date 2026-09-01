#include "ge_dam_dynamic_scene.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

static FileData read_file(const char *path)
{
    FileData result = {NULL, 0U};
    FILE *stream = fopen(path, "rb");
    long length;

    assert(stream != NULL);
    assert(fseek(stream, 0L, SEEK_END) == 0);
    length = ftell(stream);
    assert(length > 0L);
    assert(fseek(stream, 0L, SEEK_SET) == 0);
    result.size = (size_t)length;
    result.bytes = malloc(result.size);
    assert(result.bytes != NULL);
    assert(fread(result.bytes, 1U, result.size, stream) == result.size);
    assert(fclose(stream) == 0);
    return result;
}

int main(int argc, char **argv)
{
    static const uint8_t initial[] = {
        135U, 133U, 134U, 132U, 136U, 124U, 125U, 126U, 127U, 128U,
    };
    static const uint8_t resident_after_install[] = {
        135U, 133U, 134U, 132U, 136U, 124U, 125U, 126U, 127U, 128U, 1U,
    };
    const GeDamDynamicSceneLimits limits = {137U, 65536U, 8192U};
    const GeDamDynamicSceneLimits ten_room_limits = {10U, 65536U, 8192U};
    FileData background;
    GeAssetPack pack;
    GeDamWorld world;
    GeDamPreloadQueue queue;
    GeDamDynamicScene cache;
    GeDamDynamicScene bounded;
    GeDamRoomWorldVertex *old_vertices;
    GeDamRoomDrawBatch *old_batches;
    GeDamDynamicSceneTransaction transaction;
    GeDamRoomWorldVertex overlay_vertices[6] = {0};
    GeDamRoomDrawBatch overlay_batches[2] = {0};
    uint64_t old_generation;

    assert(argc == 3);
    background = read_file(argv[1]);
    assert(ge_dam_world_parse(background.bytes, background.size, &world)
        == GE_DAM_WORLD_OK);
    assert(ge_asset_pack_open(&pack, argv[2]) == GE_ASSET_PACK_OK);
    assert(ge_dam_dynamic_scene_init(&cache, &pack, &world,
        initial, sizeof(initial), &limits) == GE_DAM_DYNAMIC_SCENE_OK);
    assert(cache.room_count == sizeof(initial));
    assert(cache.scene.room_count == sizeof(initial));
    assert(cache.scene.vertex_count == 9129U);
    assert(ge_dam_dynamic_scene_is_resident(&cache, 135U));
    assert(!ge_dam_dynamic_scene_is_resident(&cache, 1U));
    overlay_batches[0].room_id = 135U;
    overlay_batches[0].vertex_count = 3U;
    overlay_batches[0].triangle_count = 1U;
    overlay_batches[1] = overlay_batches[0];
    overlay_batches[1].first_vertex = 3U;
    assert(ge_dam_dynamic_scene_set_overlay(
        &cache, overlay_vertices, 6U, overlay_batches, 2U)
        == GE_DAM_DYNAMIC_SCENE_OK);
    assert(cache.generation == 1U);
    assert(cache.overlay_vertex_count == 6U
           && cache.overlay_batch_count == 2U);
    assert(cache.overlay_vertices
           == cache.vertices + cache.scene.vertex_count
                - cache.overlay_vertex_count);
    assert(cache.scene.vertex_count == 9135U);
    assert(cache.scene.batch_count == cache.overlay_batch_count + 874U);
    assert(cache.batches[cache.scene.batch_count - 1U].first_vertex == 9132U);
    {
        GeDamRoomWorldVertex replacement[6] = {0};
        GeDamRoomDrawBatch replacement_batches[2] = {
            overlay_batches[0], overlay_batches[0]
        };
        GeDamRoomWorldVertex restore_vertices[3] = {0};
        GeDamRoomDrawBatch restore_batch = overlay_batches[0];
        const GeDamRoomWorldVertex room_vertex_before = cache.vertices[0];
        uint64_t generation = cache.generation;

        replacement[0].world[0] = 71.0f;
        replacement[3].world[0] = 72.0f;
        replacement_batches[1].first_vertex = 3U;
        assert(ge_dam_dynamic_scene_replace_overlay_segment(
            &cache, 3U, 3U, replacement, 6U, 1U, 1U,
            replacement_batches, 2U) == GE_DAM_DYNAMIC_SCENE_OK);
        assert(cache.generation == generation + 1U);
        assert(cache.overlay_vertex_count == 9U
                && cache.overlay_batch_count == 3U);
        assert(cache.scene.vertex_count == 9138U
                && cache.scene.batch_count == 877U);
        assert(cache.overlay_vertices[0].world[0] == 0.0f
                && cache.overlay_vertices[3].world[0] == 71.0f
                && cache.overlay_vertices[6].world[0] == 72.0f);
        assert(cache.overlay_batches[1].first_vertex == 3U
                && cache.overlay_batches[2].first_vertex == 6U);
        assert(cache.batches[cache.scene.batch_count - 1U].first_vertex
                == 9135U);
        assert(memcmp(&cache.vertices[0], &room_vertex_before,
                      sizeof(room_vertex_before)) == 0);

        generation = cache.generation;
        assert(ge_dam_dynamic_scene_replace_overlay_segment(
            &cache, 3U, 6U, restore_vertices, 3U, 1U, 2U,
            &restore_batch, 1U) == GE_DAM_DYNAMIC_SCENE_OK);
        assert(cache.generation == generation + 1U);
        assert(cache.overlay_vertex_count == 6U
                && cache.overlay_batch_count == 2U);
        assert(cache.scene.vertex_count == 9135U
                && cache.scene.batch_count == 876U);
        assert(cache.batches[cache.scene.batch_count - 1U].first_vertex
                == 9132U);
    }
    {
        const GeDamRoomWorldVertex room_vertex_before = cache.vertices[0];
        GeDamRoomWorldVertex replacement[3] = {0};
        GeDamRoomDrawBatch replacement_batch = overlay_batches[0];
        GeDamRoomWorldVertex *published_vertices = cache.vertices;
        GeDamRoomDrawBatch *published_batches = cache.batches;
        uint64_t generation = cache.generation;
        replacement[0].world[0] = 11.0f;
        replacement[1].world[1] = 22.0f;
        replacement[2].world[2] = 33.0f;
        assert(ge_dam_dynamic_scene_update_overlay_segment(
            &cache, 0U, replacement, 3U, 0U,
            &replacement_batch, 1U) == GE_DAM_DYNAMIC_SCENE_OK);
        assert(cache.vertices == published_vertices
                && cache.batches == published_batches);
        assert(cache.generation == generation + 1U);
        assert(cache.overlay_update_attempts == 3U
                && cache.overlay_update_successes == 3U
                && cache.overlay_update_failures == 0U);
        assert(cache.overlay_vertices[0].world[0] == 11.0f
                && cache.vertices[9129U].world[0] == 11.0f);
        assert(memcmp(&cache.vertices[0], &room_vertex_before,
                      sizeof(room_vertex_before)) == 0);
        generation = cache.generation;
        replacement_batch.first_vertex = 3U;
        replacement_batch.vertex_count = 1U;
        assert(ge_dam_dynamic_scene_update_overlay_segment(
            &cache, 0U, replacement, 3U, 0U,
            &replacement_batch, 1U)
            == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        assert(cache.generation == generation
                && cache.vertices == published_vertices
                && cache.batches == published_batches);
        assert(cache.overlay_vertices[0].world[0] == 11.0f
                && cache.overlay_update_failures == 1U);
        assert(memcmp(&cache.vertices[0], &room_vertex_before,
                      sizeof(room_vertex_before)) == 0);
    }
    {
        GeDamRoomWorldVertex replacement[3] = {0};
        GeDamRoomDrawBatch replacement_batch = overlay_batches[0];
        const uint64_t generation = cache.generation;

        replacement[0].world[0] = 44.0f;
        replacement[1].world[1] = 55.0f;
        replacement[2].world[2] = 66.0f;
        assert(ge_dam_dynamic_scene_update_overlay_segment(
            &cache, 3U, replacement, 3U, 1U,
            &replacement_batch, 1U) == GE_DAM_DYNAMIC_SCENE_OK);
        assert(cache.generation == generation + 1U);
        assert(cache.overlay_vertices[0].world[0] == 11.0f);
        assert(cache.overlay_vertices[3].world[0] == 44.0f
                && cache.vertices[9132U].world[0] == 44.0f);
        assert(cache.overlay_batches[1].first_vertex == 3U);
        assert(cache.batches[cache.scene.batch_count - 1U].first_vertex
                == 9132U);
        assert(cache.overlay_update_attempts == 5U
                && cache.overlay_update_successes == 4U
                && cache.overlay_update_failures == 1U);
    }
    {
        const size_t published_batch = cache.scene.batch_count
            - cache.overlay_batch_count;
        const uint64_t generation = cache.generation;
        const size_t attempts = cache.overlay_update_attempts;

        cache.overlay_vertices[0].processed.rgba[0] = 0x5aU;
        cache.overlay_batches[0].texture.texture_id = UINT16_C(321);
        assert(cache.vertices[9129U].processed.rgba[0] == 0x5aU);
        assert(cache.batches[published_batch].texture.texture_id != 321U);
        assert(ge_dam_dynamic_scene_commit_overlay_batches(
            &cache, 0U, 1U) == GE_DAM_DYNAMIC_SCENE_OK);
        assert(cache.generation == generation + 1U);
        assert(cache.overlay_update_attempts == attempts + 1U
                && cache.overlay_update_successes == 5U
                && cache.overlay_update_failures == 1U);
        assert(cache.batches[published_batch].texture.texture_id == 321U);
        assert(cache.batches[published_batch].first_vertex == 9129U);
        assert(ge_dam_dynamic_scene_commit_overlay_batches(
            &cache, 2U, 1U) == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        assert(cache.overlay_update_failures == 2U);
    }
    assert(ge_dam_preload_queue_init(&queue, 137U, 137U,
        initial, sizeof(initial)) == GE_DAM_PRELOAD_OK);
    assert(ge_dam_preload_queue_request(&queue, 1U) != 0U);
    old_vertices = cache.vertices;
    old_batches = cache.batches;
    old_generation = cache.generation;
    assert(ge_dam_dynamic_scene_install_next(&cache, &queue)
        == GE_DAM_DYNAMIC_SCENE_OK);
    assert(cache.generation == old_generation + 1U
           && cache.room_count == 11U);
    assert(cache.scene.room_count == 11U);
    assert(cache.scene.vertex_count == 9210U);
    assert(cache.overlay_vertices
           == cache.vertices + cache.scene.vertex_count
                - cache.overlay_vertex_count);
    assert(cache.batches[cache.scene.batch_count - 1U].first_vertex == 9207U);
    assert(cache.vertices != old_vertices || cache.batches != old_batches);
    assert(ge_dam_dynamic_scene_is_resident(&cache, 1U));
    assert(ge_dam_preload_queue_room_state(&queue, 1U)
        == GE_DAM_PRELOAD_ROOM_RESIDENT);
    assert(queue.pending_count == 0U && queue.loading_count == 0U);

    assert(ge_dam_preload_queue_init(&queue, 137U, 137U,
        resident_after_install, sizeof(resident_after_install))
        == GE_DAM_PRELOAD_OK);
    assert(ge_dam_preload_queue_request(&queue, 2U) != 0U);
    old_vertices = cache.vertices;
    old_batches = cache.batches;
    old_generation = cache.generation;
    assert(ge_dam_dynamic_scene_prepare_next(&cache, &queue, &transaction)
        == GE_DAM_DYNAMIC_SCENE_OK);
    assert(transaction.prepared != 0U && transaction.room == 2U);
    assert(transaction.scene.room_count == 12U);
    assert(cache.vertices == old_vertices && cache.batches == old_batches);
    assert(cache.generation == old_generation && cache.room_count == 11U);
    assert(ge_dam_preload_queue_room_state(&queue, 2U)
        == GE_DAM_PRELOAD_ROOM_QUEUED);
    ge_dam_dynamic_scene_abort(&cache, &transaction);
    assert(transaction.prepared == 0U);
    assert(cache.vertices == old_vertices && cache.batches == old_batches);
    assert(cache.generation == old_generation && cache.room_count == 11U);
    assert(ge_dam_preload_queue_room_state(&queue, 2U)
        == GE_DAM_PRELOAD_ROOM_QUEUED);

    assert(ge_dam_preload_queue_init(&queue, 137U, 137U,
        resident_after_install, sizeof(resident_after_install))
        == GE_DAM_PRELOAD_OK);
    old_vertices = cache.vertices;
    old_batches = cache.batches;
    old_generation = cache.generation;
    assert(ge_dam_preload_queue_request(&queue, 0U) != 0U);
    assert(ge_dam_dynamic_scene_install_next(&cache, &queue)
        == GE_DAM_DYNAMIC_SCENE_ASSET_NOT_FOUND);
    assert(cache.vertices == old_vertices && cache.batches == old_batches);
    assert(cache.generation == old_generation && cache.room_count == 11U);
    assert(!ge_dam_dynamic_scene_is_resident(&cache, 0U));
    assert(ge_dam_preload_queue_room_state(&queue, 0U)
        == GE_DAM_PRELOAD_ROOM_UNLOADED);

    assert(ge_dam_dynamic_scene_init(&bounded, &pack, &world,
        initial, sizeof(initial), &ten_room_limits)
        == GE_DAM_DYNAMIC_SCENE_OK);
    assert(ge_dam_preload_queue_init(&queue, 137U, 137U,
        initial, sizeof(initial)) == GE_DAM_PRELOAD_OK);
    assert(ge_dam_preload_queue_request(&queue, 1U) != 0U);
    old_vertices = bounded.vertices;
    old_batches = bounded.batches;
    old_generation = bounded.generation;
    assert(ge_dam_dynamic_scene_install_next(&bounded, &queue)
        == GE_DAM_DYNAMIC_SCENE_ROOM_CAPACITY);
    assert(bounded.vertices == old_vertices && bounded.batches == old_batches);
    assert(bounded.generation == old_generation && bounded.room_count == 10U);
    assert(!ge_dam_dynamic_scene_is_resident(&bounded, 1U));
    assert(ge_dam_preload_queue_room_state(&queue, 1U)
        == GE_DAM_PRELOAD_ROOM_UNLOADED);
    assert(queue.pending_count == 0U && queue.loading_count == 0U);

    /* Exact bgRoomsTickUnload lifetime with an install sharing the delete
     * transaction: rendered 135 stays age 1, the other nine reach 4 and are
     * deleted on the following tick, and room 1 consumes the recovered slot.
     * The renderer-facing aging call must not rebuild or publish anything;
     * the existing prepare/commit boundary owns that operation. */
    assert(ge_dam_preload_queue_init(&queue, 137U, 137U,
        initial, sizeof(initial)) == GE_DAM_PRELOAD_OK);
    {
        static const uint8_t rendered[] = {135U};
        size_t tick;
        old_vertices = bounded.vertices;
        old_batches = bounded.batches;
        old_generation = bounded.generation;
        for (tick = 0U; tick < 3U; ++tick) {
            assert(ge_dam_dynamic_scene_age_visibility(
                &bounded, rendered, sizeof(rendered))
                == GE_DAM_DYNAMIC_SCENE_OK);
            assert(bounded.vertices == old_vertices
                   && bounded.batches == old_batches
                   && bounded.generation == old_generation
                   && bounded.room_count == 10U
                   && bounded.scene.room_count == 10U);
        }
        assert(bounded.room_count == 10U && bounded.room_age[135U] == 1U);
        assert(bounded.room_age[133U] == 4U
               && bounded.eviction_pending[133U] == 0U);
        assert(ge_dam_dynamic_scene_age_visibility(
            &bounded, rendered, sizeof(rendered))
            == GE_DAM_DYNAMIC_SCENE_OK);
        assert(bounded.vertices == old_vertices
               && bounded.batches == old_batches
               && bounded.generation == old_generation
               && bounded.room_count == 10U
               && bounded.scene.room_count == 10U);
        assert(bounded.eviction_pending[133U] != 0U);
        assert(ge_dam_preload_queue_room_state(&queue, 133U)
            == GE_DAM_PRELOAD_ROOM_RESIDENT);
        assert(ge_dam_preload_queue_request(&queue, 1U) != 0U);
        assert(ge_dam_dynamic_scene_prepare_next(
            &bounded, &queue, &transaction)
            == GE_DAM_DYNAMIC_SCENE_OK);
        assert(transaction.prepared != 0U
               && transaction.includes_request != 0U
               && transaction.room == 1U
               && transaction.evicted_count == 9U
               && transaction.room_count == 2U);
        assert(bounded.vertices == old_vertices
               && bounded.batches == old_batches
               && bounded.generation == old_generation
               && bounded.room_count == 10U
               && bounded.scene.room_count == 10U);
        assert(ge_dam_preload_queue_room_state(&queue, 133U)
            == GE_DAM_PRELOAD_ROOM_RESIDENT);
        assert(ge_dam_preload_queue_room_state(&queue, 1U)
            == GE_DAM_PRELOAD_ROOM_QUEUED);
        assert(ge_dam_dynamic_scene_commit(&bounded, &queue, &transaction)
            == GE_DAM_DYNAMIC_SCENE_OK);
        assert(bounded.generation == old_generation + 1U);
        assert(bounded.room_count == 2U && bounded.scene.room_count == 2U);
        assert(ge_dam_dynamic_scene_is_resident(&bounded, 135U));
        assert(ge_dam_dynamic_scene_is_resident(&bounded, 1U));
        assert(!ge_dam_dynamic_scene_is_resident(&bounded, 133U));
        assert(bounded.room_age[135U] == 1U
               && bounded.room_age[1U] == 1U);
        assert(bounded.eviction_attempts == 1U
               && bounded.eviction_successes == 1U
               && bounded.eviction_failures == 0U
               && bounded.rooms_evicted == 9U);
        assert(ge_dam_preload_queue_room_state(&queue, 133U)
            == GE_DAM_PRELOAD_ROOM_UNLOADED);
        assert(ge_dam_preload_queue_room_state(&queue, 1U)
            == GE_DAM_PRELOAD_ROOM_RESIDENT);
        assert(queue.eviction_count == 9U);

        /* A missing requested room must not partially publish the aged-room
         * deletion which shared its candidate transaction. */
        for (tick = 0U; tick < 3U; ++tick) {
            assert(ge_dam_dynamic_scene_tick_visibility(
                &bounded, &queue, rendered, sizeof(rendered))
                == GE_DAM_DYNAMIC_SCENE_OK);
        }
        assert(bounded.room_age[1U] == 4U);
        old_vertices = bounded.vertices;
        old_batches = bounded.batches;
        old_generation = bounded.generation;
        assert(ge_dam_preload_queue_request(&queue, 0U) != 0U);
        assert(ge_dam_dynamic_scene_tick_visibility(
            &bounded, &queue, rendered, sizeof(rendered))
            == GE_DAM_DYNAMIC_SCENE_ASSET_NOT_FOUND);
        assert(bounded.vertices == old_vertices
               && bounded.batches == old_batches
               && bounded.generation == old_generation);
        assert(bounded.room_count == 2U
               && ge_dam_dynamic_scene_is_resident(&bounded, 1U));
        assert(bounded.eviction_pending[1U] != 0U);
        assert(ge_dam_preload_queue_room_state(&queue, 1U)
            == GE_DAM_PRELOAD_ROOM_RESIDENT);
        assert(ge_dam_preload_queue_room_state(&queue, 0U)
            == GE_DAM_PRELOAD_ROOM_UNLOADED);
        assert(bounded.eviction_attempts == 2U
               && bounded.eviction_successes == 1U
               && bounded.eviction_failures == 1U);
        {
            static const uint8_t both_rendered[] = {135U, 1U};
            assert(ge_dam_dynamic_scene_tick_visibility(
                &bounded, &queue, both_rendered, sizeof(both_rendered))
                == GE_DAM_DYNAMIC_SCENE_OK);
        }
        assert(bounded.eviction_pending[1U] == 0U
               && bounded.room_age[1U] == 1U);
    }

    ge_dam_dynamic_scene_close(&bounded);
    ge_dam_dynamic_scene_close(&cache);
    ge_asset_pack_close(&pack);
    free(background.bytes);
    puts("Dam dynamic authored-room scene transaction passed");
    return 0;
}

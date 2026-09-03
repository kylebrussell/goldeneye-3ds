#include "ge_dam_dynamic_scene.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

enum { EMPTY_GUARD_TRANSITIONS = 24 };

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

static void exercise_whole_overlay_replacement(
    GeDamDynamicScene *cache,
    const GeDamRoomWorldVertex vertices[6],
    const GeDamRoomDrawBatch batches[2])
{
    const GeDamDynamicScene before = *cache;
    const size_t room_vertices = cache->scene.vertex_count
        - cache->overlay_vertex_count;
    const size_t room_batches = cache->scene.batch_count
        - cache->overlay_batch_count;
    const size_t vertex_bytes = room_vertices * sizeof(*cache->vertices);
    const size_t batch_bytes = room_batches * sizeof(*cache->batches);
    GeDamRoomWorldVertex *room_copy = malloc(vertex_bytes);
    GeDamRoomDrawBatch *batch_copy = malloc(batch_bytes);
    GeAssetPack unavailable_pack = {0};
    GeDamRoomDrawBatch invalid[2] = {batches[0], batches[1]};
    size_t cycle;

    assert(room_copy != NULL && batch_copy != NULL);
    memcpy(room_copy, cache->vertices, vertex_bytes);
    memcpy(batch_copy, cache->batches, batch_bytes);
    /* Bind a pack with no entries only in this test cache. Overlay publication
     * must not reread any room, even for the first insertion after clearing. */
    cache->pack = &unavailable_pack;
    for (cycle = 0U; cycle < 12U; ++cycle) {
        const size_t count = cycle % 3U == 0U ? 3U
            : cycle % 3U == 1U ? 0U : 6U;
        assert(ge_dam_dynamic_scene_set_overlay(cache,
            count != 0U ? vertices : NULL, count,
            count != 0U ? batches : NULL, count / 3U)
            == GE_DAM_DYNAMIC_SCENE_OK);
        assert(cache->scene.vertex_count == room_vertices + count);
        assert(cache->scene.batch_count == room_batches + count / 3U);
        assert(cache->overlay_vertex_count == count);
        assert(cache->overlay_vertices == (count != 0U
            ? cache->vertices + room_vertices : NULL));
        assert(cache->scene.triangle_count
            == before.scene.triangle_count - 2U + count / 3U);
        assert(memcmp(room_copy, cache->vertices, vertex_bytes) == 0);
        assert(memcmp(batch_copy, cache->batches, batch_bytes) == 0);
        if (count != 0U) {
            assert(memcmp(cache->overlay_vertices, vertices,
                          count * sizeof(*vertices)) == 0);
            assert(cache->batches[room_batches].first_vertex == room_vertices);
        }
    }
    /* Existing scene pointers are valid sources until atomic commit. */
    assert(ge_dam_dynamic_scene_set_overlay(cache,
        cache->overlay_vertices, cache->overlay_vertex_count,
        cache->overlay_batches, cache->overlay_batch_count)
        == GE_DAM_DYNAMIC_SCENE_OK);
    {
        const GeDamDynamicScene unchanged = *cache;
        invalid[1].first_vertex = 6U;
        assert(ge_dam_dynamic_scene_set_overlay(cache, vertices, 6U, invalid, 2U)
            == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        assert(memcmp(cache, &unchanged, sizeof(*cache)) == 0);
        invalid[1] = batches[1];
        invalid[1].triangle_count = SIZE_MAX;
        assert(ge_dam_dynamic_scene_set_overlay(cache, vertices, 6U, invalid, 2U)
            == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        assert(memcmp(cache, &unchanged, sizeof(*cache)) == 0);
    }
    cache->limits.vertex_capacity = room_vertices + 5U;
    {
        const GeDamDynamicScene unchanged = *cache;
        assert(ge_dam_dynamic_scene_set_overlay(cache, vertices, 6U, batches, 2U)
            == GE_DAM_DYNAMIC_SCENE_VERTEX_CAPACITY);
        assert(memcmp(cache, &unchanged, sizeof(*cache)) == 0);
    }
    cache->limits = before.limits;
    cache->limits.batch_capacity = room_batches + 1U;
    {
        const GeDamDynamicScene unchanged = *cache;
        assert(ge_dam_dynamic_scene_set_overlay(cache, vertices, 6U, batches, 2U)
            == GE_DAM_DYNAMIC_SCENE_BATCH_CAPACITY);
        assert(memcmp(cache, &unchanged, sizeof(*cache)) == 0);
    }
    cache->limits = before.limits;
    cache->pack = before.pack;
    assert(cache->generation == before.generation + 13U);
    assert(cache->room_count == before.room_count
        && cache->scene.room_count == before.scene.room_count
        && cache->scene.list_count == before.scene.list_count
        && cache->scene.commands_visited == before.scene.commands_visited);
    assert(cache->overlay_update_attempts == before.overlay_update_attempts
        && cache->overlay_update_successes == before.overlay_update_successes
        && cache->overlay_update_failures == before.overlay_update_failures);
    assert(memcmp(cache->room_ids, before.room_ids, sizeof(cache->room_ids)) == 0
        && memcmp(cache->resident, before.resident, sizeof(cache->resident)) == 0
        && memcmp(cache->room_age, before.room_age, sizeof(cache->room_age)) == 0
        && memcmp(cache->eviction_pending, before.eviction_pending,
                  sizeof(cache->eviction_pending)) == 0);
    assert(memcmp(cache->vertices, room_copy, vertex_bytes) == 0);
    assert(memcmp(cache->batches, batch_copy, batch_bytes) == 0);
    assert(memcmp(cache->overlay_vertices, vertices, 6U * sizeof(*vertices)) == 0);
    assert(memcmp(cache->overlay_batches, batches, 2U * sizeof(*batches)) == 0);
    free(batch_copy);
    free(room_copy);
    puts("whole overlay: 13 no-I/O publications, exact room prefix, "
         "alias-safe commit, stale/overflow/capacity rollback verified");
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
    exercise_whole_overlay_replacement(&cache, overlay_vertices, overlay_batches);
    {
        /* A resident scene can contain ordinary props but no visible guards.
         * Its empty guard segment must retain the tail offsets. Repeated
         * visibility changes replace only that tail, never the prefix. */
        const size_t guard_vertex_offset = cache.overlay_vertex_count;
        const size_t guard_batch_offset = cache.overlay_batch_count;
        const size_t room_vertices = cache.scene.vertex_count
            - cache.overlay_vertex_count;
        const size_t room_batches = cache.scene.batch_count
            - cache.overlay_batch_count;
        GeDamRoomWorldVertex guard_vertices[9] = {0};
        GeDamRoomDrawBatch guard_batches[2] = {
            overlay_batches[0], overlay_batches[0]
        };
        GeDamRoomDrawBatch ordinary_before[2];
        GeDamRoomWorldVertex room_before = cache.vertices[0];
        uint64_t failures = cache.overlay_update_failures;
        size_t old_vertices = 0U, old_batches = 0U;
        size_t cycle;
        memcpy(ordinary_before, cache.overlay_batches, sizeof(ordinary_before));
        guard_batches[0].vertex_count = 6U;
        guard_batches[0].triangle_count = 2U;
        guard_batches[1].first_vertex = 6U;
        guard_vertices[0].world[0] = 123.0f;
        guard_vertices[6].world[0] = 456.0f;
        for (cycle = 0U; cycle < EMPTY_GUARD_TRANSITIONS; ++cycle) {
            const size_t new_vertices = cycle % 3U == 0U ? 9U
                : cycle % 3U == 1U ? 6U : 0U;
            const size_t new_batches = cycle % 3U == 0U ? 2U
                : cycle % 3U == 1U ? 1U : 0U;
            assert(ge_dam_dynamic_scene_replace_overlay_segment(
                &cache, guard_vertex_offset, old_vertices,
                new_vertices ? guard_vertices : NULL, new_vertices,
                guard_batch_offset, old_batches,
                new_batches ? guard_batches : NULL, new_batches)
                == GE_DAM_DYNAMIC_SCENE_OK);
            assert(cache.overlay_vertex_count - new_vertices
                == guard_vertex_offset);
            assert(cache.overlay_batch_count - new_batches
                == guard_batch_offset);
            assert(memcmp(cache.overlay_batches, ordinary_before,
                          sizeof(ordinary_before)) == 0);
            assert(memcmp(cache.overlay_vertices, overlay_vertices,
                          sizeof(overlay_vertices)) == 0);
            assert(memcmp(&cache.vertices[0], &room_before,
                          sizeof(room_before)) == 0);
            if (new_vertices != 0U) {
                assert(cache.overlay_vertices[guard_vertex_offset].world[0]
                    == 123.0f);
                assert(cache.overlay_batches[guard_batch_offset].first_vertex
                    == guard_vertex_offset);
                assert(cache.batches[room_batches + guard_batch_offset]
                    .first_vertex == room_vertices + guard_vertex_offset);
            }
            old_vertices = new_vertices;
            old_batches = new_batches;
        }
        assert(cache.overlay_update_failures == failures);
        assert(cache.overlay_vertex_count == 6U
            && cache.overlay_batch_count == 2U);
    }
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
        assert(cache.overlay_update_attempts == 3U + EMPTY_GUARD_TRANSITIONS
                && cache.overlay_update_successes == 3U + EMPTY_GUARD_TRANSITIONS
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
        assert(cache.overlay_update_attempts == 5U + EMPTY_GUARD_TRANSITIONS
                && cache.overlay_update_successes == 4U + EMPTY_GUARD_TRANSITIONS
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
                && cache.overlay_update_successes == 5U + EMPTY_GUARD_TRANSITIONS
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

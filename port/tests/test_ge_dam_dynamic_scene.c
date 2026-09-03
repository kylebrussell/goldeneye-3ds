#include "ge_dam_dynamic_scene.h"
#include "ge_scene_part_replace.h"

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

static void exercise_prop_segment_replacement(GeDamDynamicScene *cache)
{
    GeDamRoomWorldVertex original[12] = {0}, replacement[9] = {0};
    GeDamRoomDrawBatch batches[4] = {0}, changed_batches[3] = {0};
    GeScenePartRange *parts = calloc(2U, sizeof(*parts));
    GeScenePartRange changed_parts[3] = {0};
    GeSceneOverlaySpan tails[2] = {{6U,3U,2U,1U}, {9U,3U,3U,1U}};
    GeSceneOverlaySpan changed;
    GeAssetPack unavailable = {0};
    GeAssetPack *pack = cache->pack;
    const size_t room_v = cache->scene.vertex_count - cache->overlay_vertex_count;
    const size_t room_b = cache->scene.batch_count - cache->overlay_batch_count;
    GeDamRoomWorldVertex *room_copy = malloc(room_v * sizeof(*room_copy));
    GeDamRoomDrawBatch *room_batches = malloc(room_b * sizeof(*room_batches));
    size_t part_count = 2U, i, cycle;
    assert(parts != NULL && room_copy != NULL && room_batches != NULL);
    memcpy(room_copy, cache->vertices, room_v * sizeof(*room_copy));
    memcpy(room_batches, cache->batches, room_b * sizeof(*room_batches));
    parts[0] = (GeScenePartRange){10U,0U,&original[0],0U,3U,0U,1U};
    parts[1] = (GeScenePartRange){30U,0U,&original[3],3U,3U,1U,1U};
    for (i = 0U; i < 12U; ++i) original[i].world[0] = (float)i;
    for (i = 0U; i < 9U; ++i) replacement[i].world[0] = (float)i + 100.0f;
    for (i = 0U; i < 4U; ++i) {
        batches[i].room_id = 135U;
        batches[i].first_vertex = i * 3U;
        batches[i].vertex_count = 3U;
        batches[i].triangle_count = 1U;
    }
    for (i = 0U; i < 3U; ++i) {
        changed_batches[i] = batches[i];
        changed_parts[i] = (GeScenePartRange){20U,i,&replacement[i * 3U],
            i * 3U,3U,i,1U};
    }
    assert(ge_dam_dynamic_scene_set_overlay(cache, original, 12U, batches, 4U)
        == GE_DAM_DYNAMIC_SCENE_OK);
    cache->pack = &unavailable;
    /* Insert an absent entry, grow/shrink it, delete every part, then restore.
     * Other props, both trailing segments and room data retain exact bytes. */
    for (cycle = 0U; cycle < 16U; ++cycle) {
        const size_t count = (cycle + 2U) % 4U;
        const GeDamDynamicScene before = *cache;
        const int fits = room_v + 12U + count * 3U <= before.vertex_storage_capacity
            && room_b + 4U + count <= before.batch_storage_capacity
            && 4U + count <= before.overlay_batch_storage_capacity;
        assert(ge_scene_part_replace(cache, &parts, &part_count, tails, 2U,
            20U, changed_parts, count, replacement, count * 3U,
            changed_batches, count, &changed) == GE_DAM_DYNAMIC_SCENE_OK);
        if (fits) {
            assert(cache->vertices == before.vertices && cache->batches == before.batches
                && cache->overlay_batches == before.overlay_batches);
            assert(cache->vertex_storage_capacity == before.vertex_storage_capacity
                && cache->batch_storage_capacity == before.batch_storage_capacity
                && cache->overlay_batch_storage_capacity == before.overlay_batch_storage_capacity);
            assert(cache->overlay_inplace_replacements == before.overlay_inplace_replacements + 1U);
            assert(cache->overlay_allocating_replacements == before.overlay_allocating_replacements);
        }
        assert(part_count == 2U + count);
        assert(cache->overlay_vertex_count == 12U + count * 3U);
        assert(changed.vertex_offset == 3U && changed.vertex_count == 9U + count * 3U);
        assert(changed.batch_offset == 1U && changed.batch_count == 3U + count);
        assert(parts[0].entry_index == 10U && parts[0].vertex_offset == 0U);
        assert(parts[1U + count].entry_index == 30U
            && parts[1U + count].vertex_offset == 3U + count * 3U);
        assert(tails[0].vertex_offset == 6U + count * 3U
            && tails[1].vertex_offset == 9U + count * 3U);
        assert(tails[0].batch_offset == 2U + count && tails[1].batch_offset == 3U + count);
        assert(memcmp(cache->overlay_vertices, original, 3U * sizeof(*original)) == 0);
        assert(memcmp(cache->overlay_vertices + 3U, replacement,
            count * 3U * sizeof(*replacement)) == 0);
        assert(memcmp(cache->overlay_vertices + 3U + count * 3U,
            original + 3U, 9U * sizeof(*original)) == 0);
        assert(memcmp(cache->vertices, room_copy, room_v * sizeof(*room_copy)) == 0);
        assert(memcmp(cache->batches, room_batches, room_b * sizeof(*room_batches)) == 0);
        for (i = 0U; i < cache->overlay_batch_count; ++i) {
            assert(cache->overlay_batches[i].first_vertex == i * 3U);
            assert(cache->batches[room_b + i].first_vertex == room_v + i * 3U);
        }
    }
    {
        GeScenePartRange *before_parts = parts;
        const GeSceneOverlaySpan before_tails[2] = {tails[0], tails[1]};
        GeDamRoomWorldVertex *before_vertices = cache->vertices;
        const uint64_t generation = cache->generation;
        const size_t capacity = cache->limits.vertex_capacity;
        changed_parts[0].part_index = 1U;
        assert(ge_scene_part_replace(cache, &parts, &part_count, tails, 2U,
            20U, changed_parts, 3U, replacement, 9U, changed_batches, 3U, &changed)
            == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        changed_parts[0].part_index = 0U;
        cache->limits.vertex_capacity = room_v + 12U;
        assert(ge_scene_part_replace(cache, &parts, &part_count, tails, 2U,
            20U, changed_parts, 3U, replacement, 9U, changed_batches, 3U, &changed)
            == GE_DAM_DYNAMIC_SCENE_VERTEX_CAPACITY);
        cache->limits.vertex_capacity = capacity;
        assert(parts == before_parts && cache->vertices == before_vertices
            && cache->generation == generation);
        assert(memcmp(tails, before_tails, sizeof(tails)) == 0);
    }
    /* Alias both the old metadata and old scene data on a same-size switch. */
    assert(ge_scene_part_replace(cache, &parts, &part_count, tails, 2U,
        10U, parts, 1U, cache->overlay_vertices, 3U,
        cache->overlay_batches, 1U, &changed) == GE_DAM_DYNAMIC_SCENE_OK);
    /* A node switch with identical counts still replaces its identity/data. */
    changed_parts[0].node = &original[11];
    assert(ge_scene_part_replace(cache, &parts, &part_count, tails, 2U,
        20U, changed_parts, 1U, replacement, 3U, changed_batches, 1U, &changed)
        == GE_DAM_DYNAMIC_SCENE_OK);
    assert(parts[1].node == &original[11]);
    /* Empty ordinary prefixes and empty door/guard tails retain a usable
     * insertion point. This is the same lifecycle as a newly visible model. */
    for (i = 10U; i <= 30U; i += 10U)
        assert(ge_scene_part_replace(cache, &parts, &part_count, tails, 2U,
            i, NULL, 0U, NULL, 0U, NULL, 0U, &changed) == GE_DAM_DYNAMIC_SCENE_OK);
    assert(part_count == 0U && parts == NULL && tails[0].vertex_offset == 0U);
    assert(ge_dam_dynamic_scene_set_overlay(cache, NULL, 0U, NULL, 0U)
        == GE_DAM_DYNAMIC_SCENE_OK);
    memset(tails, 0, sizeof(tails));
    assert(ge_scene_part_replace(cache, &parts, &part_count, tails, 2U,
        20U, changed_parts, 3U, replacement, 9U, changed_batches, 3U, &changed)
        == GE_DAM_DYNAMIC_SCENE_OK);
    assert(tails[0].vertex_offset == 9U && tails[1].vertex_offset == 9U
        && tails[0].batch_offset == 3U && tails[1].batch_offset == 3U);
    assert(ge_scene_part_replace(cache, &parts, &part_count, tails, 2U,
        20U, NULL, 0U, NULL, 0U, NULL, 0U, &changed) == GE_DAM_DYNAMIC_SCENE_OK);
    assert(tails[0].vertex_offset == 0U && tails[1].vertex_offset == 0U
        && cache->overlay_vertex_count == 0U && part_count == 0U);
    cache->pack = pack;
    free(parts);
    free(room_copy);
    free(room_batches);
    puts("prop segment replacement: 16 insert/grow/shrink/clear cycles; exact room/peer/door/guard preservation and atomic failure");
}

static void exercise_middle_source_aliases_and_rollback(GeDamDynamicScene *cache)
{
    GeDamRoomWorldVertex vertices[18] = {0};
    GeDamRoomDrawBatch batches[6] = {0};
    GeDamRoomWorldVertex expected[18];
    GeDamRoomDrawBatch expected_batches[6];
    const size_t room_v = cache->scene.vertex_count - cache->overlay_vertex_count;
    const size_t room_b = cache->scene.batch_count - cache->overlay_batch_count;
    for (size_t i = 0U; i < 18U; ++i) vertices[i].world[0] = (float)i + 10.0f;
    for (size_t i = 0U; i < 6U; ++i) {
        batches[i].first_vertex = i * 3U;
        batches[i].vertex_count = 3U;
        batches[i].triangle_count = 1U;
        batches[i].room_id = (uint32_t)i + 1U;
        batches[i].material.alpha_threshold = (uint8_t)(i * 7U);
    }
    assert(ge_dam_dynamic_scene_set_overlay(cache, vertices, 18U, batches, 6U)
        == GE_DAM_DYNAMIC_SCENE_OK);
    for (size_t mode = 0U; mode < 3U; ++mode) {
        assert(ge_dam_dynamic_scene_set_overlay(cache, vertices, 12U, batches, 4U)
            == GE_DAM_DYNAMIC_SCENE_OK);
        const GeDamDynamicScene before = *cache;
        assert(room_v + 15U <= before.vertex_storage_capacity
            && room_b + 5U <= before.batch_storage_capacity
            && 5U <= before.overlay_batch_storage_capacity);
        const GeDamRoomWorldVertex *source_vertices = mode == 1U ? vertices
            : cache->overlay_vertices + 6U;
        const GeDamRoomDrawBatch *source_batches = mode == 0U ? batches
            : cache->overlay_batches;
        memcpy(expected, vertices, 3U * sizeof(*vertices));
        memcpy(expected + 3U, source_vertices, 6U * sizeof(*vertices));
        memcpy(expected + 9U, vertices + 6U, 6U * sizeof(*vertices));
        expected_batches[0] = batches[0];
        for (size_t i = 0U; i < 2U; ++i) {
            expected_batches[i + 1U] = source_batches[i];
            expected_batches[i + 1U].first_vertex += 3U;
            expected_batches[i + 3U] = batches[i + 2U];
            expected_batches[i + 3U].first_vertex += 3U;
        }
        assert(ge_dam_dynamic_scene_replace_overlay_segment(cache,
            3U, 3U, source_vertices, 6U, 1U, 1U, source_batches, 2U)
            == GE_DAM_DYNAMIC_SCENE_OK);
        /* Middle aliases must use the original allocate-before-free path. */
        assert(cache->vertices != before.vertices && cache->batches != before.batches);
        assert(cache->overlay_allocating_replacements == before.overlay_allocating_replacements + 1U);
        assert(cache->overlay_inplace_replacements == before.overlay_inplace_replacements);
        assert(cache->generation == before.generation + 1U);
        assert(memcmp(cache->overlay_vertices, expected, 15U * sizeof(*vertices)) == 0);
        assert(memcmp(cache->overlay_batches, expected_batches,
            5U * sizeof(*batches)) == 0);
        for (size_t i = 0U; i < 5U; ++i) {
            GeDamRoomDrawBatch published = expected_batches[i];
            published.first_vertex += room_v;
            assert(memcmp(&cache->batches[room_b + i], &published, sizeof(published)) == 0);
        }
    }
    assert(ge_dam_dynamic_scene_set_overlay(cache, vertices, 12U, batches, 4U)
        == GE_DAM_DYNAMIC_SCENE_OK);
    for (size_t mode = 0U; mode < 2U; ++mode) {
        GeDamRoomDrawBatch replacement[2] = {batches[0], batches[1]};
        if (mode == 0U) replacement[1].triangle_count = SIZE_MAX;
        else cache->overlay_batches[3].triangle_count = SIZE_MAX;
        GeDamDynamicScene before = *cache;
        memcpy(expected, cache->overlay_vertices, 12U * sizeof(*vertices));
        memcpy(expected_batches, cache->overlay_batches, 4U * sizeof(*batches));
        assert(ge_dam_dynamic_scene_replace_overlay_segment(cache,
            3U, 3U, vertices, 6U, 1U, 1U, replacement, 2U)
            == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        ++before.overlay_update_attempts;
        ++before.overlay_update_failures;
        assert(memcmp(cache, &before, sizeof(before)) == 0);
        assert(memcmp(cache->overlay_vertices, expected, 12U * sizeof(*vertices)) == 0);
        assert(memcmp(cache->overlay_batches, expected_batches, 4U * sizeof(*batches)) == 0);
    }
    cache->overlay_batches[3].triangle_count = 1U;
    assert(ge_dam_dynamic_scene_set_overlay(cache, NULL, 0U, NULL, 0U)
        == GE_DAM_DYNAMIC_SCENE_OK);
    puts("middle overlay: retained-capacity publication, aliased sources and preflight rollback passed");
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
        GeDamRoomWorldVertex *retained_vertices = NULL;
        GeDamRoomDrawBatch *retained_batches = NULL;
        GeDamRoomDrawBatch *retained_overlay_batches = NULL;
        GeDamRoomWorldVertex *room_copy = malloc(room_vertices * sizeof(*room_copy));
        GeDamRoomDrawBatch *room_batch_copy = malloc(room_batches * sizeof(*room_batch_copy));
        assert(room_copy != NULL && room_batch_copy != NULL);
        memcpy(room_copy, cache.vertices, room_vertices * sizeof(*room_copy));
        memcpy(room_batch_copy, cache.batches, room_batches * sizeof(*room_batch_copy));
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
            if (cycle == 0U) {
                retained_vertices = cache.vertices;
                retained_batches = cache.batches;
                retained_overlay_batches = cache.overlay_batches;
            }
            assert(cache.vertices == retained_vertices
                && cache.batches == retained_batches
                && cache.overlay_batches == retained_overlay_batches);
            assert(cache.vertex_storage_capacity >= cache.scene.vertex_count
                && cache.vertex_storage_capacity <= cache.limits.vertex_capacity);
            assert(cache.batch_storage_capacity >= cache.scene.batch_count
                && cache.batch_storage_capacity <= cache.limits.batch_capacity);
            assert(memcmp(room_copy, cache.vertices,
                room_vertices * sizeof(*room_copy)) == 0);
            assert(memcmp(room_batch_copy, cache.batches,
                room_batches * sizeof(*room_batch_copy)) == 0);
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
        {
            GeDamRoomDrawBatch invalid = guard_batches[0];
            const uint64_t generation = cache.generation;
            invalid.triangle_count = SIZE_MAX;
            assert(ge_dam_dynamic_scene_replace_overlay_segment(
                &cache, guard_vertex_offset, 0U, guard_vertices, 6U,
                guard_batch_offset, 0U, &invalid, 1U)
                == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
            assert(cache.generation == generation && cache.vertices == retained_vertices
                && cache.overlay_vertex_count == 6U && cache.overlay_batch_count == 2U);
        }
        /* The tail replacement also supports an overlapping retained source.
         * First local batch starts at zero, matching the segment-input ABI. */
        assert(ge_dam_dynamic_scene_replace_overlay_segment(
            &cache, 3U, 3U, cache.overlay_vertices, 3U,
            1U, 1U, cache.overlay_batches, 1U) == GE_DAM_DYNAMIC_SCENE_OK);
        assert(cache.vertices == retained_vertices && cache.batches == retained_batches);
        assert(memcmp(cache.overlay_vertices, cache.overlay_vertices + 3U,
            3U * sizeof(*cache.overlay_vertices)) == 0);
        assert(cache.overlay_batches[1].first_vertex == 3U);
        assert(ge_dam_dynamic_scene_set_overlay(&cache,
            overlay_vertices, 6U, overlay_batches, 2U) == GE_DAM_DYNAMIC_SCENE_OK);
        free(room_copy);
        free(room_batch_copy);
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
        assert(cache.overlay_update_attempts == 5U + EMPTY_GUARD_TRANSITIONS
            && cache.overlay_update_successes == 4U + EMPTY_GUARD_TRANSITIONS
            && cache.overlay_update_failures == 1U);
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
                && cache.overlay_update_failures == 2U);
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
        assert(cache.overlay_update_attempts == 7U + EMPTY_GUARD_TRANSITIONS
                && cache.overlay_update_successes == 5U + EMPTY_GUARD_TRANSITIONS
                && cache.overlay_update_failures == 2U);
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
                && cache.overlay_update_successes == 6U + EMPTY_GUARD_TRANSITIONS
                && cache.overlay_update_failures == 2U);
        assert(cache.batches[published_batch].texture.texture_id == 321U);
        assert(cache.batches[published_batch].first_vertex == 9129U);
        assert(ge_dam_dynamic_scene_commit_overlay_batches(
            &cache, 2U, 1U) == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        assert(cache.overlay_update_failures == 3U);
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
    exercise_prop_segment_replacement(&cache);
    exercise_middle_source_aliases_and_rollback(&cache);
    {
        GeDamRoomWorldVertex vertices[6] = {0};
        GeDamRoomDrawBatch source[2] = {overlay_batches[0], overlay_batches[1]};
        GeDamRoomDrawBatch expected_local[2], expected_published[2];
        assert(ge_dam_dynamic_scene_set_overlay(&cache, vertices, 6U, source, 2U)
            == GE_DAM_DYNAMIC_SCENE_OK);
        const size_t room_batches = cache.scene.batch_count - cache.overlay_batch_count;
        const size_t triangles = cache.scene.triangle_count;
        memcpy(expected_local, cache.overlay_batches, sizeof(expected_local));
        memcpy(expected_published, cache.batches + room_batches, sizeof(expected_published));
        const uint64_t generation = cache.generation;
        const uint64_t successes = cache.overlay_update_successes;
        /* Pose publication changes rooms only, with caller-local offsets and
         * intentionally unrelated material bytes that must not be copied. */
        memset(source, 0xa5, sizeof(source));
        source[0].room_id = 133U;
        source[1].room_id = 134U;
        for (size_t i = 0U; i < 2U; ++i) {
            expected_local[i].room_id = source[i].room_id;
            expected_published[i].room_id = source[i].room_id;
        }
        assert(ge_dam_dynamic_scene_commit_overlay_rooms(&cache, 0U, source, 2U)
            == GE_DAM_DYNAMIC_SCENE_OK);
        assert(cache.generation == generation + 1U
            && cache.overlay_update_successes == successes + 1U);
        assert(cache.scene.triangle_count == triangles);
        assert(memcmp(expected_local, cache.overlay_batches, sizeof(expected_local)) == 0);
        assert(memcmp(expected_published, cache.batches + room_batches,
            sizeof(expected_published)) == 0);
        const uint64_t failures = cache.overlay_update_failures;
        assert(ge_dam_dynamic_scene_commit_overlay_rooms(&cache, 1U, source, 2U)
            == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        assert(ge_dam_dynamic_scene_commit_overlay_rooms(&cache, 0U, NULL, 2U)
            == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
        assert(cache.generation == generation + 1U
            && cache.overlay_update_failures == failures + 2U);
        assert(memcmp(expected_published, cache.batches + room_batches,
            sizeof(expected_published)) == 0);
    }
    ge_dam_dynamic_scene_close(&cache);
    ge_asset_pack_close(&pack);
    free(background.bytes);
    puts("Dam dynamic authored-room scene transaction passed");
    return 0;
}

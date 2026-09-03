#include "ge_dam_dynamic_scene.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile the production implementation into this test with allocator fault
 * injection. No test hooks or alternative allocator enter the ARM runtime. */
static size_t allocation_calls, fail_after = SIZE_MAX, fail_once = SIZE_MAX;
static void *live_allocations[16];
static size_t live_count;

static void *scene_test_malloc(size_t bytes)
{
    const size_t call = allocation_calls++;
    if (call >= fail_after || call == fail_once) return NULL;
    void *result = malloc(bytes);
    assert(result != NULL && live_count < 16U);
    live_allocations[live_count++] = result;
    return result;
}

static void scene_test_free(void *pointer)
{
    for (size_t i = 0; i < live_count; ++i) {
        if (live_allocations[i] != pointer) continue;
        live_allocations[i] = live_allocations[--live_count];
        break;
    }
    free(pointer);
}

#define malloc scene_test_malloc
#define free scene_test_free
#include "../src/ge_dam_dynamic_scene.c"
#undef free
#undef malloc

enum { ROOM_V = 6, ROOM_B = 2, OLD_V = 24, OLD_B = 8, MAX_V = 48, MAX_B = 24 };

static void initialize_scene(GeDamDynamicScene *scene, unsigned retain_mask,
    size_t required_v, size_t required_b)
{
    memset(scene, 0, sizeof(*scene));
    scene->initialized = 1U;
    scene->limits = (GeDamDynamicSceneLimits){1U, MAX_V, MAX_B};
    scene->room_count = scene->scene.room_count = 1U;
    scene->resident[1] = scene->room_age[1] = scene->room_ids[0] = 1U;
    scene->overlay_vertex_count = OLD_V;
    scene->overlay_batch_count = OLD_B;
    scene->scene.vertex_count = scene->scene.required_vertex_count = ROOM_V + OLD_V;
    scene->scene.batch_count = scene->scene.required_batch_count = ROOM_B + OLD_B;
    scene->scene.triangle_count = ROOM_B + OLD_B;
    scene->vertex_storage_capacity = retain_mask & 1U ? MAX_V : ROOM_V + OLD_V;
    scene->batch_storage_capacity = retain_mask & 2U ? MAX_B : ROOM_B + OLD_B;
    scene->overlay_batch_storage_capacity = retain_mask & 4U ? MAX_B : OLD_B;
    assert(ROOM_V + required_v <= MAX_V && ROOM_B + required_b <= MAX_B);
    scene->vertices = malloc(scene->vertex_storage_capacity * sizeof(*scene->vertices));
    scene->batches = malloc(scene->batch_storage_capacity * sizeof(*scene->batches));
    scene->overlay_batches = malloc(scene->overlay_batch_storage_capacity * sizeof(*scene->overlay_batches));
    assert(scene->vertices && scene->batches && scene->overlay_batches);
    memset(scene->vertices, 0xa5, scene->vertex_storage_capacity * sizeof(*scene->vertices));
    memset(scene->batches, 0xa5, scene->batch_storage_capacity * sizeof(*scene->batches));
    memset(scene->overlay_batches, 0xa5, scene->overlay_batch_storage_capacity * sizeof(*scene->overlay_batches));
    for (size_t i = 0; i < ROOM_V + OLD_V; ++i) {
        scene->vertices[i].world[0] = (float)i;
        scene->vertices[i].world[1] = (float)i * 2.0f;
    }
    for (size_t i = 0; i < ROOM_B + OLD_B; ++i) {
        GeDamRoomDrawBatch *batch = &scene->batches[i];
        batch->first_vertex = i * 3U;
        batch->vertex_count = 3U;
        batch->triangle_count = 1U;
        batch->room_id = (uint32_t)i;
        batch->material.alpha_threshold = (uint8_t)(i * 9U);
        if (i >= ROOM_B) {
            scene->overlay_batches[i - ROOM_B] = *batch;
            scene->overlay_batches[i - ROOM_B].first_vertex -= ROOM_V;
        }
    }
    scene->overlay_vertices = scene->vertices + ROOM_V;
}

static void exercise_growth(unsigned retain_mask, int tail, unsigned alias,
    size_t count_v, size_t count_b, size_t failure, size_t one_failure)
{
    GeDamDynamicScene scene;
    const size_t offset_v = tail ? 18U : 6U, offset_b = tail ? 6U : 2U;
    const size_t required_v = OLD_V - 6U + count_v;
    const size_t required_b = OLD_B - 2U + count_b;
    GeDamRoomWorldVertex replacement_v[9], expected_v[MAX_V], original_v[MAX_V];
    GeDamRoomDrawBatch replacement_b[4], expected_b[MAX_B], original_b[MAX_B];
    GeDamRoomDrawBatch expected_local[MAX_B], original_local[MAX_B];
    initialize_scene(&scene, retain_mask, required_v, required_b);
    const GeDamDynamicScene before = scene;
    memcpy(original_v, scene.vertices, scene.vertex_storage_capacity * sizeof(*original_v));
    memcpy(original_b, scene.batches, scene.batch_storage_capacity * sizeof(*original_b));
    memcpy(original_local, scene.overlay_batches, scene.overlay_batch_storage_capacity * sizeof(*original_local));
    memset(replacement_v, 0x6d, sizeof(replacement_v));
    for (size_t i = 0; i < 9U; ++i) replacement_v[i].world[0] = (float)i + 1000.0f;
    for (size_t i = 0; i < count_b; ++i) {
        replacement_b[i] = scene.overlay_batches[0];
        replacement_b[i].first_vertex = (i * 3U) % count_v;
        replacement_b[i].room_id = (uint32_t)i + 100U;
    }
    const GeDamRoomWorldVertex *source_v = alias & 1U ? scene.overlay_vertices : replacement_v;
    const GeDamRoomDrawBatch *source_b = alias & 2U ? scene.overlay_batches : replacement_b;
    /* Independent expected publication, built before any source can move. */
    memcpy(expected_v, original_v, (ROOM_V + offset_v) * sizeof(*expected_v));
    memcpy(expected_v + ROOM_V + offset_v, source_v, count_v * sizeof(*expected_v));
    memcpy(expected_v + ROOM_V + offset_v + count_v, original_v + ROOM_V + offset_v + 6U,
        (OLD_V - offset_v - 6U) * sizeof(*expected_v));
    memcpy(expected_b, original_b, ROOM_B * sizeof(*expected_b));
    memcpy(expected_local, original_local, offset_b * sizeof(*expected_local));
    for (size_t i = 0; i < count_b; ++i) {
        expected_local[offset_b + i] = source_b[i];
        expected_local[offset_b + i].first_vertex += offset_v;
    }
    for (size_t i = offset_b + 2U; i < OLD_B; ++i) {
        expected_local[i - 2U + count_b] = original_local[i];
        expected_local[i - 2U + count_b].first_vertex = original_local[i].first_vertex - 6U + count_v;
    }
    for (size_t i = 0; i < required_b; ++i) {
        expected_b[ROOM_B + i] = expected_local[i];
        expected_b[ROOM_B + i].first_vertex += ROOM_V;
    }
    allocation_calls = 0U;
    fail_after = failure;
    fail_once = one_failure;
    const GeDamDynamicSceneStatus status = ge_dam_dynamic_scene_replace_overlay_segment(
        &scene, offset_v, 6U, source_v, count_v, offset_b, 2U, source_b, count_b);
    if (status == GE_DAM_DYNAMIC_SCENE_NO_MEMORY) {
        assert(failure != SIZE_MAX && allocation_calls > failure);
        GeDamDynamicScene unchanged = before;
        ++unchanged.overlay_update_attempts;
        ++unchanged.overlay_update_failures;
        assert(memcmp(&scene, &unchanged, sizeof(scene)) == 0);
        assert(memcmp(scene.vertices, original_v, scene.vertex_storage_capacity * sizeof(*original_v)) == 0);
        assert(memcmp(scene.batches, original_b, scene.batch_storage_capacity * sizeof(*original_b)) == 0);
        assert(memcmp(scene.overlay_batches, original_local, scene.overlay_batch_storage_capacity * sizeof(*original_local)) == 0);
        assert(live_count == 0U);
    } else {
        assert(status == GE_DAM_DYNAMIC_SCENE_OK);
        const int all_fit = ROOM_V + required_v <= before.vertex_storage_capacity
            && ROOM_B + required_b <= before.batch_storage_capacity
            && required_b <= before.overlay_batch_storage_capacity;
        const int force_all = alias && !(tail && all_fit);
        const int grow_v = force_all || ROOM_V + required_v > before.vertex_storage_capacity;
        const int grow_b = force_all || ROOM_B + required_b > before.batch_storage_capacity;
        const int grow_local = force_all || required_b > before.overlay_batch_storage_capacity;
        assert((scene.vertices != before.vertices) == grow_v);
        assert((scene.batches != before.batches) == grow_b);
        assert((scene.overlay_batches != before.overlay_batches) == grow_local);
        assert(scene.overlay_buffer_replacements[0] == (uint64_t)grow_v);
        assert(scene.overlay_buffer_replacements[1] == (uint64_t)grow_b);
        assert(scene.overlay_buffer_replacements[2] == (uint64_t)grow_local);
        assert(scene.overlay_room_vertices_copied == (grow_v ? ROOM_V : 0U));
        assert(scene.overlay_inplace_replacements == (uint64_t)!(grow_v || grow_b || grow_local));
        assert(scene.overlay_allocating_replacements == (uint64_t)(grow_v || grow_b || grow_local));
        assert(scene.scene.vertex_count == ROOM_V + required_v);
        assert(scene.scene.required_vertex_count == scene.scene.vertex_count);
        assert(scene.scene.batch_count == ROOM_B + required_b);
        assert(scene.scene.required_batch_count == scene.scene.batch_count);
        assert(scene.scene.triangle_count == ROOM_B + required_b);
        assert(scene.overlay_vertex_count == required_v && scene.overlay_batch_count == required_b);
        assert(scene.overlay_vertices == scene.vertices + ROOM_V);
        assert(scene.generation == before.generation + 1U);
        assert(memcmp(scene.vertices, expected_v, (ROOM_V + required_v) * sizeof(*expected_v)) == 0);
        assert(memcmp(scene.batches, expected_b, (ROOM_B + required_b) * sizeof(*expected_b)) == 0);
        assert(memcmp(scene.overlay_batches, expected_local, required_b * sizeof(*expected_local)) == 0);
        if (one_failure == 0U && grow_v) assert(scene.vertex_storage_capacity == ROOM_V + required_v);
    }
    fail_after = fail_once = SIZE_MAX;
    ge_dam_dynamic_scene_close(&scene);
    assert(live_count == 0U);
}

static void exercise_invalid_growth(unsigned retain_mask, unsigned mode)
{
    GeDamDynamicScene scene;
    GeDamRoomWorldVertex vertices[9] = {0}, original_v[MAX_V];
    GeDamRoomDrawBatch batches[3] = {0}, original_b[MAX_B], original_local[MAX_B];
    initialize_scene(&scene, retain_mask, OLD_V + 3U, OLD_B + 1U);
    for (size_t i = 0; i < 3U; ++i) {
        batches[i].first_vertex = i * 3U;
        batches[i].vertex_count = 3U;
        batches[i].triangle_count = 1U;
    }
    switch (mode) {
    case 0: batches[2].triangle_count = SIZE_MAX; break;
    case 1: scene.batches[0].triangle_count = SIZE_MAX; break;
    case 2: scene.overlay_batches[7].triangle_count = SIZE_MAX; break;
    case 3: batches[2].first_vertex = SIZE_MAX; break;
    case 4: scene.overlay_batches[7].first_vertex = SIZE_MAX; break;
    default:
        scene.batches[ROOM_B].triangle_count = SIZE_MAX;
        scene.overlay_batches[0].triangle_count = SIZE_MAX;
        break;
    }
    GeDamDynamicScene unchanged = scene;
    memcpy(original_v, scene.vertices, scene.vertex_storage_capacity * sizeof(*original_v));
    memcpy(original_b, scene.batches, scene.batch_storage_capacity * sizeof(*original_b));
    memcpy(original_local, scene.overlay_batches, scene.overlay_batch_storage_capacity * sizeof(*original_local));
    allocation_calls = 0;
    assert(ge_dam_dynamic_scene_replace_overlay_segment(&scene,
        6U, 6U, vertices, 9U, 2U, 2U, batches, 3U) == GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT);
    ++unchanged.overlay_update_attempts;
    ++unchanged.overlay_update_failures;
    assert(allocation_calls == 0U && live_count == 0U);
    assert(memcmp(&scene, &unchanged, sizeof(scene)) == 0);
    assert(memcmp(scene.vertices, original_v, scene.vertex_storage_capacity * sizeof(*original_v)) == 0);
    assert(memcmp(scene.batches, original_b, scene.batch_storage_capacity * sizeof(*original_b)) == 0);
    assert(memcmp(scene.overlay_batches, original_local, scene.overlay_batch_storage_capacity * sizeof(*original_local)) == 0);
    ge_dam_dynamic_scene_close(&scene);
}

int main(void)
{
    size_t cases = 0U;
    for (unsigned mask = 0U; mask < 8U; ++mask) {
        for (unsigned mode = 0U; mode < 6U; ++mode) {
            exercise_invalid_growth(mask, mode);
            ++cases;
        }
        for (int tail = 0; tail < 2; ++tail) {
            for (unsigned alias = 0; alias < 4U; ++alias) {
                for (size_t failure = 0; failure < 4U; ++failure) {
                    exercise_growth(mask, tail, alias, 9U, 3U, failure, SIZE_MAX);
                    ++cases;
                }
                exercise_growth(mask, tail, alias, 9U, 3U, SIZE_MAX, 0U);
                ++cases;
            }
            /* Batch-only growth with shrinking/equal vertices; vertex-only
             * growth with shrinking batches. Capacity cannot be inferred
             * from triangle or batch count. */
            exercise_growth(mask, tail, 0U, 3U, 4U, SIZE_MAX, SIZE_MAX);
            exercise_growth(mask, tail, 0U, 6U, 4U, SIZE_MAX, SIZE_MAX);
            exercise_growth(mask, tail, 0U, 9U, 1U, SIZE_MAX, SIZE_MAX);
            cases += 3U;
        }
    }
    printf("Independent scene growth: %zu capacity/alias/failure/fallback cases passed\n", cases);
    return 0;
}

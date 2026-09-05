#include "ge_dam_dynamic_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GeDamOwnedRoomBlobs {
    uint8_t *points;
    uint8_t *primary;
    uint8_t *secondary;
} GeDamOwnedRoomBlobs;

static void ge_dam_candidate_close(GeDamDynamicSceneTransaction *candidate)
{
    if (candidate == NULL) return;
    free(candidate->batches);
    free(candidate->vertices);
    free(candidate->room_ranges);
    memset(candidate, 0, sizeof(*candidate));
}

static void ge_dam_refresh_overlay_vertex_alias(GeDamDynamicScene *cache)
{
    if (cache->overlay_vertex_count == 0U) {
        cache->overlay_vertices = NULL;
        return;
    }
    cache->overlay_vertices = cache->vertices
        + (cache->scene.vertex_count - cache->overlay_vertex_count);
}

static void ge_dam_owned_rooms_close(GeDamOwnedRoomBlobs *rooms,
                                     size_t room_count)
{
    size_t index;

    if (rooms == NULL) return;
    for (index = 0U; index < room_count; ++index) {
        free(rooms[index].secondary);
        free(rooms[index].primary);
        free(rooms[index].points);
    }
    free(rooms);
}

static GeDamDynamicSceneStatus ge_dam_read_asset(
    GeAssetPack *pack, const char *path, int required,
    uint8_t **data, size_t *size)
{
    const GeAssetPackEntry *entry;
    size_t bytes_read;

    *data = NULL;
    *size = 0U;
    entry = ge_asset_pack_find(pack, path);
    if (entry == NULL) {
        return required ? GE_DAM_DYNAMIC_SCENE_ASSET_NOT_FOUND
            : GE_DAM_DYNAMIC_SCENE_OK;
    }
    if (entry->data_size == 0U || entry->data_size > SIZE_MAX) {
        return required ? GE_DAM_DYNAMIC_SCENE_ASSET_INVALID
            : GE_DAM_DYNAMIC_SCENE_OK;
    }
    *data = malloc((size_t)entry->data_size);
    if (*data == NULL) return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
    if (ge_asset_pack_read(pack, path, *data, (size_t)entry->data_size,
            &bytes_read) != GE_ASSET_PACK_OK
            || bytes_read != (size_t)entry->data_size) {
        free(*data);
        *data = NULL;
        return GE_DAM_DYNAMIC_SCENE_ASSET_INVALID;
    }
    *size = bytes_read;
    return GE_DAM_DYNAMIC_SCENE_OK;
}

static int ge_dam_room_range_valid(const GeDamDynamicScene *cache,
    const GeDamDynamicRoomRange *range)
{
    size_t vertices, batches;
    if (cache->overlay_vertex_count > cache->scene.vertex_count
            || cache->overlay_batch_count > cache->scene.batch_count)
        return 0;
    vertices = cache->scene.vertex_count - cache->overlay_vertex_count;
    batches = cache->scene.batch_count - cache->overlay_batch_count;
    return range->scene.status == GE_DAM_ROOM_OK
        && range->scene.vertex_count == range->scene.required_vertex_count
        && range->scene.batch_count == range->scene.required_batch_count
        && range->first_vertex <= vertices
        && range->scene.vertex_count <= vertices - range->first_vertex
        && range->first_batch <= batches
        && range->scene.batch_count <= batches - range->first_batch
        && (range->scene.vertex_count == 0U || cache->vertices != NULL)
        && (range->scene.batch_count == 0U || cache->batches != NULL);
}

static int ge_dam_accumulate_room_scene(GeDamRoomScene *total,
    const GeDamRoomScene *room)
{
    if (room->required_vertex_count > SIZE_MAX - total->required_vertex_count
            || room->required_batch_count > SIZE_MAX - total->required_batch_count
            || room->triangle_count > SIZE_MAX - total->triangle_count
            || room->list_count > SIZE_MAX - total->list_count
            || room->commands_visited > SIZE_MAX - total->commands_visited
            || room->unsupported_commands > SIZE_MAX - total->unsupported_commands)
        return 0;
    total->required_vertex_count += room->required_vertex_count;
    total->required_batch_count += room->required_batch_count;
    total->triangle_count += room->triangle_count;
    total->list_count += room->list_count;
    total->commands_visited += room->commands_visited;
    total->unsupported_commands += room->unsupported_commands;
    return 1;
}

/* Each authored room starts independent primary/secondary GBI contexts.
 * Preserve their exact descriptor order, but reuse immutable resident slices
 * rather than reread and traverse those lists on every streaming transaction.
 * No geometry buffer or room lifetime is changed until the usual commit. */
static GeDamDynamicSceneStatus ge_dam_build_candidate(
    const GeDamDynamicScene *cache, const uint8_t *room_ids,
    size_t room_count, GeDamDynamicSceneTransaction *candidate)
{
    GeDamOwnedRoomBlobs *owned;
    GeDamRoomBlobDescriptor *descriptors;
    GeDamDynamicSceneStatus status = GE_DAM_DYNAMIC_SCENE_OK;
    GeDamRoomStatus build_status;
    GeDamRoomSceneStorage storage;
    size_t room_vertex_count = 0U;
    size_t room_batch_count = 0U;
    size_t total_vertex_count = 0U;
    size_t total_batch_count = 0U;
    size_t index;
    const GeDamDynamicRoomRange *retained[GE_DAM_WORLD_MAX_ROOMS] = {0};

    memset(candidate, 0, sizeof(*candidate));
    if (room_count > GE_DAM_WORLD_MAX_ROOMS)
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    owned = room_count != 0U ? calloc(room_count, sizeof(*owned)) : NULL;
    descriptors = room_count != 0U
        ? calloc(room_count, sizeof(*descriptors)) : NULL;
    candidate->room_ranges = room_count != 0U
        ? calloc(room_count, sizeof(*candidate->room_ranges)) : NULL;
    if (room_count != 0U && (owned == NULL || descriptors == NULL
            || candidate->room_ranges == NULL)) {
        free(descriptors);
        ge_dam_owned_rooms_close(owned, room_count);
        ge_dam_candidate_close(candidate);
        return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
    }
    for (index = 0U; index < room_count; ++index) {
        const uint8_t room_id = room_ids[index];
        const GeDamWorldRoom *world_room = ge_dam_world_room(
            &cache->world, room_id);
        GeDamRoomBlobDescriptor *descriptor = &descriptors[index];
        char path[128];
        size_t size;

        if (world_room == NULL) {
            status = GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
            break;
        }
        if (cache->initialized && cache->room_ranges != NULL) {
            size_t prior;
            for (prior = 0U; prior < cache->room_count; ++prior) {
                if (cache->room_ids[prior] == room_id) {
                    const GeDamDynamicRoomRange *range =
                        &cache->room_ranges[prior];
                    if (!ge_dam_room_range_valid(cache, range)) {
                        status = GE_DAM_DYNAMIC_SCENE_BUILD_FAILED;
                    } else {
                        retained[index] = range;
                    }
                    break;
                }
            }
            if (status != GE_DAM_DYNAMIC_SCENE_OK) break;
            if (retained[index] != NULL) continue;
        }
        if (ge_stage_asset_room_path(cache->stage_assets, room_id,
                GE_STAGE_ROOM_POINTS, path, sizeof(path))
                != GE_STAGE_ASSET_OK) {
            status = GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
            break;
        }
        /* Canonical bgLoadRoomModelData treats a zero compressed point-table
         * size as a valid room with vertices == NULL.  Streets and other
         * stages use these authored no-geometry rooms for portal/logic
         * partitioning, so the extracted point-table entry is optional. */
        status = ge_dam_read_asset(cache->pack, path, 0,
            &owned[index].points, &size);
        if (status != GE_DAM_DYNAMIC_SCENE_OK) break;
        descriptor->point_table_size = size;
        if (ge_stage_asset_room_path(cache->stage_assets, room_id,
                GE_STAGE_ROOM_PRIMARY_GDL, path, sizeof(path))
                != GE_STAGE_ASSET_OK) {
            status = GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
            break;
        }
        status = ge_dam_read_asset(cache->pack, path, 1,
            &owned[index].primary, &size);
        if (status != GE_DAM_DYNAMIC_SCENE_OK) break;
        descriptor->primary_gdl_size = size;
        if (ge_stage_asset_room_path(cache->stage_assets, room_id,
                GE_STAGE_ROOM_SECONDARY_GDL, path, sizeof(path))
                != GE_STAGE_ASSET_OK) {
            status = GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
            break;
        }
        status = ge_dam_read_asset(cache->pack, path, 0,
            &owned[index].secondary, &size);
        if (status != GE_DAM_DYNAMIC_SCENE_OK) break;
        descriptor->secondary_gdl_size = size;
        descriptor->room_id = room_id;
        memcpy(descriptor->origin, world_room->origin,
               sizeof(descriptor->origin));
        descriptor->point_table = owned[index].points;
        descriptor->primary_gdl = owned[index].primary;
        descriptor->secondary_gdl = owned[index].secondary;
    }
    if (status == GE_DAM_DYNAMIC_SCENE_OK) {
        candidate->scene.room_count = room_count;
        for (index = 0U; index < room_count; ++index) {
            GeDamDynamicRoomRange *range = &candidate->room_ranges[index];
            if (retained[index] != NULL) {
                range->scene = retained[index]->scene;
                ++candidate->room_geometry_reuses;
            } else {
                build_status = ge_dam_rooms_build(&descriptors[index], 1U,
                    NULL, NULL, &range->scene);
                if (build_status != GE_DAM_ROOM_CAPACITY_EXCEEDED
                        && build_status != GE_DAM_ROOM_OK) {
                    status = GE_DAM_DYNAMIC_SCENE_BUILD_FAILED;
                    break;
                }
                ++candidate->room_geometry_decodes;
            }
            range->first_vertex = candidate->scene.required_vertex_count;
            range->first_batch = candidate->scene.required_batch_count;
            if (!ge_dam_accumulate_room_scene(&candidate->scene, &range->scene)) {
                status = GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
                break;
            }
        }
        if (status == GE_DAM_DYNAMIC_SCENE_OK && (cache->overlay_vertex_count
                    > SIZE_MAX - candidate->scene.required_vertex_count
                || cache->overlay_batch_count
                    > SIZE_MAX - candidate->scene.required_batch_count)) {
            status = GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
        } else if (status == GE_DAM_DYNAMIC_SCENE_OK) {
            room_vertex_count = candidate->scene.required_vertex_count;
            room_batch_count = candidate->scene.required_batch_count;
            total_vertex_count = room_vertex_count
                + cache->overlay_vertex_count;
            total_batch_count = room_batch_count + cache->overlay_batch_count;
        }
        if (status == GE_DAM_DYNAMIC_SCENE_OK
                && total_vertex_count > cache->limits.vertex_capacity) {
            status = GE_DAM_DYNAMIC_SCENE_VERTEX_CAPACITY;
        } else if (status == GE_DAM_DYNAMIC_SCENE_OK
                && total_batch_count > cache->limits.batch_capacity) {
            status = GE_DAM_DYNAMIC_SCENE_BATCH_CAPACITY;
        } else if (status == GE_DAM_DYNAMIC_SCENE_OK
                && (total_vertex_count
                    > SIZE_MAX / sizeof(*candidate->vertices)
                || total_batch_count > SIZE_MAX / sizeof(*candidate->batches))) {
            status = GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
        }
    }
    if (status == GE_DAM_DYNAMIC_SCENE_OK) {
        candidate->vertices = total_vertex_count != 0U
            ? malloc(total_vertex_count * sizeof(*candidate->vertices)) : NULL;
        candidate->batches = total_batch_count != 0U
            ? malloc(total_batch_count * sizeof(*candidate->batches)) : NULL;
        if ((total_vertex_count != 0U && candidate->vertices == NULL)
                || (total_batch_count != 0U && candidate->batches == NULL)) {
            status = GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
        }
    }
    if (status == GE_DAM_DYNAMIC_SCENE_OK) {
        for (index = 0U; index < room_count; ++index) {
            GeDamDynamicRoomRange *range = &candidate->room_ranges[index];
            const GeDamDynamicRoomRange *prior = retained[index];
            size_t batch_index;
            storage.vertices = candidate->vertices != NULL
                ? candidate->vertices + range->first_vertex : NULL;
            storage.vertex_capacity = range->scene.required_vertex_count;
            storage.batches = candidate->batches != NULL
                ? candidate->batches + range->first_batch : NULL;
            storage.batch_capacity = range->scene.required_batch_count;
            if (prior != NULL) {
                range->world_bounds = prior->world_bounds;
                if (storage.vertex_capacity != 0U)
                    memcpy(storage.vertices, cache->vertices + prior->first_vertex,
                        storage.vertex_capacity * sizeof(*storage.vertices));
                if (storage.batch_capacity != 0U)
                    memcpy(storage.batches, cache->batches + prior->first_batch,
                        storage.batch_capacity * sizeof(*storage.batches));
            } else {
                build_status = ge_dam_rooms_build(&descriptors[index], 1U, NULL,
                    &storage, &range->scene);
                if (build_status != GE_DAM_ROOM_OK) {
                    status = GE_DAM_DYNAMIC_SCENE_BUILD_FAILED;
                    break;
                }
                GeDamRoomDrawBatch span = {0};
                span.vertex_count = range->scene.vertex_count;
                (void)ge_draw_batch_world_bounds_build(storage.vertices,
                    storage.vertex_capacity, &span, &range->world_bounds);
            }
            for (batch_index = 0U; batch_index < storage.batch_capacity; ++batch_index) {
                GeDamRoomDrawBatch *batch = &storage.batches[batch_index];
                const size_t source_base = prior != NULL ? prior->first_vertex : 0U;
                if (batch->first_vertex < source_base
                        || batch->first_vertex - source_base > storage.vertex_capacity
                        || batch->vertex_count > storage.vertex_capacity - (batch->first_vertex - source_base)) {
                    status = GE_DAM_DYNAMIC_SCENE_BUILD_FAILED;
                    break;
                }
                batch->first_vertex = range->first_vertex + (batch->first_vertex - source_base);
                if (batch->room_id != room_ids[index]
                        || batch->coordinate_space != GE_DAM_ROOM_COORDINATE_AUTHORED)
                    range->world_bounds.valid = 0;
            }
            if (status != GE_DAM_DYNAMIC_SCENE_OK) break;
        }
        candidate->scene.vertex_count = room_vertex_count;
        candidate->scene.batch_count = room_batch_count;
        if (status == GE_DAM_DYNAMIC_SCENE_OK && (cache->overlay_vertex_count != 0U
                || cache->overlay_batch_count != 0U)) {
            size_t batch_index;
            if (cache->overlay_vertex_count != 0U)
                memcpy(candidate->vertices + room_vertex_count,
                   cache->overlay_vertices,
                   cache->overlay_vertex_count
                       * sizeof(*candidate->vertices));
            if (cache->overlay_batch_count != 0U)
                memcpy(candidate->batches + room_batch_count,
                   cache->overlay_batches,
                   cache->overlay_batch_count
                       * sizeof(*candidate->batches));
            for (batch_index = 0U;
                    batch_index < cache->overlay_batch_count;
                    ++batch_index) {
                candidate->batches[room_batch_count + batch_index]
                    .first_vertex += room_vertex_count;
                candidate->scene.triangle_count +=
                    candidate->batches[room_batch_count + batch_index]
                        .triangle_count;
            }
            candidate->scene.vertex_count = total_vertex_count;
            candidate->scene.batch_count = total_batch_count;
            candidate->scene.required_vertex_count = total_vertex_count;
            candidate->scene.required_batch_count = total_batch_count;
        }
    }
    free(descriptors);
    ge_dam_owned_rooms_close(owned, room_count);
    if (status != GE_DAM_DYNAMIC_SCENE_OK) ge_dam_candidate_close(candidate);
    return status;
}

static void ge_dam_fail_pending(GeDamPreloadQueue *queue, uint8_t expected)
{
    uint8_t room;

    if (ge_dam_preload_queue_pop(queue, &room) == GE_DAM_PRELOAD_OK
            && room == expected) {
        (void)ge_dam_preload_queue_complete(queue, room, 0U);
    }
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_init(
    GeDamDynamicScene *cache,
    GeAssetPack *pack,
    const GeDamWorld *world,
    const uint8_t *initial_rooms,
    size_t initial_room_count,
    const GeDamDynamicSceneLimits *limits)
{
    return ge_dam_dynamic_scene_init_for_stage(
        cache, pack, ge_stage_asset_dam(), world, initial_rooms,
        initial_room_count, limits);
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_init_for_stage(
    GeDamDynamicScene *cache,
    GeAssetPack *pack,
    const GeStageAssetDescriptor *stage,
    const GeDamWorld *world,
    const uint8_t *initial_rooms,
    size_t initial_room_count,
    const GeDamDynamicSceneLimits *limits)
{
    GeDamDynamicScene next;
    GeDamDynamicSceneTransaction candidate;
    GeDamDynamicSceneStatus status;
    size_t index;

    if (cache == NULL || pack == NULL || stage == NULL || world == NULL
            || initial_rooms == NULL || initial_room_count == 0U
            || limits == NULL || limits->room_capacity == 0U
            || limits->room_capacity > GE_DAM_WORLD_MAX_ROOMS
            || initial_room_count > limits->room_capacity
            || limits->vertex_capacity == 0U
            || limits->batch_capacity == 0U) {
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    memset(&next, 0, sizeof(next));
    next.pack = pack;
    next.stage_assets = stage;
    next.world = *world;
    next.limits = *limits;
    for (index = 0U; index < initial_room_count; ++index) {
        const uint8_t room = initial_rooms[index];

        if ((size_t)room >= world->room_count || next.resident[room] != 0U) {
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        }
        next.room_ids[index] = room;
        next.resident[room] = 1U;
        next.room_age[room] = 1U;
    }
    status = ge_dam_build_candidate(&next, next.room_ids,
                                    initial_room_count, &candidate);
    if (status != GE_DAM_DYNAMIC_SCENE_OK) return status;
    next.room_count = initial_room_count;
    next.vertices = candidate.vertices;
    next.batches = candidate.batches;
    next.room_ranges = candidate.room_ranges;
    next.room_geometry_decodes = candidate.room_geometry_decodes;
    next.room_geometry_reuses = candidate.room_geometry_reuses;
    next.scene = candidate.scene;
    next.vertex_storage_capacity = next.scene.vertex_count;
    next.batch_storage_capacity = next.scene.batch_count;
    next.initialized = 1U;
    *cache = next;
    return GE_DAM_DYNAMIC_SCENE_OK;
}

static GeDamDynamicSceneStatus ge_dam_dynamic_scene_prepare_replacement(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue,
    GeDamDynamicSceneTransaction *transaction, uint8_t includes_request)
{
    GeDamDynamicSceneStatus status;
    GeDamDynamicSceneTransaction built;
    uint8_t candidate_rooms[GE_DAM_WORLD_MAX_ROOMS];
    uint8_t room = UINT8_MAX;
    size_t candidate_count = 0U;
    size_t index;

    if (cache == NULL || queue == NULL || transaction == NULL
            || cache->initialized == 0U) {
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    memset(transaction, 0, sizeof(*transaction));
    if (includes_request != 0U) {
        if (ge_dam_preload_queue_peek(queue, &room) != GE_DAM_PRELOAD_OK)
            return GE_DAM_DYNAMIC_SCENE_NO_REQUEST;
        ++cache->install_attempts;
        cache->last_requested_room = room;
        if ((size_t)room >= cache->world.room_count
                || cache->resident[room] != 0U) {
            ++cache->install_failures;
            ge_dam_fail_pending(queue, room);
            return GE_DAM_DYNAMIC_SCENE_QUEUE_ERROR;
        }
    }
    for (index = 0U; index < cache->room_count; ++index) {
        const uint8_t resident_room = cache->room_ids[index];
        if (cache->eviction_pending[resident_room] != 0U) {
            transaction->evicted_rooms[transaction->evicted_count++] =
                resident_room;
        } else {
            candidate_rooms[candidate_count++] = resident_room;
        }
    }
    if (transaction->evicted_count != 0U) ++cache->eviction_attempts;
    if (includes_request != 0U) {
        if (candidate_count >= cache->limits.room_capacity) {
            ++cache->install_failures;
            ge_dam_fail_pending(queue, room);
            return GE_DAM_DYNAMIC_SCENE_ROOM_CAPACITY;
        }
        candidate_rooms[candidate_count++] = room;
    } else if (transaction->evicted_count == 0U) {
        return GE_DAM_DYNAMIC_SCENE_NO_REQUEST;
    }
    status = ge_dam_build_candidate(cache, candidate_rooms,
                                    candidate_count, &built);
    if (status != GE_DAM_DYNAMIC_SCENE_OK) {
        if (includes_request != 0U) {
            ++cache->install_failures;
            ge_dam_fail_pending(queue, room);
        }
        if (transaction->evicted_count != 0U) ++cache->eviction_failures;
        return status;
    }
    transaction->vertices = built.vertices;
    transaction->batches = built.batches;
    transaction->room_ranges = built.room_ranges;
    transaction->room_geometry_decodes = built.room_geometry_decodes;
    transaction->room_geometry_reuses = built.room_geometry_reuses;
    transaction->scene = built.scene;
    memcpy(transaction->room_ids, candidate_rooms, candidate_count);
    transaction->room_count = candidate_count;
    transaction->room = room;
    transaction->includes_request = includes_request;
    transaction->prepared = 1U;
    return GE_DAM_DYNAMIC_SCENE_OK;
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_reserve_overlay(
    GeDamDynamicScene *cache, size_t vertex_capacity, size_t batch_capacity)
{
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches, *overlay;
    size_t total_vertices, total_batches;
    if (cache == NULL || !cache->initialized
            || cache->scene.vertex_count < cache->overlay_vertex_count
            || cache->scene.batch_count < cache->overlay_batch_count
            || vertex_capacity < cache->overlay_vertex_count
            || batch_capacity < cache->overlay_batch_count)
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    const size_t room_vertices = cache->scene.vertex_count - cache->overlay_vertex_count;
    const size_t room_batches = cache->scene.batch_count - cache->overlay_batch_count;
    if (room_vertices > cache->limits.vertex_capacity
            || vertex_capacity > cache->limits.vertex_capacity - room_vertices)
        return GE_DAM_DYNAMIC_SCENE_VERTEX_CAPACITY;
    if (room_batches > cache->limits.batch_capacity
            || batch_capacity > cache->limits.batch_capacity - room_batches)
        return GE_DAM_DYNAMIC_SCENE_BATCH_CAPACITY;
    total_vertices = room_vertices + vertex_capacity;
    total_batches = room_batches + batch_capacity;
    if (total_vertices > SIZE_MAX / sizeof(*vertices)
            || total_batches > SIZE_MAX / sizeof(*batches)
            || batch_capacity > SIZE_MAX / sizeof(*overlay))
        return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
    vertices = cache->vertices;
    batches = cache->batches;
    overlay = cache->overlay_batches;
    /* All allocations finish before any borrowed pointer is invalidated. */
    if (total_vertices > cache->vertex_storage_capacity) {
        vertices = malloc(total_vertices * sizeof(*vertices));
        if (vertices == NULL) goto fail;
    }
    if (total_batches > cache->batch_storage_capacity) {
        batches = malloc(total_batches * sizeof(*batches));
        if (batches == NULL) goto fail;
    }
    if (batch_capacity > cache->overlay_batch_storage_capacity) {
        overlay = malloc(batch_capacity * sizeof(*overlay));
        if (overlay == NULL) goto fail;
    }
    if (vertices == cache->vertices && batches == cache->batches
            && overlay == cache->overlay_batches) return GE_DAM_DYNAMIC_SCENE_OK;
    if (vertices != cache->vertices) {
        if (cache->scene.vertex_count != 0U)
            memcpy(vertices, cache->vertices, cache->scene.vertex_count * sizeof(*vertices));
        free(cache->vertices);
        cache->vertices = vertices;
        cache->vertex_storage_capacity = total_vertices;
    }
    if (batches != cache->batches) {
        if (cache->scene.batch_count != 0U)
            memcpy(batches, cache->batches, cache->scene.batch_count * sizeof(*batches));
        free(cache->batches);
        cache->batches = batches;
        cache->batch_storage_capacity = total_batches;
    }
    if (overlay != cache->overlay_batches) {
        if (cache->overlay_batch_count != 0U)
            memcpy(overlay, cache->overlay_batches, cache->overlay_batch_count * sizeof(*overlay));
        free(cache->overlay_batches);
        cache->overlay_batches = overlay;
        cache->overlay_batch_storage_capacity = batch_capacity;
    }
    ge_dam_refresh_overlay_vertex_alias(cache);
    ++cache->generation;
    return GE_DAM_DYNAMIC_SCENE_OK;
fail:
    if (vertices != cache->vertices) free(vertices);
    if (batches != cache->batches) free(batches);
    if (overlay != cache->overlay_batches) free(overlay);
    return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_prepare_next(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue,
    GeDamDynamicSceneTransaction *transaction)
{
    return ge_dam_dynamic_scene_prepare_replacement(
        cache, queue, transaction, 1U);
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_commit(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue,
    GeDamDynamicSceneTransaction *transaction)
{
    uint8_t pending = UINT8_MAX;
    uint8_t popped = UINT8_MAX;
    const uint8_t room = transaction != NULL ? transaction->room : 0U;
    GeDamPreloadQueue old_queue;
    GeDamRoomWorldVertex *old_vertices;
    GeDamRoomDrawBatch *old_batches;
    uint8_t old_age[GE_DAM_WORLD_MAX_ROOMS];
    size_t index;

    if (cache == NULL || queue == NULL || transaction == NULL
            || cache->initialized == 0U || transaction->prepared == 0U
            || (transaction->scene.vertex_count != 0U
                && transaction->vertices == NULL)
            || (transaction->scene.batch_count != 0U
                && transaction->batches == NULL)
            || (transaction->room_count != 0U
                && transaction->room_ranges == NULL)
            || transaction->room_count > cache->limits.room_capacity) {
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    if (transaction->includes_request != 0U
            && (ge_dam_preload_queue_peek(queue, &pending)
                    != GE_DAM_PRELOAD_OK || pending != room)) {
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    old_queue = *queue;
    if (transaction->includes_request != 0U) {
        if (ge_dam_preload_queue_pop(queue, &popped) != GE_DAM_PRELOAD_OK
                || popped != room
                || ge_dam_preload_queue_complete(queue, room, 1U)
                    != GE_DAM_PRELOAD_OK) {
            *queue = old_queue;
            ge_dam_candidate_close(transaction);
            ++cache->install_failures;
            return GE_DAM_DYNAMIC_SCENE_QUEUE_ERROR;
        }
    }
    if (ge_dam_preload_queue_evict_resident(
            queue, transaction->evicted_rooms, transaction->evicted_count)
            != GE_DAM_PRELOAD_OK) {
        const uint8_t failed_install = transaction->includes_request;
        const size_t failed_evictions = transaction->evicted_count;
        *queue = old_queue;
        ge_dam_candidate_close(transaction);
        if (failed_install != 0U) ++cache->install_failures;
        if (failed_evictions != 0U) ++cache->eviction_failures;
        return GE_DAM_DYNAMIC_SCENE_QUEUE_ERROR;
    }
    old_vertices = cache->vertices;
    old_batches = cache->batches;
    memcpy(old_age, cache->room_age, sizeof(old_age));
    cache->vertices = transaction->vertices;
    cache->batches = transaction->batches;
    free(cache->room_ranges);
    cache->room_ranges = transaction->room_ranges;
    cache->room_geometry_decodes += transaction->room_geometry_decodes;
    cache->room_geometry_reuses += transaction->room_geometry_reuses;
    cache->scene = transaction->scene;
    cache->vertex_storage_capacity = cache->scene.vertex_count;
    cache->batch_storage_capacity = cache->scene.batch_count;
    ge_dam_refresh_overlay_vertex_alias(cache);
    memset(cache->room_ids, 0, sizeof(cache->room_ids));
    memset(cache->resident, 0, sizeof(cache->resident));
    memset(cache->room_age, 0, sizeof(cache->room_age));
    memset(cache->eviction_pending, 0, sizeof(cache->eviction_pending));
    memcpy(cache->room_ids, transaction->room_ids, transaction->room_count);
    cache->room_count = transaction->room_count;
    for (index = 0U; index < cache->room_count; ++index) {
        const uint8_t resident_room = cache->room_ids[index];
        cache->resident[resident_room] = 1U;
        cache->room_age[resident_room] =
            transaction->includes_request != 0U && resident_room == room
            ? 1U : old_age[resident_room];
    }
    ++cache->generation;
    if (transaction->includes_request != 0U) ++cache->install_successes;
    if (transaction->evicted_count != 0U) {
        ++cache->eviction_successes;
        cache->rooms_evicted += transaction->evicted_count;
    }
    memset(transaction, 0, sizeof(*transaction));
    free(old_batches);
    free(old_vertices);
    return GE_DAM_DYNAMIC_SCENE_OK;
}

void ge_dam_dynamic_scene_abort(
    GeDamDynamicScene *cache, GeDamDynamicSceneTransaction *transaction)
{
    if (transaction == NULL) return;
    if (transaction->prepared != 0U && cache != NULL) {
        if (transaction->includes_request != 0U) ++cache->install_failures;
        if (transaction->evicted_count != 0U) ++cache->eviction_failures;
    }
    ge_dam_candidate_close(transaction);
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_install_next(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue)
{
    GeDamDynamicSceneTransaction transaction;
    GeDamDynamicSceneStatus status = ge_dam_dynamic_scene_prepare_next(
        cache, queue, &transaction);

    if (status != GE_DAM_DYNAMIC_SCENE_OK) return status;
    return ge_dam_dynamic_scene_commit(cache, queue, &transaction);
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_age_visibility(
    GeDamDynamicScene *cache,
    const uint8_t *rendered_rooms, size_t rendered_room_count)
{
    uint8_t rendered[GE_DAM_WORLD_MAX_ROOMS] = {0};
    size_t index;

    if (cache == NULL || cache->initialized == 0U
            || (rendered_room_count != 0U && rendered_rooms == NULL))
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    for (index = 0U; index < rendered_room_count; ++index) {
        const uint8_t room = rendered_rooms[index];
        if ((size_t)room >= cache->world.room_count)
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        rendered[room] = 1U;
    }
    for (index = 0U; index < cache->room_count; ++index) {
        const uint8_t room = cache->room_ids[index];
        if (rendered[room] != 0U) {
            cache->room_age[room] = 1U;
            cache->eviction_pending[room] = 0U;
        } else if (cache->eviction_pending[room] == 0U) {
            if (cache->room_age[room] == 4U) {
                cache->eviction_pending[room] = 1U;
            } else if (cache->room_age[room] >= 1U
                    && cache->room_age[room] < 4U) {
                ++cache->room_age[room];
            } else {
                return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
            }
        }
    }
    return GE_DAM_DYNAMIC_SCENE_OK;
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_tick_visibility(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue,
    const uint8_t *rendered_rooms, size_t rendered_room_count)
{
    GeDamDynamicSceneTransaction transaction;
    GeDamDynamicSceneStatus status;
    uint8_t requested;
    uint8_t includes_request;
    GeDamPreloadStatus preload_status;
    size_t index;
    size_t pending_evictions = 0U;

    if (cache == NULL || queue == NULL || cache->initialized == 0U
            || queue->room_count != cache->world.room_count)
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    status = ge_dam_dynamic_scene_age_visibility(
        cache, rendered_rooms, rendered_room_count);
    if (status != GE_DAM_DYNAMIC_SCENE_OK) return status;
    for (index = 0U; index < cache->room_count; ++index) {
        if (cache->eviction_pending[cache->room_ids[index]] != 0U)
            ++pending_evictions;
    }
    preload_status = ge_dam_preload_queue_peek(queue, &requested);
    if (preload_status != GE_DAM_PRELOAD_OK
            && preload_status != GE_DAM_PRELOAD_EMPTY)
        return GE_DAM_DYNAMIC_SCENE_QUEUE_ERROR;
    includes_request = preload_status == GE_DAM_PRELOAD_OK;
    if (includes_request == 0U && pending_evictions == 0U)
        return GE_DAM_DYNAMIC_SCENE_OK;
    status = ge_dam_dynamic_scene_prepare_replacement(
        cache, queue, &transaction, includes_request);
    if (status != GE_DAM_DYNAMIC_SCENE_OK) return status;
    return ge_dam_dynamic_scene_commit(cache, queue, &transaction);
}

static void *ge_dam_allocate_overlay_capacity(
    size_t required, size_t *capacity, size_t element_size)
{
    void *allocation = malloc(*capacity * element_size);
    /* Headroom is optional: memory pressure must not reject a scene that
     * would fit with the original exact-size allocation policy. */
    if (allocation == NULL && *capacity > required) {
        *capacity = required;
        allocation = malloc(required * element_size);
    }
    return allocation;
}

static int ge_dam_memory_overlaps(
    const void *left, size_t left_bytes, const void *right, size_t right_bytes)
{
    const uintptr_t left_start = (uintptr_t)left;
    const uintptr_t right_start = (uintptr_t)right;
    if (left_bytes == 0U || right_bytes == 0U) return 0;
    /* Subtraction avoids overflowing an address + size interval endpoint. */
    return left_start >= right_start ? left_start - right_start < right_bytes
        : right_start - left_start < left_bytes;
}

static int ge_dam_replacement_source_overlaps(
    const GeDamDynamicScene *cache, const void *source, size_t source_bytes)
{
    return ge_dam_memory_overlaps(source, source_bytes, cache->vertices,
               cache->vertex_storage_capacity * sizeof(*cache->vertices))
        || ge_dam_memory_overlaps(source, source_bytes, cache->batches,
               cache->batch_storage_capacity * sizeof(*cache->batches))
        || ge_dam_memory_overlaps(source, source_bytes, cache->overlay_batches,
               cache->overlay_batch_storage_capacity * sizeof(*cache->overlay_batches));
}

static GeDamDynamicSceneStatus ge_dam_replace_overlay_segment(
    GeDamDynamicScene *cache,
    size_t overlay_vertex_offset, size_t old_vertex_count,
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    size_t overlay_batch_offset, size_t old_batch_count,
    const GeDamRoomDrawBatch *batches, size_t batch_count);

GeDamDynamicSceneStatus ge_dam_dynamic_scene_set_overlay(
    GeDamDynamicScene *cache,
    const GeDamRoomWorldVertex *vertices,
    size_t vertex_count,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count)
{
    if (cache == NULL || cache->initialized == 0U
            || (vertex_count != 0U && vertices == NULL)
            || (batch_count != 0U && batches == NULL)
            || (vertex_count == 0U) != (batch_count == 0U)
            || vertex_count > cache->limits.vertex_capacity
            || batch_count > cache->limits.batch_capacity)
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    /* Only the overlay changes. Residency transactions own room decoding;
     * rereading all resident GBI here made even one prop topology change
     * stall for an entire world rebuild. Share the atomic segment writer,
     * retaining the historical set_overlay counter semantics. */
    return ge_dam_replace_overlay_segment(cache,
        0U, cache->overlay_vertex_count, vertices, vertex_count,
        0U, cache->overlay_batch_count, batches, batch_count);
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_update_overlay_segment(
    GeDamDynamicScene *cache,
    size_t overlay_vertex_offset,
    const GeDamRoomWorldVertex *vertices,
    size_t vertex_count,
    size_t overlay_batch_offset,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count)
{
    size_t room_vertex_count;
    size_t room_batch_count;
    size_t old_triangles = 0U;
    size_t new_triangles = 0U;
    size_t index;

    if (cache != NULL) cache->overlay_update_attempts++;
    if (cache == NULL || cache->initialized == 0U
            || vertices == NULL || batches == NULL
            || vertex_count == 0U || batch_count == 0U
            || overlay_vertex_offset > cache->overlay_vertex_count
            || vertex_count > cache->overlay_vertex_count
                - overlay_vertex_offset
            || overlay_batch_offset > cache->overlay_batch_count
            || batch_count > cache->overlay_batch_count
                - overlay_batch_offset
            || cache->scene.vertex_count < cache->overlay_vertex_count
            || cache->scene.batch_count < cache->overlay_batch_count) {
        if (cache != NULL) cache->overlay_update_failures++;
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    room_vertex_count = cache->scene.vertex_count
        - cache->overlay_vertex_count;
    room_batch_count = cache->scene.batch_count
        - cache->overlay_batch_count;
    for (index = 0U; index < batch_count; index++) {
        const GeDamRoomDrawBatch *batch = &batches[index];
        const GeDamRoomDrawBatch *old =
            &cache->overlay_batches[overlay_batch_offset + index];
        if (batch->first_vertex > vertex_count
                || batch->vertex_count
                    > vertex_count - batch->first_vertex
                || batch->first_vertex > SIZE_MAX - overlay_vertex_offset
                || batch->first_vertex + overlay_vertex_offset
                    > cache->overlay_vertex_count
                || batch->vertex_count > cache->overlay_vertex_count
                    - (batch->first_vertex + overlay_vertex_offset)
                || old->triangle_count > SIZE_MAX - old_triangles
                || batch->triangle_count > SIZE_MAX - new_triangles) {
            cache->overlay_update_failures++;
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        }
        old_triangles += old->triangle_count;
        new_triangles += batch->triangle_count;
    }
    if (old_triangles > cache->scene.triangle_count
            || new_triangles > SIZE_MAX
                - (cache->scene.triangle_count - old_triangles)) {
        cache->overlay_update_failures++;
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }

    /* overlay_vertices is the exact tail of vertices; one write updates both
     * the retained rebuild source and the renderer publication. */
    memcpy(cache->overlay_vertices + overlay_vertex_offset,
           vertices, vertex_count * sizeof(*vertices));
    for (index = 0U; index < batch_count; index++) {
        GeDamRoomDrawBatch overlay = batches[index];
        GeDamRoomDrawBatch published = batches[index];
        overlay.first_vertex += overlay_vertex_offset;
        published.first_vertex += room_vertex_count + overlay_vertex_offset;
        cache->overlay_batches[overlay_batch_offset + index] = overlay;
        cache->batches[room_batch_count + overlay_batch_offset + index]
            = published;
    }
    cache->scene.triangle_count = cache->scene.triangle_count
        - old_triangles + new_triangles;
    cache->scene.required_vertex_count = cache->scene.vertex_count;
    cache->scene.required_batch_count = cache->scene.batch_count;
    cache->overlay_update_successes++;
    cache->generation++;
    return GE_DAM_DYNAMIC_SCENE_OK;
}

static GeDamDynamicSceneStatus ge_dam_replace_overlay_segment(
    GeDamDynamicScene *cache,
    size_t overlay_vertex_offset,
    size_t old_vertex_count,
    const GeDamRoomWorldVertex *vertices,
    size_t vertex_count,
    size_t overlay_batch_offset,
    size_t old_batch_count,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count)
{
    GeDamRoomWorldVertex *new_vertices = NULL;
    GeDamRoomDrawBatch *new_batches = NULL;
    GeDamRoomDrawBatch *new_overlay_batches = NULL;
    size_t room_vertex_count;
    size_t room_batch_count;
    size_t new_overlay_vertex_count;
    size_t new_overlay_batch_count;
    size_t total_vertex_count;
    size_t total_batch_count;
    size_t old_vertex_end;
    size_t old_batch_end;
    size_t vertex_allocation;
    size_t batch_allocation;
    size_t overlay_batch_allocation;
    size_t index;
    size_t triangle_count = 0U;
    int replaces_tail;
    int disjoint_sources;

    if (cache == NULL || cache->initialized == 0U
            || (vertex_count != 0U && vertices == NULL)
            || (batch_count != 0U && batches == NULL)
            || (vertex_count == 0U) != (batch_count == 0U)
            || (old_vertex_count == 0U) != (old_batch_count == 0U)
            || overlay_vertex_offset > cache->overlay_vertex_count
            || old_vertex_count > cache->overlay_vertex_count
                - overlay_vertex_offset
            || overlay_batch_offset > cache->overlay_batch_count
            || old_batch_count > cache->overlay_batch_count
                - overlay_batch_offset
            || cache->scene.vertex_count < cache->overlay_vertex_count
            || cache->scene.batch_count < cache->overlay_batch_count
            || cache->overlay_vertex_count - old_vertex_count
                > SIZE_MAX - vertex_count
            || cache->overlay_batch_count - old_batch_count
                > SIZE_MAX - batch_count) {
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    room_vertex_count = cache->scene.vertex_count
        - cache->overlay_vertex_count;
    room_batch_count = cache->scene.batch_count
        - cache->overlay_batch_count;
    new_overlay_vertex_count = cache->overlay_vertex_count
        - old_vertex_count + vertex_count;
    new_overlay_batch_count = cache->overlay_batch_count
        - old_batch_count + batch_count;
    if (room_vertex_count > SIZE_MAX - new_overlay_vertex_count
            || room_batch_count > SIZE_MAX - new_overlay_batch_count) {
        return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
    }
    total_vertex_count = room_vertex_count + new_overlay_vertex_count;
    total_batch_count = room_batch_count + new_overlay_batch_count;
    if (total_vertex_count > cache->limits.vertex_capacity) {
        return GE_DAM_DYNAMIC_SCENE_VERTEX_CAPACITY;
    }
    if (total_batch_count > cache->limits.batch_capacity) {
        return GE_DAM_DYNAMIC_SCENE_BATCH_CAPACITY;
    }
    old_vertex_end = overlay_vertex_offset + old_vertex_count;
    old_batch_end = overlay_batch_offset + old_batch_count;
    for (index = 0U; index < cache->overlay_batch_count; ++index) {
        const GeDamRoomDrawBatch *batch = &cache->overlay_batches[index];
        const size_t batch_end = batch->first_vertex + batch->vertex_count;
        if (batch->first_vertex > cache->overlay_vertex_count
                || batch->vertex_count > cache->overlay_vertex_count
                    - batch->first_vertex
                || (index < overlay_batch_offset
                    && batch_end > overlay_vertex_offset)
                || (index >= overlay_batch_offset && index < old_batch_end
                    && (batch->first_vertex < overlay_vertex_offset
                        || batch_end > old_vertex_end))
                || (index >= old_batch_end
                    && batch->first_vertex < old_vertex_end)) {
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < batch_count; ++index) {
        if (batches[index].first_vertex > vertex_count
                || batches[index].vertex_count
                    > vertex_count - batches[index].first_vertex) {
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        }
    }
    if (total_vertex_count > SIZE_MAX / sizeof(*new_vertices)
            || total_batch_count > SIZE_MAX / sizeof(*new_batches)
            || new_overlay_batch_count > SIZE_MAX / sizeof(*new_overlay_batches))
        return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;

    /* Validate the complete published triangle sum before allocating or
     * modifying any retained buffer. Partial growth can reuse some buffers
     * even when others need new storage. */
    for (index = 0U; index < room_batch_count + overlay_batch_offset; ++index) {
        if (cache->batches[index].triangle_count > SIZE_MAX - triangle_count)
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        triangle_count += cache->batches[index].triangle_count;
    }
    for (index = 0U; index < batch_count; ++index) {
        if (batches[index].triangle_count > SIZE_MAX - triangle_count)
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        triangle_count += batches[index].triangle_count;
    }
    for (index = old_batch_end; index < cache->overlay_batch_count; ++index) {
        if (cache->overlay_batches[index].triangle_count > SIZE_MAX - triangle_count)
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        triangle_count += cache->overlay_batches[index].triangle_count;
    }
    disjoint_sources = !ge_dam_replacement_source_overlaps(cache,
            vertices, vertex_count * sizeof(*vertices))
        && !ge_dam_replacement_source_overlaps(cache,
            batches, batch_count * sizeof(*batches));
    replaces_tail = old_vertex_end == cache->overlay_vertex_count
        && old_batch_end == cache->overlay_batch_count;
    /* Reuse capacity for both guard-tail and articulated-prop changes. A
     * middle replacement shifts only its overlay suffix, never the resident
     * room prefix. Aliased middle inputs retain the transactional allocation
     * path below, since shifting that suffix could overwrite their source.
     * Tail inputs retain the existing alias-safe memmove behavior. */
    if (total_vertex_count <= cache->vertex_storage_capacity
            && total_batch_count <= cache->batch_storage_capacity
            && new_overlay_batch_count <= cache->overlay_batch_storage_capacity
            && (replaces_tail || disjoint_sources)) {
        /* All validation/failing work is complete before the first mutation.
         * Each suffix moves as one overlapping block; materials and vertex
         * attributes retain their bytes and authored ordering. */
        if (!replaces_tail) {
            const size_t vertex_suffix = cache->overlay_vertex_count - old_vertex_end;
            const size_t batch_suffix = cache->overlay_batch_count - old_batch_end;
            if (vertex_suffix != 0U && vertex_count != old_vertex_count)
                memmove(cache->vertices + room_vertex_count + overlay_vertex_offset
                        + vertex_count,
                    cache->vertices + room_vertex_count + old_vertex_end,
                    vertex_suffix * sizeof(*cache->vertices));
            if (batch_suffix != 0U && batch_count != old_batch_count)
                memmove(cache->overlay_batches + overlay_batch_offset + batch_count,
                    cache->overlay_batches + old_batch_end,
                    batch_suffix * sizeof(*cache->overlay_batches));
            if (vertex_count != old_vertex_count) {
                for (index = overlay_batch_offset + batch_count;
                        index < new_overlay_batch_count; ++index)
                    cache->overlay_batches[index].first_vertex =
                        cache->overlay_batches[index].first_vertex - old_vertex_end
                            + overlay_vertex_offset + vertex_count;
                cache->overlay_shifted_vertices += vertex_suffix;
            }
        }
        if (vertex_count != 0U)
            memmove(cache->vertices + room_vertex_count + overlay_vertex_offset,
                vertices, vertex_count * sizeof(*vertices));
        if (batch_count != 0U)
            memmove(cache->overlay_batches + overlay_batch_offset,
                batches, batch_count * sizeof(*batches));
        for (index = 0U; index < batch_count; ++index)
            cache->overlay_batches[overlay_batch_offset + index].first_vertex
                += overlay_vertex_offset;
        const size_t publish_end = vertex_count == old_vertex_count
                && batch_count == old_batch_count
            ? overlay_batch_offset + batch_count : new_overlay_batch_count;
        /* The two views differ only in their vertex-index origin. Copy the
         * contiguous payload once rather than issuing a large struct copy
         * for every batch, then patch exactly that field in the new view. */
        if (publish_end != overlay_batch_offset)
            memcpy(cache->batches + room_batch_count + overlay_batch_offset,
                cache->overlay_batches + overlay_batch_offset,
                (publish_end - overlay_batch_offset) * sizeof(*cache->batches));
        for (index = overlay_batch_offset; index < publish_end; ++index) {
            cache->batches[room_batch_count + index]
                .first_vertex += room_vertex_count;
        }
        ++cache->overlay_inplace_replacements;
        goto publish_counts;
    }

    /* Reserve one quarter of the overlay only, not the much larger room
     * prefix. This is allocation policy, not extra geometry or simulation.
     * Clamp to both the caller's limits and representable allocation sizes. */
    vertex_allocation = cache->limits.vertex_capacity;
    if (vertex_allocation > SIZE_MAX / sizeof(*new_vertices))
        vertex_allocation = SIZE_MAX / sizeof(*new_vertices);
    if (vertex_allocation - total_vertex_count > new_overlay_vertex_count / 4U)
        vertex_allocation = total_vertex_count + new_overlay_vertex_count / 4U;
    batch_allocation = cache->limits.batch_capacity;
    if (batch_allocation > SIZE_MAX / sizeof(*new_batches))
        batch_allocation = SIZE_MAX / sizeof(*new_batches);
    if (batch_allocation - total_batch_count > new_overlay_batch_count / 4U)
        batch_allocation = total_batch_count + new_overlay_batch_count / 4U;
    overlay_batch_allocation = batch_allocation - room_batch_count;
    /* Capacity is independent for vertices and the two batch views. In
     * particular, a new material batch must not force a copy of every room
     * vertex. Aliased sources keep all old buffers alive until publication.
     * Finish every allocation before writing to any reused buffer. */
    if (disjoint_sources && total_vertex_count <= cache->vertex_storage_capacity) {
        new_vertices = cache->vertices;
        vertex_allocation = cache->vertex_storage_capacity;
    } else if (total_vertex_count != 0U) {
        new_vertices = ge_dam_allocate_overlay_capacity(
            total_vertex_count, &vertex_allocation, sizeof(*new_vertices));
        if (new_vertices == NULL) goto no_memory;
    }
    if (disjoint_sources && total_batch_count <= cache->batch_storage_capacity) {
        new_batches = cache->batches;
        batch_allocation = cache->batch_storage_capacity;
    } else if (total_batch_count != 0U) {
        new_batches = ge_dam_allocate_overlay_capacity(
            total_batch_count, &batch_allocation, sizeof(*new_batches));
        if (new_batches == NULL) goto no_memory;
    }
    if (disjoint_sources && new_overlay_batch_count <= cache->overlay_batch_storage_capacity) {
        new_overlay_batches = cache->overlay_batches;
        overlay_batch_allocation = cache->overlay_batch_storage_capacity;
    } else if (new_overlay_batch_count != 0U) {
        new_overlay_batches = ge_dam_allocate_overlay_capacity(
            new_overlay_batch_count, &overlay_batch_allocation,
            sizeof(*new_overlay_batches));
        if (new_overlay_batches == NULL) goto no_memory;
    }

    if (room_vertex_count != 0U && new_vertices != cache->vertices)
        memcpy(new_vertices, cache->vertices,
            room_vertex_count * sizeof(*new_vertices));
    if (overlay_vertex_offset != 0U && new_vertices != cache->vertices)
        memcpy(new_vertices + room_vertex_count, cache->overlay_vertices,
            overlay_vertex_offset * sizeof(*new_vertices));
    if (cache->overlay_vertex_count != old_vertex_end
            && (new_vertices != cache->vertices || vertex_count != old_vertex_count))
        memmove(new_vertices + room_vertex_count + overlay_vertex_offset
                + vertex_count,
            cache->overlay_vertices + old_vertex_end,
            (cache->overlay_vertex_count - old_vertex_end)
                * sizeof(*new_vertices));
    if (vertex_count != 0U)
        memcpy(new_vertices + room_vertex_count + overlay_vertex_offset,
            vertices, vertex_count * sizeof(*new_vertices));

    if (overlay_batch_offset != 0U && new_overlay_batches != cache->overlay_batches)
        memcpy(new_overlay_batches, cache->overlay_batches,
            overlay_batch_offset * sizeof(*new_overlay_batches));
    if (cache->overlay_batch_count != old_batch_end
            && (new_overlay_batches != cache->overlay_batches || batch_count != old_batch_count))
        memmove(new_overlay_batches + overlay_batch_offset + batch_count,
            cache->overlay_batches + old_batch_end,
            (cache->overlay_batch_count - old_batch_end) * sizeof(*new_overlay_batches));
    for (index = overlay_batch_offset + batch_count;
            index < new_overlay_batch_count; ++index)
        new_overlay_batches[index].first_vertex =
            new_overlay_batches[index].first_vertex - old_vertex_end
                + overlay_vertex_offset + vertex_count;
    for (index = 0U; index < batch_count; ++index) {
        new_overlay_batches[overlay_batch_offset + index] = batches[index];
        new_overlay_batches[overlay_batch_offset + index].first_vertex +=
            overlay_vertex_offset;
    }

    if (room_batch_count != 0U && new_batches != cache->batches)
        memcpy(new_batches, cache->batches,
            room_batch_count * sizeof(*new_batches));
    /* A retained published view already owns the unchanged prop prefix.
     * Vertex-buffer growth does not change room/local index origins. */
    const size_t publish_start = new_batches == cache->batches
        ? overlay_batch_offset : 0U;
    const size_t publish_end = new_batches == cache->batches
            && vertex_count == old_vertex_count && batch_count == old_batch_count
        ? overlay_batch_offset + batch_count : new_overlay_batch_count;
    if (publish_end != publish_start)
        memcpy(new_batches + room_batch_count + publish_start,
            new_overlay_batches + publish_start,
            (publish_end - publish_start) * sizeof(*new_batches));
    for (index = publish_start; index < publish_end; ++index) {
        /* Range validation above bounds every local offset by the new
         * overlay vertex count, so adding the room prefix cannot overflow. */
        new_batches[room_batch_count + index].first_vertex += room_vertex_count;
    }

    if (new_vertices != cache->vertices) {
        free(cache->vertices);
        ++cache->overlay_buffer_replacements[0];
        cache->overlay_room_vertices_copied += room_vertex_count;
    } else if (vertex_count != old_vertex_count) {
        cache->overlay_shifted_vertices += cache->overlay_vertex_count - old_vertex_end;
    }
    if (new_batches != cache->batches) {
        free(cache->batches);
        ++cache->overlay_buffer_replacements[1];
    }
    if (new_overlay_batches != cache->overlay_batches) {
        free(cache->overlay_batches);
        ++cache->overlay_buffer_replacements[2];
    }
    cache->vertices = new_vertices;
    cache->batches = new_batches;
    cache->overlay_batches = new_overlay_batches;
    cache->vertex_storage_capacity = vertex_allocation;
    cache->batch_storage_capacity = batch_allocation;
    cache->overlay_batch_storage_capacity = overlay_batch_allocation;
    ++cache->overlay_allocating_replacements;
publish_counts:
    cache->overlay_vertex_count = new_overlay_vertex_count;
    cache->overlay_batch_count = new_overlay_batch_count;
    cache->scene.vertex_count = total_vertex_count;
    cache->scene.batch_count = total_batch_count;
    cache->scene.required_vertex_count = total_vertex_count;
    cache->scene.required_batch_count = total_batch_count;
    cache->scene.triangle_count = triangle_count;
    ge_dam_refresh_overlay_vertex_alias(cache);
    cache->generation++;
    return GE_DAM_DYNAMIC_SCENE_OK;

no_memory:
    if (new_overlay_batches != cache->overlay_batches) free(new_overlay_batches);
    if (new_batches != cache->batches) free(new_batches);
    if (new_vertices != cache->vertices) free(new_vertices);
    return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_replace_overlay_segment(
    GeDamDynamicScene *cache,
    size_t overlay_vertex_offset, size_t old_vertex_count,
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    size_t overlay_batch_offset, size_t old_batch_count,
    const GeDamRoomDrawBatch *batches, size_t batch_count)
{
    const GeDamDynamicSceneStatus status = ge_dam_replace_overlay_segment(
        cache, overlay_vertex_offset, old_vertex_count, vertices, vertex_count,
        overlay_batch_offset, old_batch_count, batches, batch_count);
    if (cache != NULL) {
        cache->overlay_update_attempts++;
        if (status == GE_DAM_DYNAMIC_SCENE_OK)
            cache->overlay_update_successes++;
        else
            cache->overlay_update_failures++;
    }
    return status;
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_commit_overlay_batches(
    GeDamDynamicScene *cache, size_t overlay_batch_offset,
    size_t batch_count)
{
    size_t room_vertex_count;
    size_t room_batch_count;
    size_t old_triangles = 0U;
    size_t new_triangles = 0U;
    size_t index;

    if (cache != NULL) cache->overlay_update_attempts++;
    if (cache == NULL || cache->initialized == 0U || batch_count == 0U
            || overlay_batch_offset > cache->overlay_batch_count
            || batch_count > cache->overlay_batch_count
                - overlay_batch_offset
            || cache->scene.vertex_count < cache->overlay_vertex_count
            || cache->scene.batch_count < cache->overlay_batch_count) {
        if (cache != NULL) cache->overlay_update_failures++;
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    room_vertex_count = cache->scene.vertex_count
        - cache->overlay_vertex_count;
    room_batch_count = cache->scene.batch_count
        - cache->overlay_batch_count;
    for (index = 0U; index < batch_count; ++index) {
        const GeDamRoomDrawBatch *overlay =
            &cache->overlay_batches[overlay_batch_offset + index];
        const GeDamRoomDrawBatch *published =
            &cache->batches[room_batch_count + overlay_batch_offset + index];
        if (overlay->first_vertex > cache->overlay_vertex_count
                || overlay->vertex_count > cache->overlay_vertex_count
                    - overlay->first_vertex
                || overlay->first_vertex > SIZE_MAX - room_vertex_count
                || published->triangle_count > SIZE_MAX - old_triangles
                || overlay->triangle_count > SIZE_MAX - new_triangles) {
            cache->overlay_update_failures++;
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        }
        old_triangles += published->triangle_count;
        new_triangles += overlay->triangle_count;
    }
    if (old_triangles > cache->scene.triangle_count
            || new_triangles > SIZE_MAX
                - (cache->scene.triangle_count - old_triangles)) {
        cache->overlay_update_failures++;
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    for (index = 0U; index < batch_count; ++index) {
        GeDamRoomDrawBatch *published =
            &cache->batches[room_batch_count + overlay_batch_offset + index];
        *published = cache->overlay_batches[overlay_batch_offset + index];
        published->first_vertex += room_vertex_count;
    }
    cache->scene.triangle_count = cache->scene.triangle_count
        - old_triangles + new_triangles;
    cache->scene.required_vertex_count = cache->scene.vertex_count;
    cache->scene.required_batch_count = cache->scene.batch_count;
    cache->overlay_update_successes++;
    cache->generation++;
    return GE_DAM_DYNAMIC_SCENE_OK;
}

GeDamDynamicSceneStatus ge_dam_dynamic_scene_commit_overlay_rooms(
    GeDamDynamicScene *cache, size_t overlay_batch_offset,
    const GeDamRoomDrawBatch *source_batches, size_t batch_count)
{
    size_t room_batch_count, index;
    if (cache != NULL) cache->overlay_update_attempts++;
    if (cache == NULL || cache->initialized == 0U || source_batches == NULL
            || batch_count == 0U
            || overlay_batch_offset > cache->overlay_batch_count
            || batch_count > cache->overlay_batch_count - overlay_batch_offset
            || cache->scene.vertex_count < cache->overlay_vertex_count
            || cache->scene.batch_count < cache->overlay_batch_count) {
        if (cache != NULL) cache->overlay_update_failures++;
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    room_batch_count = cache->scene.batch_count - cache->overlay_batch_count;
    for (index = 0U; index < batch_count; ++index) {
        const uint8_t room = source_batches[index].room_id;
        cache->overlay_batches[overlay_batch_offset + index].room_id = room;
        cache->batches[room_batch_count + overlay_batch_offset + index].room_id = room;
    }
    cache->overlay_update_successes++;
    cache->generation++;
    return GE_DAM_DYNAMIC_SCENE_OK;
}

int ge_dam_dynamic_scene_is_resident(
    const GeDamDynamicScene *cache, uint8_t room)
{
    return cache != NULL && cache->initialized != 0U
        && (size_t)room < cache->world.room_count
        && cache->resident[room] != 0U;
}

void ge_dam_dynamic_scene_close(GeDamDynamicScene *cache)
{
    if (cache == NULL) return;
    free(cache->overlay_batches);
    free(cache->batches);
    free(cache->vertices);
    free(cache->room_ranges);
    memset(cache, 0, sizeof(*cache));
}

const char *ge_dam_dynamic_scene_status_name(GeDamDynamicSceneStatus status)
{
    switch (status) {
    case GE_DAM_DYNAMIC_SCENE_OK: return "ok";
    case GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT: return "invalid argument";
    case GE_DAM_DYNAMIC_SCENE_ASSET_NOT_FOUND: return "asset not found";
    case GE_DAM_DYNAMIC_SCENE_ASSET_INVALID: return "asset invalid";
    case GE_DAM_DYNAMIC_SCENE_NO_MEMORY: return "no memory";
    case GE_DAM_DYNAMIC_SCENE_ROOM_CAPACITY: return "room capacity";
    case GE_DAM_DYNAMIC_SCENE_VERTEX_CAPACITY: return "vertex capacity";
    case GE_DAM_DYNAMIC_SCENE_BATCH_CAPACITY: return "batch capacity";
    case GE_DAM_DYNAMIC_SCENE_BUILD_FAILED: return "build failed";
    case GE_DAM_DYNAMIC_SCENE_NO_REQUEST: return "no request";
    case GE_DAM_DYNAMIC_SCENE_QUEUE_ERROR: return "queue error";
    default: return "unknown";
    }
}

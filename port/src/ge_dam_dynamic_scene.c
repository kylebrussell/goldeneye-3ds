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

    memset(candidate, 0, sizeof(*candidate));
    owned = room_count != 0U ? calloc(room_count, sizeof(*owned)) : NULL;
    descriptors = room_count != 0U
        ? calloc(room_count, sizeof(*descriptors)) : NULL;
    if (room_count != 0U && (owned == NULL || descriptors == NULL)) {
        free(descriptors);
        ge_dam_owned_rooms_close(owned, room_count);
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
        build_status = ge_dam_rooms_build(descriptors, room_count, NULL, NULL,
                                          &candidate->scene);
        if (build_status != GE_DAM_ROOM_CAPACITY_EXCEEDED
                && build_status != GE_DAM_ROOM_OK) {
            status = GE_DAM_DYNAMIC_SCENE_BUILD_FAILED;
        } else if (cache->overlay_vertex_count
                    > SIZE_MAX - candidate->scene.required_vertex_count
                || cache->overlay_batch_count
                    > SIZE_MAX - candidate->scene.required_batch_count) {
            status = GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
        } else {
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
        storage.vertices = candidate->vertices;
        storage.vertex_capacity = room_vertex_count;
        storage.batches = candidate->batches;
        storage.batch_capacity = room_batch_count;
        build_status = ge_dam_rooms_build(descriptors, room_count, NULL,
                                          &storage, &candidate->scene);
        if (build_status != GE_DAM_ROOM_OK) {
            status = GE_DAM_DYNAMIC_SCENE_BUILD_FAILED;
        } else if (cache->overlay_vertex_count != 0U
                || cache->overlay_batch_count != 0U) {
            size_t batch_index;
            memcpy(candidate->vertices + room_vertex_count,
                   cache->overlay_vertices,
                   cache->overlay_vertex_count
                       * sizeof(*candidate->vertices));
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
    next.scene = candidate.scene;
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
    transaction->scene = built.scene;
    memcpy(transaction->room_ids, candidate_rooms, candidate_count);
    transaction->room_count = candidate_count;
    transaction->room = room;
    transaction->includes_request = includes_request;
    transaction->prepared = 1U;
    return GE_DAM_DYNAMIC_SCENE_OK;
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
    cache->scene = transaction->scene;
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

GeDamDynamicSceneStatus ge_dam_dynamic_scene_set_overlay(
    GeDamDynamicScene *cache,
    const GeDamRoomWorldVertex *vertices,
    size_t vertex_count,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count)
{
    GeDamDynamicScene shadow;
    GeDamDynamicSceneTransaction candidate;
    GeDamRoomDrawBatch *new_batches = NULL;
    GeDamDynamicSceneStatus status;
    size_t index;

    if (cache == NULL || cache->initialized == 0U
            || (vertex_count != 0U && vertices == NULL)
            || (batch_count != 0U && batches == NULL)
            || (vertex_count == 0U) != (batch_count == 0U)
            || vertex_count > cache->limits.vertex_capacity
            || batch_count > cache->limits.batch_capacity)
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    for (index = 0U; index < batch_count; ++index) {
        if (batches[index].first_vertex > vertex_count
                || batches[index].vertex_count
                    > vertex_count - batches[index].first_vertex)
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    if (vertex_count != 0U) {
        if (batch_count > SIZE_MAX / sizeof(*new_batches))
            return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
        new_batches = malloc(batch_count * sizeof(*new_batches));
        if (new_batches == NULL) return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
        memcpy(new_batches, batches, batch_count * sizeof(*new_batches));
    }
    shadow = *cache;
    /* The candidate copies the caller's vertices directly into its combined
     * room+overlay publication. Once installed, overlay_vertices aliases that
     * published tail. Retaining a second full vertex copy used several MiB on
     * object-heavy stages and made otherwise valid room-stream transactions
     * fail under the 3DS heap budget. */
    shadow.overlay_vertices = (GeDamRoomWorldVertex *)(void *)vertices;
    shadow.overlay_batches = new_batches;
    shadow.overlay_vertex_count = vertex_count;
    shadow.overlay_batch_count = batch_count;
    status = ge_dam_build_candidate(
        &shadow, shadow.room_ids, shadow.room_count, &candidate);
    if (status != GE_DAM_DYNAMIC_SCENE_OK) {
        free(new_batches);
        return status;
    }
    free(cache->batches);
    free(cache->vertices);
    free(cache->overlay_batches);
    cache->vertices = candidate.vertices;
    cache->batches = candidate.batches;
    cache->scene = candidate.scene;
    cache->overlay_batches = new_batches;
    cache->overlay_vertex_count = vertex_count;
    cache->overlay_batch_count = batch_count;
    ge_dam_refresh_overlay_vertex_alias(cache);
    ++cache->generation;
    return GE_DAM_DYNAMIC_SCENE_OK;
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

GeDamDynamicSceneStatus ge_dam_dynamic_scene_replace_overlay_segment(
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
    size_t index;
    size_t triangle_count = 0U;

    if (cache != NULL) cache->overlay_update_attempts++;
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
        if (cache != NULL) cache->overlay_update_failures++;
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
        cache->overlay_update_failures++;
        return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
    }
    total_vertex_count = room_vertex_count + new_overlay_vertex_count;
    total_batch_count = room_batch_count + new_overlay_batch_count;
    if (total_vertex_count > cache->limits.vertex_capacity) {
        cache->overlay_update_failures++;
        return GE_DAM_DYNAMIC_SCENE_VERTEX_CAPACITY;
    }
    if (total_batch_count > cache->limits.batch_capacity) {
        cache->overlay_update_failures++;
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
            cache->overlay_update_failures++;
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < batch_count; ++index) {
        if (batches[index].first_vertex > vertex_count
                || batches[index].vertex_count
                    > vertex_count - batches[index].first_vertex) {
            cache->overlay_update_failures++;
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        }
    }
    if (total_vertex_count != 0U) {
        new_vertices = malloc(total_vertex_count * sizeof(*new_vertices));
        if (new_vertices == NULL) goto no_memory;
    }
    if (total_batch_count != 0U) {
        new_batches = malloc(total_batch_count * sizeof(*new_batches));
        if (new_batches == NULL) goto no_memory;
    }
    if (new_overlay_batch_count != 0U) {
        new_overlay_batches = malloc(new_overlay_batch_count
            * sizeof(*new_overlay_batches));
        if (new_overlay_batches == NULL) goto no_memory;
    }

    memcpy(new_vertices, cache->vertices,
        room_vertex_count * sizeof(*new_vertices));
    memcpy(new_vertices + room_vertex_count, cache->overlay_vertices,
        overlay_vertex_offset * sizeof(*new_vertices));
    if (vertex_count != 0U)
        memcpy(new_vertices + room_vertex_count + overlay_vertex_offset,
            vertices, vertex_count * sizeof(*new_vertices));
    memcpy(new_vertices + room_vertex_count + overlay_vertex_offset
            + vertex_count,
        cache->overlay_vertices + old_vertex_end,
        (cache->overlay_vertex_count - old_vertex_end)
            * sizeof(*new_vertices));

    if (overlay_batch_offset != 0U)
        memcpy(new_overlay_batches, cache->overlay_batches,
            overlay_batch_offset * sizeof(*new_overlay_batches));
    for (index = 0U; index < batch_count; ++index) {
        new_overlay_batches[overlay_batch_offset + index] = batches[index];
        new_overlay_batches[overlay_batch_offset + index].first_vertex +=
            overlay_vertex_offset;
    }
    for (index = old_batch_end; index < cache->overlay_batch_count; ++index) {
        GeDamRoomDrawBatch shifted = cache->overlay_batches[index];
        shifted.first_vertex = shifted.first_vertex - old_vertex_end
            + overlay_vertex_offset + vertex_count;
        new_overlay_batches[overlay_batch_offset + batch_count
            + index - old_batch_end] = shifted;
    }

    memcpy(new_batches, cache->batches,
        room_batch_count * sizeof(*new_batches));
    for (index = 0U; index < new_overlay_batch_count; ++index) {
        GeDamRoomDrawBatch published = new_overlay_batches[index];
        if (published.first_vertex > SIZE_MAX - room_vertex_count)
            goto invalid_built;
        published.first_vertex += room_vertex_count;
        new_batches[room_batch_count + index] = published;
    }
    for (index = 0U; index < total_batch_count; ++index) {
        if (new_batches[index].triangle_count > SIZE_MAX - triangle_count)
            goto invalid_built;
        triangle_count += new_batches[index].triangle_count;
    }

    free(cache->vertices);
    free(cache->batches);
    free(cache->overlay_batches);
    cache->vertices = new_vertices;
    cache->batches = new_batches;
    cache->overlay_batches = new_overlay_batches;
    cache->overlay_vertex_count = new_overlay_vertex_count;
    cache->overlay_batch_count = new_overlay_batch_count;
    cache->scene.vertex_count = total_vertex_count;
    cache->scene.batch_count = total_batch_count;
    cache->scene.required_vertex_count = total_vertex_count;
    cache->scene.required_batch_count = total_batch_count;
    cache->scene.triangle_count = triangle_count;
    ge_dam_refresh_overlay_vertex_alias(cache);
    cache->overlay_update_successes++;
    cache->generation++;
    return GE_DAM_DYNAMIC_SCENE_OK;

invalid_built:
    free(new_overlay_batches);
    free(new_batches);
    free(new_vertices);
    cache->overlay_update_failures++;
    return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
no_memory:
    free(new_overlay_batches);
    free(new_batches);
    free(new_vertices);
    cache->overlay_update_failures++;
    return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
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
        GeDamRoomDrawBatch published =
            cache->overlay_batches[overlay_batch_offset + index];
        published.first_vertex += room_vertex_count;
        cache->batches[room_batch_count + overlay_batch_offset + index] =
            published;
    }
    cache->scene.triangle_count = cache->scene.triangle_count
        - old_triangles + new_triangles;
    cache->scene.required_vertex_count = cache->scene.vertex_count;
    cache->scene.required_batch_count = cache->scene.batch_count;
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

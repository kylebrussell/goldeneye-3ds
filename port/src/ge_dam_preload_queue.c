#include "ge_dam_preload_queue.h"

#include <string.h>

GeDamPreloadStatus ge_dam_preload_queue_init(
    GeDamPreloadQueue *queue,
    size_t room_count,
    size_t capacity,
    const uint8_t *initial_rooms,
    size_t initial_room_count)
{
    size_t index;

    if (queue == NULL || room_count == 0U
            || room_count > GE_DAM_PRELOAD_MAX_ROOMS
            || capacity == 0U || capacity > GE_DAM_PRELOAD_MAX_ROOMS
            || (initial_room_count != 0U && initial_rooms == NULL)) {
        return GE_DAM_PRELOAD_INVALID_ARGUMENT;
    }
    memset(queue, 0, sizeof(*queue));
    queue->room_count = room_count;
    queue->capacity = capacity;
    for (index = 0U; index < initial_room_count; ++index) {
        const uint8_t room = initial_rooms[index];

        if ((size_t)room >= room_count) {
            memset(queue, 0, sizeof(*queue));
            return GE_DAM_PRELOAD_INVALID_ARGUMENT;
        }
        queue->states[room] = GE_DAM_PRELOAD_ROOM_RESIDENT;
    }
    return GE_DAM_PRELOAD_OK;
}

uint8_t ge_dam_preload_queue_request(void *context, uint8_t room)
{
    GeDamPreloadQueue *queue = context;
    uint8_t state;

    if (queue == NULL || (size_t)room >= queue->room_count) {
        if (queue != NULL) ++queue->invalid_count;
        return 1U;
    }
    state = queue->states[room];
    if (state == GE_DAM_PRELOAD_ROOM_RESIDENT) return 0U;
    if (state == GE_DAM_PRELOAD_ROOM_QUEUED
            || state == GE_DAM_PRELOAD_ROOM_LOADING) {
        ++queue->duplicate_count;
        return 1U;
    }
    if (queue->pending_count >= queue->capacity) {
        ++queue->overflow_count;
        return 1U;
    }
    queue->requests[queue->write_index] = room;
    queue->write_index = (queue->write_index + 1U) % queue->capacity;
    ++queue->pending_count;
    ++queue->accepted_count;
    queue->states[room] = GE_DAM_PRELOAD_ROOM_QUEUED;
    return 1U;
}

GeDamPreloadStatus ge_dam_preload_queue_peek(
    const GeDamPreloadQueue *queue, uint8_t *room)
{
    if (queue == NULL || room == NULL) {
        return GE_DAM_PRELOAD_INVALID_ARGUMENT;
    }
    if (queue->pending_count == 0U) return GE_DAM_PRELOAD_EMPTY;
    if ((size_t)queue->requests[queue->read_index] >= queue->room_count) {
        return GE_DAM_PRELOAD_INVALID_STATE;
    }
    *room = queue->requests[queue->read_index];
    return GE_DAM_PRELOAD_OK;
}

GeDamPreloadStatus ge_dam_preload_queue_pop(
    GeDamPreloadQueue *queue, uint8_t *room)
{
    uint8_t next;

    if (queue == NULL || room == NULL) {
        return GE_DAM_PRELOAD_INVALID_ARGUMENT;
    }
    if (ge_dam_preload_queue_peek(queue, &next) != GE_DAM_PRELOAD_OK) {
        return queue->pending_count == 0U
            ? GE_DAM_PRELOAD_EMPTY : GE_DAM_PRELOAD_INVALID_STATE;
    }
    if (queue->loading_count != 0U) return GE_DAM_PRELOAD_INVALID_STATE;
    if ((size_t)next >= queue->room_count
            || queue->states[next] != GE_DAM_PRELOAD_ROOM_QUEUED) {
        return GE_DAM_PRELOAD_INVALID_STATE;
    }
    queue->read_index = (queue->read_index + 1U) % queue->capacity;
    --queue->pending_count;
    ++queue->loading_count;
    queue->states[next] = GE_DAM_PRELOAD_ROOM_LOADING;
    *room = next;
    return GE_DAM_PRELOAD_OK;
}

GeDamPreloadStatus ge_dam_preload_queue_complete(
    GeDamPreloadQueue *queue, uint8_t room, uint8_t success)
{
    if (queue == NULL || (size_t)room >= queue->room_count) {
        return GE_DAM_PRELOAD_INVALID_ARGUMENT;
    }
    if (queue->states[room] != GE_DAM_PRELOAD_ROOM_LOADING) {
        return GE_DAM_PRELOAD_INVALID_STATE;
    }
    if (queue->loading_count == 0U) return GE_DAM_PRELOAD_INVALID_STATE;
    --queue->loading_count;
    queue->states[room] = success != 0U
        ? GE_DAM_PRELOAD_ROOM_RESIDENT : GE_DAM_PRELOAD_ROOM_UNLOADED;
    return GE_DAM_PRELOAD_OK;
}

GeDamPreloadStatus ge_dam_preload_queue_evict_resident(
    GeDamPreloadQueue *queue, const uint8_t *rooms, size_t room_count)
{
    uint8_t seen[GE_DAM_PRELOAD_MAX_ROOMS] = {0};
    size_t index;

    if (queue == NULL || (room_count != 0U && rooms == NULL))
        return GE_DAM_PRELOAD_INVALID_ARGUMENT;
    for (index = 0U; index < room_count; ++index) {
        const uint8_t room = rooms[index];
        if ((size_t)room >= queue->room_count || seen[room] != 0U
                || queue->states[room] != GE_DAM_PRELOAD_ROOM_RESIDENT)
            return GE_DAM_PRELOAD_INVALID_STATE;
        seen[room] = 1U;
    }
    for (index = 0U; index < room_count; ++index)
        queue->states[rooms[index]] = GE_DAM_PRELOAD_ROOM_UNLOADED;
    queue->eviction_count += room_count;
    return GE_DAM_PRELOAD_OK;
}

GeDamPreloadRoomState ge_dam_preload_queue_room_state(
    const GeDamPreloadQueue *queue, uint8_t room)
{
    if (queue == NULL || (size_t)room >= queue->room_count) {
        return GE_DAM_PRELOAD_ROOM_UNLOADED;
    }
    return (GeDamPreloadRoomState)queue->states[room];
}

GeOriginalBgVisibilityProviders ge_dam_preload_queue_providers(
    GeDamPreloadQueue *queue,
    const uint8_t *portal_controls,
    size_t portal_control_count)
{
    GeOriginalBgVisibilityProviders providers;

    providers.context = queue;
    providers.preload_room = ge_dam_preload_queue_request;
    providers.portal_controls = portal_controls;
    providers.portal_control_count = portal_control_count;
    return providers;
}

const char *ge_dam_preload_status_name(GeDamPreloadStatus status)
{
    switch (status) {
    case GE_DAM_PRELOAD_OK: return "ok";
    case GE_DAM_PRELOAD_INVALID_ARGUMENT: return "invalid argument";
    case GE_DAM_PRELOAD_EMPTY: return "empty";
    case GE_DAM_PRELOAD_INVALID_STATE: return "invalid state";
    default: return "unknown";
    }
}

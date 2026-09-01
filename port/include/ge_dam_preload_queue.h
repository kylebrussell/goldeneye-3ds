#ifndef GE_DAM_PRELOAD_QUEUE_H
#define GE_DAM_PRELOAD_QUEUE_H

#include "ge_original_bg_visibility.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_DAM_PRELOAD_MAX_ROOMS 137U

typedef enum GeDamPreloadRoomState {
    GE_DAM_PRELOAD_ROOM_UNLOADED = 0,
    GE_DAM_PRELOAD_ROOM_QUEUED,
    GE_DAM_PRELOAD_ROOM_LOADING,
    GE_DAM_PRELOAD_ROOM_RESIDENT
} GeDamPreloadRoomState;

typedef struct GeDamPreloadQueue {
    uint8_t requests[GE_DAM_PRELOAD_MAX_ROOMS];
    uint8_t states[GE_DAM_PRELOAD_MAX_ROOMS];
    size_t room_count;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t pending_count;
    size_t loading_count;
    size_t accepted_count;
    size_t duplicate_count;
    size_t overflow_count;
    size_t invalid_count;
    size_t eviction_count;
} GeDamPreloadQueue;

typedef enum GeDamPreloadStatus {
    GE_DAM_PRELOAD_OK = 0,
    GE_DAM_PRELOAD_INVALID_ARGUMENT,
    GE_DAM_PRELOAD_EMPTY,
    GE_DAM_PRELOAD_INVALID_STATE
} GeDamPreloadStatus;

/* initial_rooms are the already-expanded startup cache. capacity bounds only
 * pending requests; it does not enlarge the live renderer's resident budget. */
GeDamPreloadStatus ge_dam_preload_queue_init(
    GeDamPreloadQueue *queue,
    size_t room_count,
    size_t capacity,
    const uint8_t *initial_rooms,
    size_t initial_room_count);

/* Exact bgCheckIfRoomModelNeedsLoad provider semantics: resident rooms return
 * zero. A missing, already-pending, or capacity-blocked room returns nonzero,
 * stopping the original interpreter after one missing-room load attempt. */
uint8_t ge_dam_preload_queue_request(void *context, uint8_t room);

GeDamPreloadStatus ge_dam_preload_queue_peek(
    const GeDamPreloadQueue *queue, uint8_t *room);

GeDamPreloadStatus ge_dam_preload_queue_pop(
    GeDamPreloadQueue *queue, uint8_t *room);

/* Completes the sole in-flight request. Success makes it resident; failure
 * returns it to unloaded so an original future request may retry it. */
GeDamPreloadStatus ge_dam_preload_queue_complete(
    GeDamPreloadQueue *queue, uint8_t room, uint8_t success);

/* Atomically returns an exact set of currently resident rooms to the unloaded
 * state after their authored bgRoomsTickUnload lifetime expires. */
GeDamPreloadStatus ge_dam_preload_queue_evict_resident(
    GeDamPreloadQueue *queue, const uint8_t *rooms, size_t room_count);

GeDamPreloadRoomState ge_dam_preload_queue_room_state(
    const GeDamPreloadQueue *queue, uint8_t room);

GeOriginalBgVisibilityProviders ge_dam_preload_queue_providers(
    GeDamPreloadQueue *queue,
    const uint8_t *portal_controls,
    size_t portal_control_count);

const char *ge_dam_preload_status_name(GeDamPreloadStatus status);

#ifdef __cplusplus
}
#endif

#endif

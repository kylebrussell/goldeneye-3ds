#ifndef GE_DAM_DYNAMIC_SCENE_H
#define GE_DAM_DYNAMIC_SCENE_H

#include "ge_asset_pack.h"
#include "ge_dam_preload_queue.h"
#include "ge_dam_room.h"
#include "ge_dam_world.h"
#include "ge_stage_assets.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GeDamDynamicSceneLimits {
    size_t room_capacity;
    size_t vertex_capacity;
    size_t batch_capacity;
} GeDamDynamicSceneLimits;

typedef struct GeDamDynamicScene {
    GeAssetPack *pack;
    const GeStageAssetDescriptor *stage_assets;
    GeDamWorld world;
    GeDamDynamicSceneLimits limits;
    uint8_t room_ids[GE_DAM_WORLD_MAX_ROOMS];
    uint8_t resident[GE_DAM_WORLD_MAX_ROOMS];
    /* Exact model_bin_loaded lifetime: 0 unloaded, 1 rendered/recent,
     * 2..4 unrendered age. eviction_pending is the canonical 4->delete. */
    uint8_t room_age[GE_DAM_WORLD_MAX_ROOMS];
    uint8_t eviction_pending[GE_DAM_WORLD_MAX_ROOMS];
    size_t room_count;
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    /* CPU allocation extents, distinct from authored/published counts. Small
     * overlay growth reserves let the final actor segment change topology
     * without copying immutable resident rooms. Never exceed scene limits. */
    size_t vertex_storage_capacity;
    size_t batch_storage_capacity;
    size_t overlay_batch_storage_capacity;
    /* Aliases the overlay tail of vertices; only overlay_batches owns a
     * separate local-index copy for future room rebuilds. */
    GeDamRoomWorldVertex *overlay_vertices;
    GeDamRoomDrawBatch *overlay_batches;
    size_t overlay_vertex_count;
    size_t overlay_batch_count;
    GeDamRoomScene scene;
    uint64_t generation;
    uint64_t install_attempts;
    uint64_t install_successes;
    uint64_t install_failures;
    uint64_t overlay_update_attempts;
    uint64_t overlay_update_successes;
    uint64_t overlay_update_failures;
    /* Successful topology publications, not model/gameplay ticks. */
    uint64_t overlay_inplace_replacements;
    uint64_t overlay_allocating_replacements;
    uint64_t overlay_shifted_vertices;
    /* Successful topology buffer replacements: vertices, published batches,
     * local batches; room vertices copied only when vertex storage changes. */
    uint64_t overlay_buffer_replacements[3];
    uint64_t overlay_room_vertices_copied;
    uint64_t eviction_attempts;
    uint64_t eviction_successes;
    uint64_t eviction_failures;
    uint64_t rooms_evicted;
    uint8_t last_requested_room;
    uint8_t initialized;
} GeDamDynamicScene;

typedef struct GeDamDynamicSceneTransaction {
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    GeDamRoomScene scene;
    uint8_t room_ids[GE_DAM_WORLD_MAX_ROOMS];
    uint8_t evicted_rooms[GE_DAM_WORLD_MAX_ROOMS];
    size_t room_count;
    size_t evicted_count;
    uint8_t room;
    uint8_t includes_request;
    uint8_t prepared;
} GeDamDynamicSceneTransaction;

typedef enum GeDamDynamicSceneStatus {
    GE_DAM_DYNAMIC_SCENE_OK = 0,
    GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT,
    GE_DAM_DYNAMIC_SCENE_ASSET_NOT_FOUND,
    GE_DAM_DYNAMIC_SCENE_ASSET_INVALID,
    GE_DAM_DYNAMIC_SCENE_NO_MEMORY,
    GE_DAM_DYNAMIC_SCENE_ROOM_CAPACITY,
    GE_DAM_DYNAMIC_SCENE_VERTEX_CAPACITY,
    GE_DAM_DYNAMIC_SCENE_BATCH_CAPACITY,
    GE_DAM_DYNAMIC_SCENE_BUILD_FAILED,
    GE_DAM_DYNAMIC_SCENE_NO_REQUEST,
    GE_DAM_DYNAMIC_SCENE_QUEUE_ERROR
} GeDamDynamicSceneStatus;

/* Builds and publishes the initial authored resident set atomically. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_init(
    GeDamDynamicScene *cache,
    GeAssetPack *pack,
    const GeDamWorld *world,
    const uint8_t *initial_rooms,
    size_t initial_room_count,
    const GeDamDynamicSceneLimits *limits);

GeDamDynamicSceneStatus ge_dam_dynamic_scene_init_for_stage(
    GeDamDynamicScene *cache,
    GeAssetPack *pack,
    const GeStageAssetDescriptor *stage,
    const GeDamWorld *world,
    const uint8_t *initial_rooms,
    size_t initial_room_count,
    const GeDamDynamicSceneLimits *limits);

/* Builds the queue head off to the side without changing cache residency. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_prepare_next(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue,
    GeDamDynamicSceneTransaction *transaction);

/* Atomically swaps a prepared scene and only then marks the exact request
 * resident. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_commit(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue,
    GeDamDynamicSceneTransaction *transaction);

/* Leaves the exact request queued for a later retry. */
void ge_dam_dynamic_scene_abort(
    GeDamDynamicScene *cache, GeDamDynamicSceneTransaction *transaction);

/* Convenience CPU-only prepare+commit operation. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_install_next(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue);

/* Updates only the exact bgRoomsTickUnload ages/pending-delete flags. It never
 * allocates, rebuilds, changes residency, advances generation, or publishes
 * scene pointers. A later prepare_next transaction consumes due evictions. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_age_visibility(
    GeDamDynamicScene *cache,
    const uint8_t *rendered_rooms, size_t rendered_room_count);

/* One canonical residency tick. Rendered resident rooms reset age to 1;
 * unrendered rooms age 1->2->3->4->delete. If a preload is queued on the
 * delete tick, its install and every due deletion publish as one scene
 * transaction. Nonresident rendered-room ids are ignored. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_tick_visibility(
    GeDamDynamicScene *cache, GeDamPreloadQueue *queue,
    const uint8_t *rendered_rooms, size_t rendered_room_count);

/* Atomically replaces the decomp-owned dynamic model geometry while retaining
 * the already-decoded room prefix. No assets are read or room GBI traversed.
 * Input batches use overlay-local vertex offsets; the cache rebases them after
 * the room vertices. A failure preserves the last published scene and overlay.
 * Subsequent residency transactions carry this overlay into their room rebuild. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_set_overlay(
    GeDamDynamicScene *cache,
    const GeDamRoomWorldVertex *vertices,
    size_t vertex_count,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count);

/* Transactionally replaces one topology-stable overlay segment in place.
 * Input batch first_vertex values are local to the supplied segment. The
 * published room scene and retained overlay copy are both updated without
 * reallocating or decoding authored room geometry. Invalid input preserves
 * the prior publication byte-for-byte. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_update_overlay_segment(
    GeDamDynamicScene *cache,
    size_t overlay_vertex_offset,
    const GeDamRoomWorldVertex *vertices,
    size_t vertex_count,
    size_t overlay_batch_offset,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count);

/* Transactionally replaces one overlay segment whose authored topology has
 * changed. This preserves the already-decoded room and every other overlay
 * segment, shifting retained suffix offsets as needed.
 * Input batches are local to the replacement vertices. Disjoint inputs can
 * reuse retained capacity by shifting only the overlay suffix; overlapping
 * middle inputs retain allocate-before-publish behavior. Disjoint growth
 * replaces only buffers lacking capacity. Every input/count check and every
 * allocation completes before any in-place mutation. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_replace_overlay_segment(
    GeDamDynamicScene *cache,
    size_t overlay_vertex_offset,
    size_t old_vertex_count,
    const GeDamRoomWorldVertex *vertices,
    size_t vertex_count,
    size_t overlay_batch_offset,
    size_t old_batch_count,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count);

/* Publishes a contiguous range of retained overlay batches after an exact
 * renderer adapter has updated them in place. Overlay vertices already alias
 * the combined scene tail, so this commits the corresponding rebased batch
 * metadata and advances the scene generation once for the whole range. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_commit_overlay_batches(
    GeDamDynamicScene *cache, size_t overlay_batch_offset,
    size_t batch_count);

/* A model cache has retained the exact topology/material publication and
 * changed only pose and room IDs. Publish those IDs to both batch views and
 * advance generation without recopying immutable material/texture payloads.
 * Source batches must not overlap either cache-owned batch array. Their
 * first_vertex values may be segment-local; only room_id is consumed. */
GeDamDynamicSceneStatus ge_dam_dynamic_scene_commit_overlay_rooms(
    GeDamDynamicScene *cache, size_t overlay_batch_offset,
    const GeDamRoomDrawBatch *source_batches, size_t batch_count);

int ge_dam_dynamic_scene_is_resident(
    const GeDamDynamicScene *cache, uint8_t room);
void ge_dam_dynamic_scene_close(GeDamDynamicScene *cache);
const char *ge_dam_dynamic_scene_status_name(GeDamDynamicSceneStatus status);

#ifdef __cplusplus
}
#endif

#endif

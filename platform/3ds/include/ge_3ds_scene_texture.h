#ifndef GE_3DS_SCENE_TEXTURE_H
#define GE_3DS_SCENE_TEXTURE_H

#include <citro3d.h>
#include <tex3ds.h>

#include "ge_dam_room.h"
#include "ge_texture_cache.h"
#include "ge_texture_uv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Ge3dsSceneTextureStatus {
    GE_3DS_SCENE_TEXTURE_OK = 0,
    GE_3DS_SCENE_TEXTURE_PARTIAL,
    GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT,
    GE_3DS_SCENE_TEXTURE_CAPACITY_EXCEEDED,
    GE_3DS_SCENE_TEXTURE_COMMIT_REJECTED,
    GE_3DS_SCENE_TEXTURE_DEPENDENCY_FAILED
} Ge3dsSceneTextureStatus;

typedef struct Ge3dsSceneTextureSlot {
    C3D_Tex texture;
    Tex3DS_SubTexture subtexture;
    uint32_t width;
    uint32_t height;
    uint16_t image_id;
    uint8_t loaded;
    /* A prepared reconciliation borrows unchanged handles until commit. */
    uint8_t owned;
} Ge3dsSceneTextureSlot;

/* The live scene holds at most 192 images. Keep the index below 50% load;
 * larger caller-owned sets retain the original exact linear lookup. */
#define GE_3DS_SCENE_TEXTURE_INDEX_CAPACITY 512U
#define GE_3DS_SCENE_TEXTURE_INDEX_MAX_ENTRIES 256U

typedef struct Ge3dsSceneTextures {
    Ge3dsSceneTextureSlot *slots;
    size_t capacity;
    size_t texture_count;
    size_t loaded_count;
    size_t missing_count;
    /* Internal image-ID index: slot offset + 1, zero means empty. Offsets
     * survive the existing candidate -> permanent slot-storage handoff.
     * Includes missing images, so failed imports remain deduplicated. */
    uint16_t image_index[GE_3DS_SCENE_TEXTURE_INDEX_CAPACITY];
    size_t indexed_count;
} Ge3dsSceneTextures;

typedef struct Ge3dsSceneTextureReconcileStats {
    size_t required_count;
    size_t retained_count;
    size_t imported_count;
    size_t released_count;
    size_t missing_count;
} Ge3dsSceneTextureReconcileStats;

/* Imports each unique authored Rare image referenced by the batches. The
 * compressed cache entry is unpinned immediately after Tex3DS uploads it, so
 * source blobs do not compete with the persistent GPU textures. */
Ge3dsSceneTextureStatus ge_3ds_scene_textures_load(
    GeTextureCache *cache,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count,
    Ge3dsSceneTextureSlot *slots,
    size_t slot_capacity,
    Ge3dsSceneTextures *scene);

/* Imports one newly referenced authored image into an existing scene without
 * deleting or rebuilding its resident GPU textures. This is used by animated
 * monitor command streams whose exact image can change at runtime. */
Ge3dsSceneTextureStatus ge_3ds_scene_textures_ensure_image(
    GeTextureCache *cache, Ge3dsSceneTextures *scene, uint16_t image_id);

/* Builds a candidate residency set without duplicating textures already owned
 * by current. Unchanged slots are borrowed (loaded but not owned), while only
 * newly required authored image IDs are imported and owned by candidate.
 * Closing an uncommitted candidate releases only those new imports and leaves
 * current untouched.
 *
 * The candidate slot array must not alias current or its slot array. A
 * capacity failure is detected before any texture import. Empty batch sets are
 * supported and prepare a valid empty candidate. */
Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_prepare(
    GeTextureCache *cache,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count,
    const Ge3dsSceneTextures *current,
    Ge3dsSceneTextureSlot *candidate_slots,
    size_t candidate_capacity,
    Ge3dsSceneTextures *candidate,
    Ge3dsSceneTextureReconcileStats *stats);

typedef struct Ge3dsSceneTextureBatchRange {
    const GeDamRoomDrawBatch *batches;
    size_t batch_count;
} Ge3dsSceneTextureBatchRange;

/* Same preparation over ordered, separate batch ranges. Used to preflight
 * resident rooms + a replacement overlay without first publishing/copying
 * the overlay. Capacity is checked across all ranges before any import. */
Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_prepare_ranges(
    GeTextureCache *cache,
    const Ge3dsSceneTextureBatchRange *ranges, size_t range_count,
    const Ge3dsSceneTextures *current,
    Ge3dsSceneTextureSlot *candidate_slots, size_t candidate_capacity,
    Ge3dsSceneTextures *candidate, Ge3dsSceneTextureReconcileStats *stats);

/* Extends a prepared transaction with a known authored resource dependency
 * even when its draw switches are inactive. Borrows resident textures with
 * the same ownership rules as prepare; imports only new IDs. Failure leaves
 * current untouched; discard the candidate to roll back the transaction. */
Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_include_image(
    GeTextureCache *cache, const Ge3dsSceneTextures *current,
    Ge3dsSceneTextures *candidate, uint16_t image_id,
    Ge3dsSceneTextureReconcileStats *stats);

/* Atomically transfers borrowed ownership to candidate, releases resident
 * textures no longer present, and clears current. After success candidate is
 * the sole owner of every loaded slot. The caller may then move its slots into
 * permanent storage using the same ownership handoff used by load(). */
Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_commit(
    Ge3dsSceneTextures *current,
    Ge3dsSceneTextures *candidate,
    Ge3dsSceneTextureReconcileStats *stats);

/* Runs a related fallible publication (such as the prepared room geometry)
 * after validating every borrow, but before releasing/transferring textures.
 * A zero return leaves both texture sets untouched. After a nonzero return,
 * texture ownership transfer has no remaining failure points. The callback
 * must not change either texture set or their slot storage. Single-threaded,
 * like the renderer; this is not a GPU synchronization primitive. */
typedef int (*Ge3dsSceneTextureCommitGate)(void *context);
Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_commit_after(
    Ge3dsSceneTextures *current,
    Ge3dsSceneTextures *candidate,
    Ge3dsSceneTextureReconcileStats *stats,
    Ge3dsSceneTextureCommitGate commit_gate, void *context);

const Ge3dsSceneTextureSlot *ge_3ds_scene_textures_find(
    const Ge3dsSceneTextures *scene,
    uint16_t image_id);

/* Per-batch snapshot of normalization and Tex3DS atlas orientation. No GPU
 * handle or material pointer is retained; recreate after either changes. */
typedef struct Ge3dsSceneTextureUvContext {
    GeTextureUvContext normalization;
    float top_left_u, top_left_v, bottom_left_u, bottom_left_v;
    float top_delta_u, top_delta_v, bottom_delta_u, bottom_delta_v;
} Ge3dsSceneTextureUvContext;

GeTextureUvStatus ge_3ds_scene_texture_uv_prepare(
    const Ge3dsSceneTextureSlot *slot, const GePicaMaterial *material,
    Ge3dsSceneTextureUvContext *context);
GeTextureUvStatus ge_3ds_scene_texture_map_uv_prepared(
    const Ge3dsSceneTextureUvContext *context,
    int16_t texture_s, int16_t texture_t, GeTextureUv *result);

/* Converts an authored N64 vertex ST pair and maps it through tex3ds' actual
 * subtexture orientation/padding. */
GeTextureUvStatus ge_3ds_scene_texture_map_uv(
    const Ge3dsSceneTextureSlot *slot,
    int16_t texture_s,
    int16_t texture_t,
    const GePicaMaterial *material,
    GeTextureUv *result);

void ge_3ds_scene_textures_close(Ge3dsSceneTextures *scene);

#ifdef __cplusplus
}
#endif

#endif

#include "ge_3ds_scene_texture.h"

#include <string.h>

_Static_assert(GE_3DS_SCENE_TEXTURE_INDEX_CAPACITY == (1U << 9U),
    "image-ID hash uses the high nine bits");
_Static_assert(GE_3DS_SCENE_TEXTURE_INDEX_MAX_ENTRIES
        <= GE_3DS_SCENE_TEXTURE_INDEX_CAPACITY / 2U,
    "image-ID index must retain an empty bucket");

static size_t ge_3ds_scene_texture_bucket(uint16_t image_id)
{
    const uint32_t hash = (uint32_t)image_id * UINT32_C(2654435761);
    return hash >> 23U;
}

/* All four append paths publish IDs before indexing. An externally supplied
 * unindexed set is rebuilt on its first append; normal appends touch one
 * probe chain only. No texture handles or residency decisions change. */
static void ge_3ds_scene_texture_index_append(Ge3dsSceneTextures *scene)
{
    size_t first, index;
    if (scene->texture_count > GE_3DS_SCENE_TEXTURE_INDEX_MAX_ENTRIES) {
        scene->indexed_count = 0U;
        return;
    }
    first = scene->texture_count - 1U;
    if (scene->indexed_count != first) {
        memset(scene->image_index, 0, sizeof(scene->image_index));
        first = 0U;
    }
    for (index = first; index < scene->texture_count; ++index) {
        size_t bucket = ge_3ds_scene_texture_bucket(scene->slots[index].image_id);
        while (scene->image_index[bucket] != 0U)
            bucket = (bucket + 1U) & (GE_3DS_SCENE_TEXTURE_INDEX_CAPACITY - 1U);
        scene->image_index[bucket] = (uint16_t)(index + 1U);
    }
    scene->indexed_count = scene->texture_count;
}

static const Ge3dsSceneTextureSlot *ge_3ds_scene_texture_find_any(
    const Ge3dsSceneTextures *scene,
    uint16_t image_id)
{
    size_t index;

    if (scene == NULL || scene->slots == NULL) return NULL;
    if (scene->indexed_count == scene->texture_count
            && scene->texture_count <= GE_3DS_SCENE_TEXTURE_INDEX_MAX_ENTRIES) {
        size_t bucket = ge_3ds_scene_texture_bucket(image_id);
        for (index = 0U; index < GE_3DS_SCENE_TEXTURE_INDEX_CAPACITY; ++index) {
            const size_t offset = scene->image_index[bucket];
            if (offset == 0U) return NULL;
            if (scene->slots[offset - 1U].image_id == image_id)
                return &scene->slots[offset - 1U];
            bucket = (bucket + 1U) & (GE_3DS_SCENE_TEXTURE_INDEX_CAPACITY - 1U);
        }
        return NULL;
    }
    for (index = 0U; index < scene->texture_count; ++index) {
        if (scene->slots[index].image_id == image_id)
            return &scene->slots[index];
    }
    return NULL;
}

static Ge3dsSceneTextureSlot *ge_3ds_scene_texture_find_mutable(
    Ge3dsSceneTextures *scene, uint16_t image_id)
{
    /* The underlying slot storage belongs to the mutable scene. */
    return (Ge3dsSceneTextureSlot *)ge_3ds_scene_texture_find_any(scene, image_id);
}

static int ge_3ds_scene_texture_batch_image_id(
    const GeDamRoomDrawBatch *batch, uint16_t *image_id)
{
    if (batch == NULL || image_id == NULL || batch->texture_valid == 0U
            || batch->material.texture_enabled == 0U
            || batch->material.texture_source
                != GE_PICA_TEXTURE_SOURCE_RARE_ID)
        return 0;
    *image_id = batch->texture.texture_id;
    return 1;
}

static int ge_3ds_scene_texture_import(GeTextureCache *cache,
                                       Ge3dsSceneTextureSlot *slot)
{
    const GeTextureCacheEntry *entry = NULL;
    const Tex3DS_SubTexture *subtexture;
    Tex3DS_Texture imported;

    if (ge_texture_cache_acquire_image_id(
            cache, slot->image_id, 0U, GE_TEXTURE_CATALOG_EXACT, &entry)
            != GE_TEXTURE_CACHE_OK) {
        return 0;
    }
    imported = Tex3DS_TextureImport(entry->data, entry->data_size,
                                    &slot->texture, NULL, false);
    if (imported == NULL || Tex3DS_GetNumSubTextures(imported) == 0U) {
        if (imported != NULL) {
            Tex3DS_TextureFree(imported);
        }
        (void)ge_texture_cache_release_entry(cache, entry);
        memset(&slot->texture, 0, sizeof(slot->texture));
        return 0;
    }
    subtexture = Tex3DS_GetSubTexture(imported, 0U);
    slot->subtexture = *subtexture;
    slot->width = entry->width;
    slot->height = entry->height;
    Tex3DS_TextureFree(imported);
    (void)ge_texture_cache_release_entry(cache, entry);
    slot->loaded = UINT8_C(1);
    slot->owned = UINT8_C(1);
    return 1;
}

Ge3dsSceneTextureStatus ge_3ds_scene_textures_load(
    GeTextureCache *cache,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count,
    Ge3dsSceneTextureSlot *slots,
    size_t slot_capacity,
    Ge3dsSceneTextures *scene)
{
    size_t index;

    if (cache == NULL || batches == NULL || batch_count == 0U || slots == NULL
            || slot_capacity == 0U || scene == NULL) {
        return GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT;
    }
    memset(scene, 0, sizeof(*scene));
    memset(slots, 0, slot_capacity * sizeof(*slots));
    scene->slots = slots;
    scene->capacity = slot_capacity;

    for (index = 0U; index < batch_count; ++index) {
        const GeDamRoomDrawBatch *batch = &batches[index];
        Ge3dsSceneTextureSlot *slot;

        if (batch->texture_valid == 0U
                || batch->material.texture_enabled == 0U
                || batch->material.texture_source
                    != GE_PICA_TEXTURE_SOURCE_RARE_ID) {
            continue;
        }
        if (ge_3ds_scene_texture_find_mutable(scene,
                                              batch->texture.texture_id) != NULL) {
            continue;
        }
        if (scene->texture_count == scene->capacity) {
            ge_3ds_scene_textures_close(scene);
            return GE_3DS_SCENE_TEXTURE_CAPACITY_EXCEEDED;
        }
        slot = &scene->slots[scene->texture_count++];
        slot->image_id = batch->texture.texture_id;
        ge_3ds_scene_texture_index_append(scene);
        if (ge_3ds_scene_texture_import(cache, slot)) {
            scene->loaded_count++;
        } else {
            scene->missing_count++;
        }
    }
    return scene->missing_count == 0U ? GE_3DS_SCENE_TEXTURE_OK
                                     : GE_3DS_SCENE_TEXTURE_PARTIAL;
}

Ge3dsSceneTextureStatus ge_3ds_scene_textures_ensure_image(
    GeTextureCache *cache, Ge3dsSceneTextures *scene, uint16_t image_id)
{
    Ge3dsSceneTextureSlot *slot;

    if (cache == NULL || scene == NULL || scene->slots == NULL
            || scene->capacity == 0U)
        return GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT;
    slot = ge_3ds_scene_texture_find_mutable(scene, image_id);
    if (slot != NULL)
        return slot->loaded != 0U ? GE_3DS_SCENE_TEXTURE_OK
                                  : GE_3DS_SCENE_TEXTURE_PARTIAL;
    if (scene->texture_count == scene->capacity)
        return GE_3DS_SCENE_TEXTURE_CAPACITY_EXCEEDED;
    slot = &scene->slots[scene->texture_count++];
    memset(slot, 0, sizeof(*slot));
    slot->image_id = image_id;
    ge_3ds_scene_texture_index_append(scene);
    if (ge_3ds_scene_texture_import(cache, slot)) {
        scene->loaded_count++;
        return GE_3DS_SCENE_TEXTURE_OK;
    }
    scene->missing_count++;
    return GE_3DS_SCENE_TEXTURE_PARTIAL;
}

Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_prepare(
    GeTextureCache *cache,
    const GeDamRoomDrawBatch *batches,
    size_t batch_count,
    const Ge3dsSceneTextures *current,
    Ge3dsSceneTextureSlot *candidate_slots,
    size_t candidate_capacity,
    Ge3dsSceneTextures *candidate,
    Ge3dsSceneTextureReconcileStats *stats)
{
    const Ge3dsSceneTextureBatchRange range = {batches, batch_count};
    return ge_3ds_scene_textures_reconcile_prepare_ranges(cache, &range, 1U,
        current, candidate_slots, candidate_capacity, candidate, stats);
}

Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_prepare_ranges(
    GeTextureCache *cache,
    const Ge3dsSceneTextureBatchRange *ranges, size_t range_count,
    const Ge3dsSceneTextures *current,
    Ge3dsSceneTextureSlot *candidate_slots, size_t candidate_capacity,
    Ge3dsSceneTextures *candidate, Ge3dsSceneTextureReconcileStats *stats)
{
    size_t batch_index;
    size_t required_count = 0U;

    if (cache == NULL || current == NULL || candidate_slots == NULL
            || candidate_capacity == 0U || candidate == NULL
            || (range_count != 0U && ranges == NULL)
            || candidate == current || candidate_slots == current->slots)
        return GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT;
    for (size_t range_index = 0U; range_index < range_count; ++range_index)
        if (ranges[range_index].batch_count != 0U
                && ranges[range_index].batches == NULL)
            return GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT;
    memset(candidate, 0, sizeof(*candidate));
    memset(candidate_slots, 0,
           candidate_capacity * sizeof(*candidate_slots));
    candidate->slots = candidate_slots;
    candidate->capacity = candidate_capacity;
    if (stats != NULL) memset(stats, 0, sizeof(*stats));

    /* Preflight the unique authored image count. This makes the capacity
     * failure side-effect free, including zero calls into Tex3DS. Retain the
     * deduplicated IDs/index in first-use order for the import pass, rather
     * than scanning every authored batch a second time. */
    for (size_t range_index = 0U; range_index < range_count; ++range_index) {
        const GeDamRoomDrawBatch *batches = ranges[range_index].batches;
        for (batch_index = 0U; batch_index < ranges[range_index].batch_count;
                ++batch_index) {
            uint16_t image_id;
            if (!ge_3ds_scene_texture_batch_image_id(
                    &batches[batch_index], &image_id))
                continue;
            if (ge_3ds_scene_texture_find_mutable(candidate, image_id) != NULL)
                continue;
            ++required_count;
            if (required_count > candidate_capacity) {
                if (stats != NULL) stats->required_count = required_count;
                memset(candidate_slots, 0,
                       candidate_capacity * sizeof(*candidate_slots));
                memset(candidate, 0, sizeof(*candidate));
                return GE_3DS_SCENE_TEXTURE_CAPACITY_EXCEEDED;
            }
            candidate_slots[required_count - 1U].image_id = image_id;
            candidate->texture_count = required_count;
            ge_3ds_scene_texture_index_append(candidate);
        }
    }

    if (stats != NULL) stats->required_count = required_count;
    for (batch_index = 0U; batch_index < required_count; ++batch_index) {
        const Ge3dsSceneTextureSlot *resident;
        Ge3dsSceneTextureSlot *slot = &candidate->slots[batch_index];
        resident = ge_3ds_scene_texture_find_any(current, slot->image_id);
        if (resident != NULL && resident->loaded != 0U
                && resident->owned != 0U) {
            *slot = *resident;
            slot->owned = 0U;
            ++candidate->loaded_count;
            if (stats != NULL) ++stats->retained_count;
        } else if (ge_3ds_scene_texture_import(cache, slot)) {
            ++candidate->loaded_count;
            if (stats != NULL) ++stats->imported_count;
        } else {
            ++candidate->missing_count;
            if (stats != NULL) ++stats->missing_count;
        }
    }
    return candidate->missing_count == 0U ? GE_3DS_SCENE_TEXTURE_OK
                                          : GE_3DS_SCENE_TEXTURE_PARTIAL;
}

Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_include_image(
    GeTextureCache *cache, const Ge3dsSceneTextures *current,
    Ge3dsSceneTextures *candidate, uint16_t image_id,
    Ge3dsSceneTextureReconcileStats *stats)
{
    const Ge3dsSceneTextureSlot *resident;
    Ge3dsSceneTextureSlot *slot;
    if (cache == NULL || current == NULL || candidate == NULL
            || current == candidate || candidate->slots == NULL
            || candidate->slots == current->slots
            || candidate->texture_count > candidate->capacity)
        return GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT;
    slot = ge_3ds_scene_texture_find_mutable(candidate, image_id);
    if (slot != NULL) return slot->loaded != 0U
        ? GE_3DS_SCENE_TEXTURE_OK : GE_3DS_SCENE_TEXTURE_PARTIAL;
    if (candidate->texture_count == candidate->capacity)
        return GE_3DS_SCENE_TEXTURE_CAPACITY_EXCEEDED;
    slot = &candidate->slots[candidate->texture_count++];
    memset(slot, 0, sizeof(*slot));
    slot->image_id = image_id;
    ge_3ds_scene_texture_index_append(candidate);
    if (stats != NULL) ++stats->required_count;
    resident = ge_3ds_scene_texture_find_any(current, image_id);
    if (resident != NULL && resident->loaded != 0U && resident->owned != 0U) {
        *slot = *resident;
        slot->owned = 0U;
        ++candidate->loaded_count;
        if (stats != NULL) ++stats->retained_count;
    } else if (ge_3ds_scene_texture_import(cache, slot)) {
        ++candidate->loaded_count;
        if (stats != NULL) ++stats->imported_count;
    } else {
        ++candidate->missing_count;
        if (stats != NULL) ++stats->missing_count;
        return GE_3DS_SCENE_TEXTURE_PARTIAL;
    }
    return GE_3DS_SCENE_TEXTURE_OK;
}

Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_commit(
    Ge3dsSceneTextures *current,
    Ge3dsSceneTextures *candidate,
    Ge3dsSceneTextureReconcileStats *stats)
{
    return ge_3ds_scene_textures_reconcile_commit_after(
        current, candidate, stats, NULL, NULL);
}

Ge3dsSceneTextureStatus ge_3ds_scene_textures_reconcile_commit_after(
    Ge3dsSceneTextures *current,
    Ge3dsSceneTextures *candidate,
    Ge3dsSceneTextureReconcileStats *stats,
    Ge3dsSceneTextureCommitGate commit_gate, void *context)
{
    size_t candidate_index;
    size_t released_count = 0U;

    if (current == NULL || candidate == NULL || current == candidate
            || candidate->slots == NULL || candidate->capacity == 0U
            || candidate->slots == current->slots
            || candidate->texture_count > candidate->capacity
            || current->texture_count > current->capacity
            || (current->texture_count != 0U && current->slots == NULL))
        return GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT;

    /* Validate every borrow before changing either set, so a malformed or
     * stale candidate cannot partially transfer ownership. */
    for (candidate_index = 0U;
            candidate_index < candidate->texture_count; ++candidate_index) {
        const Ge3dsSceneTextureSlot *slot =
            &candidate->slots[candidate_index];
        const Ge3dsSceneTextureSlot *resident;
        if (slot->loaded == 0U || slot->owned != 0U) continue;
        resident = ge_3ds_scene_texture_find_any(current, slot->image_id);
        if (resident == NULL || resident->loaded == 0U
                || resident->owned == 0U)
            return GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT;
    }
    if (commit_gate != NULL && !commit_gate(context))
        return GE_3DS_SCENE_TEXTURE_COMMIT_REJECTED;
    for (candidate_index = 0U;
            candidate_index < candidate->texture_count; ++candidate_index) {
        Ge3dsSceneTextureSlot *slot = &candidate->slots[candidate_index];
        Ge3dsSceneTextureSlot *resident;
        if (slot->loaded == 0U || slot->owned != 0U) continue;
        resident = ge_3ds_scene_texture_find_mutable(current, slot->image_id);
        *slot = *resident;
        slot->owned = UINT8_C(1);
        memset(&resident->texture, 0, sizeof(resident->texture));
        resident->loaded = 0U;
        resident->owned = 0U;
    }
    if (current->slots != NULL) {
        size_t current_index;
        for (current_index = 0U; current_index < current->texture_count;
                ++current_index) {
            if (current->slots[current_index].loaded != 0U
                    && current->slots[current_index].owned != 0U)
                ++released_count;
        }
    }
    ge_3ds_scene_textures_close(current);
    if (stats != NULL) stats->released_count += released_count;
    return candidate->missing_count == 0U ? GE_3DS_SCENE_TEXTURE_OK
                                          : GE_3DS_SCENE_TEXTURE_PARTIAL;
}

const Ge3dsSceneTextureSlot *ge_3ds_scene_textures_find(
    const Ge3dsSceneTextures *scene,
    uint16_t image_id)
{
    const Ge3dsSceneTextureSlot *slot =
        ge_3ds_scene_texture_find_any(scene, image_id);
    return slot != NULL && slot->loaded != 0U ? slot : NULL;
}

GeTextureUvStatus ge_3ds_scene_texture_map_uv(
    const Ge3dsSceneTextureSlot *slot,
    int16_t texture_s,
    int16_t texture_t,
    const GePicaMaterial *material,
    GeTextureUv *result)
{
    GeTextureUv normalized;
    float top_left_u;
    float top_left_v;
    float top_right_u;
    float top_right_v;
    float bottom_left_u;
    float bottom_left_v;
    float bottom_right_u;
    float bottom_right_v;
    float top_u;
    float top_v;
    float bottom_u;
    float bottom_v;

    if (slot == NULL || slot->loaded == 0U || result == NULL
            || ge_texture_uv_normalize(texture_s, texture_t, material,
                                       slot->width, slot->height, &normalized)
                != GE_TEXTURE_UV_OK) {
        return GE_TEXTURE_UV_INVALID_ARGUMENT;
    }
    Tex3DS_SubTextureTopLeft(&slot->subtexture, &top_left_u, &top_left_v);
    Tex3DS_SubTextureTopRight(&slot->subtexture, &top_right_u, &top_right_v);
    Tex3DS_SubTextureBottomLeft(&slot->subtexture, &bottom_left_u,
                               &bottom_left_v);
    Tex3DS_SubTextureBottomRight(&slot->subtexture, &bottom_right_u,
                                &bottom_right_v);
    top_u = top_left_u + (top_right_u - top_left_u) * normalized.u;
    top_v = top_left_v + (top_right_v - top_left_v) * normalized.u;
    bottom_u = bottom_left_u
        + (bottom_right_u - bottom_left_u) * normalized.u;
    bottom_v = bottom_left_v
        + (bottom_right_v - bottom_left_v) * normalized.u;
    result->u = top_u + (bottom_u - top_u) * normalized.v;
    result->v = top_v + (bottom_v - top_v) * normalized.v;
    return GE_TEXTURE_UV_OK;
}

void ge_3ds_scene_textures_close(Ge3dsSceneTextures *scene)
{
    size_t index;

    if (scene == NULL) {
        return;
    }
    if (scene->slots != NULL) {
        for (index = 0U; index < scene->texture_count; ++index) {
            if (scene->slots[index].loaded != 0U
                    && scene->slots[index].owned != 0U) {
                C3D_TexDelete(&scene->slots[index].texture);
            }
        }
    }
    memset(scene, 0, sizeof(*scene));
}

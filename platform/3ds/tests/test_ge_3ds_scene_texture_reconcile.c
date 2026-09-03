#include "ge_3ds_scene_texture.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct TestTex3DS_Texture {
    Tex3DS_SubTexture subtexture;
};

static struct TestTex3DS_Texture imported_texture = {
    {0.0f, 0.0f, 1.0f, 1.0f},
};
static GeTextureCacheEntry cache_entry;
static uint16_t acquired_image_id;
static size_t import_count;
static size_t delete_count;
static uint32_t deleted_handles[32];

int ge_texture_cache_acquire_image_id(
    GeTextureCache *cache, uint16_t image_id, uint32_t requested_lod,
    GeTextureCatalogFallback fallback, const GeTextureCacheEntry **entry)
{
    (void)requested_lod;
    (void)fallback;
    assert(cache != NULL);
    assert(entry != NULL);
    if (image_id == UINT16_C(9)) return GE_TEXTURE_CACHE_NOT_FOUND;
    acquired_image_id = image_id;
    memset(&cache_entry, 0, sizeof(cache_entry));
    cache_entry.data = &acquired_image_id;
    cache_entry.data_size = sizeof(acquired_image_id);
    cache_entry.width = 32U;
    cache_entry.height = 16U;
    *entry = &cache_entry;
    return GE_TEXTURE_CACHE_OK;
}

int ge_texture_cache_release_entry(
    GeTextureCache *cache, const GeTextureCacheEntry *entry)
{
    assert(cache != NULL);
    assert(entry == &cache_entry);
    return GE_TEXTURE_CACHE_OK;
}

Tex3DS_Texture Tex3DS_TextureImport(
    const void *data, size_t data_size, C3D_Tex *texture,
    void *cube_map, bool vram)
{
    uint16_t image_id;
    (void)cube_map;
    (void)vram;
    assert(data != NULL);
    assert(data_size == sizeof(image_id));
    memcpy(&image_id, data, sizeof(image_id));
    texture->test_handle = UINT32_C(1000) + image_id;
    ++import_count;
    return &imported_texture;
}

size_t Tex3DS_GetNumSubTextures(Tex3DS_Texture texture)
{
    assert(texture == &imported_texture);
    return 1U;
}

const Tex3DS_SubTexture *Tex3DS_GetSubTexture(
    Tex3DS_Texture texture, size_t index)
{
    assert(texture == &imported_texture);
    assert(index == 0U);
    return &texture->subtexture;
}

void Tex3DS_TextureFree(Tex3DS_Texture texture)
{
    assert(texture == &imported_texture);
}

void C3D_TexDelete(C3D_Tex *texture)
{
    assert(texture != NULL);
    assert(texture->test_handle != 0U);
    assert(delete_count < sizeof(deleted_handles) / sizeof(deleted_handles[0]));
    deleted_handles[delete_count++] = texture->test_handle;
    texture->test_handle = 0U;
}

void Tex3DS_SubTextureTopLeft(
    const Tex3DS_SubTexture *subtexture, float *u, float *v)
{
    *u = subtexture->left;
    *v = subtexture->top;
}

void Tex3DS_SubTextureTopRight(
    const Tex3DS_SubTexture *subtexture, float *u, float *v)
{
    *u = subtexture->right;
    *v = subtexture->top;
}

void Tex3DS_SubTextureBottomLeft(
    const Tex3DS_SubTexture *subtexture, float *u, float *v)
{
    *u = subtexture->left;
    *v = subtexture->bottom;
}

void Tex3DS_SubTextureBottomRight(
    const Tex3DS_SubTexture *subtexture, float *u, float *v)
{
    *u = subtexture->right;
    *v = subtexture->bottom;
}

static GeDamRoomDrawBatch authored_batch(uint16_t image_id)
{
    GeDamRoomDrawBatch batch;
    memset(&batch, 0, sizeof(batch));
    batch.texture_valid = UINT8_C(1);
    batch.material.texture_enabled = UINT8_C(1);
    batch.material.texture_source = GE_PICA_TEXTURE_SOURCE_RARE_ID;
    batch.texture.texture_id = image_id;
    return batch;
}

static void resident_slot(
    Ge3dsSceneTextureSlot *slot, uint16_t image_id, uint32_t handle)
{
    memset(slot, 0, sizeof(*slot));
    slot->image_id = image_id;
    slot->texture.test_handle = handle;
    slot->width = 64U;
    slot->height = 32U;
    slot->loaded = UINT8_C(1);
    slot->owned = UINT8_C(1);
}

static size_t deletion_count(uint32_t handle)
{
    size_t index;
    size_t count = 0U;
    for (index = 0U; index < delete_count; ++index)
        if (deleted_handles[index] == handle) ++count;
    return count;
}

static void reset_counters(void)
{
    import_count = 0U;
    delete_count = 0U;
    memset(deleted_handles, 0, sizeof(deleted_handles));
}

static void test_abort_keeps_current_and_releases_only_import(void)
{
    GeTextureCache cache = {0};
    Ge3dsSceneTextureSlot current_slots[3];
    Ge3dsSceneTextureSlot candidate_slots[4];
    Ge3dsSceneTextures current = {current_slots, 3U, 3U, 3U, 0U};
    Ge3dsSceneTextures candidate;
    Ge3dsSceneTextureReconcileStats stats;
    GeDamRoomDrawBatch batches[] = {
        authored_batch(2U), authored_batch(3U), authored_batch(2U),
    };

    reset_counters();
    resident_slot(&current_slots[0], 1U, 101U);
    resident_slot(&current_slots[1], 2U, 102U);
    resident_slot(&current_slots[2], 4U, 104U);
    assert(ge_3ds_scene_textures_reconcile_prepare(
        &cache, batches, 3U, &current, candidate_slots, 4U,
        &candidate, &stats) == GE_3DS_SCENE_TEXTURE_OK);
    assert(stats.required_count == 2U);
    assert(stats.retained_count == 1U);
    assert(stats.imported_count == 1U);
    assert(import_count == 1U);
    assert(candidate.texture_count == 2U);
    assert(candidate.loaded_count == 2U);
    assert(candidate_slots[0].image_id == 2U);
    assert(candidate_slots[0].owned == 0U);
    assert(candidate_slots[0].texture.test_handle == 102U);
    assert(candidate_slots[1].image_id == 3U);
    assert(candidate_slots[1].owned == 1U);
    ge_3ds_scene_textures_close(&candidate);
    assert(delete_count == 1U);
    assert(deletion_count(1003U) == 1U);
    assert(current.texture_count == 3U);
    assert(current_slots[1].loaded == 1U);
    assert(current_slots[1].owned == 1U);
    assert(current_slots[1].texture.test_handle == 102U);
    ge_3ds_scene_textures_close(&current);
    assert(delete_count == 4U);
    assert(deletion_count(101U) == 1U);
    assert(deletion_count(102U) == 1U);
    assert(deletion_count(104U) == 1U);
}

static void test_commit_moves_retained_and_releases_removed_once(void)
{
    GeTextureCache cache = {0};
    Ge3dsSceneTextureSlot current_slots[3];
    Ge3dsSceneTextureSlot candidate_slots[4];
    Ge3dsSceneTextures current = {current_slots, 3U, 3U, 3U, 0U};
    Ge3dsSceneTextures candidate;
    Ge3dsSceneTextureReconcileStats stats;
    GeDamRoomDrawBatch batches[] = {
        authored_batch(2U), authored_batch(3U),
    };

    reset_counters();
    resident_slot(&current_slots[0], 1U, 201U);
    resident_slot(&current_slots[1], 2U, 202U);
    resident_slot(&current_slots[2], 4U, 204U);
    assert(ge_3ds_scene_textures_reconcile_prepare(
        &cache, batches, 2U, &current, candidate_slots, 4U,
        &candidate, &stats) == GE_3DS_SCENE_TEXTURE_OK);
    assert(ge_3ds_scene_textures_reconcile_commit(
        &current, &candidate, &stats) == GE_3DS_SCENE_TEXTURE_OK);
    assert(current.slots == NULL);
    assert(stats.retained_count == 1U);
    assert(stats.imported_count == 1U);
    assert(stats.released_count == 2U);
    assert(delete_count == 2U);
    assert(deletion_count(201U) == 1U);
    assert(deletion_count(204U) == 1U);
    assert(candidate_slots[0].owned == 1U);
    assert(candidate_slots[0].texture.test_handle == 202U);
    assert(candidate_slots[1].owned == 1U);
    ge_3ds_scene_textures_close(&candidate);
    assert(delete_count == 4U);
    assert(deletion_count(202U) == 1U);
    assert(deletion_count(1003U) == 1U);
}

static void test_partial_missing_is_safe_to_commit(void)
{
    GeTextureCache cache = {0};
    Ge3dsSceneTextureSlot current_slots[1];
    Ge3dsSceneTextureSlot candidate_slots[2];
    Ge3dsSceneTextures current = {current_slots, 1U, 1U, 1U, 0U};
    Ge3dsSceneTextures candidate;
    Ge3dsSceneTextureReconcileStats stats;
    GeDamRoomDrawBatch batches[] = {
        authored_batch(5U), authored_batch(9U),
    };

    reset_counters();
    resident_slot(&current_slots[0], 5U, 305U);
    assert(ge_3ds_scene_textures_reconcile_prepare(
        &cache, batches, 2U, &current, candidate_slots, 2U,
        &candidate, &stats) == GE_3DS_SCENE_TEXTURE_PARTIAL);
    assert(stats.retained_count == 1U);
    assert(stats.imported_count == 0U);
    assert(stats.missing_count == 1U);
    assert(candidate.loaded_count == 1U);
    assert(candidate.missing_count == 1U);
    assert(ge_3ds_scene_textures_reconcile_commit(
        &current, &candidate, &stats) == GE_3DS_SCENE_TEXTURE_PARTIAL);
    assert(stats.released_count == 0U);
    ge_3ds_scene_textures_close(&candidate);
    assert(delete_count == 1U);
    assert(deletion_count(305U) == 1U);
}

static void test_capacity_preflight_has_no_side_effects(void)
{
    GeTextureCache cache = {0};
    Ge3dsSceneTextureSlot current_slots[1];
    Ge3dsSceneTextureSlot candidate_slots[1];
    Ge3dsSceneTextures current = {current_slots, 1U, 1U, 1U, 0U};
    Ge3dsSceneTextures candidate;
    Ge3dsSceneTextureReconcileStats stats;
    GeDamRoomDrawBatch batches[] = {
        authored_batch(1U), authored_batch(2U),
    };

    reset_counters();
    resident_slot(&current_slots[0], 1U, 401U);
    memset(&candidate, 0xa5, sizeof(candidate));
    assert(ge_3ds_scene_textures_reconcile_prepare(
        &cache, batches, 2U, &current, candidate_slots, 1U,
        &candidate, &stats) == GE_3DS_SCENE_TEXTURE_CAPACITY_EXCEEDED);
    assert(import_count == 0U);
    assert(delete_count == 0U);
    assert(candidate.slots == NULL);
    assert(current_slots[0].texture.test_handle == 401U);
    assert(current_slots[0].owned == 1U);
    ge_3ds_scene_textures_close(&current);
    assert(delete_count == 1U);
}

static void test_empty_candidate_releases_all_on_commit(void)
{
    GeTextureCache cache = {0};
    Ge3dsSceneTextureSlot current_slots[2];
    Ge3dsSceneTextureSlot candidate_slots[1];
    Ge3dsSceneTextures current = {current_slots, 2U, 2U, 2U, 0U};
    Ge3dsSceneTextures candidate;
    Ge3dsSceneTextureReconcileStats stats;

    reset_counters();
    resident_slot(&current_slots[0], 1U, 501U);
    resident_slot(&current_slots[1], 2U, 502U);
    assert(ge_3ds_scene_textures_reconcile_prepare(
        &cache, NULL, 0U, &current, candidate_slots, 1U,
        &candidate, &stats) == GE_3DS_SCENE_TEXTURE_OK);
    assert(candidate.texture_count == 0U);
    assert(ge_3ds_scene_textures_reconcile_commit(
        &current, &candidate, &stats) == GE_3DS_SCENE_TEXTURE_OK);
    assert(stats.released_count == 2U);
    assert(delete_count == 2U);
    ge_3ds_scene_textures_close(&candidate);
    assert(delete_count == 2U);
}

static void test_stale_borrow_commit_is_side_effect_free(void)
{
    Ge3dsSceneTextureSlot current_slots[1];
    Ge3dsSceneTextureSlot candidate_slots[1];
    Ge3dsSceneTextures current = {current_slots, 1U, 1U, 1U, 0U};
    Ge3dsSceneTextures candidate = {candidate_slots, 1U, 1U, 1U, 0U};

    reset_counters();
    resident_slot(&current_slots[0], 6U, 606U);
    resident_slot(&candidate_slots[0], 7U, 607U);
    candidate_slots[0].owned = 0U;
    assert(ge_3ds_scene_textures_reconcile_commit(
        &current, &candidate, NULL)
        == GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT);
    assert(current.texture_count == 1U);
    assert(current_slots[0].loaded == 1U);
    assert(current_slots[0].owned == 1U);
    assert(current_slots[0].texture.test_handle == 606U);
    assert(delete_count == 0U);
    ge_3ds_scene_textures_close(&candidate);
    assert(delete_count == 0U);
    ge_3ds_scene_textures_close(&current);
    assert(delete_count == 1U);
    assert(deletion_count(606U) == 1U);
}

static void test_authored_hidden_dependencies(void)
{
    for (int commit = 0; commit < 2; ++commit) {
        GeTextureCache cache = {0};
        Ge3dsSceneTextureSlot current_slots[2], candidate_slots[4];
        Ge3dsSceneTextures current = {current_slots, 2U, 2U, 2U, 0U};
        Ge3dsSceneTextures candidate;
        Ge3dsSceneTextureReconcileStats stats;
        GeDamRoomDrawBatch batch = authored_batch(2U);
        reset_counters();
        resident_slot(&current_slots[0], 1U, 701U);
        resident_slot(&current_slots[1], 2U, 702U);
        assert(ge_3ds_scene_textures_reconcile_prepare(&cache, &batch, 1U,
            &current, candidate_slots, 4U, &candidate, &stats)
            == GE_3DS_SCENE_TEXTURE_OK);
        /* Hidden resident body part is borrowed, new head part is imported. */
        assert(ge_3ds_scene_textures_reconcile_include_image(&cache, &current,
            &candidate, 1U, &stats) == GE_3DS_SCENE_TEXTURE_OK);
        assert(ge_3ds_scene_textures_reconcile_include_image(&cache, &current,
            &candidate, 3U, &stats) == GE_3DS_SCENE_TEXTURE_OK);
        assert(ge_3ds_scene_textures_reconcile_include_image(&cache, &current,
            &candidate, 3U, &stats) == GE_3DS_SCENE_TEXTURE_OK);
        assert(stats.retained_count == 2U && stats.imported_count == 1U
            && stats.required_count == 3U && import_count == 1U);
        assert(ge_3ds_scene_textures_reconcile_include_image(&cache, &current,
            &candidate, 9U, &stats) == GE_3DS_SCENE_TEXTURE_PARTIAL);
        assert(ge_3ds_scene_textures_reconcile_include_image(&cache, &current,
            &candidate, 9U, &stats) == GE_3DS_SCENE_TEXTURE_PARTIAL);
        assert(stats.missing_count == 1U && stats.required_count == 4U);
        assert(ge_3ds_scene_textures_reconcile_include_image(&cache, &current,
            &candidate, 4U, &stats) == GE_3DS_SCENE_TEXTURE_CAPACITY_EXCEEDED);
        assert(import_count == 1U && delete_count == 0U);
        assert(ge_3ds_scene_textures_reconcile_include_image(&cache, &current,
            &current, 1U, &stats) == GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT);
        if (commit) {
            assert(ge_3ds_scene_textures_reconcile_commit(
                &current, &candidate, &stats) == GE_3DS_SCENE_TEXTURE_PARTIAL);
            assert(stats.released_count == 0U);
        }
        ge_3ds_scene_textures_close(&candidate);
        assert(delete_count == (commit ? 3U : 1U));
        if (!commit) {
            assert(current_slots[0].owned == 1U
                && current_slots[0].texture.test_handle == 701U);
            assert(current_slots[1].owned == 1U
                && current_slots[1].texture.test_handle == 702U);
            ge_3ds_scene_textures_close(&current);
        }
        assert(delete_count == 3U);
        assert(deletion_count(701U) == 1U && deletion_count(702U) == 1U
            && deletion_count(1003U) == 1U);
    }
}

int main(void)
{
    test_abort_keeps_current_and_releases_only_import();
    test_commit_moves_retained_and_releases_removed_once();
    test_partial_missing_is_safe_to_commit();
    test_capacity_preflight_has_no_side_effects();
    test_empty_candidate_releases_all_on_commit();
    test_stale_borrow_commit_is_side_effect_free();
    test_authored_hidden_dependencies();
    puts("ge_3ds_scene_texture reconciliation: 8 cases passed");
    return 0;
}

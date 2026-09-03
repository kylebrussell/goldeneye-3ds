#include "ge_3ds_scene_texture.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
static uint32_t deleted_handles[1024];

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
    Ge3dsSceneTextures current = {.slots = current_slots, .capacity = 3U,
        .texture_count = 3U, .loaded_count = 3U};
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
    Ge3dsSceneTextures current = {.slots = current_slots, .capacity = 3U,
        .texture_count = 3U, .loaded_count = 3U};
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
    Ge3dsSceneTextures current = {.slots = current_slots, .capacity = 1U,
        .texture_count = 1U, .loaded_count = 1U};
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
    Ge3dsSceneTextures current = {.slots = current_slots, .capacity = 1U,
        .texture_count = 1U, .loaded_count = 1U};
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
    Ge3dsSceneTextures current = {.slots = current_slots, .capacity = 2U,
        .texture_count = 2U, .loaded_count = 2U};
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
    Ge3dsSceneTextures current = {.slots = current_slots, .capacity = 1U,
        .texture_count = 1U, .loaded_count = 1U};
    Ge3dsSceneTextures candidate = {.slots = candidate_slots, .capacity = 1U,
        .texture_count = 1U, .loaded_count = 1U};

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
        Ge3dsSceneTextures current = {.slots = current_slots, .capacity = 2U,
            .texture_count = 2U, .loaded_count = 2U};
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

static void check_all_image_ids(const Ge3dsSceneTextures *scene)
{
    Ge3dsSceneTextures unindexed = *scene;
    unindexed.indexed_count = SIZE_MAX;
    for (uint32_t image = 0U; image <= UINT16_MAX; ++image)
        assert(ge_3ds_scene_textures_find(scene, (uint16_t)image)
            == ge_3ds_scene_textures_find(&unindexed, (uint16_t)image));
}

static void test_index_load_append_move_and_large_fallback(void)
{
    GeTextureCache cache = {0};
    GeDamRoomDrawBatch batches[193];
    Ge3dsSceneTextureSlot slots[300], moved_slots[300];
    Ge3dsSceneTextures scene, moved;
    reset_counters();
    for (size_t i = 0; i < 191U; ++i) batches[i] = authored_batch((uint16_t)i);
    batches[191] = authored_batch(UINT16_MAX);
    batches[192] = authored_batch(9U); /* Failed import must deduplicate. */
    assert(ge_3ds_scene_textures_load(&cache, batches, 193U, slots, 300U, &scene)
        == GE_3DS_SCENE_TEXTURE_PARTIAL);
    assert(scene.texture_count == 192U && scene.indexed_count == 192U);
    assert(scene.loaded_count == 191U && scene.missing_count == 1U);
    assert(ge_3ds_scene_textures_find(&scene, 0U) == &slots[0]);
    assert(ge_3ds_scene_textures_find(&scene, UINT16_MAX) == &slots[191]);
    check_all_image_ids(&scene);

#ifdef GE_SCENE_TEXTURE_LOOKUP_BENCH
    /* Compare the actual indexed and fallback paths with identical inputs.
     * Timing is host-only evidence, not a 3DS or emulator FPS claim. */
    Ge3dsSceneTextures unindexed = scene;
    const size_t iterations = 4000000U;
    volatile uintptr_t checksum = 0U;
    unindexed.indexed_count = SIZE_MAX;
    clock_t begin = clock();
    for (size_t i = 0; i < iterations; ++i)
        checksum ^= (uintptr_t)ge_3ds_scene_textures_find(&unindexed,
            (uint16_t)(i % 256U));
    clock_t middle = clock();
    for (size_t i = 0; i < iterations; ++i)
        checksum ^= (uintptr_t)ge_3ds_scene_textures_find(&scene,
            (uint16_t)(i % 256U));
    clock_t end = clock();
    printf("192-texture lookup, %zu queries: linear %.2f ms, indexed %.2f ms (%lu)\n",
        iterations, 1000.0 * (middle - begin) / CLOCKS_PER_SEC,
        1000.0 * (end - middle) / CLOCKS_PER_SEC, (unsigned long)checksum);
#endif

    /* Permanent storage relocation must use offsets, never stale pointers. */
    memcpy(moved_slots, slots, sizeof(slots));
    moved = scene;
    moved.slots = moved_slots;
    scene.slots = NULL;
    assert(ge_3ds_scene_textures_find(&moved, UINT16_MAX) == &moved_slots[191]);
    assert(ge_3ds_scene_textures_find(&scene, UINT16_MAX) == NULL);
    assert(ge_3ds_scene_textures_ensure_image(&cache, &moved, 9U)
        == GE_3DS_SCENE_TEXTURE_PARTIAL);
    assert(moved.texture_count == 192U && import_count == 191U);
    for (uint16_t image = 191U; image < 256U; ++image) {
        assert(ge_3ds_scene_textures_ensure_image(&cache, &moved, image)
            == GE_3DS_SCENE_TEXTURE_OK);
        if (moved.texture_count <= GE_3DS_SCENE_TEXTURE_INDEX_MAX_ENTRIES)
            assert(moved.indexed_count == moved.texture_count);
        if (moved.texture_count == GE_3DS_SCENE_TEXTURE_INDEX_MAX_ENTRIES)
            check_all_image_ids(&moved);
    }
    assert(moved.texture_count == 257U && moved.indexed_count == 0U);
    check_all_image_ids(&moved);
    ge_3ds_scene_textures_close(&moved);
    assert(delete_count == 256U);
    assert(moved.indexed_count == 0U && moved.slots == NULL);
    assert(ge_3ds_scene_textures_find(&moved, 0U) == NULL);
}

static void test_index_collisions_reconcile_and_unindexed_append(void)
{
    GeTextureCache cache = {0};
    GeDamRoomDrawBatch batches[64], replacement[33];
    Ge3dsSceneTextureSlot slots[64], candidate_slots[64];
    Ge3dsSceneTextures scene, candidate;
    Ge3dsSceneTextureReconcileStats stats;
    size_t count = 0U;
    reset_counters();
    /* A long probe chain starts at the last bucket and wraps to bucket 0. */
    for (uint32_t id = 0U; id <= UINT16_MAX && count < 64U; ++id) {
        uint32_t hash = id * UINT32_C(2654435761);
        if ((hash >> 23U) == GE_3DS_SCENE_TEXTURE_INDEX_CAPACITY - 1U)
            batches[count++] = authored_batch((uint16_t)id);
    }
    assert(count == 64U);
    assert(ge_3ds_scene_textures_load(&cache, batches, count, slots, count, &scene)
        == GE_3DS_SCENE_TEXTURE_OK);
    assert(scene.indexed_count == count);
    check_all_image_ids(&scene);
    for (size_t i = 0U; i < 32U; ++i) replacement[i] = batches[i * 2U];
    replacement[32] = authored_batch(9U);
    assert(ge_3ds_scene_textures_reconcile_prepare(&cache, replacement, 33U,
        &scene, candidate_slots, 64U, &candidate, &stats)
        == GE_3DS_SCENE_TEXTURE_PARTIAL);
    assert(stats.retained_count == 32U && stats.imported_count == 0U);
    assert(candidate.indexed_count == candidate.texture_count);
    check_all_image_ids(&candidate);
    assert(ge_3ds_scene_textures_reconcile_include_image(&cache, &scene,
        &candidate, batches[1].texture.texture_id, &stats) == GE_3DS_SCENE_TEXTURE_OK);
    assert(candidate.texture_count == 34U && candidate.indexed_count == 34U);
    assert(ge_3ds_scene_textures_reconcile_commit(&scene, &candidate, &stats)
        == GE_3DS_SCENE_TEXTURE_PARTIAL);
    check_all_image_ids(&candidate);
    assert(delete_count == 31U);

    /* Existing externally constructed sets still work; their next append
     * creates a complete index rather than indexing only the appended ID. */
    memset(candidate.image_index, 0, sizeof(candidate.image_index));
    candidate.indexed_count = 0U;
    assert(ge_3ds_scene_textures_ensure_image(&cache, &candidate, 0U)
        == GE_3DS_SCENE_TEXTURE_OK);
    assert(candidate.indexed_count == 35U);
    check_all_image_ids(&candidate);
    ge_3ds_scene_textures_close(&candidate);
    assert(delete_count == 65U);
}

static void test_large_preflight_first_use_order_and_rollback(void)
{
    GeTextureCache cache = {0};
    Ge3dsSceneTextures current = {0}, candidate;
    Ge3dsSceneTextureSlot slots[192];
    GeDamRoomDrawBatch batches[4096];
    Ge3dsSceneTextureReconcileStats stats;
    uint16_t expected_ids[192];
    size_t expected_count = 0U;
    reset_counters();
    for (size_t i = 0U; i < 4096U; ++i) {
        const uint16_t image = (uint16_t)((i * 73U) % 192U);
        batches[i] = authored_batch(image);
        if (i % 7U == 0U) {
            batches[i].texture_valid = 0U;
            continue;
        }
        size_t prior = 0;
        while (prior < expected_count && expected_ids[prior] != image) ++prior;
        if (prior == expected_count) expected_ids[expected_count++] = image;
    }
    assert(expected_count == 192U);
    assert(ge_3ds_scene_textures_reconcile_prepare(&cache, batches, 4096U,
        &current, slots, 191U, &candidate, &stats)
        == GE_3DS_SCENE_TEXTURE_CAPACITY_EXCEEDED);
    assert(stats.required_count == 192U && import_count == 0U && delete_count == 0U);
    Ge3dsSceneTextures empty = {0};
    assert(memcmp(&candidate, &empty, sizeof(empty)) == 0);
    for (size_t i = 0U; i < 191U; ++i) {
        Ge3dsSceneTextureSlot empty_slot = {0};
        assert(memcmp(&slots[i], &empty_slot, sizeof(empty_slot)) == 0);
    }
    assert(ge_3ds_scene_textures_reconcile_prepare(&cache, batches, 4096U,
        &current, slots, 192U, &candidate, &stats) == GE_3DS_SCENE_TEXTURE_PARTIAL);
    assert(stats.required_count == 192U && stats.imported_count == 191U
        && stats.missing_count == 1U && candidate.indexed_count == 192U);
    for (size_t i = 0U; i < expected_count; ++i)
        assert(candidate.slots[i].image_id == expected_ids[i]);
    check_all_image_ids(&candidate);
    ge_3ds_scene_textures_close(&candidate);
    assert(delete_count == 191U);
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
    test_index_load_append_move_and_large_fallback();
    test_index_collisions_reconcile_and_unindexed_append();
    test_large_preflight_first_use_order_and_rollback();
    puts("ge_3ds_scene_texture reconciliation/index: 11 cases passed");
    return 0;
}

#include "ge_texture_cache.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Fixture {
    uint8_t *catalog_data;
    size_t catalog_size;
    GeTextureCatalog catalog;
    GeAssetPack pack;
} Fixture;

static void fixture_open(Fixture *fixture, const char *catalog_path, const char *pack_path)
{
    FILE *file = fopen(catalog_path, "rb");
    long size;

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && fseek(file, 0, SEEK_SET) == 0);
    fixture->catalog_size = (size_t)size;
    fixture->catalog_data = malloc(fixture->catalog_size);
    assert(fixture->catalog_data != NULL);
    assert(fread(fixture->catalog_data, 1U, fixture->catalog_size, file) ==
           fixture->catalog_size);
    fclose(file);
    assert(ge_texture_catalog_open_memory(&fixture->catalog, fixture->catalog_data,
                                          fixture->catalog_size) == GE_TEXTURE_CATALOG_OK);
    assert(ge_asset_pack_open(&fixture->pack, pack_path) == GE_ASSET_PACK_OK);
}

static void fixture_close(Fixture *fixture)
{
    ge_asset_pack_close(&fixture->pack);
    ge_texture_catalog_close(&fixture->catalog);
    free(fixture->catalog_data);
    memset(fixture, 0, sizeof(*fixture));
}

static int cache_contains(const GeTextureCache *cache, const char *source, uint32_t lod)
{
    size_t index;
    for (index = 0U; index < cache->entry_count; index++) {
        const GeTextureCacheEntry *entry = &cache->entries[index];
        if (entry->data != NULL && entry->lod == lod && strcmp(entry->source, source) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_metadata_hits_and_lru(Fixture *fixture)
{
    GeTextureCache cache;
    GeTextureCacheEntry slots[2];
    GeTextureCacheStats stats;
    GeTextureAsset asset;
    const GeTextureCacheEntry *entry;
    const void *first_data;

    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, slots, 2U,
                                 1024U * 1024U, GE_TEXTURE_FORMAT_RGBA5551,
                                 NULL, NULL, NULL) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_catalog_find(&fixture->catalog, "COPYICON.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, &asset)
           == GE_TEXTURE_CATALOG_OK);
    assert(ge_texture_cache_acquire(&cache, "COPYICON.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(entry->lod == 0U && entry->width == 32U && entry->height == 28U);
    assert(entry->format == GE_TEXTURE_FORMAT_RGBA5551);
    assert(entry->data_size == asset.data_size && entry->data_size > 0U);
    assert(entry->pin_count == 1U && entry->data != NULL);
    first_data = entry->data;
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);

    assert(ge_texture_cache_acquire(&cache, "1000.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "COPYICON.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(entry->data == first_data);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "1001.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);

    assert(cache_contains(&cache, "COPYICON.bin", 0U));
    assert(!cache_contains(&cache, "1000.bin", 0U));
    assert(cache_contains(&cache, "1001.bin", 0U));
    ge_texture_cache_get_stats(&cache, &stats);
    assert(stats.hits == 1U && stats.misses == 3U && stats.evictions == 1U);
    assert(stats.occupied_slots == 2U && stats.slot_count == 2U);
    assert(stats.loaded_bytes <= stats.byte_limit);
    ge_texture_cache_close(&cache);
}

static void test_lookup_fallback(Fixture *fixture)
{
    GeTextureCache cache;
    GeTextureCacheEntry slot;
    const GeTextureCacheEntry *entry;

    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, &slot, 1U,
                                 4096U, GE_TEXTURE_FORMAT_RGBA5551,
                                 NULL, NULL, NULL) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "448.bin", 6U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) ==
           GE_TEXTURE_CACHE_NOT_FOUND);
    assert(entry == NULL);
    assert(ge_texture_cache_acquire(&cache, "448.bin", 6U,
                                    GE_TEXTURE_CATALOG_NEAREST_LOD, &entry) ==
           GE_TEXTURE_CACHE_OK);
    assert(entry->lod == 5U && entry->width == 2U && entry->height == 2U);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);
    ge_texture_cache_close(&cache);
}

static void test_image_id_lookup(Fixture *fixture)
{
    static const uint16_t dam_image_ids[] = {
        8U, 18U, 22U, 152U, 174U, 239U, 240U, 286U, 288U,
        291U, 292U, 299U, 436U, 438U, 452U, 495U, 505U, 631U,
        949U, 1125U, 1126U, 1127U, 1167U, 1203U, 1368U, 1369U,
        1383U,
    };
    GeTextureCache cache;
    GeTextureCacheEntry slot;
    const GeTextureCacheEntry *entry;
    size_t index;

    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack,
                                 &slot, 1U, 64U * 1024U,
                                 GE_TEXTURE_FORMAT_RGBA5551,
                                 NULL, NULL, NULL) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire_image_id(&cache, UINT16_C(22), 0U,
                                             GE_TEXTURE_CATALOG_EXACT,
                                             &entry) == GE_TEXTURE_CACHE_OK);
    assert(strcmp(entry->source, "TARDETAIL.bin") == 0);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);
    for (index = 0U;
            index < sizeof(dam_image_ids) / sizeof(dam_image_ids[0]); ++index) {
        assert(ge_texture_cache_acquire_image_id(
                   &cache, dam_image_ids[index], 0U,
                   GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
        assert(entry->width != 0U && entry->height != 0U);
        assert(ge_texture_cache_release_entry(&cache, entry)
               == GE_TEXTURE_CACHE_OK);
    }
    assert(ge_texture_cache_acquire_image_id(&cache, UINT16_C(0x0fff), 0U,
                                             GE_TEXTURE_CATALOG_EXACT,
                                             &entry) == GE_TEXTURE_CACHE_NOT_FOUND);
    ge_texture_cache_close(&cache);
}

static void test_pins_and_flush(Fixture *fixture)
{
    GeTextureCache cache;
    GeTextureCacheEntry slots[2];
    GeTextureCacheStats stats;
    const GeTextureCacheEntry *first;
    const GeTextureCacheEntry *second;

    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, slots, 2U,
                                 1024U * 1024U, GE_TEXTURE_FORMAT_RGBA5551,
                                 NULL, NULL, NULL) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "COPYICON.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &first) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "1000.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &second) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_release_entry(&cache, second) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_flush(&cache) == GE_TEXTURE_CACHE_BUSY);
    ge_texture_cache_get_stats(&cache, &stats);
    assert(stats.occupied_slots == 1U && stats.loaded_bytes == first->data_size);
    assert(ge_texture_cache_release_entry(&cache, first) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_release_entry(&cache, first) == GE_TEXTURE_CACHE_INVALID);
    assert(ge_texture_cache_flush(&cache) == GE_TEXTURE_CACHE_OK);
    ge_texture_cache_get_stats(&cache, &stats);
    assert(stats.occupied_slots == 0U && stats.loaded_bytes == 0U);
    ge_texture_cache_close(&cache);
}

static void test_all_pinned_is_busy(Fixture *fixture)
{
    GeTextureCache cache;
    GeTextureCacheEntry slot;
    const GeTextureCacheEntry *entry;
    const GeTextureCacheEntry *blocked = (const GeTextureCacheEntry *)1;

    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, &slot, 1U,
                                 1024U * 1024U, GE_TEXTURE_FORMAT_RGBA5551,
                                 NULL, NULL, NULL) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "COPYICON.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "1000.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &blocked) == GE_TEXTURE_CACHE_BUSY);
    assert(blocked == NULL);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "1000.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(cache.loaded_bytes <= cache.byte_limit);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);
    ge_texture_cache_close(&cache);
}

static void test_byte_budget_eviction(Fixture *fixture)
{
    GeTextureAsset second_asset;
    GeTextureAsset third_asset;
    GeTextureCache cache;
    GeTextureCacheEntry slots[3];
    GeTextureCacheStats stats;
    const GeTextureCacheEntry *entry;
    size_t byte_limit;

    assert(ge_texture_catalog_find(&fixture->catalog, "1000.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, &second_asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(ge_texture_catalog_find(&fixture->catalog, "1001.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, &third_asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(second_asset.data_size <= SIZE_MAX - third_asset.data_size);
    byte_limit = (size_t)(second_asset.data_size + third_asset.data_size);
    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, slots, 3U,
                                 byte_limit, GE_TEXTURE_FORMAT_RGBA5551,
                                 NULL, NULL, NULL) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "COPYICON.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "1000.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "1001.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_release_entry(&cache, entry) == GE_TEXTURE_CACHE_OK);

    assert(!cache_contains(&cache, "COPYICON.bin", 0U));
    assert(cache_contains(&cache, "1000.bin", 0U));
    assert(cache_contains(&cache, "1001.bin", 0U));
    ge_texture_cache_get_stats(&cache, &stats);
    assert(stats.evictions == 1U && stats.loaded_bytes == byte_limit);
    ge_texture_cache_close(&cache);
}

static void *failing_allocate(void *context, size_t size)
{
    (void)context;
    (void)size;
    return NULL;
}

static void ignored_release(void *context, void *allocation)
{
    (void)context;
    (void)allocation;
}

static void test_limits_and_allocator_failure(Fixture *fixture)
{
    GeTextureAsset asset;
    GeTextureCache cache;
    GeTextureCacheEntry slot;
    const GeTextureCacheEntry *entry;

    assert(ge_texture_catalog_find(&fixture->catalog, "COPYICON.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, &asset) == GE_TEXTURE_CATALOG_OK);
    assert(asset.data_size > 1U);
    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, &slot, 1U,
                                 (size_t)asset.data_size - 1U, GE_TEXTURE_FORMAT_RGBA5551,
                                 NULL, NULL, NULL) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "COPYICON.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) ==
           GE_TEXTURE_CACHE_TOO_LARGE);
    ge_texture_cache_close(&cache);

    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, &slot, 1U,
                                 4096U, GE_TEXTURE_FORMAT_RGBA5551,
                                 failing_allocate, ignored_release, NULL) == GE_TEXTURE_CACHE_OK);
    assert(ge_texture_cache_acquire(&cache, "COPYICON.bin", 0U,
                                    GE_TEXTURE_CATALOG_EXACT, &entry) ==
           GE_TEXTURE_CACHE_NO_MEMORY);
    ge_texture_cache_close(&cache);
}

static void test_invalid_initialization(Fixture *fixture)
{
    GeTextureCache cache;
    GeTextureCacheEntry slot;

    assert(ge_texture_cache_init(NULL, &fixture->catalog, &fixture->pack, &slot, 1U, 1U,
                                 GE_TEXTURE_FORMAT_RGBA5551, NULL, NULL, NULL) ==
           GE_TEXTURE_CACHE_INVALID);
    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, &slot, 0U, 1U,
                                 GE_TEXTURE_FORMAT_RGBA5551, NULL, NULL, NULL) ==
           GE_TEXTURE_CACHE_INVALID);
    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, &slot, 1U, 0U,
                                 GE_TEXTURE_FORMAT_RGBA5551, NULL, NULL, NULL) ==
           GE_TEXTURE_CACHE_INVALID);
    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, &slot, 1U, 1U,
                                 GE_TEXTURE_FORMAT_UNKNOWN, NULL, NULL, NULL) ==
           GE_TEXTURE_CACHE_INVALID);
    assert(ge_texture_cache_init(&cache, &fixture->catalog, &fixture->pack, &slot, 1U, 1U,
                                 GE_TEXTURE_FORMAT_RGBA5551,
                                 failing_allocate, NULL, NULL) == GE_TEXTURE_CACHE_INVALID);
}

int main(int argc, char **argv)
{
    Fixture fixture = {0};

    assert(argc == 3);
    fixture_open(&fixture, argv[1], argv[2]);
    test_metadata_hits_and_lru(&fixture);
    test_lookup_fallback(&fixture);
    test_image_id_lookup(&fixture);
    test_pins_and_flush(&fixture);
    test_all_pinned_is_busy(&fixture);
    test_byte_budget_eviction(&fixture);
    test_limits_and_allocator_failure(&fixture);
    test_invalid_initialization(&fixture);
    fixture_close(&fixture);
    puts("bounded demand-loaded texture cache tests passed");
    return 0;
}

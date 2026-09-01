#include "ge_texture_cache.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void *default_allocate(void *context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void default_release(void *context, void *allocation)
{
    (void)context;
    free(allocation);
}

static uint64_t next_access_stamp(GeTextureCache *cache)
{
    size_t index;

    if (cache->access_serial == UINT64_MAX) {
        /* A serial wrap is not reachable in normal play. Rebase by slot order
         * so even deliberately forced wraparound remains deterministic. */
        for (index = 0U; index < cache->entry_count; index++) {
            if (cache->entries[index].data != NULL) {
                cache->entries[index].access_stamp = (uint64_t)index + 1U;
            }
        }
        cache->access_serial = (uint64_t)cache->entry_count + 1U;
    }
    cache->access_serial++;
    return cache->access_serial;
}

static void evict_entry(GeTextureCache *cache, size_t index)
{
    GeTextureCacheEntry *entry = &cache->entries[index];

    cache->release(cache->allocator_context, entry->data);
    cache->loaded_bytes -= entry->data_size;
    memset(entry, 0, sizeof(*entry));
    cache->evictions++;
}

static size_t find_free_slot(const GeTextureCache *cache)
{
    size_t index;

    for (index = 0U; index < cache->entry_count; index++) {
        if (cache->entries[index].data == NULL) {
            return index;
        }
    }
    return cache->entry_count;
}

static size_t find_victim(const GeTextureCache *cache)
{
    size_t victim = cache->entry_count;
    size_t index;

    for (index = 0U; index < cache->entry_count; index++) {
        const GeTextureCacheEntry *entry = &cache->entries[index];
        if (entry->data != NULL && entry->pin_count == 0U &&
            (victim == cache->entry_count || entry->access_stamp < cache->entries[victim].access_stamp ||
             (entry->access_stamp == cache->entries[victim].access_stamp && index < victim))) {
            victim = index;
        }
    }
    return victim;
}

static int can_make_room(const GeTextureCache *cache, size_t incoming_size)
{
    size_t reclaimable = 0U;
    int has_slot = 0;
    size_t index;

    for (index = 0U; index < cache->entry_count; index++) {
        const GeTextureCacheEntry *entry = &cache->entries[index];
        if (entry->data == NULL) {
            has_slot = 1;
        } else if (entry->pin_count == 0U) {
            has_slot = 1;
            if (entry->data_size > SIZE_MAX - reclaimable) {
                reclaimable = SIZE_MAX;
            } else {
                reclaimable += entry->data_size;
            }
        }
    }
    if (!has_slot) {
        return 0;
    }
    if (incoming_size <= cache->byte_limit - cache->loaded_bytes) {
        return 1;
    }
    return reclaimable >= incoming_size - (cache->byte_limit - cache->loaded_bytes);
}

int ge_texture_cache_init(GeTextureCache *cache, GeTextureCatalog *catalog,
                          GeAssetPack *pack, GeTextureCacheEntry *entries,
                          size_t entry_count, size_t byte_limit,
                          GeTextureFormat format, GeTextureCacheAllocate allocate,
                          GeTextureCacheRelease release, void *allocator_context)
{
    if (cache == NULL || catalog == NULL || catalog->data == NULL || pack == NULL ||
        pack->file == NULL || entries == NULL || entry_count == 0U ||
        entry_count > SIZE_MAX / sizeof(*entries) || byte_limit == 0U ||
        format <= GE_TEXTURE_FORMAT_UNKNOWN || format > GE_TEXTURE_FORMAT_ETC1A4 ||
        ((allocate == NULL) != (release == NULL))) {
        return GE_TEXTURE_CACHE_INVALID;
    }
    memset(cache, 0, sizeof(*cache));
    memset(entries, 0, entry_count * sizeof(*entries));
    cache->catalog = catalog;
    cache->pack = pack;
    cache->entries = entries;
    cache->entry_count = entry_count;
    cache->byte_limit = byte_limit;
    cache->format = format;
    cache->allocate = allocate != NULL ? allocate : default_allocate;
    cache->release = release != NULL ? release : default_release;
    cache->allocator_context = allocator_context;
    return GE_TEXTURE_CACHE_OK;
}

int ge_texture_cache_acquire(GeTextureCache *cache, const char *source,
                             uint32_t requested_lod, GeTextureCatalogFallback fallback,
                             const GeTextureCacheEntry **entry)
{
    GeTextureAsset asset;
    size_t index;
    size_t slot;
    void *data;
    size_t bytes_read = 0U;
    int result;

    if (entry != NULL) {
        *entry = NULL;
    }
    if (cache == NULL || cache->entries == NULL || source == NULL || entry == NULL) {
        return GE_TEXTURE_CACHE_INVALID;
    }
    result = ge_texture_catalog_find(cache->catalog, source, requested_lod, fallback, &asset);
    if (result == GE_TEXTURE_CATALOG_NOT_FOUND) {
        return GE_TEXTURE_CACHE_NOT_FOUND;
    }
    if (result != GE_TEXTURE_CATALOG_OK) {
        return GE_TEXTURE_CACHE_INVALID;
    }

    for (index = 0U; index < cache->entry_count; index++) {
        GeTextureCacheEntry *candidate = &cache->entries[index];
        if (candidate->data != NULL && candidate->lod == asset.lod &&
            strcmp(candidate->source, asset.source) == 0) {
            if (candidate->pin_count == UINT32_MAX) {
                return GE_TEXTURE_CACHE_BUSY;
            }
            candidate->pin_count++;
            candidate->access_stamp = next_access_stamp(cache);
            cache->hits++;
            *entry = candidate;
            return GE_TEXTURE_CACHE_OK;
        }
    }

    cache->misses++;
    if (asset.data_size > SIZE_MAX || (size_t)asset.data_size > cache->byte_limit) {
        return GE_TEXTURE_CACHE_TOO_LARGE;
    }
    if (!can_make_room(cache, (size_t)asset.data_size)) {
        return GE_TEXTURE_CACHE_BUSY;
    }
    data = cache->allocate(cache->allocator_context, (size_t)asset.data_size);
    if (data == NULL) {
        return GE_TEXTURE_CACHE_NO_MEMORY;
    }
    result = ge_asset_pack_read(cache->pack, asset.resource_path, data,
                                (size_t)asset.data_size, &bytes_read);
    if (result != GE_ASSET_PACK_OK || bytes_read != (size_t)asset.data_size) {
        cache->release(cache->allocator_context, data);
        return GE_TEXTURE_CACHE_PACK_ERROR;
    }

    while (cache->loaded_bytes > cache->byte_limit - (size_t)asset.data_size ||
           find_free_slot(cache) == cache->entry_count) {
        const size_t victim = find_victim(cache);
        if (victim == cache->entry_count) {
            cache->release(cache->allocator_context, data);
            return GE_TEXTURE_CACHE_BUSY;
        }
        evict_entry(cache, victim);
    }
    slot = find_free_slot(cache);
    cache->entries[slot].source = asset.source;
    cache->entries[slot].resource_path = asset.resource_path;
    cache->entries[slot].data = data;
    cache->entries[slot].data_size = (size_t)asset.data_size;
    cache->entries[slot].access_stamp = next_access_stamp(cache);
    cache->entries[slot].lod = asset.lod;
    cache->entries[slot].width = asset.width;
    cache->entries[slot].height = asset.height;
    cache->entries[slot].pin_count = 1U;
    cache->entries[slot].format = cache->format;
    cache->loaded_bytes += (size_t)asset.data_size;
    *entry = &cache->entries[slot];
    return GE_TEXTURE_CACHE_OK;
}

int ge_texture_cache_acquire_image_id(GeTextureCache *cache, uint16_t image_id,
                                      uint32_t requested_lod,
                                      GeTextureCatalogFallback fallback,
                                      const GeTextureCacheEntry **entry)
{
    GeTextureAsset asset;
    int result;

    if (entry != NULL) {
        *entry = NULL;
    }
    if (cache == NULL || cache->catalog == NULL || entry == NULL) {
        return GE_TEXTURE_CACHE_INVALID;
    }
    result = ge_texture_catalog_find_image_id(cache->catalog, image_id,
                                              requested_lod, fallback, &asset);
    if (result == GE_TEXTURE_CATALOG_NOT_FOUND) {
        return GE_TEXTURE_CACHE_NOT_FOUND;
    }
    if (result != GE_TEXTURE_CATALOG_OK) {
        return GE_TEXTURE_CACHE_INVALID;
    }
    return ge_texture_cache_acquire(cache, asset.source, requested_lod,
                                    fallback, entry);
}

int ge_texture_cache_release_entry(GeTextureCache *cache,
                                   const GeTextureCacheEntry *entry)
{
    size_t index;

    if (cache == NULL || cache->entries == NULL || entry == NULL) {
        return GE_TEXTURE_CACHE_INVALID;
    }
    for (index = 0U; index < cache->entry_count; index++) {
        if (&cache->entries[index] == entry) {
            if (cache->entries[index].data == NULL || cache->entries[index].pin_count == 0U) {
                return GE_TEXTURE_CACHE_INVALID;
            }
            cache->entries[index].pin_count--;
            return GE_TEXTURE_CACHE_OK;
        }
    }
    return GE_TEXTURE_CACHE_INVALID;
}

int ge_texture_cache_flush(GeTextureCache *cache)
{
    size_t index;
    int pinned = 0;

    if (cache == NULL || cache->entries == NULL) {
        return GE_TEXTURE_CACHE_INVALID;
    }
    for (index = 0U; index < cache->entry_count; index++) {
        if (cache->entries[index].data == NULL) {
            continue;
        }
        if (cache->entries[index].pin_count != 0U) {
            pinned = 1;
        } else {
            evict_entry(cache, index);
        }
    }
    return pinned ? GE_TEXTURE_CACHE_BUSY : GE_TEXTURE_CACHE_OK;
}

void ge_texture_cache_close(GeTextureCache *cache)
{
    size_t index;

    if (cache == NULL) {
        return;
    }
    if (cache->entries != NULL && cache->release != NULL) {
        for (index = 0U; index < cache->entry_count; index++) {
            if (cache->entries[index].data != NULL) {
                cache->release(cache->allocator_context, cache->entries[index].data);
                memset(&cache->entries[index], 0, sizeof(cache->entries[index]));
            }
        }
    }
    memset(cache, 0, sizeof(*cache));
}

void ge_texture_cache_get_stats(const GeTextureCache *cache, GeTextureCacheStats *stats)
{
    size_t index;

    if (stats == NULL) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    if (cache == NULL || cache->entries == NULL) {
        return;
    }
    stats->hits = cache->hits;
    stats->misses = cache->misses;
    stats->evictions = cache->evictions;
    stats->loaded_bytes = cache->loaded_bytes;
    stats->byte_limit = cache->byte_limit;
    stats->slot_count = cache->entry_count;
    for (index = 0U; index < cache->entry_count; index++) {
        if (cache->entries[index].data != NULL) {
            stats->occupied_slots++;
        }
    }
}

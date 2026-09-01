#ifndef GE_TEXTURE_CACHE_H
#define GE_TEXTURE_CACHE_H

#include "ge_texture_catalog.h"

#include <stddef.h>
#include <stdint.h>

typedef enum GeTextureFormat {
    GE_TEXTURE_FORMAT_UNKNOWN = 0,
    GE_TEXTURE_FORMAT_RGBA8,
    GE_TEXTURE_FORMAT_RGB8,
    GE_TEXTURE_FORMAT_RGBA5551,
    GE_TEXTURE_FORMAT_RGB565,
    GE_TEXTURE_FORMAT_RGBA4,
    GE_TEXTURE_FORMAT_LA8,
    GE_TEXTURE_FORMAT_L8,
    GE_TEXTURE_FORMAT_LA4,
    GE_TEXTURE_FORMAT_L4,
    GE_TEXTURE_FORMAT_A8,
    GE_TEXTURE_FORMAT_A4,
    GE_TEXTURE_FORMAT_ETC1,
    GE_TEXTURE_FORMAT_ETC1A4
} GeTextureFormat;

typedef enum GeTextureCacheStatus {
    GE_TEXTURE_CACHE_OK = 0,
    GE_TEXTURE_CACHE_NOT_FOUND = -1,
    GE_TEXTURE_CACHE_INVALID = -2,
    GE_TEXTURE_CACHE_TOO_LARGE = -3,
    GE_TEXTURE_CACHE_NO_MEMORY = -4,
    GE_TEXTURE_CACHE_BUSY = -5,
    GE_TEXTURE_CACHE_PACK_ERROR = -6
} GeTextureCacheStatus;

typedef void *(*GeTextureCacheAllocate)(void *context, size_t size);
typedef void (*GeTextureCacheRelease)(void *context, void *allocation);

typedef struct GeTextureCacheEntry {
    const char *source;
    const char *resource_path;
    void *data;
    size_t data_size;
    uint64_t access_stamp;
    uint32_t lod;
    uint32_t width;
    uint32_t height;
    uint32_t pin_count;
    GeTextureFormat format;
} GeTextureCacheEntry;

typedef struct GeTextureCacheStats {
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    size_t loaded_bytes;
    size_t byte_limit;
    size_t occupied_slots;
    size_t slot_count;
} GeTextureCacheStats;

typedef struct GeTextureCache {
    GeTextureCatalog *catalog;
    GeAssetPack *pack;
    GeTextureCacheEntry *entries;
    size_t entry_count;
    size_t byte_limit;
    size_t loaded_bytes;
    uint64_t access_serial;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    GeTextureFormat format;
    GeTextureCacheAllocate allocate;
    GeTextureCacheRelease release;
    void *allocator_context;
} GeTextureCache;

int ge_texture_cache_init(GeTextureCache *cache, GeTextureCatalog *catalog,
                          GeAssetPack *pack, GeTextureCacheEntry *entries,
                          size_t entry_count, size_t byte_limit,
                          GeTextureFormat format, GeTextureCacheAllocate allocate,
                          GeTextureCacheRelease release, void *allocator_context);

/* Each successful acquire pins the returned entry. Its pointers remain valid
 * until the matching release; callers must not retain them after release. */
int ge_texture_cache_acquire(GeTextureCache *cache, const char *source,
                             uint32_t requested_lod, GeTextureCatalogFallback fallback,
                             const GeTextureCacheEntry **entry);
int ge_texture_cache_acquire_image_id(GeTextureCache *cache, uint16_t image_id,
                                      uint32_t requested_lod,
                                      GeTextureCatalogFallback fallback,
                                      const GeTextureCacheEntry **entry);
int ge_texture_cache_release_entry(GeTextureCache *cache,
                                   const GeTextureCacheEntry *entry);

/* Evicts every unpinned entry. Returns BUSY if pinned entries remain. */
int ge_texture_cache_flush(GeTextureCache *cache);
/* Shutdown-only: frees all entries, including pinned entries. */
void ge_texture_cache_close(GeTextureCache *cache);
void ge_texture_cache_get_stats(const GeTextureCache *cache, GeTextureCacheStats *stats);

#endif

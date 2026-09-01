#ifndef GE_TEXTURE_CATALOG_H
#define GE_TEXTURE_CATALOG_H

#include "ge_asset_pack.h"

#include <stddef.h>
#include <stdint.h>

#define GE_TEXTURE_CATALOG_VERSION 2u

typedef enum GeTextureCatalogStatus {
    GE_TEXTURE_CATALOG_OK = 0,
    GE_TEXTURE_CATALOG_NOT_FOUND = -1,
    GE_TEXTURE_CATALOG_INVALID = -2,
    GE_TEXTURE_CATALOG_BUFFER_TOO_SMALL = -3,
    GE_TEXTURE_CATALOG_PACK_ERROR = -4
} GeTextureCatalogStatus;

typedef enum GeTextureCatalogFallback {
    GE_TEXTURE_CATALOG_EXACT = 0,
    GE_TEXTURE_CATALOG_NEAREST_LOD = 1
} GeTextureCatalogFallback;

typedef struct GeTextureCatalog {
    const uint8_t *data;
    size_t data_size;
    uint32_t entry_count;
    uint64_t entries_offset;
    uint64_t strings_offset;
    uint32_t version;
} GeTextureCatalog;

typedef struct GeTextureAsset {
    const char *source;
    const char *resource_path;
    uint32_t lod;
    uint32_t width;
    uint32_t height;
    uint64_t data_size;
    uint16_t image_id;
    uint8_t image_id_valid;
} GeTextureAsset;

/* The caller retains ownership of data and must keep it alive while catalog is used. */
int ge_texture_catalog_open_memory(GeTextureCatalog *catalog, const void *data, size_t data_size);
void ge_texture_catalog_close(GeTextureCatalog *catalog);

int ge_texture_catalog_find(const GeTextureCatalog *catalog, const char *source,
                            uint32_t requested_lod, GeTextureCatalogFallback fallback,
                            GeTextureAsset *asset);

/* Version-2 catalogs retain the original 12-bit GBI image number even when
 * assets/images.def gives the source a symbolic filename. */
int ge_texture_catalog_find_image_id(const GeTextureCatalog *catalog,
                                     uint16_t image_id,
                                     uint32_t requested_lod,
                                     GeTextureCatalogFallback fallback,
                                     GeTextureAsset *asset);

int ge_texture_catalog_read(GeTextureCatalog const *catalog, GeAssetPack *pack,
                            const char *source, uint32_t requested_lod,
                            GeTextureCatalogFallback fallback, void *destination,
                            size_t destination_size, size_t *bytes_read,
                            GeTextureAsset *selected_asset);

#endif

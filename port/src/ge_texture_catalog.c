#include "ge_texture_catalog.h"

#include <limits.h>
#include <string.h>

#define GE_TEXTURE_CATALOG_HEADER_SIZE UINT32_C(40)
#define GE_TEXTURE_CATALOG_ENTRY_SIZE UINT32_C(48)
#define GE_TEXTURE_CATALOG_MAX_ENTRIES UINT32_C(1000000)
#define GE_TEXTURE_CATALOG_MAX_LOD UINT32_C(31)
#define GE_TEXTURE_RESOURCE_PREFIX "converted/textures/t3x/"

static const uint8_t ge_texture_catalog_magic[8] = {
    'G', 'E', 'T', 'E', 'X', 'C', 'A', 'T'
};

static uint32_t read_u32_le(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *value)
{
    return (uint64_t)read_u32_le(value) | ((uint64_t)read_u32_le(value + 4) << 32);
}

static const uint8_t *entry_at(const GeTextureCatalog *catalog, uint32_t index)
{
    return catalog->data + catalog->entries_offset +
           (uint64_t)index * GE_TEXTURE_CATALOG_ENTRY_SIZE;
}

static const char *entry_string(const GeTextureCatalog *catalog, const uint8_t *entry,
                                size_t offset_field)
{
    return (const char *)(catalog->data + catalog->strings_offset +
                          read_u32_le(entry + offset_field));
}

static int compare_bytes(const char *left, uint32_t left_length,
                         const char *right, uint32_t right_length)
{
    const uint32_t common = left_length < right_length ? left_length : right_length;
    const int result = memcmp(left, right, common);

    if (result != 0) {
        return result;
    }
    return left_length < right_length ? -1 : left_length != right_length;
}

static int validate_string(const GeTextureCatalog *catalog, uint32_t offset, uint32_t length)
{
    const uint64_t strings_size = catalog->data_size - catalog->strings_offset;
    const uint8_t *string;

    if (length == 0U || offset > strings_size || length >= strings_size - offset) {
        return 0;
    }
    string = catalog->data + catalog->strings_offset + offset;
    return string[length] == 0U && memchr(string, 0, length) == NULL;
}

static int validate_resource_path(const char *path, uint32_t length)
{
    static const char prefix[] = GE_TEXTURE_RESOURCE_PREFIX;
    static const char suffix[] = ".t3x";
    uint32_t index;

    if (length <= sizeof(prefix) - 1U + sizeof(suffix) - 1U ||
        memcmp(path, prefix, sizeof(prefix) - 1U) != 0 ||
        memcmp(path + length - (sizeof(suffix) - 1U), suffix, sizeof(suffix) - 1U) != 0) {
        return 0;
    }
    for (index = (uint32_t)(sizeof(prefix) - 1U); index < length; index++) {
        if (path[index] == '\\' ||
            (path[index] == '.' && index + 1U < length && path[index + 1U] == '.' &&
             (index == 0U || path[index - 1U] == '/') &&
             (index + 2U == length || path[index + 2U] == '/'))) {
            return 0;
        }
    }
    return 1;
}

static void decode_asset(const GeTextureCatalog *catalog, const uint8_t *entry,
                         GeTextureAsset *asset)
{
    const uint32_t image_key = read_u32_le(entry + 36U);

    asset->source = entry_string(catalog, entry, 8U);
    asset->resource_path = entry_string(catalog, entry, 16U);
    asset->lod = read_u32_le(entry + 24U);
    asset->width = read_u32_le(entry + 28U);
    asset->height = read_u32_le(entry + 32U);
    asset->data_size = read_u64_le(entry + 40U);
    asset->image_id = image_key != 0U ? (uint16_t)(image_key - 1U) : 0U;
    asset->image_id_valid = image_key != 0U ? UINT8_C(1) : UINT8_C(0);
}

void ge_texture_catalog_close(GeTextureCatalog *catalog)
{
    if (catalog != NULL) {
        memset(catalog, 0, sizeof(*catalog));
    }
}

int ge_texture_catalog_open_memory(GeTextureCatalog *catalog, const void *data, size_t data_size)
{
    const uint8_t *bytes = data;
    uint64_t entries_size;
    uint32_t index;

    if (catalog == NULL || data == NULL) {
        return GE_TEXTURE_CATALOG_INVALID;
    }
    memset(catalog, 0, sizeof(*catalog));
    if (data_size < GE_TEXTURE_CATALOG_HEADER_SIZE ||
        memcmp(bytes, ge_texture_catalog_magic, sizeof(ge_texture_catalog_magic)) != 0 ||
        (read_u32_le(bytes + 8U) != 1U &&
         read_u32_le(bytes + 8U) != GE_TEXTURE_CATALOG_VERSION) ||
        read_u32_le(bytes + 12U) != 0U ||
        read_u32_le(bytes + 20U) != GE_TEXTURE_CATALOG_HEADER_SIZE) {
        return GE_TEXTURE_CATALOG_INVALID;
    }
    catalog->data = bytes;
    catalog->data_size = data_size;
    catalog->entry_count = read_u32_le(bytes + 16U);
    catalog->version = read_u32_le(bytes + 8U);
    catalog->entries_offset = read_u64_le(bytes + 24U);
    catalog->strings_offset = read_u64_le(bytes + 32U);
    entries_size = (uint64_t)catalog->entry_count * GE_TEXTURE_CATALOG_ENTRY_SIZE;
    if (catalog->entry_count > GE_TEXTURE_CATALOG_MAX_ENTRIES ||
        catalog->entries_offset != GE_TEXTURE_CATALOG_HEADER_SIZE ||
        catalog->strings_offset != catalog->entries_offset + entries_size ||
        catalog->strings_offset > data_size) {
        ge_texture_catalog_close(catalog);
        return GE_TEXTURE_CATALOG_INVALID;
    }

    for (index = 0; index < catalog->entry_count; index++) {
        const uint8_t *entry = entry_at(catalog, index);
        const uint32_t source_offset = read_u32_le(entry + 8U);
        const uint32_t source_length = read_u32_le(entry + 12U);
        const uint32_t resource_offset = read_u32_le(entry + 16U);
        const uint32_t resource_length = read_u32_le(entry + 20U);
        const uint32_t lod = read_u32_le(entry + 24U);
        const uint32_t width = read_u32_le(entry + 28U);
        const uint32_t height = read_u32_le(entry + 32U);
        const uint32_t flags = read_u32_le(entry + 36U);
        const uint64_t data_length = read_u64_le(entry + 40U);
        const char *source;
        const char *resource;

        if (!validate_string(catalog, source_offset, source_length) ||
            !validate_string(catalog, resource_offset, resource_length) ||
            lod > GE_TEXTURE_CATALOG_MAX_LOD || width == 0U || height == 0U ||
            (catalog->version == 1U ? flags != 0U : flags > UINT32_C(0x1000)) ||
            data_length == 0U) {
            ge_texture_catalog_close(catalog);
            return GE_TEXTURE_CATALOG_INVALID;
        }
        source = entry_string(catalog, entry, 8U);
        resource = entry_string(catalog, entry, 16U);
        if (ge_asset_path_hash(source) != read_u64_le(entry) ||
            !validate_resource_path(resource, resource_length)) {
            ge_texture_catalog_close(catalog);
            return GE_TEXTURE_CATALOG_INVALID;
        }
        if (index > 0U) {
            const uint8_t *previous = entry_at(catalog, index - 1U);
            const uint64_t previous_hash = read_u64_le(previous);
            const uint64_t current_hash = read_u64_le(entry);
            const int source_order = compare_bytes(
                entry_string(catalog, previous, 8U), read_u32_le(previous + 12U),
                source, source_length);

            if (previous_hash > current_hash ||
                (previous_hash == current_hash && source_order > 0) ||
                (previous_hash == current_hash && source_order == 0 &&
                 read_u32_le(previous + 24U) >= lod)) {
                ge_texture_catalog_close(catalog);
                return GE_TEXTURE_CATALOG_INVALID;
            }
        }
    }
    return GE_TEXTURE_CATALOG_OK;
}

int ge_texture_catalog_find_image_id(const GeTextureCatalog *catalog,
                                     uint16_t image_id,
                                     uint32_t requested_lod,
                                     GeTextureCatalogFallback fallback,
                                     GeTextureAsset *asset)
{
    const uint32_t wanted_key = (uint32_t)image_id + 1U;
    const uint8_t *best = NULL;
    uint32_t best_distance = UINT32_MAX;
    uint32_t index;

    if (catalog == NULL || catalog->data == NULL || asset == NULL ||
        image_id > UINT16_C(0x0fff) ||
        (fallback != GE_TEXTURE_CATALOG_EXACT &&
         fallback != GE_TEXTURE_CATALOG_NEAREST_LOD)) {
        return GE_TEXTURE_CATALOG_INVALID;
    }
    if (catalog->version < 2U) {
        return GE_TEXTURE_CATALOG_NOT_FOUND;
    }
    for (index = 0U; index < catalog->entry_count; ++index) {
        const uint8_t *entry = entry_at(catalog, index);
        const uint32_t lod = read_u32_le(entry + 24U);
        uint32_t distance;

        if (read_u32_le(entry + 36U) != wanted_key) {
            continue;
        }
        if (lod == requested_lod) {
            decode_asset(catalog, entry, asset);
            return GE_TEXTURE_CATALOG_OK;
        }
        if (fallback != GE_TEXTURE_CATALOG_NEAREST_LOD) {
            continue;
        }
        distance = lod > requested_lod ? lod - requested_lod
                                       : requested_lod - lod;
        if (best == NULL || distance < best_distance ||
            (distance == best_distance && lod < read_u32_le(best + 24U))) {
            best = entry;
            best_distance = distance;
        }
    }
    if (best == NULL) {
        return GE_TEXTURE_CATALOG_NOT_FOUND;
    }
    decode_asset(catalog, best, asset);
    return GE_TEXTURE_CATALOG_OK;
}

int ge_texture_catalog_find(const GeTextureCatalog *catalog, const char *source,
                            uint32_t requested_lod, GeTextureCatalogFallback fallback,
                            GeTextureAsset *asset)
{
    uint64_t wanted_hash;
    size_t source_size;
    uint32_t left;
    uint32_t right;
    uint32_t index;
    const uint8_t *best = NULL;
    uint32_t best_distance = UINT32_MAX;

    if (catalog == NULL || catalog->data == NULL || source == NULL || asset == NULL ||
        (fallback != GE_TEXTURE_CATALOG_EXACT &&
         fallback != GE_TEXTURE_CATALOG_NEAREST_LOD)) {
        return GE_TEXTURE_CATALOG_INVALID;
    }
    source_size = strlen(source);
    if (source_size == 0U || source_size > UINT32_MAX) {
        return GE_TEXTURE_CATALOG_NOT_FOUND;
    }
    wanted_hash = ge_asset_path_hash(source);
    left = 0U;
    right = catalog->entry_count;
    while (left < right) {
        const uint32_t middle = left + (right - left) / 2U;
        if (read_u64_le(entry_at(catalog, middle)) < wanted_hash) {
            left = middle + 1U;
        } else {
            right = middle;
        }
    }

    for (index = left; index < catalog->entry_count; index++) {
        const uint8_t *entry = entry_at(catalog, index);
        const uint64_t entry_hash = read_u64_le(entry);
        const uint32_t entry_source_length = read_u32_le(entry + 12U);
        const uint32_t lod = read_u32_le(entry + 24U);
        uint32_t distance;

        if (entry_hash != wanted_hash) {
            break;
        }
        if (entry_source_length != (uint32_t)source_size ||
            memcmp(entry_string(catalog, entry, 8U), source, source_size) != 0) {
            continue;
        }
        if (lod == requested_lod) {
            decode_asset(catalog, entry, asset);
            return GE_TEXTURE_CATALOG_OK;
        }
        if (fallback != GE_TEXTURE_CATALOG_NEAREST_LOD) {
            continue;
        }
        distance = lod > requested_lod ? lod - requested_lod : requested_lod - lod;
        if (best == NULL || distance < best_distance ||
            (distance == best_distance && lod < read_u32_le(best + 24U))) {
            best = entry;
            best_distance = distance;
        }
    }
    if (best == NULL) {
        return GE_TEXTURE_CATALOG_NOT_FOUND;
    }
    decode_asset(catalog, best, asset);
    return GE_TEXTURE_CATALOG_OK;
}

int ge_texture_catalog_read(GeTextureCatalog const *catalog, GeAssetPack *pack,
                            const char *source, uint32_t requested_lod,
                            GeTextureCatalogFallback fallback, void *destination,
                            size_t destination_size, size_t *bytes_read,
                            GeTextureAsset *selected_asset)
{
    GeTextureAsset local_asset;
    int result;

    if (pack == NULL || destination == NULL) {
        return GE_TEXTURE_CATALOG_INVALID;
    }
    result = ge_texture_catalog_find(catalog, source, requested_lod, fallback, &local_asset);
    if (result != GE_TEXTURE_CATALOG_OK) {
        return result;
    }
    if (local_asset.data_size > destination_size) {
        return GE_TEXTURE_CATALOG_BUFFER_TOO_SMALL;
    }
    result = ge_asset_pack_read(pack, local_asset.resource_path, destination,
                                destination_size, bytes_read);
    if (result != GE_ASSET_PACK_OK) {
        return GE_TEXTURE_CATALOG_PACK_ERROR;
    }
    if (selected_asset != NULL) {
        *selected_asset = local_asset;
    }
    return GE_TEXTURE_CATALOG_OK;
}

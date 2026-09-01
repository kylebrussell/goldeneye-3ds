#include "ge_asset_pack.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define GE_ASSET_PACK_HEADER_SIZE 80u
#define GE_ASSET_PACK_ENTRY_SIZE 32u
#define GE_ASSET_PACK_MAX_ENTRIES 1000000u

static const uint8_t ge_asset_pack_magic[8] = {'G', 'E', 'P', 'A', 'C', 'K', 0, 0};

static uint32_t read_u32_le(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16) |
           ((uint32_t)value[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *value)
{
    return (uint64_t)read_u32_le(value) | ((uint64_t)read_u32_le(value + 4) << 32);
}

static int seek_to(FILE *file, uint64_t offset)
{
    if (offset > (uint64_t)LONG_MAX) {
        return GE_ASSET_PACK_INVALID;
    }
    return fseek(file, (long)offset, SEEK_SET) == 0 ? GE_ASSET_PACK_OK : GE_ASSET_PACK_IO_ERROR;
}

uint64_t ge_asset_path_hash(const char *path)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    while (*path != '\0') {
        hash ^= (uint8_t)*path++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void ge_asset_pack_close(GeAssetPack *pack)
{
    if (pack == NULL) {
        return;
    }
    if (pack->file != NULL) {
        fclose(pack->file);
    }
    free(pack->entries);
    free(pack->paths);
    memset(pack, 0, sizeof(*pack));
}

int ge_asset_pack_open(GeAssetPack *pack, const char *filename)
{
    uint8_t header[GE_ASSET_PACK_HEADER_SIZE];
    uint64_t index_offset;
    uint64_t paths_offset;
    uint64_t data_offset;
    uint64_t index_size;
    uint32_t i;
    long end_position;

    if (pack == NULL || filename == NULL) {
        return GE_ASSET_PACK_INVALID;
    }
    memset(pack, 0, sizeof(*pack));
    pack->file = fopen(filename, "rb");
    if (pack->file == NULL) {
        return GE_ASSET_PACK_NOT_FOUND;
    }
    if (fseek(pack->file, 0, SEEK_END) != 0 || (end_position = ftell(pack->file)) < 0 ||
        fseek(pack->file, 0, SEEK_SET) != 0) {
        ge_asset_pack_close(pack);
        return GE_ASSET_PACK_IO_ERROR;
    }
    pack->file_size = (uint64_t)end_position;
    if (fread(header, 1, sizeof(header), pack->file) != sizeof(header)) {
        ge_asset_pack_close(pack);
        return GE_ASSET_PACK_IO_ERROR;
    }

    pack->entry_count = read_u32_le(header + 16);
    index_offset = read_u64_le(header + 24);
    paths_offset = read_u64_le(header + 32);
    data_offset = read_u64_le(header + 40);
    index_size = (uint64_t)pack->entry_count * GE_ASSET_PACK_ENTRY_SIZE;

    if (memcmp(header, ge_asset_pack_magic, sizeof(ge_asset_pack_magic)) != 0 ||
        read_u32_le(header + 8) != GE_ASSET_PACK_VERSION ||
        read_u32_le(header + 20) != GE_ASSET_PACK_HEADER_SIZE ||
        pack->entry_count > GE_ASSET_PACK_MAX_ENTRIES || index_offset != GE_ASSET_PACK_HEADER_SIZE ||
        paths_offset != index_offset + index_size || data_offset < paths_offset ||
        data_offset > pack->file_size || data_offset - paths_offset > SIZE_MAX) {
        ge_asset_pack_close(pack);
        return GE_ASSET_PACK_INVALID;
    }
    memcpy(pack->source_sha1, header + 48, sizeof(pack->source_sha1));
    pack->paths_size = (size_t)(data_offset - paths_offset);
    pack->entries = calloc(pack->entry_count == 0 ? 1u : pack->entry_count,
                           sizeof(*pack->entries));
    pack->paths = malloc(pack->paths_size == 0 ? 1u : pack->paths_size);
    if (pack->entries == NULL || pack->paths == NULL) {
        ge_asset_pack_close(pack);
        return GE_ASSET_PACK_NO_MEMORY;
    }

    if (seek_to(pack->file, index_offset) != GE_ASSET_PACK_OK) {
        ge_asset_pack_close(pack);
        return GE_ASSET_PACK_IO_ERROR;
    }
    for (i = 0; i < pack->entry_count; i++) {
        uint8_t encoded[GE_ASSET_PACK_ENTRY_SIZE];
        GeAssetPackEntry *entry = &pack->entries[i];

        if (fread(encoded, 1, sizeof(encoded), pack->file) != sizeof(encoded)) {
            ge_asset_pack_close(pack);
            return GE_ASSET_PACK_IO_ERROR;
        }
        entry->path_hash = read_u64_le(encoded);
        entry->path_offset = read_u32_le(encoded + 8);
        entry->path_length = read_u32_le(encoded + 12);
        entry->data_offset = read_u64_le(encoded + 16);
        entry->data_size = read_u64_le(encoded + 24);
        if ((uint64_t)entry->path_offset + entry->path_length > pack->paths_size ||
            entry->data_offset < data_offset || entry->data_offset > pack->file_size ||
            entry->data_size > pack->file_size - entry->data_offset ||
            (i > 0 && pack->entries[i - 1].path_hash > entry->path_hash)) {
            ge_asset_pack_close(pack);
            return GE_ASSET_PACK_INVALID;
        }
    }
    if (seek_to(pack->file, paths_offset) != GE_ASSET_PACK_OK ||
        fread(pack->paths, 1, pack->paths_size, pack->file) != pack->paths_size) {
        ge_asset_pack_close(pack);
        return GE_ASSET_PACK_IO_ERROR;
    }
    return GE_ASSET_PACK_OK;
}

const GeAssetPackEntry *ge_asset_pack_find(const GeAssetPack *pack, const char *path)
{
    const uint64_t wanted_hash = ge_asset_path_hash(path);
    const size_t wanted_length = strlen(path);
    uint32_t left = 0;
    uint32_t right = pack->entry_count;
    uint32_t i;

    while (left < right) {
        const uint32_t middle = left + (right - left) / 2;
        if (pack->entries[middle].path_hash < wanted_hash) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    for (i = left; i < pack->entry_count && pack->entries[i].path_hash == wanted_hash; i++) {
        const GeAssetPackEntry *entry = &pack->entries[i];
        if (entry->path_length == wanted_length &&
            memcmp(pack->paths + entry->path_offset, path, wanted_length) == 0) {
            return entry;
        }
    }
    return NULL;
}

int ge_asset_pack_read(GeAssetPack *pack, const char *path, void *destination,
                       size_t destination_size, size_t *bytes_read)
{
    const GeAssetPackEntry *entry;

    if (pack == NULL || pack->file == NULL || path == NULL || destination == NULL) {
        return GE_ASSET_PACK_INVALID;
    }
    entry = ge_asset_pack_find(pack, path);
    if (entry == NULL) {
        return GE_ASSET_PACK_NOT_FOUND;
    }
    if (entry->data_size > destination_size) {
        return GE_ASSET_PACK_BUFFER_TOO_SMALL;
    }
    if (seek_to(pack->file, entry->data_offset) != GE_ASSET_PACK_OK ||
        fread(destination, 1, (size_t)entry->data_size, pack->file) != (size_t)entry->data_size) {
        return GE_ASSET_PACK_IO_ERROR;
    }
    if (bytes_read != NULL) {
        *bytes_read = (size_t)entry->data_size;
    }
    return GE_ASSET_PACK_OK;
}

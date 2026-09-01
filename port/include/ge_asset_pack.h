#ifndef GE_ASSET_PACK_H
#define GE_ASSET_PACK_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define GE_ASSET_PACK_VERSION 1u
#define GE_ASSET_PACK_SOURCE_SHA1_SIZE 20u

typedef struct GeAssetPackEntry {
    uint64_t path_hash;
    uint64_t data_offset;
    uint64_t data_size;
    uint32_t path_offset;
    uint32_t path_length;
} GeAssetPackEntry;

typedef struct GeAssetPack {
    FILE *file;
    GeAssetPackEntry *entries;
    char *paths;
    uint32_t entry_count;
    size_t paths_size;
    uint64_t file_size;
    uint8_t source_sha1[GE_ASSET_PACK_SOURCE_SHA1_SIZE];
} GeAssetPack;

enum {
    GE_ASSET_PACK_OK = 0,
    GE_ASSET_PACK_NOT_FOUND = -1,
    GE_ASSET_PACK_IO_ERROR = -2,
    GE_ASSET_PACK_INVALID = -3,
    GE_ASSET_PACK_NO_MEMORY = -4,
    GE_ASSET_PACK_BUFFER_TOO_SMALL = -5,
};

uint64_t ge_asset_path_hash(const char *path);
int ge_asset_pack_open(GeAssetPack *pack, const char *filename);
void ge_asset_pack_close(GeAssetPack *pack);
const GeAssetPackEntry *ge_asset_pack_find(const GeAssetPack *pack, const char *path);
int ge_asset_pack_read(GeAssetPack *pack, const char *path, void *destination,
                       size_t destination_size, size_t *bytes_read);

#endif

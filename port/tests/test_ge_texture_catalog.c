#include "ge_texture_catalog.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_HEADER_SIZE 40U
#define FIXTURE_ENTRY_SIZE 48U

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static void write_u64_le(uint8_t *destination, uint64_t value)
{
    write_u32_le(destination, (uint32_t)value);
    write_u32_le(destination + 4, (uint32_t)(value >> 32));
}

static size_t build_fixture(uint8_t *data, size_t capacity)
{
    static const char source[] = "COPYICON.bin";
    static const char resource0[] = "converted/textures/t3x/COPYICON-0.t3x";
    static const char resource2[] = "converted/textures/t3x/COPYICON-2.t3x";
    const uint32_t strings_offset = FIXTURE_HEADER_SIZE + 2U * FIXTURE_ENTRY_SIZE;
    const uint32_t source_offset = 0U;
    const uint32_t resource0_offset = (uint32_t)sizeof(source);
    const uint32_t resource2_offset = resource0_offset + (uint32_t)sizeof(resource0);
    const size_t total_size = strings_offset + sizeof(source) + sizeof(resource0) + sizeof(resource2);
    uint8_t *entry0;
    uint8_t *entry2;
    uint64_t source_hash;

    assert(capacity >= total_size);
    memset(data, 0, total_size);
    memcpy(data, "GETEXCAT", 8U);
    write_u32_le(data + 8U, GE_TEXTURE_CATALOG_VERSION);
    write_u32_le(data + 16U, 2U);
    write_u32_le(data + 20U, FIXTURE_HEADER_SIZE);
    write_u64_le(data + 24U, FIXTURE_HEADER_SIZE);
    write_u64_le(data + 32U, strings_offset);

    source_hash = ge_asset_path_hash(source);
    entry0 = data + FIXTURE_HEADER_SIZE;
    write_u64_le(entry0, source_hash);
    write_u32_le(entry0 + 8U, source_offset);
    write_u32_le(entry0 + 12U, (uint32_t)strlen(source));
    write_u32_le(entry0 + 16U, resource0_offset);
    write_u32_le(entry0 + 20U, (uint32_t)strlen(resource0));
    write_u32_le(entry0 + 24U, 0U);
    write_u32_le(entry0 + 28U, 32U);
    write_u32_le(entry0 + 32U, 28U);
    write_u32_le(entry0 + 36U, 1U); /* Image ID 0, biased by one. */
    write_u64_le(entry0 + 40U, 128U);

    entry2 = entry0 + FIXTURE_ENTRY_SIZE;
    memcpy(entry2, entry0, FIXTURE_ENTRY_SIZE);
    write_u32_le(entry2 + 16U, resource2_offset);
    write_u32_le(entry2 + 20U, (uint32_t)strlen(resource2));
    write_u32_le(entry2 + 24U, 2U);
    write_u32_le(entry2 + 28U, 8U);
    write_u32_le(entry2 + 32U, 7U);
    write_u64_le(entry2 + 40U, 48U);

    memcpy(data + strings_offset + source_offset, source, sizeof(source));
    memcpy(data + strings_offset + resource0_offset, resource0, sizeof(resource0));
    memcpy(data + strings_offset + resource2_offset, resource2, sizeof(resource2));
    return total_size;
}

static void test_lookup_and_fallback(void)
{
    uint8_t fixture[512];
    const size_t fixture_size = build_fixture(fixture, sizeof(fixture));
    GeTextureCatalog catalog;
    GeTextureAsset asset;

    assert(ge_texture_catalog_open_memory(&catalog, fixture, fixture_size) ==
           GE_TEXTURE_CATALOG_OK);
    assert(catalog.entry_count == 2U);
    assert(ge_texture_catalog_find(&catalog, "COPYICON.bin", 2U,
                                   GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(asset.lod == 2U && asset.width == 8U && asset.height == 7U);
    assert(asset.data_size == 48U);
    assert(strcmp(asset.resource_path, "converted/textures/t3x/COPYICON-2.t3x") == 0);
    assert(asset.image_id_valid == 1U && asset.image_id == 0U);
    assert(ge_texture_catalog_find_image_id(&catalog, 0U, 2U,
                                            GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(strcmp(asset.source, "COPYICON.bin") == 0 && asset.lod == 2U);
    assert(ge_texture_catalog_find_image_id(&catalog, 1U, 0U,
                                            GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_NOT_FOUND);

    assert(ge_texture_catalog_find(&catalog, "COPYICON.bin", 1U,
                                   GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_NOT_FOUND);
    /* Equal-distance ties prefer the lower numbered (higher resolution) LOD. */
    assert(ge_texture_catalog_find(&catalog, "COPYICON.bin", 1U,
                                   GE_TEXTURE_CATALOG_NEAREST_LOD, &asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(asset.lod == 0U);
    assert(ge_texture_catalog_find(&catalog, "COPYICON.bin", 6U,
                                   GE_TEXTURE_CATALOG_NEAREST_LOD, &asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(asset.lod == 2U);
    assert(ge_texture_catalog_find(&catalog, "missing.bin", 0U,
                                   GE_TEXTURE_CATALOG_NEAREST_LOD, &asset) ==
           GE_TEXTURE_CATALOG_NOT_FOUND);
    ge_texture_catalog_close(&catalog);
    assert(catalog.data == NULL && catalog.entry_count == 0U);
}

static void test_malformed_and_truncated(void)
{
    uint8_t fixture[512];
    uint8_t damaged[512];
    const size_t fixture_size = build_fixture(fixture, sizeof(fixture));
    GeTextureCatalog catalog;

    assert(ge_texture_catalog_open_memory(NULL, fixture, fixture_size) ==
           GE_TEXTURE_CATALOG_INVALID);
    assert(ge_texture_catalog_open_memory(&catalog, NULL, fixture_size) ==
           GE_TEXTURE_CATALOG_INVALID);
    assert(ge_texture_catalog_open_memory(&catalog, fixture, FIXTURE_HEADER_SIZE - 1U) ==
           GE_TEXTURE_CATALOG_INVALID);
    assert(ge_texture_catalog_open_memory(&catalog, fixture, fixture_size - 1U) ==
           GE_TEXTURE_CATALOG_INVALID);

    memcpy(damaged, fixture, fixture_size);
    damaged[0] ^= UINT8_C(0xff);
    assert(ge_texture_catalog_open_memory(&catalog, damaged, fixture_size) ==
           GE_TEXTURE_CATALOG_INVALID);

    memcpy(damaged, fixture, fixture_size);
    write_u32_le(damaged + FIXTURE_HEADER_SIZE + 8U, UINT32_MAX);
    assert(ge_texture_catalog_open_memory(&catalog, damaged, fixture_size) ==
           GE_TEXTURE_CATALOG_INVALID);

    memcpy(damaged, fixture, fixture_size);
    write_u32_le(damaged + FIXTURE_HEADER_SIZE + FIXTURE_ENTRY_SIZE + 24U, 0U);
    assert(ge_texture_catalog_open_memory(&catalog, damaged, fixture_size) ==
           GE_TEXTURE_CATALOG_INVALID);

    memcpy(damaged, fixture, fixture_size);
    damaged[FIXTURE_HEADER_SIZE] ^= 1U;
    assert(ge_texture_catalog_open_memory(&catalog, damaged, fixture_size) ==
           GE_TEXTURE_CATALOG_INVALID);

    memcpy(damaged, fixture, fixture_size);
    write_u32_le(damaged + FIXTURE_HEADER_SIZE + 36U, UINT32_C(0x1001));
    assert(ge_texture_catalog_open_memory(&catalog, damaged, fixture_size) ==
           GE_TEXTURE_CATALOG_INVALID);

    memcpy(damaged, fixture, fixture_size);
    damaged[FIXTURE_HEADER_SIZE + 2U * FIXTURE_ENTRY_SIZE + sizeof("COPYICON.bin")] = 'x';
    assert(ge_texture_catalog_open_memory(&catalog, damaged, fixture_size) ==
           GE_TEXTURE_CATALOG_INVALID);
}

static void test_invalid_lookup_arguments(void)
{
    uint8_t fixture[512];
    const size_t fixture_size = build_fixture(fixture, sizeof(fixture));
    GeTextureCatalog catalog;
    GeTextureAsset asset;
    GeAssetPack unopened_pack = {0};
    uint8_t destination[128];

    assert(ge_texture_catalog_open_memory(&catalog, fixture, fixture_size) ==
           GE_TEXTURE_CATALOG_OK);
    assert(ge_texture_catalog_find(NULL, "COPYICON.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_INVALID);
    assert(ge_texture_catalog_find(&catalog, NULL, 0U,
                                   GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_INVALID);
    assert(ge_texture_catalog_find(&catalog, "COPYICON.bin", 0U,
                                   (GeTextureCatalogFallback)99, &asset) ==
           GE_TEXTURE_CATALOG_INVALID);
    assert(ge_texture_catalog_find_image_id(&catalog, UINT16_C(0x1000), 0U,
                                            GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_INVALID);
    assert(ge_texture_catalog_read(&catalog, NULL, "COPYICON.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, fixture,
                                   sizeof(fixture), NULL, NULL) ==
           GE_TEXTURE_CATALOG_INVALID);
    assert(ge_texture_catalog_read(&catalog, &unopened_pack, "COPYICON.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, destination, 1U,
                                   NULL, NULL) == GE_TEXTURE_CATALOG_BUFFER_TOO_SMALL);
    assert(ge_texture_catalog_read(&catalog, &unopened_pack, "COPYICON.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, destination,
                                   sizeof(destination), NULL, NULL) ==
           GE_TEXTURE_CATALOG_PACK_ERROR);
}

static void test_real_catalog_and_pack(const char *catalog_filename, const char *pack_filename)
{
    FILE *catalog_file = fopen(catalog_filename, "rb");
    GeTextureCatalog catalog;
    GeTextureAsset asset;
    GeAssetPack pack;
    uint8_t *catalog_data;
    uint8_t *texture_data;
    long catalog_size;
    size_t bytes_read = 0U;

    assert(catalog_file != NULL);
    assert(fseek(catalog_file, 0, SEEK_END) == 0);
    catalog_size = ftell(catalog_file);
    assert(catalog_size > 0 && fseek(catalog_file, 0, SEEK_SET) == 0);
    catalog_data = malloc((size_t)catalog_size);
    assert(catalog_data != NULL);
    assert(fread(catalog_data, 1U, (size_t)catalog_size, catalog_file) == (size_t)catalog_size);
    fclose(catalog_file);

    assert(ge_texture_catalog_open_memory(&catalog, catalog_data, (size_t)catalog_size) ==
           GE_TEXTURE_CATALOG_OK);
    assert(ge_texture_catalog_find(&catalog, "COPYICON.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(strcmp(asset.resource_path, "converted/textures/t3x/COPYICON-0.t3x") == 0);
    texture_data = malloc((size_t)asset.data_size);
    assert(texture_data != NULL);
    assert(ge_asset_pack_open(&pack, pack_filename) == GE_ASSET_PACK_OK);
    assert(ge_texture_catalog_read(&catalog, &pack, "COPYICON.bin", 0U,
                                   GE_TEXTURE_CATALOG_EXACT, texture_data,
                                   (size_t)asset.data_size, &bytes_read, &asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(bytes_read == asset.data_size);
    assert(ge_texture_catalog_find_image_id(&catalog, UINT16_C(22), 0U,
                                            GE_TEXTURE_CATALOG_EXACT, &asset) ==
           GE_TEXTURE_CATALOG_OK);
    assert(strcmp(asset.source, "TARDETAIL.bin") == 0);
    assert(asset.image_id_valid == 1U && asset.image_id == UINT16_C(22));

    ge_asset_pack_close(&pack);
    ge_texture_catalog_close(&catalog);
    free(texture_data);
    free(catalog_data);
}

int main(int argc, char **argv)
{
    test_lookup_and_fallback();
    test_malformed_and_truncated();
    test_invalid_lookup_arguments();
    if (argc == 3) {
        test_real_catalog_and_pack(argv[1], argv[2]);
    } else {
        assert(argc == 1);
    }
    puts("portable texture catalog parser and lookup tests passed");
    return 0;
}

#include "ge_asset_pack.h"
#include "ge_texture_catalog.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char resource[] = "converted/textures/t3x/sample-0.t3x";
static const char source[] = "sample.bin";
static int allocation_attempt;
static int failed_allocation;

/* Only the pack parser translation unit substitutes these allocators. */
void *test_asset_malloc(size_t size)
{
    return ++allocation_attempt == failed_allocation ? NULL : malloc(size);
}

void *test_asset_calloc(size_t count, size_t size)
{
    return ++allocation_attempt == failed_allocation ? NULL : calloc(count, size);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put64(uint8_t *p, uint64_t v)
{
    put32(p, (uint32_t)v);
    put32(p + 4, (uint32_t)(v >> 32));
}

static size_t pack_fixture(uint8_t *data, unsigned copies)
{
    const size_t paths_offset = 80U + copies * 32U;
    const size_t data_offset = (paths_offset + sizeof(resource) - 1U + 15U) & ~(size_t)15U;
    unsigned i;

    assert(copies <= 2U && data_offset + 16U <= 512U);
    memset(data, 0, 512U);
    memcpy(data, "GEPACK\0\0", 8U);
    put32(data + 8U, 1U);
    put32(data + 16U, copies);
    put32(data + 20U, 80U);
    put64(data + 24U, 80U);
    put64(data + 32U, paths_offset);
    put64(data + 40U, data_offset);
    for (i = 0; i < copies; i++) {
        uint8_t *entry = data + 80U + i * 32U;
        put64(entry, ge_asset_path_hash(resource));
        put32(entry + 12U, (uint32_t)strlen(resource));
        put64(entry + 16U, data_offset);
        put64(entry + 24U, 16U);
    }
    memcpy(data + paths_offset, resource, sizeof(resource) - 1U);
    for (i = 0; i < 16U; i++) {
        data[data_offset + i] = (uint8_t)i;
    }
    return data_offset + 16U;
}

static size_t catalog_fixture(uint8_t *data)
{
    memset(data, 0, 512U);
    memcpy(data, "GETEXCAT", 8U);
    put32(data + 8U, 2U);
    put32(data + 16U, 1U);
    put32(data + 20U, 40U);
    put64(data + 24U, 40U);
    put64(data + 32U, 88U);
    put64(data + 40U, ge_asset_path_hash(source));
    put32(data + 52U, (uint32_t)strlen(source));
    put32(data + 56U, (uint32_t)sizeof(source));
    put32(data + 60U, (uint32_t)strlen(resource));
    put32(data + 68U, 8U);
    put32(data + 72U, 8U);
    put32(data + 76U, 1U);
    put64(data + 80U, 16U);
    memcpy(data + 88U, source, sizeof(source));
    memcpy(data + 88U + sizeof(source), resource, sizeof(resource));
    return 88U + sizeof(source) + sizeof(resource);
}

static void write_fixture(const char *filename, const uint8_t *data, size_t size)
{
    FILE *file = fopen(filename, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1U, size, file) == size);
    assert(fclose(file) == 0);
}

static void assert_closed(const GeAssetPack *pack)
{
    assert(pack->file == NULL && pack->entries == NULL && pack->paths == NULL);
    assert(pack->entry_count == 0U && pack->file_size == 0U && pack->paths_size == 0U);
    assert(ge_asset_pack_find(pack, resource) == NULL);
}

static void assert_bad_pack(const char *filename, const uint8_t *data, size_t size)
{
    GeAssetPack pack;
    int result;
    write_fixture(filename, data, size);
    result = ge_asset_pack_open(&pack, filename);
    assert(result == GE_ASSET_PACK_INVALID || result == GE_ASSET_PACK_IO_ERROR);
    assert_closed(&pack);
    ge_asset_pack_close(&pack);
}

static void test_pack_validation(const char *filename)
{
    static const size_t fields32[] = {8U, 12U, 16U, 20U, 88U, 92U};
    static const size_t fields64[] = {24U, 32U, 40U, 80U, 96U, 104U};
    uint8_t fixture[512], damaged[512], output[16];
    size_t size = pack_fixture(fixture, 1U), i, bytes_read;
    GeAssetPack pack;

    for (i = 0; i < size; i++) {
        assert_bad_pack(filename, fixture, i);
    }
    for (i = 0; i < sizeof(fields32) / sizeof(fields32[0]); i++) {
        memcpy(damaged, fixture, size);
        put32(damaged + fields32[i], UINT32_MAX);
        assert_bad_pack(filename, damaged, size);
    }
    for (i = 0; i < sizeof(fields64) / sizeof(fields64[0]); i++) {
        memcpy(damaged, fixture, size);
        put64(damaged + fields64[i], UINT64_MAX);
        assert_bad_pack(filename, damaged, size);
    }
    memcpy(damaged, fixture, size);
    put32(damaged + 92U, 0U);
    assert_bad_pack(filename, damaged, size);
    memcpy(damaged, fixture, size);
    damaged[112U] = 0U;
    assert_bad_pack(filename, damaged, size);
    size = pack_fixture(damaged, 2U);
    assert_bad_pack(filename, damaged, size);

    size = pack_fixture(fixture, 1U);
    write_fixture(filename, fixture, size);
    for (failed_allocation = 1; failed_allocation <= 2; failed_allocation++) {
        allocation_attempt = 0;
        assert(ge_asset_pack_open(&pack, filename) == GE_ASSET_PACK_NO_MEMORY);
        assert_closed(&pack);
    }
    failed_allocation = 0;
    assert(ge_asset_pack_open(&pack, filename) == GE_ASSET_PACK_OK);
    assert(ge_asset_pack_find(NULL, resource) == NULL);
    assert(ge_asset_pack_find(&pack, NULL) == NULL);
    assert(ge_asset_pack_find(&pack, "") == NULL);
    bytes_read = 99U;
    assert(ge_asset_pack_read(NULL, resource, output, sizeof(output), &bytes_read)
           == GE_ASSET_PACK_INVALID && bytes_read == 0U);
    bytes_read = 99U;
    assert(ge_asset_pack_read(&pack, "missing", output, sizeof(output), &bytes_read)
           == GE_ASSET_PACK_NOT_FOUND && bytes_read == 0U);
    bytes_read = 99U;
    assert(ge_asset_pack_read(&pack, resource, output, 1U, &bytes_read)
           == GE_ASSET_PACK_BUFFER_TOO_SMALL && bytes_read == 0U);
    assert(ge_asset_pack_read(&pack, resource, output, sizeof(output), &bytes_read)
           == GE_ASSET_PACK_OK && bytes_read == 16U);
    for (i = 0; i < bytes_read; i++) {
        assert(output[i] == i);
    }
    ge_asset_pack_close(&pack);
    ge_asset_pack_close(&pack);
    assert_closed(&pack);

    /* Empty files remain valid pack members. */
    put64(fixture + 104U, 0U);
    write_fixture(filename, fixture, size);
    assert(ge_asset_pack_open(&pack, filename) == GE_ASSET_PACK_OK);
    assert(ge_asset_pack_read(&pack, resource, output, 0U, &bytes_read)
           == GE_ASSET_PACK_OK && bytes_read == 0U);
    ge_asset_pack_close(&pack);

    /* An empty pack has no path or payload sections to read. */
    put32(fixture + 16U, 0U);
    put64(fixture + 32U, 80U);
    put64(fixture + 40U, 80U);
    write_fixture(filename, fixture, 80U);
    assert(ge_asset_pack_open(&pack, filename) == GE_ASSET_PACK_OK);
    assert(ge_asset_pack_find(&pack, resource) == NULL);
    ge_asset_pack_close(&pack);
}

static void test_catalog_validation(void)
{
    static const size_t fields32[] = {8U, 12U, 16U, 20U, 48U, 52U, 56U, 60U, 64U, 76U};
    static const size_t fields64[] = {24U, 32U, 40U};
    uint8_t fixture[512], damaged[512];
    const size_t size = catalog_fixture(fixture);
    GeTextureCatalog catalog;
    GeTextureAsset asset;
    size_t i;

    for (i = 0; i < size; i++) {
        uint8_t *short_data = malloc(i == 0U ? 1U : i);
        assert(short_data != NULL);
        memcpy(short_data, fixture, i);
        assert(ge_texture_catalog_open_memory(&catalog, short_data, i)
               == GE_TEXTURE_CATALOG_INVALID);
        assert(catalog.data == NULL && catalog.entry_count == 0U);
        free(short_data);
    }
    for (i = 0; i < sizeof(fields32) / sizeof(fields32[0]); i++) {
        memcpy(damaged, fixture, size);
        put32(damaged + fields32[i], UINT32_MAX);
        assert(ge_texture_catalog_open_memory(&catalog, damaged, size)
               == GE_TEXTURE_CATALOG_INVALID);
        assert(catalog.data == NULL);
    }
    for (i = 0; i < sizeof(fields64) / sizeof(fields64[0]); i++) {
        memcpy(damaged, fixture, size);
        put64(damaged + fields64[i], UINT64_MAX);
        assert(ge_texture_catalog_open_memory(&catalog, damaged, size)
               == GE_TEXTURE_CATALOG_INVALID);
    }
    assert(ge_texture_catalog_open_memory(&catalog, fixture, size) == GE_TEXTURE_CATALOG_OK);
    assert(ge_texture_catalog_find(&catalog, source, UINT32_MAX,
                                  GE_TEXTURE_CATALOG_NEAREST_LOD, &asset) == GE_TEXTURE_CATALOG_OK);
    assert(asset.lod == 0U);
    assert(ge_texture_catalog_open_memory(&catalog, NULL, size) == GE_TEXTURE_CATALOG_INVALID);
    assert(catalog.data == NULL);
    assert(ge_texture_catalog_find(&catalog, source, 0U, GE_TEXTURE_CATALOG_EXACT, &asset)
           == GE_TEXTURE_CATALOG_INVALID);
}

static void test_catalog_pack_size(const char *filename)
{
    uint8_t pack_data[512], catalog_data[512], output[32];
    const size_t pack_size = pack_fixture(pack_data, 1U);
    const size_t catalog_size = catalog_fixture(catalog_data);
    GeAssetPack pack;
    GeTextureCatalog catalog;
    GeTextureAsset selected, unchanged;
    size_t bytes_read, i;
    static const uint64_t sizes[] = {1U, 15U, 17U, 32U};

    write_fixture(filename, pack_data, pack_size);
    assert(ge_asset_pack_open(&pack, filename) == GE_ASSET_PACK_OK);
    memset(&unchanged, 0, sizeof(unchanged));
    unchanged.width = 99U;
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        put64(catalog_data + 80U, sizes[i]);
        assert(ge_texture_catalog_open_memory(&catalog, catalog_data, catalog_size)
               == GE_TEXTURE_CATALOG_OK);
        memset(output, 0x5a, sizeof(output));
        selected = unchanged;
        bytes_read = 99U;
        assert(ge_texture_catalog_read(&catalog, &pack, source, 0U,
            GE_TEXTURE_CATALOG_EXACT, output, sizeof(output), &bytes_read, &selected)
            == GE_TEXTURE_CATALOG_PACK_ERROR);
        assert(bytes_read == 0U && selected.width == unchanged.width);
        for (size_t j = 0; j < sizeof(output); j++) {
            assert(output[j] == 0x5a);
        }
        ge_texture_catalog_close(&catalog);
    }
    put64(catalog_data + 80U, 16U);
    assert(ge_texture_catalog_open_memory(&catalog, catalog_data, catalog_size)
           == GE_TEXTURE_CATALOG_OK);
    assert(ge_texture_catalog_read(&catalog, &pack, source, 0U,
        GE_TEXTURE_CATALOG_EXACT, output, sizeof(output), &bytes_read, &selected)
        == GE_TEXTURE_CATALOG_OK);
    assert(bytes_read == 16U && selected.data_size == 16U);
    ge_texture_catalog_close(&catalog);
    ge_asset_pack_close(&pack);
}

static uint32_t next_random(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static void test_seeded_mutations(const char *filename)
{
    uint8_t good_pack[512], good_catalog[512];
    const size_t pack_size = pack_fixture(good_pack, 1U);
    const size_t catalog_size = catalog_fixture(good_catalog);
    uint32_t state = UINT32_C(0x7061636b);
    unsigned iteration;

    for (iteration = 0; iteration < 8192U; iteration++) {
        const int is_pack = iteration < 2048U;
        const size_t size = is_pack ? pack_size : catalog_size;
        uint8_t *data = malloc(size);
        unsigned mutations = 1U + next_random(&state) % 8U;
        assert(data != NULL);
        memcpy(data, is_pack ? good_pack : good_catalog, size);
        while (mutations-- != 0U) {
            const size_t offset = next_random(&state) % size;
            data[offset] ^= (uint8_t)next_random(&state);
        }
        if (is_pack) {
            GeAssetPack pack;
            int result;
            write_fixture(filename, data, size);
            result = ge_asset_pack_open(&pack, filename);
            if (result == GE_ASSET_PACK_OK) {
                uint32_t i;
                for (i = 0; i < pack.entry_count; i++) {
                    const GeAssetPackEntry *entry = &pack.entries[i];
                    char name[513];
                    uint8_t output[512];
                    size_t bytes_read;
                    assert(entry->path_length < sizeof(name));
                    memcpy(name, pack.paths + entry->path_offset, entry->path_length);
                    name[entry->path_length] = '\0';
                    assert(ge_asset_pack_find(&pack, name) == entry);
                    assert(ge_asset_pack_read(&pack, name, output, sizeof(output), &bytes_read)
                           == GE_ASSET_PACK_OK);
                    assert(bytes_read == entry->data_size);
                }
                ge_asset_pack_close(&pack);
            } else {
                assert_closed(&pack);
            }
        } else {
            GeTextureCatalog catalog;
            if (ge_texture_catalog_open_memory(&catalog, data, size) == GE_TEXTURE_CATALOG_OK) {
                GeTextureAsset asset;
                (void)ge_texture_catalog_find(&catalog, source, UINT32_MAX,
                    GE_TEXTURE_CATALOG_NEAREST_LOD, &asset);
                (void)ge_texture_catalog_find_image_id(&catalog, 0U, UINT32_MAX,
                    GE_TEXTURE_CATALOG_NEAREST_LOD, &asset);
                ge_texture_catalog_close(&catalog);
            } else {
                assert(catalog.data == NULL);
            }
        }
        free(data);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    test_pack_validation(argv[1]);
    test_catalog_validation();
    test_catalog_pack_size(argv[1]);
    test_seeded_mutations(argv[1]);
    assert(remove(argv[1]) == 0);
    puts("asset malformed-data tests passed: truncations, bounds, allocation failures, size mismatch, 8192 mutations");
    return 0;
}

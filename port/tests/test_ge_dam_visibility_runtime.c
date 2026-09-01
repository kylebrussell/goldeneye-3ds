#include "ge_dam_visibility_runtime.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

static FileData read_file(const char *path)
{
    FILE *stream = fopen(path, "rb");
    FileData result = {NULL, 0U};
    long length;

    assert(stream != NULL);
    assert(fseek(stream, 0L, SEEK_END) == 0);
    length = ftell(stream);
    assert(length >= 0L);
    assert(fseek(stream, 0L, SEEK_SET) == 0);
    result.size = (size_t)length;
    result.bytes = malloc(result.size == 0U ? 1U : result.size);
    assert(result.bytes != NULL);
    assert(fread(result.bytes, 1U, result.size, stream) == result.size);
    assert(fclose(stream) == 0);
    return result;
}

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
        | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

int main(int argc, char **argv)
{
    static const uint8_t expected[] = {
        135U, 133U, 134U, 132U, 124U, 125U,
    };
    FileData background;
    FileData bounds;
    FileData view;
    GeDamVisibilityRuntime runtime;
    GeOriginalBondCameraResult camera = {0};
    GeOriginalBgVisibilityResult result;
    const float position[3] = {
        4719.0f / 0.23363999f,
        -18.0f / 0.23363999f,
        3949.0f / 0.23363999f,
    };
    uint8_t *changed;
    uint8_t portal_controls[194];
    GeOriginalBgVisibilityProviders providers = {0};
    size_t index;

    assert(argc == 4 || argc == 5);
    background = read_file(argv[1]);
    bounds = read_file(argv[2]);
    view = read_file(argv[3]);
    assert(view.size == sizeof(camera.view));
    assert(ge_dam_visibility_runtime_init(background.bytes, background.size,
        bounds.bytes, bounds.size, &runtime) == GE_DAM_VISIBILITY_RUNTIME_OK);
    assert(runtime.loaded != 0U && runtime.room_count == 137U);
    assert(runtime.source_fnv1a64 == UINT64_C(0x548dab6894be896c));
    assert(runtime.payload_fnv1a64 == UINT64_C(0x905af0d1245561d6));
    memcpy(camera.view, view.bytes, sizeof(camera.view));
    camera.viewport_scale[0] = 640;
    camera.viewport_scale[1] = 480;
    camera.room = 135U;
    assert(ge_dam_visibility_runtime_run(
        &runtime, &camera, position, &result)
        == GE_DAM_VISIBILITY_RUNTIME_OK);
    assert(result.portal_count == 194U);
    assert(result.global_visibility_used != 0U);
    assert(result.global_command_count == 389U);
    assert(result.global_stream_fnv1a64
        == UINT64_C(0x39a4179260579f89));
    assert(result.room_count == sizeof(expected));
    for (index = 0U; index < sizeof(expected); ++index) {
        assert(result.rooms[index].room == expected[index]);
    }
    for (index = 0U; index < sizeof(portal_controls); ++index) {
        const size_t portal_table = read_be32(background.bytes + 8U)
            - UINT32_C(0x0f000000);
        portal_controls[index] = background.bytes[
            portal_table + index * 8U + 6U];
    }
    providers.portal_controls = portal_controls;
    providers.portal_control_count = sizeof(portal_controls);
    assert(ge_dam_visibility_runtime_run_with_providers(
        &runtime, &camera, position, &providers, &result)
        == GE_DAM_VISIBILITY_RUNTIME_OK);
    assert(result.room_count == sizeof(expected));
    ge_dam_visibility_runtime_close(&runtime);
    assert(runtime.loaded == 0U && runtime.background == NULL);

    if (argc == 5) {
        GeAssetPack pack;

        assert(ge_asset_pack_open(&pack, argv[4]) == GE_ASSET_PACK_OK);
        assert(ge_dam_visibility_runtime_load(&pack, &runtime)
            == GE_DAM_VISIBILITY_RUNTIME_OK);
        assert(ge_dam_visibility_runtime_run(
            &runtime, &camera, position, &result)
            == GE_DAM_VISIBILITY_RUNTIME_OK);
        assert(result.room_count == sizeof(expected));
        for (index = 0U; index < sizeof(expected); ++index) {
            assert(result.rooms[index].room == expected[index]);
        }
        ge_dam_visibility_runtime_close(&runtime);
        ge_asset_pack_close(&pack);
    }

    changed = malloc(bounds.size);
    assert(changed != NULL);
    memcpy(changed, bounds.bytes, bounds.size);
    changed[bounds.size - 1U] ^= UINT8_C(1);
    assert(ge_dam_visibility_runtime_init(background.bytes, background.size,
        changed, bounds.size, &runtime)
        == GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET);
    memcpy(changed, bounds.bytes, bounds.size);
    changed[16] = 140U;
    assert(ge_dam_visibility_runtime_init(background.bytes, background.size,
        changed, bounds.size, &runtime)
        == GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET);
    background.bytes[background.size - 1U] ^= UINT8_C(1);
    assert(ge_dam_visibility_runtime_init(background.bytes, background.size,
        bounds.bytes, bounds.size, &runtime)
        == GE_DAM_VISIBILITY_RUNTIME_INVALID_ASSET);
    free(changed);
    free(view.bytes);
    free(bounds.bytes);
    free(background.bytes);
    puts("Dam packaged bounds -> exact live camera visibility runtime passed");
    return 0;
}

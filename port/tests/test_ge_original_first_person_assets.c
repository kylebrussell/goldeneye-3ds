#include "ge_original_first_person_assets.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct texpool;
typedef struct ModelFileHeader ModelFileHeader;
void load_object_fill_header(ModelFileHeader *objheader, uint8_t *name,
                             uint8_t *dst, int32_t size,
                             struct texpool *buffer);

static uint8_t right_buffer[GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE];
static uint8_t left_buffer[GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE];

enum {
    TEST_MODELNODE_OPCODE_HEADER = 1,
    TEST_MODELNODE_OPCODE_GROUP = 2,
};

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalFirstPersonAssets assets;
    size_t size = 0U;
    uint8_t source_sha1[GE_ASSET_PACK_SOURCE_SHA1_SIZE];
    void *native_header = NULL;
    GeOriginalFirstPersonLoaderState loader;
    union {
        max_align_t alignment;
        uint8_t bytes[128];
    } bug_header;

    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    memcpy(source_sha1, pack.source_sha1, sizeof(source_sha1));
    pack.source_sha1[0] ^= 1U;
    assert(ge_original_first_person_assets_init(
        &assets, &pack, right_buffer, sizeof(right_buffer),
        left_buffer, sizeof(left_buffer))
        == GE_ORIGINAL_FIRST_PERSON_ASSET_WRONG_ROM);
    memcpy(pack.source_sha1, source_sha1, sizeof(source_sha1));
    assert(ge_original_first_person_assets_init(
        &assets, &pack, right_buffer,
        GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE - 1U,
        left_buffer, sizeof(left_buffer))
        == GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_SIZE);
    assert(ge_original_first_person_assets_init(
        &assets, &pack, right_buffer, sizeof(right_buffer),
        left_buffer, sizeof(left_buffer)) == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(assets.loaded_model[0] == GE_ORIGINAL_FIRST_PERSON_MODEL_NONE);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_PP7, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 18512U);
    assert(assets.loaded_size[0] == size);
    assert(assets.loaded_model[0] == GE_ORIGINAL_FIRST_PERSON_MODEL_PP7);
    assert(right_buffer[0] == 0x05U && right_buffer[3] == 0xb0U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(native_header != NULL);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 48U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_PP7_SILENCED, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 19536U);
    assert(assets.loaded_model[1]
           == GE_ORIGINAL_FIRST_PERSON_MODEL_PP7_SILENCED);
    assert(left_buffer[0x120U] == 0U && left_buffer[0x121U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 49U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_BUG, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 6848U);
    assert(assets.loaded_model[0] == GE_ORIGINAL_FIRST_PERSON_MODEL_BUG);
    assert(right_buffer[0xb8U] == 0U && right_buffer[0xb9U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 2U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_SNIPER_RIFLE, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 13040U && right_buffer[0xe4U] == 0U
           && right_buffer[0xe5U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 26U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_TRIGGER, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 35936U && left_buffer[0x198U] == 0U
           && left_buffer[0x199U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 19U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_FIST, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 15072U && right_buffer[0x138U] == 0U
           && right_buffer[0x139U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 11U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K_SILENCED, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 10336U && right_buffer[0xfcU] == 0U
           && right_buffer[0xfdU] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 27U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_PLASTIQUE, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 2336U && left_buffer[0x94U] == 0U
           && left_buffer[0x95U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 6U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_CAMERA, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 3232U && right_buffer[0xf4U] == 0U
           && right_buffer[0xf5U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 2U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_MAGNET_ATTRACT,
        &size) == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 15312U && left_buffer[0xe0U] == 0U
           && left_buffer[0xe1U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 2U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_UZI, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 7504U && right_buffer[0x120U] == 0U
           && right_buffer[0x121U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 22U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_LASER, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 35936U && left_buffer[0x198U] == 0U
           && left_buffer[0x199U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 19U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE_LAUNCHER,
        &size) == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 13520U && right_buffer[0x144U] == 0U
           && right_buffer[0x145U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 36U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 7840U && left_buffer[0xccU] == 0U
           && left_buffer[0xcdU] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 2U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_TIMED_MINE, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 7136U && right_buffer[0xa0U] == 0U
           && right_buffer[0xa1U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 2U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_BOMB_CASE, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 6272U && left_buffer[0xb8U] == 0U
           && left_buffer[0xb9U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 2U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_MICROCAMERA, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 4016U && right_buffer[0xe0U] == 0U
           && right_buffer[0xe1U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 2U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDENEYE_KEY, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 7648U && left_buffer[0xacU] == 0U
           && left_buffer[0xadU] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 2U);
    {
        static const struct {
            GeOriginalFirstPersonModel model;
            size_t blob_size;
            size_t root_offset;
            size_t node_count;
        } reachable_tier[] = {
            {GE_ORIGINAL_FIRST_PERSON_MODEL_FNP90, 9552U, 0x108U, 30U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_RUGER, 19376U, 0x138U, 40U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_SPECTRE, 9824U, 0x114U, 28U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_M16, 7472U, 0xf0U, 30U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_SHOTGUN, 12112U, 0x10cU, 39U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_AUTOSHOT, 20016U, 0x150U, 63U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K, 9504U, 0xfcU, 25U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_TT33, 17584U, 0x144U, 47U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_SKORPION, 13536U, 0x120U, 42U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_KNIFE, 18416U, 0xfcU, 30U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_THROW_KNIFE,
             18240U, 0xfcU, 32U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDEN_GUN,
             16320U, 0x114U, 32U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_SILVER_PP7,
             16480U, 0x114U, 45U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_GOLD_PP7,
             16480U, 0x114U, 45U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_LASER, 10512U, 0x12cU, 25U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_ROCKET_LAUNCHER,
             12704U, 0x108U, 35U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_PROXIMITY_MINE,
             5360U, 0x94U, 2U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_TASER, 19984U, 0x158U, 34U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_FLARE_PISTOL,
             1952U, 0x88U, 2U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_PITON_GUN,
             1952U, 0x88U, 2U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_SUIT_LEFT_HAND,
             38688U, 0x130U, 28U},
            {GE_ORIGINAL_FIRST_PERSON_MODEL_JOYPAD, 21008U, 0x68U, 27U},
        };
        size_t index;
        for (index = 0U;
             index < sizeof(reachable_tier) / sizeof(reachable_tier[0]);
             ++index) {
            const unsigned slot = (unsigned)(index & 1U);
            uint8_t *buffer = slot == 0U ? right_buffer : left_buffer;
            assert(ge_original_first_person_assets_load_raw(
                &assets, slot, reachable_tier[index].model, &size)
                == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
            assert(size == reachable_tier[index].blob_size);
            assert(buffer[reachable_tier[index].root_offset] == 0U);
            assert(buffer[reachable_tier[index].root_offset + 1U]
                   == (reachable_tier[index].model
                           == GE_ORIGINAL_FIRST_PERSON_MODEL_SUIT_LEFT_HAND
                       ? TEST_MODELNODE_OPCODE_HEADER
                       : TEST_MODELNODE_OPCODE_GROUP));
            assert(ge_original_first_person_assets_relocate_native(
                &assets, slot, NULL, &native_header)
                == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
            assert(ge_original_first_person_assets_native_node_count(
                &assets, slot) == reachable_tier[index].node_count);
        }
    }
    assert(ge_original_first_person_assets_load_raw(
        &assets, 1U, GE_ORIGINAL_FIRST_PERSON_MODEL_AK47, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 8240U);
    assert(assets.loaded_model[1] == GE_ORIGINAL_FIRST_PERSON_MODEL_AK47);
    assert(left_buffer[0x168U] == 0U && left_buffer[0x169U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 1U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U)
           == 28U);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_REMOTE_MINE, &size)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(size == 6512U);
    assert(assets.loaded_model[0]
           == GE_ORIGINAL_FIRST_PERSON_MODEL_REMOTE_MINE);
    assert(right_buffer[0xb0U] == 0U && right_buffer[0xb1U] == 2U);
    assert(ge_original_first_person_assets_relocate_native(
        &assets, 0U, NULL, &native_header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 2U);
    memset(&bug_header, 0, sizeof(bug_header));
    ge_original_first_person_assets_bind_loader(&assets, &loader);
    load_object_fill_header((ModelFileHeader *)bug_header.bytes,
                            (uint8_t *)"GbugZ", right_buffer,
                            0xbd70, NULL);
    assert(loader.load_calls == 1U);
    assert(loader.successful_loads == 1U);
    assert(loader.last_status == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U)
           == 2U);
    ge_original_first_person_assets_bind_loader(NULL, NULL);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 2U, GE_ORIGINAL_FIRST_PERSON_MODEL_PP7, NULL)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT);
    assert(ge_original_first_person_assets_load_raw(
        &assets, 0U, GE_ORIGINAL_FIRST_PERSON_MODEL_NONE, NULL)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT);

    ge_original_first_person_assets_close(&assets);
    ge_asset_pack_close(&pack);
    puts("original authored intro first-person packed resources: ok");
    return 0;
}

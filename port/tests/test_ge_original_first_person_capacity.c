#include "ge_original_first_person_assets.h"
#include "ge_original_first_person_scene.h"
#include "ge_original_model_scene.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondtypes.h>

enum {
    LIVE_DISPLAY_CAPACITY =
        GE_ORIGINAL_FIRST_PERSON_SUPPORTED_DISPLAY_LIST_CAPACITY,
    LIVE_VERTEX_CAPACITY =
        GE_ORIGINAL_FIRST_PERSON_SUPPORTED_VERTEX_CAPACITY,
    LIVE_BATCH_CAPACITY =
        GE_ORIGINAL_FIRST_PERSON_SUPPORTED_BATCH_CAPACITY,
    LIVE_TEXTURE_CAPACITY =
        GE_ORIGINAL_FIRST_PERSON_SUPPORTED_TEXTURE_CAPACITY,
};

static size_t measured_max_displays;
static size_t measured_max_vertices;
static size_t measured_max_batches;
static size_t measured_max_textures;

static uint8_t right_buffer[GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE];
static uint8_t left_buffer[GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE];

typedef struct TextureVisitCheck {
    const ModelFileHeader *header;
    size_t count;
    int reject;
} TextureVisitCheck;

static int check_texture_id(void *context, uint16_t image_id)
{
    TextureVisitCheck *check = context;
    assert(check->count < (size_t)check->header->numtextures);
    assert(image_id == check->header->Textures[check->count].TextureID);
    ++check->count;
    return !check->reject;
}

static uint32_t blob_offset(const uint8_t *blob, size_t size,
                            const void *pointer)
{
    const uintptr_t base = (uintptr_t)blob;
    const uintptr_t address = (uintptr_t)pointer;
    assert(pointer != NULL && address >= base && address - base < size);
    assert(address - base <= UINT32_MAX);
    return (uint32_t)(address - base);
}

static void identity(float matrix[4][4])
{
    size_t index;
    memset(matrix, 0, sizeof(float) * 16U);
    for (index = 0U; index < 4U; ++index) matrix[index][index] = 1.0f;
}

static void measure(GeOriginalFirstPersonAssets *assets,
                    GeOriginalFirstPersonModel model_id, const char *name)
{
    float matrices[16][4][4];
    ModelFileHeader *header = NULL;
    ModelNode *node;
    const uint8_t *blob;
    size_t blob_size;
    size_t display_count = 0U;
    size_t vertices = 0U;
    size_t batches = 0U;
    size_t continuation_count = 0U;
    size_t index;

    assert(ge_original_first_person_assets_load_raw(
        assets, 0U, model_id, NULL) == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_relocate_native(
        assets, 0U, NULL, (void **)&header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    blob = ge_original_first_person_assets_blob(assets, 0U, &blob_size);
    assert(blob != NULL && header != NULL && header->RootNode != NULL);
    for (index = 0U; index < 16U; ++index) identity(matrices[index]);

    node = header->RootNode;
    while (node != NULL) {
        const uint16_t opcode = node->Opcode & UINT16_C(0xff);
        if (opcode == MODELNODE_OPCODE_DL
                || opcode == MODELNODE_OPCODE_DLCOLLISION) {
            const void *primary = opcode == MODELNODE_OPCODE_DL
                ? (const void *)node->Data->DisplayList.Primary
                : (const void *)node->Data->DisplayListCollisions.Primary;
            const void *secondary = opcode == MODELNODE_OPCODE_DL
                ? (const void *)node->Data->DisplayList.Secondary
                : (const void *)node->Data->DisplayListCollisions.Secondary;
            GeOriginalModelSceneInput input;
            GeOriginalModelScene scene;
            memset(&input, 0, sizeof(input));
            input.blob = blob;
            input.blob_size = blob_size;
            input.primary_offset = primary != NULL
                ? blob_offset(blob, blob_size, primary)
                : GE_ORIGINAL_MODEL_SCENE_NO_LIST;
            input.secondary_offset = secondary != NULL
                ? blob_offset(blob, blob_size, secondary)
                : GE_ORIGINAL_MODEL_SCENE_NO_LIST;
            if (primary != NULL && secondary != NULL)
                ++continuation_count;
            input.segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
            if (opcode == MODELNODE_OPCODE_DLCOLLISION)
                assert(ge_original_first_person_assets_collision_vertex_blob_offset(
                    assets, node, &input.segment4_offset));
            input.segment3_matrices = matrices;
            input.segment3_matrix_count = (size_t)header->numMatrices;
            identity(input.matrix);
            assert(ge_original_model_scene_build(&input, NULL, &scene)
                   == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
            display_count++;
            vertices += scene.required_vertex_count;
            batches += scene.required_batch_count;
        }
        if (node->Child != NULL) {
            node = node->Child;
        } else {
            while (node != NULL && node->Next == NULL) node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
    assert(display_count != 0U && vertices != 0U && batches != 0U);
    assert(header->numtextures >= 0);
    {
        TextureVisitCheck check = {header, 0U, 0};
        assert(ge_original_first_person_assets_visit_texture_ids(
            assets, 0U, &check, check_texture_id));
        assert(check.count == (size_t)header->numtextures);
        assert(!ge_original_first_person_assets_visit_texture_ids(
            assets, 2U, &check, check_texture_id));
        assert(!ge_original_first_person_assets_visit_texture_ids(
            assets, 0U, &check, NULL));
        if (header->numtextures > 0) {
            check.count = 0U;
            check.reject = 1;
            assert(!ge_original_first_person_assets_visit_texture_ids(
                assets, 0U, &check, check_texture_id));
            assert(check.count == 1U);
        }
    }
    assert((size_t)header->numtextures <= LIVE_TEXTURE_CAPACITY);
    assert(display_count <= LIVE_DISPLAY_CAPACITY);
    assert(vertices <= LIVE_VERTEX_CAPACITY);
    assert(batches <= LIVE_BATCH_CAPACITY);
    if (display_count > measured_max_displays)
        measured_max_displays = display_count;
    if (vertices > measured_max_vertices) measured_max_vertices = vertices;
    if (batches > measured_max_batches) measured_max_batches = batches;
    if ((size_t)header->numtextures > measured_max_textures)
        measured_max_textures = (size_t)header->numtextures;
    if (model_id == GE_ORIGINAL_FIRST_PERSON_MODEL_TRIGGER) {
        assert(display_count == 9U && vertices == 4119U && batches == 419U);
        assert(continuation_count != 0U);
        assert(header->numtextures == 22);
    } else if (model_id == GE_ORIGINAL_FIRST_PERSON_MODEL_CAMERA) {
        assert(display_count == 1U && vertices == 246U && batches == 23U);
        assert(continuation_count == 1U);
    } else if (model_id
               == GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_MAGNET_ATTRACT) {
        assert(display_count == 1U && vertices == 1863U && batches == 171U);
        assert(continuation_count == 1U);
    } else if (model_id == GE_ORIGINAL_FIRST_PERSON_MODEL_AUTOSHOT) {
        assert(display_count == 28U && vertices == 1386U && batches == 143U);
    } else if (model_id
               == GE_ORIGINAL_FIRST_PERSON_MODEL_SUIT_LEFT_HAND) {
        static const uint32_t suit_texture_ids[22] = {
            1695U, 2057U, 1608U, 1597U, 1598U, 1651U, 1644U, 1632U,
            1639U, 1605U, 1794U, 1795U, 1501U, 1798U, 1796U, 1797U,
            1793U, 1507U, 1503U, 1502U, 1505U, 1504U,
        };
        assert(display_count == 12U && vertices == 4065U && batches == 418U);
        assert(header->numSwitches == 10 && header->numMatrices == 9
               && header->numtextures == 22);
        for (index = 0U;
             index < sizeof(suit_texture_ids) / sizeof(suit_texture_ids[0]);
             ++index)
            assert(header->Textures[index].TextureID
                   == suit_texture_ids[index]);
    }
    printf("%s: %zu/%u display, %zu/%u vertex, %zu/%u batch, %u/%u texture\n",
           name, display_count, LIVE_DISPLAY_CAPACITY,
           vertices, LIVE_VERTEX_CAPACITY, batches, LIVE_BATCH_CAPACITY,
           (unsigned)header->numtextures, LIVE_TEXTURE_CAPACITY);
}

int main(int argc, char **argv)
{
    static const struct {
        GeOriginalFirstPersonModel model;
        const char *name;
    } models[] = {
        {GE_ORIGINAL_FIRST_PERSON_MODEL_PP7, "PP7"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_PP7_SILENCED, "PP7 silenced"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_BUG, "modem"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_AK47, "AK-47"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_REMOTE_MINE, "remote mine"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_SNIPER_RIFLE, "sniper rifle"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_TRIGGER, "trigger"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_FIST, "fist"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K_SILENCED, "MP5K silenced"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_PLASTIQUE, "plastique"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_CAMERA, "camera"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_MAGNET_ATTRACT,
         "watch magnet attract"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_UZI, "Uzi"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_LASER, "watch laser"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE_LAUNCHER,
         "grenade launcher"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE, "grenade"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_TIMED_MINE, "timed mine"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_BOMB_CASE, "bomb case"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_MICROCAMERA, "microcamera"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDENEYE_KEY, "GoldenEye key"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_FNP90, "RCP90"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_RUGER, "magnum"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_SPECTRE, "Phantom"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_M16, "AR33"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_SHOTGUN, "shotgun"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_AUTOSHOT, "automatic shotgun"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K, "D5K"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_TT33, "Tokarev"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_SKORPION, "Skorpion"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_KNIFE, "knife"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_THROW_KNIFE, "throwing knife"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDEN_GUN, "Golden Gun"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_SILVER_PP7, "silver PP7"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_GOLD_PP7, "gold PP7"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_LASER, "laser"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_ROCKET_LAUNCHER, "rocket launcher"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_PROXIMITY_MINE, "proximity mine"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_TASER, "taser"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_FLARE_PISTOL, "flare pistol"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_PITON_GUN, "piton gun"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_SUIT_LEFT_HAND, "suit/watch hand"},
        {GE_ORIGINAL_FIRST_PERSON_MODEL_JOYPAD, "joypad"},
    };
    GeAssetPack pack;
    GeOriginalFirstPersonAssets assets;
    GeOriginalFirstPersonSceneRequirements requirements;
    size_t index;
    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    assert(ge_original_first_person_assets_init(
        &assets, &pack, right_buffer, sizeof(right_buffer),
        left_buffer, sizeof(left_buffer)) == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    for (index = 0U; index < sizeof(models) / sizeof(models[0]); ++index)
        measure(&assets, models[index].model, models[index].name);
    requirements = ge_original_first_person_scene_supported_requirements();
    assert(requirements.display_list_capacity == measured_max_displays);
    assert(requirements.vertex_capacity == measured_max_vertices);
    assert(requirements.batch_capacity == measured_max_batches);
    assert(requirements.texture_capacity == measured_max_textures);
    ge_original_first_person_assets_close(&assets);
    ge_asset_pack_close(&pack);
    return 0;
}

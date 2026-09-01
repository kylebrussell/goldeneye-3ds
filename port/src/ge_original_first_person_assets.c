#include "ge_original_first_person_assets.h"

#include <ultra64.h>
#include <bondtypes.h>
#include <stdlib.h>
#include <string.h>

enum {
    GE_PP7_BLOB_SIZE = 18512,
    GE_PP7_SILENCED_BLOB_SIZE = 19536,
    GE_BUG_BLOB_SIZE = 6848,
    GE_AK47_BLOB_SIZE = 8240,
    GE_REMOTE_MINE_BLOB_SIZE = 6512,
    GE_SNIPER_RIFLE_BLOB_SIZE = 13040,
    GE_TRIGGER_BLOB_SIZE = 35936,
    GE_FIST_BLOB_SIZE = 15072,
    GE_MP5K_SILENCED_BLOB_SIZE = 10336,
    GE_PLASTIQUE_BLOB_SIZE = 2336,
    GE_CAMERA_BLOB_SIZE = 3232,
    GE_WATCH_MAGNET_ATTRACT_BLOB_SIZE = 15312,
    GE_UZI_BLOB_SIZE = 7504,
    GE_WATCH_LASER_BLOB_SIZE = 35936,
    GE_GRENADE_LAUNCHER_BLOB_SIZE = 13520,
    GE_GRENADE_BLOB_SIZE = 7840,
    GE_TIMED_MINE_BLOB_SIZE = 7136,
    GE_BOMB_CASE_BLOB_SIZE = 6272,
    GE_MICROCAMERA_BLOB_SIZE = 4016,
    GE_GOLDENEYE_KEY_BLOB_SIZE = 7648,
    GE_FNP90_BLOB_SIZE = 9552,
    GE_RUGER_BLOB_SIZE = 19376,
    GE_SPECTRE_BLOB_SIZE = 9824,
    GE_M16_BLOB_SIZE = 7472,
    GE_SHOTGUN_BLOB_SIZE = 12112,
    GE_AUTOSHOT_BLOB_SIZE = 20016,
    GE_MP5K_BLOB_SIZE = 9504,
    GE_TT33_BLOB_SIZE = 17584,
    GE_SKORPION_BLOB_SIZE = 13536,
    GE_KNIFE_BLOB_SIZE = 18416,
    GE_THROW_KNIFE_BLOB_SIZE = 18240,
    GE_GOLDEN_GUN_BLOB_SIZE = 16320,
    GE_SILVER_PP7_BLOB_SIZE = 16480,
    GE_GOLD_PP7_BLOB_SIZE = 16480,
    GE_LASER_BLOB_SIZE = 10512,
    GE_ROCKET_LAUNCHER_BLOB_SIZE = 12704,
    GE_PROXIMITY_MINE_BLOB_SIZE = 5360,
    GE_TASER_BLOB_SIZE = 19984,
    GE_FLARE_PISTOL_BLOB_SIZE = 1952,
    GE_PITON_GUN_BLOB_SIZE = 1952,
    GE_SUIT_LEFT_HAND_BLOB_SIZE = 38688,
    GE_JOYPAD_BLOB_SIZE = 21008,
    GE_FIRST_PERSON_MAX_SWITCH_COUNT = 36,
    GE_FIRST_PERSON_MAX_TEXTURE_COUNT = 22,
    GE_FIRST_PERSON_MAX_NODES = 64,
    GE_FIRST_PERSON_MAX_COLLISION_VERTICES = 4,
    GE_MODEL_SEGMENT_BASE = 0x05000000
};

typedef struct GeOriginalFirstPersonModelSpec {
    const char *path;
    size_t blob_size;
    uint32_t switch_count;
    uint32_t texture_count;
    uint32_t root_offset;
    uint32_t node_count;
    int16_t matrix_count;
    float bounding_radius;
} GeOriginalFirstPersonModelSpec;

static const GeOriginalFirstPersonModelSpec ge_model_specs[] = {
    {GE_ORIGINAL_FIRST_PERSON_PP7_PATH, GE_PP7_BLOB_SIZE,
     36U, 12U, 0x120U, 48U, 6, 293.60767f},
    {GE_ORIGINAL_FIRST_PERSON_PP7_SILENCED_PATH,
     GE_PP7_SILENCED_BLOB_SIZE, 36U, 12U, 0x120U, 49U, 6, 438.66476f},
    {GE_ORIGINAL_FIRST_PERSON_BUG_PATH, GE_BUG_BLOB_SIZE,
     28U, 6U, 0xb8U, 2U, 3, 106.2163f},
    {GE_ORIGINAL_FIRST_PERSON_AK47_PATH, GE_AK47_BLOB_SIZE,
     36U, 18U, 0x168U, 28U, 4, 941.9339f},
    {GE_ORIGINAL_FIRST_PERSON_REMOTE_MINE_PATH, GE_REMOTE_MINE_BLOB_SIZE,
     35U, 3U, 0xb0U, 2U, 3, 50.999378f},
    {GE_ORIGINAL_FIRST_PERSON_SNIPER_RIFLE_PATH,
     GE_SNIPER_RIFLE_BLOB_SIZE, 36U, 7U, 0xe4U, 26U, 4, 808.03253f},
    {GE_ORIGINAL_FIRST_PERSON_TRIGGER_PATH, GE_TRIGGER_BLOB_SIZE,
     36U, 22U, 0x198U, 19U, 4, 283.9006f},
    {GE_ORIGINAL_FIRST_PERSON_FIST_PATH, GE_FIST_BLOB_SIZE,
     36U, 14U, 0x138U, 11U, 3, 243.84764f},
    {GE_ORIGINAL_FIRST_PERSON_MP5K_SILENCED_PATH,
     GE_MP5K_SILENCED_BLOB_SIZE, 36U, 9U, 0xfcU, 27U, 4, 655.97717f},
    {GE_ORIGINAL_FIRST_PERSON_PLASTIQUE_PATH, GE_PLASTIQUE_BLOB_SIZE,
     28U, 3U, 0x94U, 6U, 3, 255.35242f},
    {GE_ORIGINAL_FIRST_PERSON_CAMERA_PATH, GE_CAMERA_BLOB_SIZE,
     28U, 11U, 0xf4U, 2U, 3, 52.775627f},
    {GE_ORIGINAL_FIRST_PERSON_WATCH_MAGNET_ATTRACT_PATH,
     GE_WATCH_MAGNET_ATTRACT_BLOB_SIZE,
     29U, 9U, 0xe0U, 2U, 3, 384.9288f},
    {GE_ORIGINAL_FIRST_PERSON_UZI_PATH, GE_UZI_BLOB_SIZE,
     36U, 12U, 0x120U, 22U, 5, 436.95404f},
    {GE_ORIGINAL_FIRST_PERSON_WATCH_LASER_PATH, GE_WATCH_LASER_BLOB_SIZE,
     36U, 22U, 0x198U, 19U, 4, 283.9006f},
    {GE_ORIGINAL_FIRST_PERSON_GRENADE_LAUNCHER_PATH,
     GE_GRENADE_LAUNCHER_BLOB_SIZE,
     36U, 15U, 0x144U, 36U, 5, 768.33496f},
    {GE_ORIGINAL_FIRST_PERSON_GRENADE_PATH, GE_GRENADE_BLOB_SIZE,
     36U, 5U, 0xccU, 2U, 3, 427.27081f},
    {GE_ORIGINAL_FIRST_PERSON_TIMED_MINE_PATH, GE_TIMED_MINE_BLOB_SIZE,
     28U, 4U, 0xa0U, 2U, 3, 49.368877f},
    {GE_ORIGINAL_FIRST_PERSON_BOMB_CASE_PATH, GE_BOMB_CASE_BLOB_SIZE,
     28U, 6U, 0xb8U, 2U, 3, 116.11074f},
    {GE_ORIGINAL_FIRST_PERSON_MICROCAMERA_PATH, GE_MICROCAMERA_BLOB_SIZE,
     35U, 7U, 0xe0U, 2U, 3, 70.039436f},
    {GE_ORIGINAL_FIRST_PERSON_GOLDENEYE_KEY_PATH,
     GE_GOLDENEYE_KEY_BLOB_SIZE,
     28U, 5U, 0xacU, 2U, 3, 98.987083f},
    {GE_ORIGINAL_FIRST_PERSON_FNP90_PATH, GE_FNP90_BLOB_SIZE,
     36U, 10U, 0x108U, 30U, 4, 460.81909f},
    {GE_ORIGINAL_FIRST_PERSON_RUGER_PATH, GE_RUGER_BLOB_SIZE,
     36U, 14U, 0x138U, 40U, 6, 553.44312f},
    {GE_ORIGINAL_FIRST_PERSON_SPECTRE_PATH, GE_SPECTRE_BLOB_SIZE,
     36U, 11U, 0x114U, 28U, 4, 598.42865f},
    {GE_ORIGINAL_FIRST_PERSON_M16_PATH, GE_M16_BLOB_SIZE,
     36U, 8U, 0xf0U, 30U, 4, 1096.2413f},
    {GE_ORIGINAL_FIRST_PERSON_SHOTGUN_PATH, GE_SHOTGUN_BLOB_SIZE,
     28U, 13U, 0x10cU, 39U, 4, 919.33038f},
    {GE_ORIGINAL_FIRST_PERSON_AUTOSHOT_PATH, GE_AUTOSHOT_BLOB_SIZE,
     36U, 16U, 0x150U, 63U, 4, 840.15125f},
    {GE_ORIGINAL_FIRST_PERSON_MP5K_PATH, GE_MP5K_BLOB_SIZE,
     36U, 9U, 0xfcU, 25U, 4, 499.24536f},
    {GE_ORIGINAL_FIRST_PERSON_TT33_PATH, GE_TT33_BLOB_SIZE,
     36U, 15U, 0x144U, 47U, 6, 438.16788f},
    {GE_ORIGINAL_FIRST_PERSON_SKORPION_PATH, GE_SKORPION_BLOB_SIZE,
     36U, 12U, 0x120U, 42U, 4, 390.40039f},
    {GE_ORIGINAL_FIRST_PERSON_KNIFE_PATH, GE_KNIFE_BLOB_SIZE,
     36U, 9U, 0xfcU, 30U, 3, 376.97263f},
    {GE_ORIGINAL_FIRST_PERSON_THROW_KNIFE_PATH, GE_THROW_KNIFE_BLOB_SIZE,
     36U, 9U, 0xfcU, 32U, 3, 373.31387f},
    {GE_ORIGINAL_FIRST_PERSON_GOLDEN_GUN_PATH, GE_GOLDEN_GUN_BLOB_SIZE,
     36U, 11U, 0x114U, 32U, 5, 384.92172f},
    {GE_ORIGINAL_FIRST_PERSON_SILVER_PP7_PATH, GE_SILVER_PP7_BLOB_SIZE,
     36U, 11U, 0x114U, 45U, 6, 293.60767f},
    {GE_ORIGINAL_FIRST_PERSON_GOLD_PP7_PATH, GE_GOLD_PP7_BLOB_SIZE,
     36U, 11U, 0x114U, 45U, 6, 293.60767f},
    {GE_ORIGINAL_FIRST_PERSON_LASER_PATH, GE_LASER_BLOB_SIZE,
     36U, 13U, 0x12cU, 25U, 3, 442.81848f},
    {GE_ORIGINAL_FIRST_PERSON_ROCKET_LAUNCHER_PATH,
     GE_ROCKET_LAUNCHER_BLOB_SIZE,
     36U, 10U, 0x108U, 35U, 3, 566.51208f},
    {GE_ORIGINAL_FIRST_PERSON_PROXIMITY_MINE_PATH,
     GE_PROXIMITY_MINE_BLOB_SIZE,
     28U, 3U, 0x94U, 2U, 3, 51.00029f},
    {GE_ORIGINAL_FIRST_PERSON_TASER_PATH, GE_TASER_BLOB_SIZE,
     35U, 17U, 0x158U, 34U, 3, 182.78622f},
    {GE_ORIGINAL_FIRST_PERSON_FLARE_PISTOL_PATH, GE_FLARE_PISTOL_BLOB_SIZE,
     28U, 2U, 0x88U, 2U, 3, 134.8334f},
    {GE_ORIGINAL_FIRST_PERSON_PITON_GUN_PATH, GE_PITON_GUN_BLOB_SIZE,
     28U, 2U, 0x88U, 2U, 3, 134.8334f},
    {GE_ORIGINAL_FIRST_PERSON_SUIT_LEFT_HAND_PATH,
     GE_SUIT_LEFT_HAND_BLOB_SIZE,
     10U, 22U, 0x130U, 28U, 9, 12231.949f},
    {GE_ORIGINAL_FIRST_PERSON_JOYPAD_PATH, GE_JOYPAD_BLOB_SIZE,
     14U, 4U, 0x68U, 27U, 13, 523.96826f},
};

typedef struct GeOriginalFirstPersonNativeModel {
    const uint8_t *blob;
    size_t blob_size;
    ModelFileHeader header;
    ModelNode *switches[GE_FIRST_PERSON_MAX_SWITCH_COUNT];
    ModelFileTextures textures[GE_FIRST_PERSON_MAX_TEXTURE_COUNT];
    ModelNode nodes[GE_FIRST_PERSON_MAX_NODES];
    union ModelRoData data[GE_FIRST_PERSON_MAX_NODES];
    uint32_t offsets[GE_FIRST_PERSON_MAX_NODES];
    size_t node_count;
    Vertex collision_visual[GE_FIRST_PERSON_MAX_COLLISION_VERTICES];
    Vertex collision_points[GE_FIRST_PERSON_MAX_COLLISION_VERTICES];
    s16 collision_usage[GE_FIRST_PERSON_MAX_COLLISION_VERTICES];
    size_t collision_visual_count;
    size_t collision_point_count;
    size_t collision_node_index;
    uint32_t collision_vertices_offset;
} GeOriginalFirstPersonNativeModel;

static GeOriginalFirstPersonAssets *loader_assets;
static GeOriginalFirstPersonLoaderState *loader_state;
struct texpool;

static const uint8_t ge_us_rom_sha1[GE_ASSET_PACK_SOURCE_SHA1_SIZE] = {
    0xab, 0xe0, 0x1e, 0x4a, 0xeb, 0x03, 0x3b, 0x6c, 0x08, 0x36,
    0x81, 0x9f, 0x54, 0x9c, 0x79, 0x1b, 0x26, 0xcf, 0xde, 0x83
};

static uint32_t read_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) | ((uint32_t)source[1] << 16)
        | ((uint32_t)source[2] << 8) | (uint32_t)source[3];
}

static uint16_t read_be16(const uint8_t *source)
{
    return (uint16_t)(((uint16_t)source[0] << 8) | source[1]);
}

static int16_t read_bes16(const uint8_t *source)
{
    return (int16_t)read_be16(source);
}

static float read_bef32(const uint8_t *source)
{
    const uint32_t bits = read_be32(source);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void read_native_vertex(Vertex *vertex, const uint8_t *source)
{
    memset(vertex, 0, sizeof(*vertex));
    vertex->coord.x = read_bes16(source);
    vertex->coord.y = read_bes16(source + 2U);
    vertex->coord.z = read_bes16(source + 4U);
    vertex->index = read_bes16(source + 6U);
    vertex->s = read_bes16(source + 8U);
    vertex->t = read_bes16(source + 10U);
    vertex->r = source[12U];
    vertex->g = source[13U];
    vertex->b = source[14U];
    vertex->a = source[15U];
}

static int model_offset(uint32_t address, size_t blob_size, uint32_t *offset)
{
    uint32_t candidate;
    if (address == 0U) return 0;
    if ((address & UINT32_C(0xff000000)) != GE_MODEL_SEGMENT_BASE)
        return -1;
    candidate = address & UINT32_C(0x00ffffff);
    if ((size_t)candidate >= blob_size) return -1;
    *offset = candidate;
    return 1;
}

static int native_node_index(const GeOriginalFirstPersonNativeModel *model,
                             uint32_t offset)
{
    size_t index;
    for (index = 0U; index < model->node_count; ++index)
        if (model->offsets[index] == offset) return (int)index;
    return -1;
}

static int collect_native_node(GeOriginalFirstPersonNativeModel *model,
                               uint32_t offset)
{
    const uint8_t *node;
    const uint32_t links[2] = {12U, 20U};
    size_t link_index;
    if ((offset & 3U) != 0U || offset > model->blob_size
            || model->blob_size - offset < 24U) return 0;
    if (native_node_index(model, offset) >= 0) return 1;
    if (model->node_count >= GE_FIRST_PERSON_MAX_NODES) return 0;
    model->offsets[model->node_count++] = offset;
    node = model->blob + offset;
    for (link_index = 0U; link_index < 2U; ++link_index) {
        uint32_t child_offset = 0U;
        const int state = model_offset(read_be32(node + links[link_index]),
                                       model->blob_size, &child_offset);
        if (state < 0 || (state > 0
                && !collect_native_node(model, child_offset))) return 0;
    }
    return 1;
}

static ModelNode *native_node_pointer(
    GeOriginalFirstPersonNativeModel *model, uint32_t address)
{
    uint32_t offset = 0U;
    int index;
    const int state = model_offset(address, model->blob_size, &offset);
    if (state <= 0) return NULL;
    index = native_node_index(model, offset);
    return index >= 0 ? &model->nodes[index] : NULL;
}

static void *native_blob_pointer(GeOriginalFirstPersonNativeModel *model,
                                 uint32_t address)
{
    uint32_t offset = 0U;
    return model_offset(address, model->blob_size, &offset) > 0
        ? (void *)(uintptr_t)(model->blob + offset) : NULL;
}

static int relocate_node_data(GeOriginalFirstPersonNativeModel *model,
                              size_t index, uint16_t opcode,
                              uint32_t data_address)
{
    uint32_t offset = 0U;
    const uint8_t *source;
    union ModelRoData *data = &model->data[index];
    if (model_offset(data_address, model->blob_size, &offset) <= 0
            || offset > model->blob_size
            || model->blob_size - offset < 8U) return 0;
    source = model->blob + offset;
    memset(data, 0, sizeof(*data));
    switch (opcode & UINT16_C(0xff)) {
    case MODELNODE_OPCODE_HEADER:
        if (model->blob_size - offset < 16U) return 0;
        data->Header.AnimPart = read_be16(source);
        data->Header.MatrixIndex = read_bes16(source + 2U);
        data->Header.FirstGroupNode = native_node_pointer(
            model, read_be32(source + 4U));
        data->Header.GroupsAsF32 = read_bef32(source + 8U);
        data->Header.RwDataIndex = read_be16(source + 12U);
        data->Header.reserved = read_be16(source + 14U);
        break;
    case MODELNODE_OPCODE_GROUP:
        if (model->blob_size - offset < 28U) return 0;
        data->Group.Origin.x = read_bef32(source);
        data->Group.Origin.y = read_bef32(source + 4U);
        data->Group.Origin.z = read_bef32(source + 8U);
        data->Group.JointID = read_be16(source + 12U);
        data->Group.MatrixID0 = read_bes16(source + 14U);
        data->Group.MatrixID1 = read_bes16(source + 16U);
        data->Group.MatrixID2 = read_bes16(source + 18U);
        data->Group.ChildGroupNode = native_node_pointer(
            model, read_be32(source + 20U));
        data->Group.BoundingVolumeRadius = read_bef32(source + 24U);
        break;
    case MODELNODE_OPCODE_SWITCH:
        data->Switch.Controls = native_node_pointer(model, read_be32(source));
        data->Switch.RwDataIndex = read_be16(source + 4U);
        data->Switch.reserved = read_be16(source + 6U);
        break;
    case MODELNODE_OPCODE_GROUPSIMPLE:
        if (model->blob_size - offset < 20U) return 0;
        data->GroupSimple.Origin.x = read_bef32(source);
        data->GroupSimple.Origin.y = read_bef32(source + 4U);
        data->GroupSimple.Origin.z = read_bef32(source + 8U);
        data->GroupSimple.Group1 = read_bes16(source + 12U);
        data->GroupSimple.Group2 = read_be16(source + 14U);
        data->GroupSimple.BoundingVolumeRadius = read_bef32(source + 16U);
        break;
    case MODELNODE_OPCODE_BSP:
        if (model->blob_size - offset < 36U) return 0;
        data->BSP.Point.x = read_bef32(source);
        data->BSP.Point.y = read_bef32(source + 4U);
        data->BSP.Point.z = read_bef32(source + 8U);
        data->BSP.Vector.x = read_bef32(source + 12U);
        data->BSP.Vector.y = read_bef32(source + 16U);
        data->BSP.Vector.z = read_bef32(source + 20U);
        data->BSP.leftChild = native_node_pointer(
            model, read_be32(source + 24U));
        data->BSP.rightChild = native_node_pointer(
            model, read_be32(source + 28U));
        data->BSP.reserved = read_bes16(source + 32U);
        data->BSP.RwDataIndex = read_be16(source + 34U);
        break;
    case MODELNODE_OPCODE_DL:
        if (model->blob_size - offset < 20U) return 0;
        data->DisplayList.Primary = native_blob_pointer(
            model, read_be32(source));
        data->DisplayList.Secondary = native_blob_pointer(
            model, read_be32(source + 4U));
        data->DisplayList.BaseAddr = (void *)(uintptr_t)model->blob;
        data->DisplayList.Vertices = native_blob_pointer(
            model, read_be32(source + 12U));
        data->DisplayList.numVertices = read_be16(source + 16U);
        data->DisplayList.ModelType = (int8_t)source[18U];
        break;
    case MODELNODE_OPCODE_DLPRIMARY:
        if (model->blob_size - offset < 16U) return 0;
        data->DisplayListPrimary.numVertices = (s32)read_be32(source);
        data->DisplayListPrimary.Vertices = native_blob_pointer(
            model, read_be32(source + 4U));
        data->DisplayListPrimary.Primary = native_blob_pointer(
            model, read_be32(source + 8U));
        data->DisplayListPrimary.BaseAddr = native_blob_pointer(
            model, read_be32(source + 12U));
        break;
    case MODELNODE_OPCODE_DLCOLLISION:
    {
        ModelRoData_DisplayList_CollisionRecord *out =
            &data->DisplayListCollisions;
        const s16 vertex_count = read_bes16(source + 12U);
        const s16 collision_count = read_bes16(source + 14U);
        uint32_t vertices_offset = 0U;
        uint32_t collision_offset = 0U;
        uint32_t usage_offset = 0U;
        size_t vertex_index;
        if (model->blob_size - offset < 32U || vertex_count < 0
                || collision_count < 0
                || (size_t)vertex_count > GE_FIRST_PERSON_MAX_COLLISION_VERTICES
                || (size_t)collision_count
                    > GE_FIRST_PERSON_MAX_COLLISION_VERTICES
                || model_offset(read_be32(source + 8U), model->blob_size,
                                &vertices_offset) <= 0
                || model_offset(read_be32(source + 16U), model->blob_size,
                                &collision_offset) <= 0
                || model_offset(read_be32(source + 20U), model->blob_size,
                                &usage_offset) <= 0
                || (size_t)vertices_offset + (size_t)vertex_count * 16U
                    > model->blob_size
                || (size_t)collision_offset
                    + (size_t)collision_count * 16U > model->blob_size
                || (size_t)usage_offset + (size_t)vertex_count * 2U
                    > model->blob_size) return 0;
        model->collision_visual_count = (size_t)vertex_count;
        model->collision_point_count = (size_t)collision_count;
        model->collision_node_index = index;
        model->collision_vertices_offset = vertices_offset;
        for (vertex_index = 0U; vertex_index < (size_t)vertex_count;
             ++vertex_index) {
            read_native_vertex(&model->collision_visual[vertex_index],
                model->blob + vertices_offset + vertex_index * 16U);
            model->collision_usage[vertex_index] = read_bes16(
                model->blob + usage_offset + vertex_index * 2U);
        }
        for (vertex_index = 0U; vertex_index < (size_t)collision_count;
             ++vertex_index) {
            const uint8_t *raw = model->blob + collision_offset
                + vertex_index * 16U;
            const uint32_t related = read_be32(raw + 8U);
            read_native_vertex(&model->collision_points[vertex_index], raw);
            model->collision_points[vertex_index].CollisionRelatedNode =
                native_node_pointer(model, related);
            if (related != 0U
                    && model->collision_points[vertex_index]
                        .CollisionRelatedNode == NULL) return 0;
            model->collision_points[vertex_index].CollisionRelatedIndex =
                read_bes16(raw + 12U);
            model->collision_points[vertex_index].CollisionReserved =
                read_bes16(raw + 14U);
        }
        out->Primary = native_blob_pointer(model, read_be32(source));
        out->Secondary = native_blob_pointer(model, read_be32(source + 4U));
        out->Vertices = model->collision_visual;
        out->numVertices = vertex_count;
        out->numCollisionVertices = collision_count;
        out->CollisionVertices = model->collision_points;
        out->PointUsage = model->collision_usage;
        out->ModelType = read_bes16(source + 24U);
        out->RwDataIndex = read_be16(source + 26U);
        out->BaseAddr = (void *)(uintptr_t)model->blob;
        break;
    }
    case MODELNODE_OPCODE_INTERLINK:
        if (model->blob_size - offset < 28U) return 0;
        data->Interlinkage.pos.f[0] = read_bef32(source);
        data->Interlinkage.pos.f[1] = read_bef32(source + 4U);
        data->Interlinkage.pos.f[2] = read_bef32(source + 8U);
        data->Interlinkage.pos2.f[0] = read_bef32(source + 12U);
        data->Interlinkage.pos2.f[1] = read_bef32(source + 16U);
        data->Interlinkage.pos2.f[2] = read_bef32(source + 20U);
        data->Interlinkage.Scale = read_bef32(source + 24U);
        break;
    default:
        return 0;
    }
    return 1;
}

static int relocate_native_model(GeOriginalFirstPersonNativeModel *model,
                                 const uint8_t *blob, size_t blob_size,
                                 const ModelFileHeader *header_template,
                                 const GeOriginalFirstPersonModelSpec *spec)
{
    size_t index;
    memset(model, 0, sizeof(*model));
    model->blob = blob;
    model->blob_size = blob_size;
    if (!collect_native_node(model, spec->root_offset)) return 0;
    for (index = 0U; index < model->node_count; ++index) {
        const uint8_t *source = blob + model->offsets[index];
        ModelNode *node = &model->nodes[index];
        const uint16_t opcode = read_be16(source);
        node->Opcode = opcode;
        node->Parent = native_node_pointer(model, read_be32(source + 8U));
        node->Next = native_node_pointer(model, read_be32(source + 12U));
        node->Prev = native_node_pointer(model, read_be32(source + 16U));
        node->Child = native_node_pointer(model, read_be32(source + 20U));
        node->Data = &model->data[index];
        if (!relocate_node_data(model, index, opcode,
                                read_be32(source + 4U))) return 0;
    }
    for (index = 0U; index < spec->switch_count; ++index) {
        const uint32_t address = read_be32(blob + index * 4U);
        model->switches[index] = native_node_pointer(model, address);
        if (address != 0U && model->switches[index] == NULL) return 0;
    }
    for (index = 0U; index < spec->texture_count; ++index) {
        const uint8_t *source = blob + spec->switch_count * 4U
            + index * 12U;
        ModelFileTextures *texture = &model->textures[index];
        texture->TextureID = read_be32(source);
        texture->Width = source[4U];
        texture->Height = source[5U];
        texture->MipMapTiles = source[6U];
        texture->Type = source[7U];
        texture->RenderDepth = source[8U];
        texture->sflags = source[9U];
        texture->tflags = source[10U];
    }
    model->header = *header_template;
    model->header.Switches = model->switches;
    model->header.Textures = model->textures;
    model->header.RootNode = &model->nodes[0];
    return model->node_count == spec->node_count;
}

static int segmented_pointer_is_in_blob(uint32_t address, size_t blob_size)
{
    uint32_t offset;

    if (address == 0U) {
        return 1;
    }
    if ((address & UINT32_C(0xff000000)) != GE_MODEL_SEGMENT_BASE) {
        return 0;
    }
    offset = address & UINT32_C(0x00ffffff);
    return offset < blob_size && (offset & UINT32_C(3)) == 0U;
}

static int validate_serialized_model(
    const uint8_t *blob, size_t blob_size,
    const GeOriginalFirstPersonModelSpec *spec)
{
    size_t index;

    if (spec == NULL
            || spec->switch_count > GE_FIRST_PERSON_MAX_SWITCH_COUNT
            || spec->texture_count > GE_FIRST_PERSON_MAX_TEXTURE_COUNT
            || spec->switch_count * sizeof(uint32_t)
                + spec->texture_count * 12U != spec->root_offset
            || blob_size < spec->root_offset + 24U) {
        return 0;
    }
    for (index = 0U; index < spec->switch_count; ++index) {
        if (!segmented_pointer_is_in_blob(read_be32(blob + index * 4U),
                                          blob_size)) {
            return 0;
        }
    }

    /* Gun/item resources begin at an authored group root. Character-body
     * resources such as Csuit_lf_handZ begin at an authored header root. */
    return blob[spec->root_offset] == 0U
        && (blob[spec->root_offset + 1U] == MODELNODE_OPCODE_GROUP
            || blob[spec->root_offset + 1U] == MODELNODE_OPCODE_HEADER)
        && segmented_pointer_is_in_blob(
            read_be32(blob + spec->root_offset + 4U), blob_size)
        && read_be32(blob + spec->root_offset + 8U) == 0U
        && read_be32(blob + spec->root_offset + 12U) == 0U
        && read_be32(blob + spec->root_offset + 16U) == 0U
        && segmented_pointer_is_in_blob(
            read_be32(blob + spec->root_offset + 20U), blob_size);
}

GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_init(
    GeOriginalFirstPersonAssets *assets, GeAssetPack *pack,
    void *right_buffer, size_t right_size,
    void *left_buffer, size_t left_size)
{
    if (assets == NULL || pack == NULL || pack->file == NULL
            || right_buffer == NULL || left_buffer == NULL) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    }
    if (right_size < GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE
            || left_size < GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_SIZE;
    }
    if (memcmp(pack->source_sha1, ge_us_rom_sha1,
               sizeof(ge_us_rom_sha1)) != 0) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_WRONG_ROM;
    }

    memset(assets, 0, sizeof(*assets));
    assets->pack = pack;
    assets->hand_buffer[0] = right_buffer;
    assets->hand_buffer[1] = left_buffer;
    assets->hand_buffer_size[0] = right_size;
    assets->hand_buffer_size[1] = left_size;
    assets->loaded_model[0] = GE_ORIGINAL_FIRST_PERSON_MODEL_NONE;
    assets->loaded_model[1] = GE_ORIGINAL_FIRST_PERSON_MODEL_NONE;
    return GE_ORIGINAL_FIRST_PERSON_ASSET_OK;
}

GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_load_raw(
    GeOriginalFirstPersonAssets *assets, unsigned hand,
    GeOriginalFirstPersonModel model, size_t *bytes_read)
{
    const GeOriginalFirstPersonModelSpec *spec;
    const GeAssetPackEntry *entry;
    size_t actual_size = 0U;
    int read_status;

    if (assets == NULL || assets->pack == NULL || hand >= 2U
            || assets->hand_buffer[hand] == NULL) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    }
    free(assets->native_model[hand]);
    assets->native_model[hand] = NULL;
    if ((unsigned)model >= (unsigned)GE_ORIGINAL_FIRST_PERSON_MODEL_NONE) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    }
    spec = &ge_model_specs[(unsigned)model];
    entry = ge_asset_pack_find(assets->pack, spec->path);
    if (entry == NULL) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_MISSING;
    }
    if (entry->data_size != spec->blob_size
            || spec->blob_size > assets->hand_buffer_size[hand]) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_SIZE;
    }
    read_status = ge_asset_pack_read(assets->pack, spec->path,
                                     assets->hand_buffer[hand], spec->blob_size,
                                     &actual_size);
    if (read_status != GE_ASSET_PACK_OK || actual_size != spec->blob_size) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_IO_ERROR;
    }
    if (!validate_serialized_model(
            assets->hand_buffer[hand], actual_size, spec)) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_LAYOUT;
    }

    assets->loaded_size[hand] = actual_size;
    assets->loaded_model[hand] = model;
    if (bytes_read != NULL) {
        *bytes_read = actual_size;
    }
    return GE_ORIGINAL_FIRST_PERSON_ASSET_OK;
}

GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_relocate_native(
    GeOriginalFirstPersonAssets *assets, unsigned hand,
    const void *header_template, void **native_header)
{
    GeOriginalFirstPersonNativeModel *model;
    ModelFileHeader fallback_header;
    const GeOriginalFirstPersonModelSpec *spec;
    if (assets == NULL || hand >= 2U || native_header == NULL
            || assets->loaded_model[hand]
                == GE_ORIGINAL_FIRST_PERSON_MODEL_NONE
            || assets->loaded_size[hand] == 0U) {
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    }
    if (header_template == NULL) {
        spec = &ge_model_specs[(unsigned)assets->loaded_model[hand]];
        memset(&fallback_header, 0, sizeof(fallback_header));
        fallback_header.numSwitches = (s16)spec->switch_count;
        fallback_header.numtextures = (s16)spec->texture_count;
        fallback_header.numMatrices = spec->matrix_count;
        fallback_header.BoundingVolumeRadius = spec->bounding_radius;
        header_template = &fallback_header;
    }
    spec = &ge_model_specs[(unsigned)assets->loaded_model[hand]];
    model = calloc(1U, sizeof(*model));
    if (model == NULL) return GE_ORIGINAL_FIRST_PERSON_ASSET_IO_ERROR;
    if (!relocate_native_model(model, assets->hand_buffer[hand],
                               assets->loaded_size[hand], header_template,
                               spec)) {
        free(model);
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_LAYOUT;
    }
    free(assets->native_model[hand]);
    assets->native_model[hand] = model;
    *native_header = &model->header;
    return GE_ORIGINAL_FIRST_PERSON_ASSET_OK;
}

int ge_original_first_person_assets_native_ready(
    const GeOriginalFirstPersonAssets *assets, unsigned hand)
{
    return assets != NULL && hand < 2U && assets->native_model[hand] != NULL;
}

size_t ge_original_first_person_assets_native_node_count(
    const GeOriginalFirstPersonAssets *assets, unsigned hand)
{
    const GeOriginalFirstPersonNativeModel *model;
    if (!ge_original_first_person_assets_native_ready(assets, hand)) return 0U;
    model = assets->native_model[hand];
    return model->node_count;
}

void *ge_original_first_person_assets_native_header(
    const GeOriginalFirstPersonAssets *assets, unsigned hand)
{
    const GeOriginalFirstPersonNativeModel *model;
    if (!ge_original_first_person_assets_native_ready(assets, hand)) return NULL;
    model = assets->native_model[hand];
    return (void *)(uintptr_t)&model->header;
}

const uint8_t *ge_original_first_person_assets_blob(
    const GeOriginalFirstPersonAssets *assets, unsigned hand,
    size_t *blob_size)
{
    if (blob_size != NULL) *blob_size = 0U;
    if (!ge_original_first_person_assets_native_ready(assets, hand)
            || assets->loaded_size[hand] == 0U) return NULL;
    if (blob_size != NULL) *blob_size = assets->loaded_size[hand];
    return assets->hand_buffer[hand];
}

const uint8_t *ge_original_first_person_assets_blob_for_root(
    const GeOriginalFirstPersonAssets *assets, const void *root_node,
    size_t *blob_size, unsigned *asset_slot)
{
    unsigned hand;
    if (blob_size != NULL) *blob_size = 0U;
    if (asset_slot != NULL) *asset_slot = 2U;
    if (assets == NULL || root_node == NULL) return NULL;
    for (hand = 0U; hand < 2U; ++hand) {
        const GeOriginalFirstPersonNativeModel *model =
            assets->native_model[hand];
        size_t node;
        if (model == NULL) continue;
        for (node = 0U; node < model->node_count; ++node) {
            if (root_node != &model->nodes[node]) continue;
            if (blob_size != NULL) *blob_size = assets->loaded_size[hand];
            if (asset_slot != NULL) *asset_slot = hand;
            return assets->hand_buffer[hand];
        }
    }
    return NULL;
}

int ge_original_first_person_assets_slot_for_header(
    const GeOriginalFirstPersonAssets *assets, const void *native_header,
    unsigned *asset_slot)
{
    unsigned hand;
    if (asset_slot != NULL) *asset_slot = 2U;
    if (assets == NULL || native_header == NULL) return 0;
    for (hand = 0U; hand < 2U; ++hand) {
        const GeOriginalFirstPersonNativeModel *model =
            assets->native_model[hand];
        if (model == NULL || native_header != &model->header) continue;
        if (asset_slot != NULL) *asset_slot = hand;
        return 1;
    }
    return 0;
}

int ge_original_first_person_assets_collision_vertex_blob_offset(
    const GeOriginalFirstPersonAssets *assets, const void *native_node,
    uint32_t *blob_offset)
{
    unsigned slot;
    if (assets == NULL || native_node == NULL || blob_offset == NULL)
        return 0;
    for (slot = 0U; slot < 2U; ++slot) {
        const GeOriginalFirstPersonNativeModel *model =
            assets->native_model[slot];
        if (model == NULL || model->collision_visual_count == 0U
                || model->collision_node_index >= model->node_count)
            continue;
        if (native_node == &model->nodes[model->collision_node_index]) {
            *blob_offset = model->collision_vertices_offset;
            return 1;
        }
    }
    return 0;
}

void ge_original_first_person_assets_close(GeOriginalFirstPersonAssets *assets)
{
    if (assets == NULL) return;
    free(assets->native_model[0]);
    free(assets->native_model[1]);
    memset(assets, 0, sizeof(*assets));
}

void ge_original_first_person_assets_bind_loader(
    GeOriginalFirstPersonAssets *assets,
    GeOriginalFirstPersonLoaderState *state)
{
    loader_assets = assets;
    loader_state = state;
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->last_status = GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    }
}

void load_object_fill_header(ModelFileHeader *objheader, u8 *name, u8 *dst,
                             s32 size, struct texpool *buffer)
{
    GeOriginalFirstPersonModel model_id;
    unsigned hand;
    void *native_header = NULL;
    GeOriginalFirstPersonAssetStatus status =
        GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    if (loader_state != NULL) loader_state->load_calls++;
    if (loader_assets == NULL || objheader == NULL || name == NULL
            || dst == NULL)
        goto done;
    if (strcmp((const char *)name, "GwppkZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_PP7;
    else if (strcmp((const char *)name, "GwppksilZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_PP7_SILENCED;
    else if (strcmp((const char *)name, "GbugZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_BUG;
    else if (strcmp((const char *)name, "Gak47Z") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_AK47;
    else if (strcmp((const char *)name, "GremotemineZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_REMOTE_MINE;
    else if (strcmp((const char *)name, "GsniperrifleZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_SNIPER_RIFLE;
    else if (strcmp((const char *)name, "GtriggerZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_TRIGGER;
    else if (strcmp((const char *)name, "GfistZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_FIST;
    else if (strcmp((const char *)name, "Gmp5ksilZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K_SILENCED;
    else if (strcmp((const char *)name, "GplastiqueZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_PLASTIQUE;
    else if (strcmp((const char *)name, "GcameraZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_CAMERA;
    else if (strcmp((const char *)name, "GwatchmagnetattractZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_MAGNET_ATTRACT;
    else if (strcmp((const char *)name, "GuziZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_UZI;
    else if (strcmp((const char *)name, "GwatchlaserZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_LASER;
    else if (strcmp((const char *)name, "GgrenadelaunchZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE_LAUNCHER;
    else if (strcmp((const char *)name, "GgrenadeZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE;
    else if (strcmp((const char *)name, "GtimedmineZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_TIMED_MINE;
    else if (strcmp((const char *)name, "GbombcaseZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_BOMB_CASE;
    else if (strcmp((const char *)name, "GmicrocameraZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_MICROCAMERA;
    else if (strcmp((const char *)name, "GgoldeneyekeyZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDENEYE_KEY;
    else if (strcmp((const char *)name, "Gfnp90Z") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_FNP90;
    else if (strcmp((const char *)name, "GrugerZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_RUGER;
    else if (strcmp((const char *)name, "GspectreZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_SPECTRE;
    else if (strcmp((const char *)name, "Gm16Z") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_M16;
    else if (strcmp((const char *)name, "GshotgunZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_SHOTGUN;
    else if (strcmp((const char *)name, "GautoshotZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_AUTOSHOT;
    else if (strcmp((const char *)name, "Gmp5kZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K;
    else if (strcmp((const char *)name, "Gtt33Z") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_TT33;
    else if (strcmp((const char *)name, "GskorpionZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_SKORPION;
    else if (strcmp((const char *)name, "GknifeZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_KNIFE;
    else if (strcmp((const char *)name, "GthrowknifeZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_THROW_KNIFE;
    else if (strcmp((const char *)name, "GgoldengunZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDEN_GUN;
    else if (strcmp((const char *)name, "GsilverwppkZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_SILVER_PP7;
    else if (strcmp((const char *)name, "GgoldwppkZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_GOLD_PP7;
    else if (strcmp((const char *)name, "GlaserZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_LASER;
    else if (strcmp((const char *)name, "GrocketlaunchZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_ROCKET_LAUNCHER;
    else if (strcmp((const char *)name, "GproximitymineZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_PROXIMITY_MINE;
    else if (strcmp((const char *)name, "GtaserZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_TASER;
    else if (strcmp((const char *)name, "GflarepistolZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_FLARE_PISTOL;
    else if (strcmp((const char *)name, "GpitongunZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_PITON_GUN;
    else if (strcmp((const char *)name, "Csuit_lf_handZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_SUIT_LEFT_HAND;
    else if (strcmp((const char *)name, "GjoypadZ") == 0)
        model_id = GE_ORIGINAL_FIRST_PERSON_MODEL_JOYPAD;
    else {
        status = GE_ORIGINAL_FIRST_PERSON_ASSET_MISSING;
        goto done;
    }
    if (size < 0
            || (size_t)size
                < ge_model_specs[(unsigned)model_id].blob_size) {
        status = GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_SIZE;
        goto done;
    }
    if (dst == loader_assets->hand_buffer[0]) hand = 0U;
    else if (dst == loader_assets->hand_buffer[1]) hand = 1U;
    else goto done;
    status = ge_original_first_person_assets_load_raw(
        loader_assets, hand, model_id, NULL);
    if (status != GE_ORIGINAL_FIRST_PERSON_ASSET_OK) goto done;
    status = ge_original_first_person_assets_relocate_native(
        loader_assets, hand, objheader, &native_header);
    if (status != GE_ORIGINAL_FIRST_PERSON_ASSET_OK) goto done;
    *objheader = *(ModelFileHeader *)native_header;
    if (loader_state != NULL) {
        loader_state->successful_loads++;
        if (buffer != NULL) loader_state->texture_pool_handoffs++;
    }
done:
    if (loader_state != NULL) loader_state->last_status = status;
}

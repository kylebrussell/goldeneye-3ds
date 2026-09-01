#include "ge_original_dam_objective_models.h"

#include <stdlib.h>
#include <string.h>

#define GE_OBJECTIVE_MODEL_BASE 0x05000000U

static uint32_t read_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) | ((uint32_t)source[1] << 16)
        | ((uint32_t)source[2] << 8) | source[3];
}

static uint16_t read_be16(const uint8_t *source)
{
    return (uint16_t)(((uint16_t)source[0] << 8) | source[1]);
}

static float read_bef32(const uint8_t *source)
{
    uint32_t bits = read_be32(source);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int segmented_offset(uint32_t address, size_t expected)
{
    return address == GE_OBJECTIVE_MODEL_BASE + (uint32_t)expected;
}

static void read_texture(ModelFileTextures *texture, const uint8_t *raw)
{
    texture->TextureID = read_be32(raw);
    texture->Width = raw[4];
    texture->Height = raw[5];
    texture->MipMapTiles = raw[6];
    texture->Type = raw[7];
    texture->RenderDepth = raw[8];
    texture->sflags = raw[9];
    texture->tflags = raw[10];
}

static void read_vertex(Vertex *vertex, const uint8_t *raw)
{
    vertex->coord.x = (s16)read_be16(raw);
    vertex->coord.y = (s16)read_be16(raw + 2U);
    vertex->coord.z = (s16)read_be16(raw + 4U);
    vertex->index = (s16)read_be16(raw + 6U);
    vertex->s = (s16)read_be16(raw + 8U);
    vertex->t = (s16)read_be16(raw + 10U);
    vertex->r = raw[12];
    vertex->g = raw[13];
    vertex->b = raw[14];
    vertex->a = raw[15];
}

static void read_group(union ModelRoData *data, const uint8_t *raw)
{
    data->Group.Origin.x = read_bef32(raw);
    data->Group.Origin.y = read_bef32(raw + 4U);
    data->Group.Origin.z = read_bef32(raw + 8U);
    data->Group.JointID = read_be16(raw + 12U);
    data->Group.MatrixID0 = (s16)read_be16(raw + 14U);
    data->Group.MatrixID1 = (s16)read_be16(raw + 16U);
    data->Group.MatrixID2 = (s16)read_be16(raw + 18U);
    data->Group.ChildGroup = NULL;
    data->Group.BoundingVolumeRadius = read_bef32(raw + 24U);
}

static void read_bbox(union ModelRoData *data, const uint8_t *raw)
{
    data->BoundingBox.ModelNumber = read_be32(raw);
    data->BoundingBox.Bounds.xmin = read_bef32(raw + 4U);
    data->BoundingBox.Bounds.xmax = read_bef32(raw + 8U);
    data->BoundingBox.Bounds.ymin = read_bef32(raw + 12U);
    data->BoundingBox.Bounds.ymax = read_bef32(raw + 16U);
    data->BoundingBox.Bounds.zmin = read_bef32(raw + 20U);
    data->BoundingBox.Bounds.zmax = read_bef32(raw + 24U);
}

static void init_skeleton(ModelJoint *joint, ModelSkeleton *skeleton)
{
    joint->NodeType = 0x0002U;
    joint->mtxA = 0U;
    joint->mtxB = 0U;
    skeleton->numjoints = 1;
    skeleton->Joints = joint;
    skeleton->SkeletonSize = 3;
}

static void init_model(Model *model, ModelFileHeader *header,
                       RenderPosView *render_positions,
                       uintptr_t *rwdata_words, s16 rw_word_count)
{
    model->obj = header;
    model->render_pos = render_positions;
    model->datas = (union ModelRwData **)(void *)rwdata_words;
    model->rwdatalen = rw_word_count;
    model->attachedto = NULL;
    model->attachedto_objinst = NULL;
    model->scale = 1.0f;
}

GeOriginalDamObjectiveModelsStatus ge_original_modembox_model_relocate(
    GeOriginalModemboxModel *runtime, const void *source_blob,
    size_t source_size)
{
    static const size_t node_offsets[GE_ORIGINAL_MODEMBOX_NODE_COUNT] = {
        0x34U, 0x4cU, 0x64U, 0x7cU, 0x94U
    };
    static const uint16_t node_opcodes[GE_ORIGINAL_MODEMBOX_NODE_COUNT] = {
        MODELNODE_OPCODE_GROUP, MODELNODE_OPCODE_BBOX,
        MODELNODE_OPCODE_DLCOLLISION, MODELNODE_OPCODE_SWITCH,
        MODELNODE_OPCODE_DLCOLLISION
    };
    const uint8_t *blob = source_blob;
    ModelRoData_DisplayList_CollisionRecord *collision;
    ModelRwData_DisplayList_CollisionRecord *collision_rw;
    size_t index;

    if (runtime == NULL || blob == NULL)
        return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    if (source_size != GE_ORIGINAL_MODEMBOX_BLOB_SIZE)
        return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_SIZE;
    for (index = 0U; index < GE_ORIGINAL_MODEMBOX_NODE_COUNT; index++) {
        if (read_be16(blob + node_offsets[index]) != node_opcodes[index])
            return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_LAYOUT;
    }
    if (!segmented_offset(read_be32(blob), 0x94U)
            || !segmented_offset(read_be32(blob + 0x38U), 0xacU)
            || !segmented_offset(read_be32(blob + 0x48U), 0x4cU)
            || !segmented_offset(read_be32(blob + 0x50U), 0xc8U)
            || !segmented_offset(read_be32(blob + 0x54U), 0x34U)
            || !segmented_offset(read_be32(blob + 0x60U), 0x64U)
            || !segmented_offset(read_be32(blob + 0x68U), 0x5d8U)
            || !segmented_offset(read_be32(blob + 0x6cU), 0x4cU)
            || !segmented_offset(read_be32(blob + 0x70U), 0x7cU)
            || !segmented_offset(read_be32(blob + 0x80U), 0x5f8U)
            || !segmented_offset(read_be32(blob + 0x84U), 0x4cU)
            || !segmented_offset(read_be32(blob + 0x8cU), 0x64U)
            || !segmented_offset(read_be32(blob + 0x90U), 0x94U)
            || !segmented_offset(read_be32(blob + 0x98U), 0x688U)
            || !segmented_offset(read_be32(blob + 0x9cU), 0x7cU)
            || !segmented_offset(read_be32(blob + 0x5d8U), 0x6a8U)
            || read_be32(blob + 0x5dcU) != 0U
            || !segmented_offset(read_be32(blob + 0x5e0U), 0xe8U)
            || !segmented_offset(read_be32(blob + 0x5e8U), 0x468U)
            || !segmented_offset(read_be32(blob + 0x5ecU), 0x568U)
            || !segmented_offset(read_be32(blob + 0x5f8U), 0x94U)
            || !segmented_offset(read_be32(blob + 0x688U), 0x768U)
            || read_be32(blob + 0x68cU) != 0U
            || !segmented_offset(read_be32(blob + 0x690U), 0x600U)
            || !segmented_offset(read_be32(blob + 0x698U), 0x640U)
            || !segmented_offset(read_be32(blob + 0x69cU), 0x680U))
        return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_LAYOUT;

    runtime->source_blob = blob;
    runtime->source_size = source_size;
    for (index = 0U; index < 4U; index++)
        read_texture(&runtime->textures[index], blob + 4U + index * 12U);
    read_group(&runtime->group_data, blob + 0xacU);
    read_bbox(&runtime->bbox_data, blob + 0xc8U);
    for (index = 0U; index < GE_ORIGINAL_MODEMBOX_VERTEX_COUNT; index++) {
        read_vertex(&runtime->vertices[index], blob + 0xe8U + index * 16U);
        runtime->point_usage[index] =
            (s16)read_be16(blob + 0x568U + index * 2U);
    }
    for (index = 0U;
            index < GE_ORIGINAL_MODEMBOX_COLLISION_VERTEX_COUNT; index++)
        read_vertex(&runtime->collision_vertices[index],
                    blob + 0x468U + index * 16U);
    for (index = 0U; index < GE_ORIGINAL_MODEMBOX_SCREEN_VERTEX_COUNT;
            index++) {
        read_vertex(&runtime->screen_vertices[index],
                    blob + 0x600U + index * 16U);
        read_vertex(&runtime->screen_collision_vertices[index],
                    blob + 0x640U + index * 16U);
        runtime->screen_point_usage[index] =
            (s16)read_be16(blob + 0x680U + index * 2U);
    }

    collision = &runtime->collision_data[0].DisplayListCollisions;
    collision->Primary = (Gfx *)(uintptr_t)(
        blob + GE_ORIGINAL_MODEMBOX_PRIMARY_GDL_OFFSET);
    collision->Secondary = NULL;
    collision->Vertices = runtime->vertices;
    collision->numVertices = (s16)read_be16(blob + 0x5e4U);
    collision->numCollisionVertices = (s16)read_be16(blob + 0x5e6U);
    collision->CollisionVertices = runtime->collision_vertices;
    collision->PointUsage = runtime->point_usage;
    collision->ModelType = (s16)read_be16(blob + 0x5f0U);
    collision->RwDataIndex = 0U;
    collision->BaseAddr = (void *)(uintptr_t)blob;
    runtime->switch_data.Switch.Controls = &runtime->nodes[4];
    runtime->switch_data.Switch.RwDataIndex = 2U;
    collision = &runtime->collision_data[1].DisplayListCollisions;
    collision->Primary = (Gfx *)(uintptr_t)(
        blob + GE_ORIGINAL_MODEMBOX_SCREEN_GDL_OFFSET);
    collision->Secondary = NULL;
    collision->Vertices = runtime->screen_vertices;
    collision->numVertices = (s16)read_be16(blob + 0x694U);
    collision->numCollisionVertices = (s16)read_be16(blob + 0x696U);
    collision->CollisionVertices = runtime->screen_collision_vertices;
    collision->PointUsage = runtime->screen_point_usage;
    collision->ModelType = (s16)read_be16(blob + 0x6a0U);
    collision->RwDataIndex = 3U;
    collision->BaseAddr = (void *)(uintptr_t)blob;

    runtime->nodes[0] = (ModelNode){node_opcodes[0], &runtime->group_data,
        NULL, NULL, NULL, &runtime->nodes[1]};
    runtime->nodes[1] = (ModelNode){node_opcodes[1], &runtime->bbox_data,
        &runtime->nodes[0], NULL, NULL, &runtime->nodes[2]};
    runtime->nodes[2] = (ModelNode){node_opcodes[2],
        &runtime->collision_data[0], &runtime->nodes[1], &runtime->nodes[3],
        NULL, NULL};
    runtime->nodes[3] = (ModelNode){node_opcodes[3], &runtime->switch_data,
        &runtime->nodes[1], NULL, &runtime->nodes[2], &runtime->nodes[4]};
    runtime->nodes[4] = (ModelNode){node_opcodes[4],
        &runtime->collision_data[1], &runtime->nodes[3], NULL, NULL, NULL};
    runtime->switch_nodes[0] = &runtime->nodes[4];
    init_skeleton(runtime->joints, &runtime->skeleton);
    runtime->header.RootNode = &runtime->nodes[0];
    runtime->header.Skeleton = &runtime->skeleton;
    runtime->header.Switches = runtime->switch_nodes;
    runtime->header.numSwitches = 1;
    runtime->header.numMatrices = 1;
    runtime->header.BoundingVolumeRadius =
        runtime->group_data.Group.BoundingVolumeRadius;
    runtime->header.numRecords = GE_ORIGINAL_MODEMBOX_RW_WORD_COUNT;
    runtime->header.numtextures = 4;
    runtime->header.Textures = runtime->textures;
    collision_rw = (ModelRwData_DisplayList_CollisionRecord *)(void *)
        &runtime->rwdata_words[0];
    collision_rw->Vertices = runtime->vertices;
    collision_rw->gdl = (Gfx *)(uintptr_t)(
        blob + GE_ORIGINAL_MODEMBOX_PRIMARY_GDL_OFFSET);
    runtime->rwdata_words[2] = 1U;
    collision_rw = (ModelRwData_DisplayList_CollisionRecord *)(void *)
        &runtime->rwdata_words[3];
    collision_rw->Vertices = runtime->screen_vertices;
    collision_rw->gdl = (Gfx *)(uintptr_t)(
        blob + GE_ORIGINAL_MODEMBOX_SCREEN_GDL_OFFSET);
    init_model(&runtime->model, &runtime->header, runtime->render_positions,
               runtime->rwdata_words, GE_ORIGINAL_MODEMBOX_RW_WORD_COUNT);
    return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_OK;
}

GeOriginalDamObjectiveModelsStatus ge_original_satdish_model_relocate(
    GeOriginalSatdishModel *runtime, const void *source_blob,
    size_t source_size)
{
    static const size_t node_offsets[GE_ORIGINAL_SATDISH_NODE_COUNT] = {
        0x18U, 0x30U, 0x48U
    };
    static const uint16_t node_opcodes[GE_ORIGINAL_SATDISH_NODE_COUNT] = {
        MODELNODE_OPCODE_GROUP, MODELNODE_OPCODE_BBOX,
        MODELNODE_OPCODE_DLCOLLISION
    };
    const uint8_t *blob = source_blob;
    ModelRoData_DisplayList_CollisionRecord *collision;
    ModelRwData_DisplayList_CollisionRecord *collision_rw;
    size_t index;

    if (runtime == NULL || blob == NULL)
        return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    if (source_size != GE_ORIGINAL_SATDISH_BLOB_SIZE)
        return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_SIZE;
    for (index = 0U; index < GE_ORIGINAL_SATDISH_NODE_COUNT; index++) {
        if (read_be16(blob + node_offsets[index]) != node_opcodes[index])
            return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_LAYOUT;
    }
    if (!segmented_offset(read_be32(blob + 0x1cU), 0x60U)
            || !segmented_offset(read_be32(blob + 0x2cU), 0x30U)
            || !segmented_offset(read_be32(blob + 0x34U), 0x7cU)
            || !segmented_offset(read_be32(blob + 0x38U), 0x18U)
            || !segmented_offset(read_be32(blob + 0x44U), 0x48U)
            || !segmented_offset(read_be32(blob + 0x4cU), 0x9b0U)
            || !segmented_offset(read_be32(blob + 0x50U), 0x30U)
            || !segmented_offset(read_be32(blob + 0x9b0U), 0x9d0U)
            || !segmented_offset(read_be32(blob + 0x9b4U), 0xad0U)
            || !segmented_offset(read_be32(blob + 0x9b8U), 0x98U)
            || !segmented_offset(read_be32(blob + 0x9c0U), 0x658U)
            || !segmented_offset(read_be32(blob + 0x9c4U), 0x8f8U))
        return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_LAYOUT;

    runtime->source_blob = blob;
    runtime->source_size = source_size;
    for (index = 0U; index < 2U; index++)
        read_texture(&runtime->textures[index], blob + index * 12U);
    read_group(&runtime->group_data, blob + 0x60U);
    read_bbox(&runtime->bbox_data, blob + 0x7cU);
    for (index = 0U; index < GE_ORIGINAL_SATDISH_VERTEX_COUNT; index++) {
        read_vertex(&runtime->vertices[index], blob + 0x98U + index * 16U);
        runtime->point_usage[index] =
            (s16)read_be16(blob + 0x8f8U + index * 2U);
    }
    for (index = 0U;
            index < GE_ORIGINAL_SATDISH_COLLISION_VERTEX_COUNT; index++)
        read_vertex(&runtime->collision_vertices[index],
                    blob + 0x658U + index * 16U);
    collision = &runtime->collision_data.DisplayListCollisions;
    collision->Primary = (Gfx *)(uintptr_t)(
        blob + GE_ORIGINAL_SATDISH_PRIMARY_GDL_OFFSET);
    collision->Secondary = (Gfx *)(uintptr_t)(
        blob + GE_ORIGINAL_SATDISH_SECONDARY_GDL_OFFSET);
    collision->Vertices = runtime->vertices;
    collision->numVertices = (s16)read_be16(blob + 0x9bcU);
    collision->numCollisionVertices = (s16)read_be16(blob + 0x9beU);
    collision->CollisionVertices = runtime->collision_vertices;
    collision->PointUsage = runtime->point_usage;
    collision->ModelType = (s16)read_be16(blob + 0x9c8U);
    collision->RwDataIndex = 0U;
    collision->BaseAddr = (void *)(uintptr_t)blob;
    runtime->nodes[0] = (ModelNode){node_opcodes[0], &runtime->group_data,
        NULL, NULL, NULL, &runtime->nodes[1]};
    runtime->nodes[1] = (ModelNode){node_opcodes[1], &runtime->bbox_data,
        &runtime->nodes[0], NULL, NULL, &runtime->nodes[2]};
    runtime->nodes[2] = (ModelNode){node_opcodes[2],
        &runtime->collision_data, &runtime->nodes[1], NULL, NULL, NULL};
    init_skeleton(runtime->joints, &runtime->skeleton);
    runtime->header.RootNode = &runtime->nodes[0];
    runtime->header.Skeleton = &runtime->skeleton;
    runtime->header.numMatrices = 1;
    runtime->header.BoundingVolumeRadius =
        runtime->group_data.Group.BoundingVolumeRadius;
    runtime->header.numRecords = GE_ORIGINAL_SATDISH_RW_WORD_COUNT;
    runtime->header.numtextures = 2;
    runtime->header.Textures = runtime->textures;
    collision_rw = (ModelRwData_DisplayList_CollisionRecord *)(void *)
        runtime->rwdata_words;
    collision_rw->Vertices = runtime->vertices;
    collision_rw->gdl = (Gfx *)(uintptr_t)(
        blob + GE_ORIGINAL_SATDISH_PRIMARY_GDL_OFFSET);
    init_model(&runtime->model, &runtime->header, runtime->render_positions,
               runtime->rwdata_words, GE_ORIGINAL_SATDISH_RW_WORD_COUNT);
    return GE_ORIGINAL_DAM_OBJECTIVE_MODELS_OK;
}

GeOriginalDamObjectiveModels *ge_original_dam_objective_models_create(
    const void *modembox_blob, size_t modembox_size,
    const void *satdish_blob, size_t satdish_size,
    GeOriginalDamObjectiveModelsStatus *status)
{
    GeOriginalDamObjectiveModels *runtime = malloc(sizeof(*runtime));
    GeOriginalDamObjectiveModelsStatus result;
    if (runtime == NULL) {
        if (status != NULL)
            *status = GE_ORIGINAL_DAM_OBJECTIVE_MODELS_ALLOCATION_FAILED;
        return NULL;
    }
    result = ge_original_modembox_model_relocate(
        &runtime->modembox, modembox_blob, modembox_size);
    if (result == GE_ORIGINAL_DAM_OBJECTIVE_MODELS_OK)
        result = ge_original_satdish_model_relocate(
            &runtime->satdish, satdish_blob, satdish_size);
    if (status != NULL) *status = result;
    if (result != GE_ORIGINAL_DAM_OBJECTIVE_MODELS_OK) {
        free(runtime);
        return NULL;
    }
    return runtime;
}

void ge_original_dam_objective_models_destroy(
    GeOriginalDamObjectiveModels *runtime)
{
    free(runtime);
}

int32_t ge_original_dam_objective_models_model_load(
    void *context, int32_t model_id)
{
    GeOriginalDamObjectiveModels *runtime = context;
    if (runtime == NULL) return 0;
    if (model_id == GE_ORIGINAL_MODEMBOX_MODEL_ID)
        return runtime->modembox.source_blob != NULL;
    if (model_id == GE_ORIGINAL_SATDISH_MODEL_ID)
        return runtime->satdish.source_blob != NULL;
    return 0;
}

int ge_original_dam_objective_models_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale)
{
    GeOriginalDamObjectiveModels *runtime = context;
    if (runtime == NULL || model_header == NULL || model_instance == NULL
            || pitem_scale == NULL)
        return 0;
    if (model_id == GE_ORIGINAL_MODEMBOX_MODEL_ID
            && runtime->modembox.source_blob != NULL) {
        *model_header = &runtime->modembox.header;
        *model_instance = &runtime->modembox.model;
    } else if (model_id == GE_ORIGINAL_SATDISH_MODEL_ID
            && runtime->satdish.source_blob != NULL) {
        *model_header = &runtime->satdish.header;
        *model_instance = &runtime->satdish.model;
    } else {
        return 0;
    }
    *pitem_scale = GE_ORIGINAL_DAM_OBJECTIVE_PITEM_SCALE;
    return 1;
}

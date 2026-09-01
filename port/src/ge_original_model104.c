#include "ge_original_model104.h"

#include <stdlib.h>
#include <string.h>

#define GE_MODEL104_BASE 0x05000000U

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
    return address == GE_MODEL104_BASE + (uint32_t)expected;
}

static void read_vertex(Vertex *vertex, const uint8_t *raw)
{
    vertex->coord.x=(s16)read_be16(raw);
    vertex->coord.y=(s16)read_be16(raw+2U);
    vertex->coord.z=(s16)read_be16(raw+4U);
    vertex->index=(s16)read_be16(raw+6U);
    vertex->s=(s16)read_be16(raw+8U);
    vertex->t=(s16)read_be16(raw+10U);
    vertex->r=raw[12]; vertex->g=raw[13];
    vertex->b=raw[14]; vertex->a=raw[15];
}

GeOriginalModel104Status ge_original_model104_relocate(
    GeOriginalModel104 *runtime, const void *source_blob,
    size_t source_size)
{
    static const size_t node_offsets[3] = {0x0cU,0x24U,0x3cU};
    static const uint16_t node_opcodes[3] = {
        MODELNODE_OPCODE_GROUP, MODELNODE_OPCODE_BBOX,
        MODELNODE_OPCODE_DLCOLLISION};
    const uint8_t *blob = source_blob;
    size_t index;

    if (runtime == NULL || blob == NULL)
        return GE_ORIGINAL_MODEL104_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    if (source_size != GE_ORIGINAL_MODEL104_BLOB_SIZE)
        return GE_ORIGINAL_MODEL104_INVALID_SIZE;
    for (index=0U; index<3U; index++) {
        if (read_be16(blob+node_offsets[index]) != node_opcodes[index])
            return GE_ORIGINAL_MODEL104_INVALID_LAYOUT;
    }
    if (!segmented_offset(read_be32(blob+0x10U),0x54U)
            || !segmented_offset(read_be32(blob+0x20U),0x24U)
            || !segmented_offset(read_be32(blob+0x28U),0x70U)
            || !segmented_offset(read_be32(blob+0x38U),0x3cU)
            || !segmented_offset(read_be32(blob+0x40U),0x118U)
            || !segmented_offset(read_be32(blob+0x11cU),0x150U)
            || !segmented_offset(read_be32(blob+0x120U),0x90U)
            || !segmented_offset(read_be32(blob+0x128U),0xd0U)
            || !segmented_offset(read_be32(blob+0x12cU),0x110U))
        return GE_ORIGINAL_MODEL104_INVALID_LAYOUT;

    runtime->source_blob=blob; runtime->source_size=source_size;
    runtime->textures[0].TextureID=read_be32(blob);
    runtime->textures[0].Width=blob[4]; runtime->textures[0].Height=blob[5];
    runtime->textures[0].MipMapTiles=blob[6]; runtime->textures[0].Type=blob[7];
    runtime->textures[0].RenderDepth=blob[8];
    runtime->textures[0].sflags=blob[9]; runtime->textures[0].tflags=blob[10];

    runtime->group_data.Group.Origin.x=read_bef32(blob+0x54U);
    runtime->group_data.Group.Origin.y=read_bef32(blob+0x58U);
    runtime->group_data.Group.Origin.z=read_bef32(blob+0x5cU);
    runtime->group_data.Group.JointID=read_be16(blob+0x60U);
    runtime->group_data.Group.MatrixID0=(s16)read_be16(blob+0x62U);
    runtime->group_data.Group.MatrixID1=(s16)read_be16(blob+0x64U);
    runtime->group_data.Group.MatrixID2=(s16)read_be16(blob+0x66U);
    runtime->group_data.Group.ChildGroup=NULL;
    runtime->group_data.Group.BoundingVolumeRadius=read_bef32(blob+0x6cU);
    runtime->bbox_data.BoundingBox.ModelNumber=read_be32(blob+0x70U);
    runtime->bbox_data.BoundingBox.Bounds.xmin=read_bef32(blob+0x74U);
    runtime->bbox_data.BoundingBox.Bounds.xmax=read_bef32(blob+0x78U);
    runtime->bbox_data.BoundingBox.Bounds.ymin=read_bef32(blob+0x7cU);
    runtime->bbox_data.BoundingBox.Bounds.ymax=read_bef32(blob+0x80U);
    runtime->bbox_data.BoundingBox.Bounds.zmin=read_bef32(blob+0x84U);
    runtime->bbox_data.BoundingBox.Bounds.zmax=read_bef32(blob+0x88U);
    for (index=0U; index<GE_ORIGINAL_MODEL104_VERTEX_COUNT; index++) {
        read_vertex(&runtime->vertices[index],blob+0x90U+index*16U);
        read_vertex(&runtime->collision_vertices[index],blob+0xd0U+index*16U);
        runtime->point_usage[index]=(s16)read_be16(blob+0x110U+index*2U);
    }
    runtime->collision_data.DisplayListCollisions.Primary=
        (Gfx *)(uintptr_t)(blob+0x138U);
    runtime->collision_data.DisplayListCollisions.Secondary=
        (Gfx *)(uintptr_t)(blob+0x150U);
    runtime->collision_data.DisplayListCollisions.Vertices=runtime->vertices;
    runtime->collision_data.DisplayListCollisions.numVertices=
        (s16)read_be16(blob+0x124U);
    runtime->collision_data.DisplayListCollisions.numCollisionVertices=
        (s16)read_be16(blob+0x126U);
    runtime->collision_data.DisplayListCollisions.CollisionVertices=
        runtime->collision_vertices;
    runtime->collision_data.DisplayListCollisions.PointUsage=runtime->point_usage;
    runtime->collision_data.DisplayListCollisions.ModelType=
        (s16)read_be16(blob+0x130U);
    runtime->collision_data.DisplayListCollisions.RwDataIndex=
        read_be16(blob+0x132U);
    runtime->collision_data.DisplayListCollisions.BaseAddr=(void *)(uintptr_t)blob;

    runtime->nodes[0]=(ModelNode){node_opcodes[0],&runtime->group_data,
        NULL,NULL,NULL,&runtime->nodes[1]};
    runtime->nodes[1]=(ModelNode){node_opcodes[1],&runtime->bbox_data,
        &runtime->nodes[0],NULL,NULL,&runtime->nodes[2]};
    runtime->nodes[2]=(ModelNode){node_opcodes[2],&runtime->collision_data,
        &runtime->nodes[1],NULL,NULL,NULL};
    runtime->joints[0].NodeType=0x15U;
    runtime->joints[0].mtxA=0; runtime->joints[0].mtxB=0;
    runtime->joints[1].NodeType=0x15U;
    runtime->joints[1].mtxA=1; runtime->joints[1].mtxB=1;
    runtime->skeleton.numjoints=2; runtime->skeleton.Joints=runtime->joints;
    runtime->header.RootNode=&runtime->nodes[0];
    runtime->header.Skeleton=&runtime->skeleton;
    runtime->header.numMatrices=1;
    runtime->header.BoundingVolumeRadius=
        runtime->group_data.Group.BoundingVolumeRadius;
    runtime->header.numtextures=1; runtime->header.Textures=runtime->textures;
    runtime->model.rwdatalen=1; runtime->model.obj=&runtime->header;
    runtime->model.render_pos=runtime->render_positions;
    runtime->model.datas=(union ModelRwData **)(void *)runtime->rwdata_words;
    runtime->model.scale=1.0f;
    return GE_ORIGINAL_MODEL104_OK;
}

GeOriginalModel104 *ge_original_model104_create(
    const void *source_blob, size_t source_size,
    GeOriginalModel104Status *status)
{
    GeOriginalModel104 *runtime=malloc(sizeof(*runtime));
    GeOriginalModel104Status result;
    if (runtime==NULL) {
        if (status) *status=GE_ORIGINAL_MODEL104_ALLOCATION_FAILED;
        return NULL;
    }
    result=ge_original_model104_relocate(runtime,source_blob,source_size);
    if (status) *status=result;
    if (result!=GE_ORIGINAL_MODEL104_OK) { free(runtime); return NULL; }
    return runtime;
}

void ge_original_model104_destroy(GeOriginalModel104 *runtime) { free(runtime); }

int32_t ge_original_model104_model_load(void *context, int32_t model_id)
{
    GeOriginalModel104 *runtime=context;
    return runtime!=NULL && runtime->source_blob!=NULL
        && model_id==GE_ORIGINAL_MODEL104_ID;
}

int ge_original_model104_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale)
{
    GeOriginalModel104 *runtime=context;
    if (runtime==NULL || runtime->source_blob==NULL
            || model_id!=GE_ORIGINAL_MODEL104_ID || model_header==NULL
            || model_instance==NULL || pitem_scale==NULL) return 0;
    *model_header=&runtime->header; *model_instance=&runtime->model;
    *pitem_scale=GE_ORIGINAL_MODEL104_PITEM_SCALE;
    return 1;
}

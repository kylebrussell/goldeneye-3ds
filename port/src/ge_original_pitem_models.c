#include "ge_original_pitem_models.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrobjdata.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GE_PITEM_SEGMENT_BASE UINT32_C(0x05000000)
#define GE_PITEM_SERIALIZED_TEXTURE_SIZE 12U
#define GE_PITEM_SERIALIZED_NODE_SIZE 24U
#define GE_PITEM_ASSET_PREFIX "converted/models/pitem/"
#define GE_PITEM_ASSET_SUFFIX ".bin"
#define GE_PITEM_PATH_CAPACITY 160U

extern const u32 ge_original_pitem_model_table_count;

typedef struct GeOriginalPitemCollisionStorage {
    Vertex *vertices;
    Vertex *collision_vertices;
    s16 *point_usage;
    size_t bytes;
    uint32_t primary_offset;
    uint32_t secondary_offset;
    uint32_t vertices_offset;
} GeOriginalPitemCollisionStorage;

typedef struct GeOriginalPitemModelResource {
    int32_t model_id;
    uint8_t *blob;
    size_t blob_size;
    uint32_t *node_offsets;
    uint32_t *rodata_offsets;
    size_t node_count;
    ModelNode *nodes;
    union ModelRoData *rodatas;
    GeOriginalPitemCollisionStorage *collision;
    ModelNode **switches;
    ModelFileTextures *textures;
    ModelFileHeader header;
    float pitem_scale;
    size_t rw_words;
    size_t native_bytes;
    int hit_ready;
} GeOriginalPitemModelResource;

typedef struct GeOriginalPitemModelInstance {
    Model model;
    RenderPosView *render_positions;
    uintptr_t *rwdata;
    size_t bytes;
    GeOriginalPitemModelResource *resource;
    int active;
} GeOriginalPitemModelInstance;

struct GeOriginalPitemModelProvider {
    GeAssetPack *pack;
    GeOriginalPitemModelResource *resources;
    GeOriginalPitemModelInstance *instances;
    size_t resource_capacity;
    size_t resource_count;
    size_t instance_capacity;
    size_t instance_count;
    size_t source_blob_bytes;
    size_t native_resource_bytes;
    size_t native_instance_bytes;
    GeOriginalPitemModelStatus last_status;
    uint16_t last_unsupported_opcode;
    struct GeOriginalPitemModelProvider *registry_next;
};

static GeOriginalPitemModelProvider *ge_pitem_provider_registry;

static int range_valid(size_t size, size_t offset, size_t count,
                       size_t stride);
static int segmented_offset(uint32_t value, size_t size, size_t required,
                            uint32_t *offset);

static uint16_t read_be16(const uint8_t *source)
{
    return (uint16_t)(((uint16_t)source[0] << 8) | source[1]);
}

static uint32_t read_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) | ((uint32_t)source[1] << 16)
        | ((uint32_t)source[2] << 8) | source[3];
}

static float read_bef32(const uint8_t *source)
{
    uint32_t bits = read_be32(source);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int hit_display_list_valid(const GeOriginalPitemModelResource *resource,
                                  uint32_t offset)
{
    size_t cursor = offset;
    size_t command_budget;
    if (resource == NULL || !range_valid(resource->blob_size, cursor, 8U, 1U))
        return 0;
    command_budget = (resource->blob_size - cursor) / 8U;
    while (command_budget-- != 0U) {
        const uint8_t *command = resource->blob + cursor;
        const uint8_t opcode = command[0];
        if (opcode == (uint8_t)G_ENDDL) return 1;
        if (opcode == (uint8_t)G_SETTIMG) {
            uint32_t texture_offset;
            const uint32_t address = read_be32(command + 4U);
            if (!segmented_offset(address, resource->blob_size, 1U,
                                  &texture_offset)
                    || texture_offset < 8U
                    || !range_valid(resource->blob_size,
                                    texture_offset - 8U, 2U, 1U))
                return 0;
        }
        cursor += 8U;
        if (!range_valid(resource->blob_size, cursor, 8U, 1U)) return 0;
    }
    return 0;
}

static int resource_hit_geometry_valid(
    const GeOriginalPitemModelResource *resource)
{
    size_t index;
    int found = 0;
    if (resource == NULL) return 0;
    for (index = 0U; index < resource->node_count; ++index) {
        const uint16_t opcode = resource->nodes[index].Opcode & 0xffU;
        const GeOriginalPitemCollisionStorage *collision;
        if (opcode != MODELNODE_OPCODE_DLCOLLISION
                && opcode != MODELNODE_OPCODE_DL) continue;
        collision = &resource->collision[index];
        if (collision->primary_offset == UINT32_MAX
                || !hit_display_list_valid(resource,
                                           collision->primary_offset)
                || (collision->secondary_offset != UINT32_MAX
                    && !hit_display_list_valid(resource,
                                               collision->secondary_offset)))
            return 0;
        found = 1;
    }
    return found;
}

static int range_valid(size_t size, size_t offset, size_t count,
                       size_t stride)
{
    return stride == 0U || (count <= (SIZE_MAX - offset) / stride
        && offset + count * stride <= size);
}

static int segmented_offset(uint32_t value, size_t size, size_t required,
                            uint32_t *offset)
{
    uint32_t local;
    if ((value & UINT32_C(0xff000000)) != GE_PITEM_SEGMENT_BASE) return 0;
    local = value & UINT32_C(0x00ffffff);
    if (!range_valid(size, local, required, 1U)) return 0;
    if (offset != NULL) *offset = local;
    return 1;
}

static int supported_opcode(uint16_t opcode)
{
    switch (opcode & 0xffU) {
    case MODELNODE_OPCODE_DL:
    case MODELNODE_OPCODE_GROUP:
    case MODELNODE_OPCODE_LOD:
    case MODELNODE_OPCODE_BSP:
    case MODELNODE_OPCODE_BBOX:
    case MODELNODE_OPCODE_SWITCH:
    case MODELNODE_OPCODE_DLCOLLISION:
    case MODELNODE_OPCODE_GUNFIRE:
    case MODELNODE_OPCODE_GROUPSIMPLE:
    case MODELNODE_OPCODE_DLPRIMARY:
        return 1;
    default: return 0;
    }
}

static size_t rodata_serialized_size(uint16_t opcode)
{
    switch (opcode & 0xffU) {
    case MODELNODE_OPCODE_DL: return 0x14U;
    case MODELNODE_OPCODE_GROUP: return 0x1cU;
    case MODELNODE_OPCODE_LOD: return 0x10U;
    case MODELNODE_OPCODE_BSP: return 0x24U;
    case MODELNODE_OPCODE_BBOX: return 0x1cU;
    case MODELNODE_OPCODE_SWITCH: return 0x08U;
    case MODELNODE_OPCODE_DLCOLLISION: return 0x20U;
    case MODELNODE_OPCODE_GUNFIRE: return 0x28U;
    case MODELNODE_OPCODE_GROUPSIMPLE: return 0x14U;
    case MODELNODE_OPCODE_DLPRIMARY: return 0x10U;
    default: return 0U;
    }
}

static size_t find_offset(const uint32_t *offsets, size_t count,
                          uint32_t wanted)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (offsets[index] == wanted) return index;
    }
    return SIZE_MAX;
}

static int append_node_offset(const uint8_t *blob, size_t blob_size,
                              uint32_t **offsets, size_t *count,
                              size_t *capacity, uint32_t offset)
{
    uint16_t opcode;
    uint32_t *next;
    if (find_offset(*offsets, *count, offset) != SIZE_MAX) return 1;
    if (!range_valid(blob_size, offset, GE_PITEM_SERIALIZED_NODE_SIZE, 1U))
        return 0;
    opcode = read_be16(blob + offset);
    if (!supported_opcode(opcode)) return -(int)(opcode & 0xffU);
    if (*count == *capacity) {
        size_t next_capacity = *capacity == 0U ? 8U : *capacity * 2U;
        if (next_capacity < *capacity
                || next_capacity > SIZE_MAX / sizeof(**offsets)) return 0;
        next = realloc(*offsets, next_capacity * sizeof(**offsets));
        if (next == NULL) return 0;
        *offsets = next;
        *capacity = next_capacity;
    }
    (*offsets)[(*count)++] = offset;
    return 1;
}

static GeOriginalPitemModelStatus collect_node_offsets(
    GeOriginalPitemModelProvider *provider,
    GeOriginalPitemModelResource *resource, uint32_t root_offset)
{
    size_t capacity = 0U;
    size_t cursor;
    int result = append_node_offset(resource->blob, resource->blob_size,
        &resource->node_offsets, &resource->node_count, &capacity, root_offset);
    if (result <= 0) {
        if (result < 0) provider->last_unsupported_opcode = (uint16_t)-result;
        return result < 0 ? GE_ORIGINAL_PITEM_MODEL_UNSUPPORTED_OPCODE
                          : GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    }
    for (cursor = 0U; cursor < resource->node_count; ++cursor) {
        const uint8_t *node = resource->blob + resource->node_offsets[cursor];
        size_t field;
        for (field = 8U; field <= 20U; field += 4U) {
            uint32_t address = read_be32(node + field);
            uint32_t offset;
            if (address == 0U) continue;
            if (!segmented_offset(address, resource->blob_size,
                                  GE_PITEM_SERIALIZED_NODE_SIZE, &offset)) {
                return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
            }
            result = append_node_offset(resource->blob, resource->blob_size,
                &resource->node_offsets, &resource->node_count,
                &capacity, offset);
            if (result <= 0) {
                if (result < 0)
                    provider->last_unsupported_opcode = (uint16_t)-result;
                return result < 0
                    ? GE_ORIGINAL_PITEM_MODEL_UNSUPPORTED_OPCODE
                    : GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
            }
        }
    }
    return GE_ORIGINAL_PITEM_MODEL_OK;
}

static ModelNode *node_from_address(GeOriginalPitemModelResource *resource,
                                    uint32_t address)
{
    uint32_t offset;
    size_t index;
    if (address == 0U) return NULL;
    if (!segmented_offset(address, resource->blob_size,
                          GE_PITEM_SERIALIZED_NODE_SIZE, &offset)) return NULL;
    index = find_offset(resource->node_offsets, resource->node_count, offset);
    return index == SIZE_MAX ? NULL : &resource->nodes[index];
}

static void *blob_from_address(GeOriginalPitemModelResource *resource,
                               uint32_t address, size_t required)
{
    uint32_t offset;
    if (address == 0U) return NULL;
    if (!segmented_offset(address, resource->blob_size, required, &offset))
        return NULL;
    return resource->blob + offset;
}

static void read_vertex_visual(Vertex *vertex, const uint8_t *raw)
{
    memset(vertex, 0, sizeof(*vertex));
    vertex->coord.x = (s16)read_be16(raw);
    vertex->coord.y = (s16)read_be16(raw + 2U);
    vertex->coord.z = (s16)read_be16(raw + 4U);
    vertex->index = (s16)read_be16(raw + 6U);
    vertex->s = (s16)read_be16(raw + 8U);
    vertex->t = (s16)read_be16(raw + 10U);
    vertex->r = raw[12]; vertex->g = raw[13];
    vertex->b = raw[14]; vertex->a = raw[15];
}

static GeOriginalPitemModelStatus decode_collision(
    GeOriginalPitemModelResource *resource, size_t node_index,
    const uint8_t *raw)
{
    ModelRoData_DisplayList_CollisionRecord *out =
        &resource->rodatas[node_index].DisplayListCollisions;
    GeOriginalPitemCollisionStorage *storage = &resource->collision[node_index];
    int16_t vertex_count = (int16_t)read_be16(raw + 0x0cU);
    int16_t collision_count = (int16_t)read_be16(raw + 0x0eU);
    uint32_t vertices_offset;
    uint32_t collision_offset;
    uint32_t usage_offset;
    size_t index;
    if (vertex_count < 0 || collision_count < 0
            || !segmented_offset(read_be32(raw + 8U), resource->blob_size,
                                 (size_t)vertex_count * 16U,
                                 &vertices_offset)
            || !segmented_offset(read_be32(raw + 0x10U), resource->blob_size,
                                 (size_t)collision_count * 16U,
                                 &collision_offset)
            || !segmented_offset(read_be32(raw + 0x14U), resource->blob_size,
                                 (size_t)vertex_count * 2U,
                                 &usage_offset)) {
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    }
    storage->vertices = calloc(vertex_count == 0 ? 1U : (size_t)vertex_count,
                               sizeof(*storage->vertices));
    storage->collision_vertices = calloc(
        collision_count == 0 ? 1U : (size_t)collision_count,
        sizeof(*storage->collision_vertices));
    storage->point_usage = calloc(vertex_count == 0 ? 1U : (size_t)vertex_count,
                                  sizeof(*storage->point_usage));
    if (storage->vertices == NULL || storage->collision_vertices == NULL
            || storage->point_usage == NULL) {
        return GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED;
    }
    storage->bytes = (size_t)vertex_count * sizeof(*storage->vertices)
        + (size_t)collision_count * sizeof(*storage->collision_vertices)
        + (size_t)vertex_count * sizeof(*storage->point_usage);
    for (index = 0U; index < (size_t)vertex_count; ++index) {
        read_vertex_visual(&storage->vertices[index],
            resource->blob + vertices_offset + index * 16U);
        storage->point_usage[index] = (s16)read_be16(
            resource->blob + usage_offset + index * 2U);
    }
    for (index = 0U; index < (size_t)collision_count; ++index) {
        const uint8_t *source = resource->blob + collision_offset + index * 16U;
        Vertex *vertex = &storage->collision_vertices[index];
        uint32_t related = read_be32(source + 8U);
        read_vertex_visual(vertex, source);
        vertex->CollisionRelatedNode = node_from_address(resource, related);
        if (related != 0U && vertex->CollisionRelatedNode == NULL)
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        vertex->CollisionRelatedIndex = (s16)read_be16(source + 0x0cU);
        vertex->CollisionReserved = (s16)read_be16(source + 0x0eU);
    }
    out->Primary = blob_from_address(resource, read_be32(raw), sizeof(Gfx));
    out->Secondary = blob_from_address(resource, read_be32(raw + 4U),
                                       sizeof(Gfx));
    if (read_be32(raw) != 0U && out->Primary == NULL) {
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    }
    if (read_be32(raw + 4U) != 0U && out->Secondary == NULL) {
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    }
    out->Vertices = storage->vertices;
    out->numVertices = vertex_count;
    out->numCollisionVertices = collision_count;
    out->CollisionVertices = storage->collision_vertices;
    out->PointUsage = storage->point_usage;
    out->ModelType = (s16)read_be16(raw + 0x18U);
    out->BaseAddr = resource->blob;
    storage->primary_offset = read_be32(raw) & UINT32_C(0x00ffffff);
    storage->secondary_offset = read_be32(raw + 4U) == 0U
        ? UINT32_MAX
        : read_be32(raw + 4U) & UINT32_C(0x00ffffff);
    storage->vertices_offset = vertices_offset;
    return GE_ORIGINAL_PITEM_MODEL_OK;
}

static GeOriginalPitemModelStatus decode_rodata(
    GeOriginalPitemModelResource *resource, size_t index)
{
    const uint8_t *node = resource->blob + resource->node_offsets[index];
    const uint8_t *raw = resource->blob + resource->rodata_offsets[index];
    uint16_t opcode = read_be16(node) & 0xffU;
    switch (opcode) {
    case MODELNODE_OPCODE_DL:
    {
        ModelRoData_DisplayListRecord *out=&resource->rodatas[index].DisplayList;
        GeOriginalPitemCollisionStorage *storage=&resource->collision[index];
        uint16_t vertex_count=read_be16(raw+0x10U);uint32_t vertices_offset;
        if(!segmented_offset(read_be32(raw+0x0cU),resource->blob_size,
                (size_t)vertex_count*16U,&vertices_offset))
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        storage->vertices=calloc(vertex_count==0U?1U:(size_t)vertex_count,
                                 sizeof(*storage->vertices));
        if(storage->vertices==NULL)return GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED;
        for(size_t vertex=0U;vertex<(size_t)vertex_count;++vertex)
            read_vertex_visual(&storage->vertices[vertex],
                resource->blob+vertices_offset+vertex*16U);
        out->Primary=blob_from_address(resource,read_be32(raw),sizeof(Gfx));
        out->Secondary=blob_from_address(resource,read_be32(raw+4U),sizeof(Gfx));
        if((read_be32(raw)!=0U&&out->Primary==NULL)
                ||(read_be32(raw+4U)!=0U&&out->Secondary==NULL))
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        out->BaseAddr=resource->blob;out->Vertices=storage->vertices;
        out->numVertices=vertex_count;out->ModelType=(s8)raw[0x12U];
        storage->primary_offset=read_be32(raw)&UINT32_C(0x00ffffff);
        storage->secondary_offset=read_be32(raw+4U)==0U?UINT32_MAX
            :read_be32(raw+4U)&UINT32_C(0x00ffffff);
        storage->vertices_offset=vertices_offset;
        storage->bytes=(size_t)vertex_count*sizeof(*storage->vertices);
        break;
    }
    case MODELNODE_OPCODE_GROUP:
    {
        ModelRoData_GroupRecord *out = &resource->rodatas[index].Group;
        out->Origin.x = read_bef32(raw); out->Origin.y = read_bef32(raw + 4U);
        out->Origin.z = read_bef32(raw + 8U);
        out->JointID = read_be16(raw + 0x0cU);
        out->MatrixID0 = (s16)read_be16(raw + 0x0eU);
        out->MatrixID1 = (s16)read_be16(raw + 0x10U);
        out->MatrixID2 = (s16)read_be16(raw + 0x12U);
        /* This serialized union is a ModelNode pointer. Articulated models
         * such as the Caverns eyelid and iris use it to name the next joint;
         * treating it as a rodata address rejects their exact PitemZ graph. */
        out->ChildGroupNode = node_from_address(
            resource, read_be32(raw + 0x14U));
        if (read_be32(raw + 0x14U) != 0U && out->ChildGroupNode == NULL)
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        out->BoundingVolumeRadius = read_bef32(raw + 0x18U);
        break;
    }
    case MODELNODE_OPCODE_LOD:
    {
        ModelRoData_LODRecord *out=&resource->rodatas[index].LOD;
        out->MinDistance=read_bef32(raw);out->MaxDistance=read_bef32(raw+4U);
        out->Affects=node_from_address(resource,read_be32(raw+8U));
        if(read_be32(raw+8U)!=0U&&out->Affects==NULL)
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        out->reserved=read_be16(raw+0x0eU);
        break;
    }
    case MODELNODE_OPCODE_BSP:
    {
        ModelRoData_BSPRecord *out = &resource->rodatas[index].BSP;
        out->Point.x = read_bef32(raw); out->Point.y = read_bef32(raw + 4U);
        out->Point.z = read_bef32(raw + 8U);
        out->Vector.x = read_bef32(raw + 0x0cU);
        out->Vector.y = read_bef32(raw + 0x10U);
        out->Vector.z = read_bef32(raw + 0x14U);
        out->leftChild = node_from_address(resource, read_be32(raw + 0x18U));
        out->rightChild = node_from_address(resource, read_be32(raw + 0x1cU));
        if ((read_be32(raw + 0x18U) != 0U && out->leftChild == NULL)
                || (read_be32(raw + 0x1cU) != 0U
                    && out->rightChild == NULL)) {
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        }
        out->reserved = (s16)read_be16(raw + 0x20U);
        break;
    }
    case MODELNODE_OPCODE_BBOX:
    {
        ModelRoData_BoundingBoxRecord *out =
            &resource->rodatas[index].BoundingBox;
        out->ModelNumber = read_be32(raw);
        out->Bounds.xmin = read_bef32(raw + 4U);
        out->Bounds.xmax = read_bef32(raw + 8U);
        out->Bounds.ymin = read_bef32(raw + 0x0cU);
        out->Bounds.ymax = read_bef32(raw + 0x10U);
        out->Bounds.zmin = read_bef32(raw + 0x14U);
        out->Bounds.zmax = read_bef32(raw + 0x18U);
        break;
    }
    case MODELNODE_OPCODE_SWITCH:
    {
        ModelRoData_SwitchRecord *out = &resource->rodatas[index].Switch;
        out->Controls = node_from_address(resource, read_be32(raw));
        if (read_be32(raw) != 0U && out->Controls == NULL)
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        out->reserved = read_be16(raw + 6U);
        break;
    }
    case MODELNODE_OPCODE_GUNFIRE:
    {
        ModelRoData_GunfireRecord *out=&resource->rodatas[index].Gunfire;
        out->Offset.x=read_bef32(raw);out->Offset.y=read_bef32(raw+4U);
        out->Offset.z=read_bef32(raw+8U);out->Size.x=read_bef32(raw+0x0cU);
        out->Size.y=read_bef32(raw+0x10U);out->Size.z=read_bef32(raw+0x14U);
        out->Image=blob_from_address(resource,read_be32(raw+0x18U),1U);
        if(read_be32(raw+0x18U)!=0U&&out->Image==NULL)
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        out->Scale=read_bef32(raw+0x1cU);out->reserved=read_be16(raw+0x22U);
        out->BaseAddr=read_be32(raw+0x24U);
        break;
    }
    case MODELNODE_OPCODE_GROUPSIMPLE:
    {
        ModelRoData_GroupSimpleRecord *out=&resource->rodatas[index].GroupSimple;
        out->Origin.x=read_bef32(raw);out->Origin.y=read_bef32(raw+4U);
        out->Origin.z=read_bef32(raw+8U);out->Group1=(s16)read_be16(raw+0x0cU);
        out->Group2=read_be16(raw+0x0eU);
        out->BoundingVolumeRadius=read_bef32(raw+0x10U);
        break;
    }
    case MODELNODE_OPCODE_DLPRIMARY:
    {
        ModelRoData_DisplayListPrimaryRecord *out=
            &resource->rodatas[index].DisplayListPrimary;
        GeOriginalPitemCollisionStorage *storage=&resource->collision[index];
        uint32_t vertex_count=read_be32(raw),vertices_offset;
        if(vertex_count>(uint32_t)INT_MAX
                ||vertex_count>UINT32_MAX/16U
                ||!segmented_offset(read_be32(raw+4U),resource->blob_size,
                    (size_t)vertex_count*16U,&vertices_offset))
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        storage->vertices=calloc(vertex_count==0U?1U:(size_t)vertex_count,
            sizeof(*storage->vertices));
        if(storage->vertices==NULL)
            return GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED;
        for(size_t vertex=0U;vertex<(size_t)vertex_count;++vertex)
            read_vertex_visual(&storage->vertices[vertex],
                resource->blob+vertices_offset+vertex*16U);
        out->numVertices=(s32)vertex_count;out->Vertices=storage->vertices;
        out->Primary=blob_from_address(resource,read_be32(raw+8U),sizeof(Gfx));
        if(read_be32(raw+8U)!=0U&&out->Primary==NULL)
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        out->BaseAddr=resource->blob;
        storage->primary_offset=read_be32(raw+8U)&UINT32_C(0x00ffffff);
        storage->secondary_offset=UINT32_MAX;
        storage->vertices_offset=vertices_offset;
        storage->bytes=(size_t)vertex_count*sizeof(*storage->vertices);
        break;
    }
    case MODELNODE_OPCODE_DLCOLLISION:
        return decode_collision(resource, index, raw);
    default: return GE_ORIGINAL_PITEM_MODEL_UNSUPPORTED_OPCODE;
    }
    return GE_ORIGINAL_PITEM_MODEL_OK;
}

static void apply_bsp_order(ModelNode *base, int visible)
{
    ModelRoData_BSPRecord *rodata = &base->Data->BSP;
    ModelNode *node1 = visible ? rodata->leftChild : rodata->rightChild;
    ModelNode *node2 = visible ? rodata->rightChild : rodata->leftChild;
    ModelNode *cursor;
    if (node1 != NULL) {
        base->Child = node1;
        node1->Prev = NULL;
        cursor = node1;
        while (cursor->Next != NULL && cursor->Next != node2)
            cursor = cursor->Next;
        cursor->Next = node2;
        if (node2 != NULL) {
            node2->Prev = cursor;
            cursor = node2;
            while (cursor->Next != NULL && cursor->Next != node1)
                cursor = cursor->Next;
            cursor->Next = NULL;
        }
    } else {
        base->Child = node2;
        if (node2 != NULL) node2->Prev = NULL;
    }
}

static GeOriginalPitemModelStatus assign_rw_indexes(
    GeOriginalPitemModelResource *resource)
{
    ModelNode *node = resource->header.RootNode;
    size_t visited = 0U;
    size_t words = 0U;
    while (node != NULL && visited++ <= resource->node_count * 2U) {
        switch (node->Opcode & 0xffU) {
        case MODELNODE_OPCODE_LOD:
            if(words>UINT16_MAX)return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
            node->Data->LOD.RwDataIndex=(u16)words++;
            node->Child=node->Data->LOD.Affects;
            break;
        case MODELNODE_OPCODE_BSP:
            if (words > UINT16_MAX) return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
            node->Data->BSP.RwDataIndex = (u16)words++;
            apply_bsp_order(node, 0);
            break;
        case MODELNODE_OPCODE_SWITCH:
            if (words > UINT16_MAX) return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
            node->Data->Switch.RwDataIndex = (u16)words++;
            node->Child = node->Data->Switch.Controls;
            break;
        case MODELNODE_OPCODE_DLCOLLISION:
            if (words > UINT16_MAX - 2U)
                return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
            node->Data->DisplayListCollisions.RwDataIndex = (u16)words;
            words += 2U;
            break;
        case MODELNODE_OPCODE_GUNFIRE:
            if(words>UINT16_MAX)return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
            node->Data->Gunfire.RwDataIndex=(u16)words++;
            break;
        default: break;
        }
        if (node->Child != NULL) {
            node = node->Child;
        } else {
            while (node != NULL && node->Next == NULL) node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
    if (node != NULL || visited == 0U || words > INT16_MAX)
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    resource->rw_words = words;
    resource->header.numRecords = (s16)words;
    return GE_ORIGINAL_PITEM_MODEL_OK;
}

static void free_resource(GeOriginalPitemModelResource *resource)
{
    size_t index;
    if (resource == NULL) return;
    if (resource->collision != NULL) {
        for (index = 0U; index < resource->node_count; ++index) {
            free(resource->collision[index].vertices);
            free(resource->collision[index].collision_vertices);
            free(resource->collision[index].point_usage);
        }
    }
    free(resource->blob); free(resource->node_offsets);
    free(resource->rodata_offsets); free(resource->nodes);
    free(resource->rodatas); free(resource->collision);
    free(resource->switches); free(resource->textures);
    memset(resource, 0, sizeof(*resource));
}

static GeOriginalPitemModelStatus relocate_resource(
    GeOriginalPitemModelProvider *provider,
    GeOriginalPitemModelResource *resource,
    const ItemModelFileRecord *item)
{
    const ModelFileHeader *metadata = item->header;
    size_t switch_count;
    size_t texture_count;
    size_t root_offset;
    size_t index;
    GeOriginalPitemModelStatus status;
    if (metadata == NULL || metadata->numSwitches < 0
            || metadata->numMatrices <= 0 || metadata->numtextures < 0)
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    switch_count = (size_t)metadata->numSwitches;
    texture_count = (size_t)metadata->numtextures;
    if (switch_count > (SIZE_MAX - texture_count
                        * GE_PITEM_SERIALIZED_TEXTURE_SIZE) / 4U)
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    root_offset = switch_count * 4U
        + texture_count * GE_PITEM_SERIALIZED_TEXTURE_SIZE;
    if (!range_valid(resource->blob_size, root_offset,
                     GE_PITEM_SERIALIZED_NODE_SIZE, 1U))
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    status = collect_node_offsets(provider, resource, (uint32_t)root_offset);
    if (status != GE_ORIGINAL_PITEM_MODEL_OK) return status;
    resource->rodata_offsets = calloc(resource->node_count,
                                      sizeof(*resource->rodata_offsets));
    resource->nodes = calloc(resource->node_count, sizeof(*resource->nodes));
    resource->rodatas = calloc(resource->node_count, sizeof(*resource->rodatas));
    resource->collision = calloc(resource->node_count,
                                 sizeof(*resource->collision));
    resource->switches = calloc(switch_count == 0U ? 1U : switch_count,
                                sizeof(*resource->switches));
    resource->textures = calloc(texture_count == 0U ? 1U : texture_count,
                                sizeof(*resource->textures));
    if (resource->rodata_offsets == NULL || resource->nodes == NULL
            || resource->rodatas == NULL || resource->collision == NULL
            || resource->switches == NULL || resource->textures == NULL)
        return GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED;
    for (index = 0U; index < resource->node_count; ++index) {
        const uint8_t *raw = resource->blob + resource->node_offsets[index];
        uint16_t opcode = read_be16(raw);
        uint32_t data_offset;
        if (!segmented_offset(read_be32(raw + 4U), resource->blob_size,
                              rodata_serialized_size(opcode), &data_offset))
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
        resource->rodata_offsets[index] = data_offset;
        resource->nodes[index].Opcode = opcode;
        resource->nodes[index].Data = &resource->rodatas[index];
    }
    for (index = 0U; index < resource->node_count; ++index) {
        const uint8_t *raw = resource->blob + resource->node_offsets[index];
        ModelNode *out = &resource->nodes[index];
        out->Parent = node_from_address(resource, read_be32(raw + 8U));
        out->Next = node_from_address(resource, read_be32(raw + 12U));
        out->Prev = node_from_address(resource, read_be32(raw + 16U));
        out->Child = node_from_address(resource, read_be32(raw + 20U));
        if ((read_be32(raw + 8U) != 0U && out->Parent == NULL)
                || (read_be32(raw + 12U) != 0U && out->Next == NULL)
                || (read_be32(raw + 16U) != 0U && out->Prev == NULL)
                || (read_be32(raw + 20U) != 0U && out->Child == NULL))
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    }
    for (index = 0U; index < texture_count; ++index) {
        const uint8_t *raw = resource->blob + switch_count * 4U
            + index * GE_PITEM_SERIALIZED_TEXTURE_SIZE;
        ModelFileTextures *texture = &resource->textures[index];
        texture->TextureID = read_be32(raw);
        texture->Width = raw[4]; texture->Height = raw[5];
        texture->MipMapTiles = raw[6]; texture->Type = raw[7];
        texture->RenderDepth = raw[8]; texture->sflags = raw[9];
        texture->tflags = raw[10];
    }
    for (index = 0U; index < switch_count; ++index) {
        uint32_t address = read_be32(resource->blob + index * 4U);
        resource->switches[index] = node_from_address(resource, address);
        if (address != 0U && resource->switches[index] == NULL)
            return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    }
    for (index = 0U; index < resource->node_count; ++index) {
        status = decode_rodata(resource, index);
        if (status != GE_ORIGINAL_PITEM_MODEL_OK) return status;
    }
    resource->header = *metadata;
    resource->header.RootNode = &resource->nodes[0];
    resource->header.Switches = resource->switches;
    resource->header.Textures = resource->textures;
#if !defined(VERSION_EU)
    resource->header.isLoaded = 1;
#endif
    status = assign_rw_indexes(resource);
    if (status != GE_ORIGINAL_PITEM_MODEL_OK) return status;
    resource->hit_ready = resource_hit_geometry_valid(resource);
    resource->native_bytes = resource->node_count
            * (sizeof(*resource->node_offsets)
               + sizeof(*resource->rodata_offsets)
               + sizeof(*resource->nodes) + sizeof(*resource->rodatas)
               + sizeof(*resource->collision))
        + (switch_count == 0U ? 1U : switch_count)
            * sizeof(*resource->switches)
        + (texture_count == 0U ? 1U : texture_count)
            * sizeof(*resource->textures);
    for (index = 0U; index < resource->node_count; ++index)
        resource->native_bytes += resource->collision[index].bytes;
    return GE_ORIGINAL_PITEM_MODEL_OK;
}

static GeOriginalPitemModelResource *find_resource(
    GeOriginalPitemModelProvider *provider, int32_t model_id)
{
    size_t index;
    for (index = 0U; index < provider->resource_count; ++index) {
        if (provider->resources[index].model_id == model_id)
            return &provider->resources[index];
    }
    return NULL;
}

static GeOriginalPitemModelStatus load_resource(
    GeOriginalPitemModelProvider *provider, int32_t model_id,
    GeOriginalPitemModelResource **result)
{
    const ItemModelFileRecord *item;
    const GeAssetPackEntry *entry;
    GeOriginalPitemModelResource next;
    GeOriginalPitemModelStatus status;
    char path[GE_PITEM_PATH_CAPACITY];
    int length;
    if (provider == NULL || provider->pack == NULL) {
        return GE_ORIGINAL_PITEM_MODEL_INVALID_ARGUMENT;
    }
    if (model_id < 0 || (u32)model_id >= ge_original_pitem_model_table_count)
        return GE_ORIGINAL_PITEM_MODEL_INVALID_ID;
    *result = find_resource(provider, model_id);
    if (*result != NULL) return GE_ORIGINAL_PITEM_MODEL_OK;
    if (provider->resource_count >= provider->resource_capacity)
        return GE_ORIGINAL_PITEM_MODEL_CAPACITY_EXHAUSTED;
    item = &PitemZ_entries[model_id];
    if (item->header == NULL || item->filename == NULL || item->scale <= 0.0f)
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    length = snprintf(path, sizeof(path), "%s%s%s", GE_PITEM_ASSET_PREFIX,
                      item->filename, GE_PITEM_ASSET_SUFFIX);
    if (length < 0 || (size_t)length >= sizeof(path))
        return GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT;
    entry = ge_asset_pack_find(provider->pack, path);
    if (entry == NULL || entry->data_size == 0U || entry->data_size > SIZE_MAX)
        return GE_ORIGINAL_PITEM_MODEL_NOT_FOUND;
    memset(&next, 0, sizeof(next));
    next.model_id = model_id;
    next.blob_size = (size_t)entry->data_size;
    next.pitem_scale = item->scale;
    next.blob = malloc(next.blob_size);
    if (next.blob == NULL) return GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED;
    if (ge_asset_pack_read(provider->pack, path, next.blob, next.blob_size,
                           NULL) != GE_ASSET_PACK_OK) {
        free_resource(&next);
        return GE_ORIGINAL_PITEM_MODEL_NOT_FOUND;
    }
    status = relocate_resource(provider, &next, item);
    if (status != GE_ORIGINAL_PITEM_MODEL_OK) {
        free_resource(&next);
        return status;
    }
    provider->resources[provider->resource_count] = next;
    *result = &provider->resources[provider->resource_count++];
    provider->source_blob_bytes += next.blob_size;
    provider->native_resource_bytes += next.native_bytes;
    return GE_ORIGINAL_PITEM_MODEL_OK;
}

static void init_instance_rw(GeOriginalPitemModelInstance *instance,
                             GeOriginalPitemModelResource *resource)
{
    ModelNode *node = resource->header.RootNode;
    size_t visited = 0U;
    while (node != NULL && visited++ <= resource->node_count * 2U) {
        size_t rw_index;
        switch (node->Opcode & 0xffU) {
        case MODELNODE_OPCODE_LOD:
            rw_index=node->Data->LOD.RwDataIndex;
            ((ModelRwData_LODRecord *)&instance->rwdata[rw_index])->visible=FALSE;
            node->Child=node->Data->LOD.Affects;
            break;
        case MODELNODE_OPCODE_BSP:
            rw_index = node->Data->BSP.RwDataIndex;
            ((ModelRwData_BSPRecord *)&instance->rwdata[rw_index])->visible = 0;
            apply_bsp_order(node, 0);
            break;
        case MODELNODE_OPCODE_SWITCH:
            rw_index = node->Data->Switch.RwDataIndex;
            ((ModelRwData_SwitchRecord *)&instance->rwdata[rw_index])->visible = 1;
            node->Child = node->Data->Switch.Controls;
            break;
        case MODELNODE_OPCODE_DLCOLLISION:
        {
            ModelRwData_DisplayList_CollisionRecord *rw;
            rw_index = node->Data->DisplayListCollisions.RwDataIndex;
            rw = (ModelRwData_DisplayList_CollisionRecord *)
                &instance->rwdata[rw_index];
            rw->Vertices = node->Data->DisplayListCollisions.Vertices;
            rw->gdl = node->Data->DisplayListCollisions.Primary;
            break;
        }
        case MODELNODE_OPCODE_GUNFIRE:
            rw_index=node->Data->Gunfire.RwDataIndex;
            ((ModelRwData_GunfireRecord *)&instance->rwdata[rw_index])->visible=0;
            break;
        default: break;
        }
        if (node->Child != NULL) node = node->Child;
        else {
            while (node != NULL && node->Next == NULL) node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
}

GeOriginalPitemModelProvider *ge_original_pitem_model_provider_create(
    GeAssetPack *pack, size_t model_capacity, size_t instance_capacity,
    GeOriginalPitemModelStatus *status)
{
    GeOriginalPitemModelProvider *provider;
    GeOriginalPitemModelStatus result = GE_ORIGINAL_PITEM_MODEL_OK;
    if (pack == NULL || pack->file == NULL || model_capacity == 0U
            || instance_capacity == 0U
            || model_capacity > SIZE_MAX / sizeof(*provider->resources)
            || instance_capacity > SIZE_MAX / sizeof(*provider->instances)) {
        result = GE_ORIGINAL_PITEM_MODEL_INVALID_ARGUMENT;
        if (status != NULL) *status = result;
        return NULL;
    }
    provider = calloc(1U, sizeof(*provider));
    if (provider != NULL) {
        provider->resources = calloc(model_capacity, sizeof(*provider->resources));
        provider->instances = calloc(instance_capacity, sizeof(*provider->instances));
    }
    if (provider == NULL || provider->resources == NULL
            || provider->instances == NULL) {
        if (provider != NULL) {
            free(provider->resources); free(provider->instances); free(provider);
        }
        result = GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED;
        if (status != NULL) *status = result;
        return NULL;
    }
    provider->pack = pack;
    provider->resource_capacity = model_capacity;
    provider->instance_capacity = instance_capacity;
    provider->last_status = GE_ORIGINAL_PITEM_MODEL_OK;
    provider->registry_next = ge_pitem_provider_registry;
    ge_pitem_provider_registry = provider;
    if (status != NULL) *status = result;
    return provider;
}

void ge_original_pitem_model_provider_destroy(
    GeOriginalPitemModelProvider *provider)
{
    size_t index;
    GeOriginalPitemModelProvider **link;
    if (provider == NULL) return;
    for (link = &ge_pitem_provider_registry; *link != NULL;
            link = &(*link)->registry_next) {
        if (*link == provider) {
            *link = provider->registry_next;
            break;
        }
    }
    for (index = 0U; index < provider->resource_count; ++index)
        free_resource(&provider->resources[index]);
    for (index = 0U; index < provider->instance_capacity; ++index) {
        free(provider->instances[index].render_positions);
        free(provider->instances[index].rwdata);
    }
    free(provider->resources); free(provider->instances); free(provider);
}

int32_t ge_original_pitem_model_load(void *context, int32_t model_id)
{
    GeOriginalPitemModelProvider *provider = context;
    GeOriginalPitemModelResource *resource = NULL;
    GeOriginalPitemModelStatus status = load_resource(provider, model_id,
                                                       &resource);
    if (provider != NULL) provider->last_status = status;
    return status == GE_ORIGINAL_PITEM_MODEL_OK && resource != NULL;
}

int ge_original_pitem_model_available(void *context, int32_t model_id)
{
    return ge_original_pitem_model_load(context, model_id) != 0;
}

int ge_original_pitem_model_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale)
{
    GeOriginalPitemModelProvider *provider = context;
    GeOriginalPitemModelResource *resource = NULL;
    GeOriginalPitemModelInstance *instance;
    GeOriginalPitemModelStatus status;
    size_t matrix_count;
    size_t rw_count;
    size_t slot;
    if (provider == NULL || model_header == NULL || model_instance == NULL
            || pitem_scale == NULL) {
        if (provider != NULL)
            provider->last_status = GE_ORIGINAL_PITEM_MODEL_INVALID_ARGUMENT;
        return 0;
    }
    status = load_resource(provider, model_id, &resource);
    if (status != GE_ORIGINAL_PITEM_MODEL_OK) {
        provider->last_status = status;
        return 0;
    }
    if (provider->instance_count >= provider->instance_capacity) {
        provider->last_status = GE_ORIGINAL_PITEM_MODEL_CAPACITY_EXHAUSTED;
        return 0;
    }
    for (slot = 0U; slot < provider->instance_capacity; ++slot)
        if (!provider->instances[slot].active) break;
    if (slot == provider->instance_capacity) {
        provider->last_status = GE_ORIGINAL_PITEM_MODEL_CAPACITY_EXHAUSTED;
        return 0;
    }
    instance = &provider->instances[slot];
    matrix_count = (size_t)resource->header.numMatrices;
    rw_count = resource->rw_words == 0U ? 1U : resource->rw_words;
    instance->render_positions = calloc(matrix_count,
                                        sizeof(*instance->render_positions));
    instance->rwdata = calloc(rw_count, sizeof(*instance->rwdata));
    if (instance->render_positions == NULL || instance->rwdata == NULL) {
        free(instance->render_positions); free(instance->rwdata);
        memset(instance, 0, sizeof(*instance));
        provider->last_status = GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED;
        return 0;
    }
    instance->model.rwdatalen = (s16)resource->rw_words;
    instance->model.obj = &resource->header;
    instance->model.render_pos = instance->render_positions;
    instance->model.datas = (union ModelRwData **)(void *)instance->rwdata;
    instance->model.scale = 1.0f;
    init_instance_rw(instance, resource);
    instance->bytes = matrix_count * sizeof(*instance->render_positions)
        + rw_count * sizeof(*instance->rwdata);
    instance->resource = resource;
    instance->active = 1;
    provider->native_instance_bytes += instance->bytes;
    ++provider->instance_count;
    *model_header = &resource->header;
    *model_instance = &instance->model;
    *pitem_scale = resource->pitem_scale;
    provider->last_status = GE_ORIGINAL_PITEM_MODEL_OK;
    return 1;
}

int ge_original_pitem_model_release_instance(
    GeOriginalPitemModelProvider *provider, void *model_instance)
{
    size_t index;
    if (provider == NULL || model_instance == NULL) return 0;
    for (index = 0U; index < provider->instance_capacity; ++index) {
        GeOriginalPitemModelInstance *instance = &provider->instances[index];
        if (!instance->active || &instance->model != model_instance) continue;
        if (provider->native_instance_bytes < instance->bytes
                || provider->instance_count == 0U) return 0;
        provider->native_instance_bytes -= instance->bytes;
        --provider->instance_count;
        free(instance->render_positions);
        free(instance->rwdata);
        memset(instance, 0, sizeof(*instance));
        return 1;
    }
    return 0;
}

size_t ge_original_pitem_model_scene_part_count(
    const GeOriginalPitemModelProvider *provider, int32_t model_id)
{
    size_t resource_index;
    if (provider == NULL) return 0U;
    for (resource_index = 0U;
            resource_index < provider->resource_count; ++resource_index) {
        const GeOriginalPitemModelResource *resource =
            &provider->resources[resource_index];
        size_t node_index;
        size_t count = 0U;
        if (resource->model_id != model_id) continue;
        for (node_index = 0U; node_index < resource->node_count; ++node_index)
            count += ((resource->nodes[node_index].Opcode & 0xffU)
                    == MODELNODE_OPCODE_DLCOLLISION
                ||(resource->nodes[node_index].Opcode & 0xffU)
                    == MODELNODE_OPCODE_DL);
        return count;
    }
    return 0U;
}

int ge_original_pitem_model_scene_part(
    const GeOriginalPitemModelProvider *provider, int32_t model_id,
    size_t part_index, GeOriginalPitemModelScenePart *part)
{
    size_t resource_index;
    if (provider == NULL || part == NULL) return 0;
    for (resource_index = 0U;
            resource_index < provider->resource_count; ++resource_index) {
        const GeOriginalPitemModelResource *resource =
            &provider->resources[resource_index];
        size_t node_index;
        size_t cursor = 0U;
        if (resource->model_id != model_id) continue;
        for (node_index = 0U; node_index < resource->node_count; ++node_index) {
            const GeOriginalPitemCollisionStorage *collision;
            uint16_t opcode=resource->nodes[node_index].Opcode&0xffU;
            if(opcode!=MODELNODE_OPCODE_DLCOLLISION
                    &&opcode!=MODELNODE_OPCODE_DL)continue;
            if (cursor++ != part_index) continue;
            collision = &resource->collision[node_index];
            part->node = &resource->nodes[node_index];
            part->blob = resource->blob;
            part->blob_size = resource->blob_size;
            part->primary_offset = collision->primary_offset;
            part->secondary_offset = collision->secondary_offset;
            part->segment4_offset = collision->vertices_offset;
            part->model_type = opcode == MODELNODE_OPCODE_DLCOLLISION
                ? resource->nodes[node_index].Data
                    ->DisplayListCollisions.ModelType
                : resource->nodes[node_index].Data->DisplayList.ModelType;
            part->vertex_count = opcode == MODELNODE_OPCODE_DLCOLLISION
                    && resource->nodes[node_index].Data
                        ->DisplayListCollisions.numVertices > 0
                ? (uint16_t)resource->nodes[node_index].Data
                    ->DisplayListCollisions.numVertices : 0U;
            {
                const ModelNode *ancestor =
                    resource->nodes[node_index].Parent;
                part->matrix_index = 0U;
                while (ancestor != NULL) {
                    if ((ancestor->Opcode & 0xffU)
                            == MODELNODE_OPCODE_GROUP) {
                        if (ancestor->Data->Group.MatrixID0 >= 0)
                            part->matrix_index = (uint16_t)
                                ancestor->Data->Group.MatrixID0;
                        break;
                    }
                    ancestor = ancestor->Parent;
                }
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

int ge_original_pitem_model_scene_part_for_node(
    const GeOriginalPitemModelProvider *provider, int32_t model_id,
    const void *node, size_t *part_index,
    GeOriginalPitemModelScenePart *part)
{
    size_t resource_index;
    if (provider == NULL || node == NULL || part_index == NULL || part == NULL)
        return 0;
    for (resource_index = 0U;
            resource_index < provider->resource_count; ++resource_index) {
        const GeOriginalPitemModelResource *resource =
            &provider->resources[resource_index];
        size_t node_index;
        size_t cursor = 0U;
        if (resource->model_id != model_id) continue;
        for (node_index = 0U; node_index < resource->node_count; ++node_index) {
            uint16_t opcode = resource->nodes[node_index].Opcode & 0xffU;
            if (opcode != MODELNODE_OPCODE_DLCOLLISION
                    && opcode != MODELNODE_OPCODE_DL) continue;
            if ((const void *)&resource->nodes[node_index] == node) {
                *part_index = cursor;
                return ge_original_pitem_model_scene_part(
                    provider, model_id, cursor, part);
            }
            ++cursor;
        }
        return 0;
    }
    return 0;
}

static GeOriginalPitemModelInstance *find_instance(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance)
{
    size_t index;
    if (provider == NULL || model_instance == NULL) return NULL;
    for (index = 0U; index < provider->instance_capacity; ++index) {
        GeOriginalPitemModelInstance *instance =
            &provider->instances[index];
        if (instance->active && &instance->model == model_instance)
            return instance;
    }
    return NULL;
}

static union ModelRwData *instance_rw_at(
    GeOriginalPitemModelInstance *instance, size_t index)
{
    return (union ModelRwData *)(void *)&instance->rwdata[index];
}

static void apply_instance_relations(
    GeOriginalPitemModelInstance *instance)
{
    GeOriginalPitemModelResource *resource = instance->resource;
    size_t index;
    for (index = 0U; index < resource->node_count; ++index) {
        ModelNode *node = &resource->nodes[index];
        union ModelRwData *rw;
        switch (node->Opcode & 0xffU) {
        case MODELNODE_OPCODE_LOD:
            rw = instance_rw_at(instance, node->Data->LOD.RwDataIndex);
            node->Child = rw->LOD.visible
                ? node->Data->LOD.Affects : NULL;
            break;
        case MODELNODE_OPCODE_BSP:
            rw = instance_rw_at(instance, node->Data->BSP.RwDataIndex);
            apply_bsp_order(node, rw->BSP.visible != 0);
            break;
        case MODELNODE_OPCODE_SWITCH:
            rw = instance_rw_at(instance, node->Data->Switch.RwDataIndex);
            node->Child = rw->Switch.visible
                ? node->Data->Switch.Controls : NULL;
            break;
        default:
            break;
        }
    }
}

static int resource_part_for_node(
    const GeOriginalPitemModelResource *resource,
    const ModelNode *node, GeOriginalPitemModelScenePart *part)
{
    size_t node_index;
    if (resource == NULL || node == NULL) return 0;
    for (node_index = 0U; node_index < resource->node_count; ++node_index) {
        const GeOriginalPitemCollisionStorage *collision;
        const ModelNode *candidate = &resource->nodes[node_index];
        uint16_t opcode;
        if (candidate != node) continue;
        opcode = candidate->Opcode & 0xffU;
        if (opcode != MODELNODE_OPCODE_DLCOLLISION
                && opcode != MODELNODE_OPCODE_DL) return 0;
        if (part == NULL) return 1;
        collision = &resource->collision[node_index];
        part->node = candidate;
        part->blob = resource->blob;
        part->blob_size = resource->blob_size;
        part->primary_offset = collision->primary_offset;
        part->secondary_offset = collision->secondary_offset;
        part->segment4_offset = collision->vertices_offset;
        part->model_type = opcode == MODELNODE_OPCODE_DLCOLLISION
            ? candidate->Data->DisplayListCollisions.ModelType
            : candidate->Data->DisplayList.ModelType;
        part->vertex_count = opcode == MODELNODE_OPCODE_DLCOLLISION
                && candidate->Data->DisplayListCollisions.numVertices > 0
            ? (uint16_t)candidate->Data
                ->DisplayListCollisions.numVertices : 0U;
        part->matrix_index = 0U;
        for (candidate = candidate->Parent; candidate != NULL;
                candidate = candidate->Parent) {
            if ((candidate->Opcode & 0xffU) == MODELNODE_OPCODE_GROUP) {
                if (candidate->Data->Group.MatrixID0 >= 0)
                    part->matrix_index = (uint16_t)
                        candidate->Data->Group.MatrixID0;
                break;
            }
        }
        return 1;
    }
    return 0;
}

static size_t instance_scene_part(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance, size_t wanted,
    GeOriginalPitemModelScenePart *part)
{
    GeOriginalPitemModelInstance *instance =
        find_instance(provider, model_instance);
    ModelNode *node;
    size_t count = 0U;
    size_t visited = 0U;
    size_t limit;
    if (instance == NULL || instance->resource == NULL
            || instance->model.obj == NULL) return 0U;
    apply_instance_relations(instance);
    node = instance->model.obj->RootNode;
    limit = instance->resource->node_count + 1U;
    while (node != NULL && visited++ < limit) {
        if (resource_part_for_node(instance->resource, node, NULL)) {
            if (count == wanted) {
                if (part != NULL)
                    (void)resource_part_for_node(
                        instance->resource, node, part);
                return count + 1U;
            }
            ++count;
        }
        if (node->Child != NULL) node = node->Child;
        else {
            while (node != NULL && node->Next == NULL)
                node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
    return node == NULL ? count : 0U;
}

size_t ge_original_pitem_model_instance_scene_part_count(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance)
{
    return instance_scene_part(provider, model_instance, SIZE_MAX, NULL);
}

int ge_original_pitem_model_instance_scene_part(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance, size_t part_index,
    GeOriginalPitemModelScenePart *part)
{
    return part != NULL && instance_scene_part(
        provider, model_instance, part_index, part) == part_index + 1U;
}

static int node_matrix_index(const ModelNode *node, uint16_t *index)
{
    for (; node != NULL; node = node->Parent) {
        switch (node->Opcode & 0xffU) {
        case MODELNODE_OPCODE_GROUP:
            if (node->Data->Group.MatrixID0 < 0) return 0;
            *index = (uint16_t)node->Data->Group.MatrixID0; return 1;
        case MODELNODE_OPCODE_GROUPSIMPLE:
            if (node->Data->GroupSimple.Group1 < 0) return 0;
            *index = (uint16_t)node->Data->GroupSimple.Group1; return 1;
        default: break;
        }
    }
    return 0;
}

static size_t instance_gunfire(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance, size_t wanted,
    GeOriginalPitemModelGunfire *gunfire)
{
    GeOriginalPitemModelInstance *instance =
        find_instance(provider, model_instance);
    size_t node_index, count = 0U;
    if (instance == NULL || instance->resource == NULL) return 0U;
    for (node_index = 0U; node_index < instance->resource->node_count;
            ++node_index) {
        ModelNode *node = &instance->resource->nodes[node_index];
        ModelRoData_GunfireRecord *rodata;
        const uint8_t *image;
        uintptr_t image_address, blob_address;
        uint16_t matrix_index;
        if ((node->Opcode & 0xffU) != MODELNODE_OPCODE_GUNFIRE) continue;
        if (count++ != wanted) continue;
        if (gunfire == NULL) return count;
        rodata = &node->Data->Gunfire;
        image = (const uint8_t *)rodata->Image;
        image_address = (uintptr_t)image;
        blob_address = (uintptr_t)instance->resource->blob;
        if (instance->resource->blob_size < 12U || image == NULL
                || image_address < blob_address
                || image_address - blob_address
                    > instance->resource->blob_size - 12U
                || !node_matrix_index(node, &matrix_index)) return 0U;
        memset(gunfire, 0, sizeof(*gunfire));
        memcpy(gunfire->offset, rodata->Offset.f, sizeof(gunfire->offset));
        memcpy(gunfire->size, rodata->Size.f, sizeof(gunfire->size));
        gunfire->scale = rodata->Scale;
        gunfire->image_id = (uint16_t)read_be32(image);
        gunfire->image_width = image[4]; gunfire->image_height = image[5];
        gunfire->image_format = image[7]; gunfire->image_depth = image[8];
        gunfire->flags_s = image[9]; gunfire->flags_t = image[10];
        gunfire->matrix_index = matrix_index;
        gunfire->visible = (uint8_t)(instance_rw_at(instance,
            rodata->RwDataIndex)->Gunfire.visible != 0);
        return count;
    }
    return wanted == SIZE_MAX ? count : 0U;
}

size_t ge_original_pitem_model_instance_gunfire_count(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance)
{
    return instance_gunfire(provider, model_instance, SIZE_MAX, NULL);
}

int ge_original_pitem_model_instance_gunfire(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance, size_t gunfire_index,
    GeOriginalPitemModelGunfire *gunfire)
{
    return gunfire != NULL && instance_gunfire(provider, model_instance,
        gunfire_index, gunfire) == gunfire_index + 1U;
}

int ge_original_pitem_model_visit_texture_ids(
    const GeOriginalPitemModelProvider *provider, int32_t model_id,
    void *context, int (*visitor)(void *context, uint16_t image_id))
{
    if (provider == NULL || visitor == NULL) return 0;
    for (size_t resource_index = 0U; resource_index < provider->resource_count;
            ++resource_index) {
        const GeOriginalPitemModelResource *resource = &provider->resources[resource_index];
        if (resource->model_id != model_id) continue;
        if (resource->header.numtextures < 0
                || resource->header.Textures != resource->textures) return 0;
        /* Validate the whole dependency table before publishing any ID. */
        for (size_t i = 0U; i < (size_t)resource->header.numtextures; ++i) {
            const uint32_t id = resource->textures[i].TextureID;
            if (id > UINT16_MAX && ((id >> 24U) != 5U
                    || (id & UINT32_C(0x00ffffff)) >= resource->blob_size))
                return 0;
        }
        for (size_t i = 0U; i < (size_t)resource->header.numtextures; ++i) {
            const uint32_t id = resource->textures[i].TextureID;
            if (id <= UINT16_MAX && !visitor(context, (uint16_t)id)) return 0;
        }
        return 1;
    }
    return 0;
}

int ge_original_pitem_model_embedded_texture(
    const GeOriginalPitemModelProvider *provider, int32_t model_id,
    uint32_t segmented_address,
    GeOriginalPitemEmbeddedTexture *texture)
{
    size_t resource_index;
    if (provider == NULL || texture == NULL
            || (segmented_address >> 24U) != 5U) return 0;
    for (resource_index = 0U;
            resource_index < provider->resource_count; ++resource_index) {
        const GeOriginalPitemModelResource *resource =
            &provider->resources[resource_index];
        size_t texture_index;
        if (resource->model_id != model_id) continue;
        for (texture_index = 0U;
                texture_index < (size_t)resource->header.numtextures;
                ++texture_index) {
            const ModelFileTextures *entry =
                &resource->textures[texture_index];
            const uint32_t address = (uint32_t)(uintptr_t)entry->TextureID;
            const uint32_t offset = address & UINT32_C(0x00ffffff);
            if (address != segmented_address || offset >= resource->blob_size)
                continue;
            memset(texture, 0, sizeof(*texture));
            texture->pixels = resource->blob + offset;
            texture->available_bytes = resource->blob_size - offset;
            texture->segmented_address = address;
            texture->width = entry->Width;
            texture->height = entry->Height;
            texture->render_depth = entry->RenderDepth;
            texture->flags_s = entry->sflags;
            texture->flags_t = entry->tflags;
            return texture->width != 0U && texture->height != 0U;
        }
        return 0;
    }
    return 0;
}

int ge_original_pitem_model_instance_disable_switches(
    GeOriginalPitemModelProvider *provider, void *model_instance)
{
    GeOriginalPitemModelInstance *instance =
        find_instance(provider, model_instance);
    size_t index;
    if (instance == NULL || instance->resource == NULL
            || instance->resource->header.numSwitches < 0) return 0;
    for (index = 0U;
            index < (size_t)instance->resource->header.numSwitches;
            ++index) {
        ModelNode *node = instance->resource->header.Switches[index];
        if (node != NULL
                && (node->Opcode & 0xffU) == MODELNODE_OPCODE_SWITCH)
            instance_rw_at(instance,
                node->Data->Switch.RwDataIndex)->Switch.visible = FALSE;
    }
    provider->last_status = GE_ORIGINAL_PITEM_MODEL_OK;
    return 1;
}

int ge_original_pitem_model_instance_set_switch(
    GeOriginalPitemModelProvider *provider, void *model_instance,
    size_t switch_index, int visible)
{
    GeOriginalPitemModelInstance *instance =
        find_instance(provider, model_instance);
    ModelNode *node;
    if (instance == NULL || instance->resource == NULL
            || instance->resource->header.numSwitches < 0
            || switch_index
                >= (size_t)instance->resource->header.numSwitches)
        return 0;
    node = instance->resource->header.Switches[switch_index];
    if (node == NULL
            || (node->Opcode & 0xffU) != MODELNODE_OPCODE_SWITCH) return 0;
    instance_rw_at(instance,
        node->Data->Switch.RwDataIndex)->Switch.visible = visible != 0;
    provider->last_status = GE_ORIGINAL_PITEM_MODEL_OK;
    return 1;
}

const void *ge_original_pitem_model_instance_switch_node(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance, size_t switch_index)
{
    GeOriginalPitemModelInstance *instance =
        find_instance(provider, model_instance);
    if (instance == NULL || instance->resource == NULL
            || instance->resource->header.numSwitches < 0
            || switch_index
                >= (size_t)instance->resource->header.numSwitches)
        return NULL;
    return instance->resource->header.Switches[switch_index];
}

int ge_original_pitem_model_hit_ready(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance)
{
    size_t index;
    if (provider == NULL || model_instance == NULL) return 0;
    for (index = 0U; index < provider->instance_capacity; ++index) {
        const GeOriginalPitemModelInstance *instance =
            &provider->instances[index];
        if (instance->active && &instance->model == model_instance)
            return instance->resource != NULL
                && instance->resource->hit_ready;
    }
    return 0;
}

int ge_original_native_model_hit_vertex_offset(const void *base_address,
    const void *source_vertices, uint32_t *offset)
{
    const GeOriginalPitemModelProvider *provider;
    if (base_address == NULL || source_vertices == NULL || offset == NULL)
        return 0;
    for (provider = ge_pitem_provider_registry; provider != NULL;
            provider = provider->registry_next) {
        for (size_t index = 0U; index < provider->resource_count; ++index) {
            const GeOriginalPitemModelResource *resource =
                &provider->resources[index];
            if (resource->blob != base_address) continue;
            for (size_t node = 0U; node < resource->node_count; ++node) {
                const GeOriginalPitemCollisionStorage *storage =
                    &resource->collision[node];
                if (storage->vertices == source_vertices) {
                    *offset = storage->vertices_offset;
                    return 1;
                }
            }
        }
    }
    return 0;
}

size_t ge_original_native_model_hit_blob_size(const void *base_address)
{
    const GeOriginalPitemModelProvider *provider;
    size_t index;
    if (base_address == NULL) return 0U;
    for (provider = ge_pitem_provider_registry; provider != NULL;
            provider = provider->registry_next) {
        for (index = 0U; index < provider->resource_count; ++index) {
            const GeOriginalPitemModelResource *resource =
                &provider->resources[index];
            if (resource->hit_ready && resource->blob == base_address)
                return resource->blob_size;
        }
    }
    return 0U;
}

GeOriginalPitemModelStatus ge_original_pitem_model_last_status(
    const GeOriginalPitemModelProvider *provider)
{
    return provider == NULL ? GE_ORIGINAL_PITEM_MODEL_INVALID_ARGUMENT
                            : provider->last_status;
}

void ge_original_pitem_model_get_stats(
    const GeOriginalPitemModelProvider *provider,
    GeOriginalPitemModelStats *stats)
{
    if (stats == NULL) return;
    memset(stats, 0, sizeof(*stats));
    if (provider == NULL) return;
    stats->model_capacity = provider->resource_capacity;
    stats->loaded_models = provider->resource_count;
    stats->instance_capacity = provider->instance_capacity;
    stats->instantiated_models = provider->instance_count;
    stats->fixed_capacity_bytes = sizeof(*provider)
        + provider->resource_capacity * sizeof(*provider->resources)
        + provider->instance_capacity * sizeof(*provider->instances);
    stats->source_blob_bytes = provider->source_blob_bytes;
    stats->native_resource_bytes = provider->native_resource_bytes;
    stats->native_instance_bytes = provider->native_instance_bytes;
    stats->last_unsupported_opcode = provider->last_unsupported_opcode;
}

const char *ge_original_pitem_model_status_name(
    GeOriginalPitemModelStatus status)
{
    switch (status) {
    case GE_ORIGINAL_PITEM_MODEL_OK: return "ok";
    case GE_ORIGINAL_PITEM_MODEL_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_PITEM_MODEL_INVALID_ID: return "invalid model id";
    case GE_ORIGINAL_PITEM_MODEL_NOT_FOUND: return "asset not found";
    case GE_ORIGINAL_PITEM_MODEL_CAPACITY_EXHAUSTED: return "capacity exhausted";
    case GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED: return "allocation failed";
    case GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT: return "invalid PitemZ layout";
    case GE_ORIGINAL_PITEM_MODEL_UNSUPPORTED_OPCODE: return "unsupported opcode";
    default: return "unknown";
    }
}

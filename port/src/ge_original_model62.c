#include "ge_original_model62.h"

#include <stdlib.h>
#include <string.h>

#define GE_MODEL62_BASE 0x05000000U

static const uint8_t expected_sha256[32] = {
    0xd2, 0x99, 0xcc, 0x45, 0x40, 0x8f, 0x05, 0xac,
    0xf0, 0x6e, 0xee, 0xbc, 0x20, 0xe9, 0x93, 0x0f,
    0xcd, 0xe4, 0x29, 0xee, 0x5a, 0x4d, 0xc3, 0xc2,
    0x9c, 0xd4, 0xeb, 0x71, 0x28, 0x69, 0x46, 0x80
};

typedef struct GeSha256 {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t used;
} GeSha256;

static uint32_t rotr32(uint32_t value, unsigned shift)
{
    return (value >> shift) | (value << (32U - shift));
}

static uint32_t read_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) | ((uint32_t)source[1] << 16)
        | ((uint32_t)source[2] << 8) | (uint32_t)source[3];
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

static void sha256_transform(GeSha256 *hash, const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,
        0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,
        0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,
        0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,
        0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,
        0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,
        0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,
        0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,
        0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    size_t index;

    for (index = 0; index < 16U; ++index)
        words[index] = read_be32(block + index * 4U);
    for (; index < 64U; ++index) {
        uint32_t s0 = rotr32(words[index - 15U], 7U)
            ^ rotr32(words[index - 15U], 18U) ^ (words[index - 15U] >> 3);
        uint32_t s1 = rotr32(words[index - 2U], 17U)
            ^ rotr32(words[index - 2U], 19U) ^ (words[index - 2U] >> 10);
        words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }
    a=hash->state[0]; b=hash->state[1]; c=hash->state[2]; d=hash->state[3];
    e=hash->state[4]; f=hash->state[5]; g=hash->state[6]; h=hash->state[7];
    for (index = 0; index < 64U; ++index) {
        uint32_t sum1 = rotr32(e,6U)^rotr32(e,11U)^rotr32(e,25U);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t temp1 = h + sum1 + choice + constants[index] + words[index];
        uint32_t sum0 = rotr32(a,2U)^rotr32(a,13U)^rotr32(a,22U);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    hash->state[0]+=a; hash->state[1]+=b; hash->state[2]+=c;
    hash->state[3]+=d; hash->state[4]+=e; hash->state[5]+=f;
    hash->state[6]+=g; hash->state[7]+=h;
}

static void sha256_update(GeSha256 *hash, const uint8_t *data, size_t size)
{
    while (size != 0U) {
        size_t available = 64U - hash->used;
        size_t count = size < available ? size : available;
        memcpy(hash->block + hash->used, data, count);
        hash->used += count;
        hash->bit_count += (uint64_t)count * 8U;
        data += count;
        size -= count;
        if (hash->used == 64U) {
            sha256_transform(hash, hash->block);
            hash->used = 0U;
        }
    }
}

static void sha256(const uint8_t *data, size_t size, uint8_t result[32])
{
    GeSha256 hash = {{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                      0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U},0,{0},0};
    uint64_t bits;
    size_t index;
    sha256_update(&hash, data, size);
    bits = hash.bit_count;
    hash.block[hash.used++] = 0x80U;
    if (hash.used > 56U) {
        memset(hash.block + hash.used, 0, 64U - hash.used);
        sha256_transform(&hash, hash.block);
        hash.used = 0U;
    }
    memset(hash.block + hash.used, 0, 56U - hash.used);
    for (index = 0U; index < 8U; ++index)
        hash.block[63U - index] = (uint8_t)(bits >> (index * 8U));
    sha256_transform(&hash, hash.block);
    for (index = 0U; index < 8U; ++index) {
        result[index*4U]=(uint8_t)(hash.state[index]>>24);
        result[index*4U+1U]=(uint8_t)(hash.state[index]>>16);
        result[index*4U+2U]=(uint8_t)(hash.state[index]>>8);
        result[index*4U+3U]=(uint8_t)hash.state[index];
    }
}

static int segmented_offset(uint32_t address, size_t expected)
{
    return address == GE_MODEL62_BASE + (uint32_t)expected;
}

GeOriginalModel62Status ge_original_model62_relocate(
    GeOriginalModel62 *runtime, const void *source_blob, size_t source_size)
{
    static const size_t node_offsets[4] = {0x54U,0x6cU,0x84U,0x9cU};
    static const uint16_t node_opcodes[4] = {
        MODELNODE_OPCODE_GROUPSIMPLE, MODELNODE_OPCODE_BBOX,
        MODELNODE_OPCODE_DL, MODELNODE_OPCODE_GUNFIRE};
    const uint8_t *blob = source_blob;
    uint8_t digest[32];
    size_t index;

    if (runtime == NULL || blob == NULL)
        return GE_ORIGINAL_MODEL62_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    if (source_size != GE_ORIGINAL_MODEL62_BLOB_SIZE)
        return GE_ORIGINAL_MODEL62_INVALID_SIZE;
    sha256(blob, source_size, digest);
    if (memcmp(digest, expected_sha256, sizeof(digest)) != 0)
        return GE_ORIGINAL_MODEL62_HASH_MISMATCH;

    for (index = 0U; index < 4U; ++index) {
        const uint8_t *raw = blob + node_offsets[index];
        if (read_be16(raw) != node_opcodes[index])
            return GE_ORIGINAL_MODEL62_INVALID_LAYOUT;
    }
    if (!segmented_offset(read_be32(blob), 0x9cU)
            || read_be32(blob+4U) != 0U || read_be32(blob+8U) != 0U
            || !segmented_offset(read_be32(blob+0x58U), 0xb4U)
            || !segmented_offset(read_be32(blob+0x70U), 0xc8U)
            || !segmented_offset(read_be32(blob+0x88U), 0x588U)
            || !segmented_offset(read_be32(blob+0xa0U), 0x59cU))
        return GE_ORIGINAL_MODEL62_INVALID_LAYOUT;

    runtime->source_blob = blob;
    runtime->source_size = source_size;
    for (index = 0U; index < GE_ORIGINAL_MODEL62_TEXTURE_COUNT; ++index) {
        const uint8_t *raw = blob + 0x0cU + index * 12U;
        ModelFileTextures *texture = &runtime->textures[index];
        texture->TextureID=read_be32(raw); texture->Width=raw[4];
        texture->Height=raw[5]; texture->MipMapTiles=raw[6];
        texture->Type=raw[7]; texture->RenderDepth=raw[8];
        texture->sflags=raw[9]; texture->tflags=raw[10];
    }
    runtime->joints[0].NodeType=0x15U; runtime->joints[0].mtxA=0U;
    runtime->joints[0].mtxB=0U; runtime->joints[1].NodeType=0x15U;
    runtime->joints[1].mtxA=1U; runtime->joints[1].mtxB=1U;
    runtime->skeleton.numjoints=2; runtime->skeleton.Joints=runtime->joints;

    runtime->group_data.GroupSimple.Origin.x=read_bef32(blob+0xb4U);
    runtime->group_data.GroupSimple.Origin.y=read_bef32(blob+0xb8U);
    runtime->group_data.GroupSimple.Origin.z=read_bef32(blob+0xbcU);
    runtime->group_data.GroupSimple.Group1=(s16)read_be16(blob+0xc0U);
    runtime->group_data.GroupSimple.Group2=read_be16(blob+0xc2U);
    runtime->group_data.GroupSimple.BoundingVolumeRadius=read_bef32(blob+0xc4U);
    runtime->bbox_data.BoundingBox.ModelNumber=read_be32(blob+0xc8U);
    runtime->bbox_data.BoundingBox.Bounds.xmin=read_bef32(blob+0xccU);
    runtime->bbox_data.BoundingBox.Bounds.xmax=read_bef32(blob+0xd0U);
    runtime->bbox_data.BoundingBox.Bounds.ymin=read_bef32(blob+0xd4U);
    runtime->bbox_data.BoundingBox.Bounds.ymax=read_bef32(blob+0xd8U);
    runtime->bbox_data.BoundingBox.Bounds.zmin=read_bef32(blob+0xdcU);
    runtime->bbox_data.BoundingBox.Bounds.zmax=read_bef32(blob+0xe0U);

    for (index = 0U; index < GE_ORIGINAL_MODEL62_VERTEX_COUNT; ++index) {
        const uint8_t *raw=blob+0xe8U+index*16U;
        Vertex *vertex=&runtime->vertices[index];
        vertex->coord.x=(s16)read_be16(raw); vertex->coord.y=(s16)read_be16(raw+2U);
        vertex->coord.z=(s16)read_be16(raw+4U); vertex->index=(s16)read_be16(raw+6U);
        vertex->s=(s16)read_be16(raw+8U); vertex->t=(s16)read_be16(raw+10U);
        vertex->r=raw[12]; vertex->g=raw[13]; vertex->b=raw[14]; vertex->a=raw[15];
    }
    runtime->display_list_data.DisplayList.Primary=(Gfx *)(uintptr_t)(blob+0x5c8U);
    runtime->display_list_data.DisplayList.Secondary=(Gfx *)(uintptr_t)(blob+0x6b8U);
    runtime->display_list_data.DisplayList.BaseAddr=(void *)(uintptr_t)blob;
    runtime->display_list_data.DisplayList.Vertices=runtime->vertices;
    runtime->display_list_data.DisplayList.numVertices=read_be16(blob+0x598U);
    runtime->display_list_data.DisplayList.ModelType=(s8)blob[0x59aU];
    runtime->gunfire_data.Gunfire.Offset.x=read_bef32(blob+0x59cU);
    runtime->gunfire_data.Gunfire.Offset.y=read_bef32(blob+0x5a0U);
    runtime->gunfire_data.Gunfire.Offset.z=read_bef32(blob+0x5a4U);
    runtime->gunfire_data.Gunfire.Size.x=read_bef32(blob+0x5a8U);
    runtime->gunfire_data.Gunfire.Size.y=read_bef32(blob+0x5acU);
    runtime->gunfire_data.Gunfire.Size.z=read_bef32(blob+0x5b0U);
    runtime->gunfire_data.Gunfire.Image=(void *)(uintptr_t)(blob+0x3cU);
    runtime->gunfire_data.Gunfire.Scale=read_bef32(blob+0x5b8U);
    runtime->gunfire_data.Gunfire.RwDataIndex=read_be16(blob+0x5bcU);
    runtime->gunfire_data.Gunfire.reserved=read_be16(blob+0x5beU);
    /* Canonical field is a 32-bit address. Populate it on the 32-bit target;
     * source_blob is the non-truncating provider on wider host tests. */
#if UINTPTR_MAX <= UINT32_MAX
    runtime->gunfire_data.Gunfire.BaseAddr=(u32)(uintptr_t)blob;
#else
    runtime->gunfire_data.Gunfire.BaseAddr=0U;
#endif

    runtime->nodes[0]=(ModelNode){node_opcodes[0],&runtime->group_data,NULL,NULL,NULL,&runtime->nodes[1]};
    runtime->nodes[1]=(ModelNode){node_opcodes[1],&runtime->bbox_data,&runtime->nodes[0],&runtime->nodes[3],NULL,&runtime->nodes[2]};
    runtime->nodes[2]=(ModelNode){node_opcodes[2],&runtime->display_list_data,&runtime->nodes[1],NULL,NULL,NULL};
    runtime->nodes[3]=(ModelNode){node_opcodes[3],&runtime->gunfire_data,&runtime->nodes[0],NULL,&runtime->nodes[1],NULL};
    runtime->switches[0]=&runtime->nodes[3];
    runtime->header.RootNode=&runtime->nodes[0]; runtime->header.Skeleton=&runtime->skeleton;
    runtime->header.Switches=runtime->switches; runtime->header.numSwitches=3;
    runtime->header.numMatrices=1; runtime->header.BoundingVolumeRadius=runtime->group_data.GroupSimple.BoundingVolumeRadius;
    runtime->header.numRecords=1; runtime->header.numtextures=6; runtime->header.Textures=runtime->textures;
    runtime->model.rwdatalen=1; runtime->model.obj=&runtime->header;
    runtime->model.render_pos=runtime->render_positions;
    runtime->model.datas=(union ModelRwData **)(void *)runtime->rwdata_words;
    runtime->model.scale=1.0f;
    return GE_ORIGINAL_MODEL62_OK;
}

int32_t ge_original_model62_model_load(void *context, int32_t model_id)
{
    GeOriginalModel62 *runtime = context;
    return runtime != NULL && runtime->source_blob != NULL
        && model_id == GE_ORIGINAL_MODEL62_ID;
}

GeOriginalModel62 *ge_original_model62_create(
    const void *source_blob, size_t source_size,
    GeOriginalModel62Status *status)
{
    GeOriginalModel62 *runtime = malloc(sizeof(*runtime));
    GeOriginalModel62Status result;

    if (runtime == NULL) {
        if (status != NULL) *status = GE_ORIGINAL_MODEL62_ALLOCATION_FAILED;
        return NULL;
    }
    result = ge_original_model62_relocate(runtime, source_blob, source_size);
    if (status != NULL) *status = result;
    if (result != GE_ORIGINAL_MODEL62_OK) {
        free(runtime);
        return NULL;
    }
    return runtime;
}

void ge_original_model62_destroy(GeOriginalModel62 *runtime)
{
    free(runtime);
}

int ge_original_model62_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale)
{
    GeOriginalModel62 *runtime = context;
    if (runtime == NULL || runtime->source_blob == NULL
            || model_id != GE_ORIGINAL_MODEL62_ID || model_header == NULL
            || model_instance == NULL || pitem_scale == NULL) return 0;
    *model_header=&runtime->header; *model_instance=&runtime->model;
    *pitem_scale=GE_ORIGINAL_MODEL62_PITEM_SCALE;
    return 1;
}

const char *ge_original_model62_status_name(GeOriginalModel62Status status)
{
    switch (status) {
    case GE_ORIGINAL_MODEL62_OK:return "ok";
    case GE_ORIGINAL_MODEL62_INVALID_ARGUMENT:return "invalid argument";
    case GE_ORIGINAL_MODEL62_INVALID_SIZE:return "invalid size";
    case GE_ORIGINAL_MODEL62_HASH_MISMATCH:return "hash mismatch";
    case GE_ORIGINAL_MODEL62_INVALID_LAYOUT:return "invalid layout";
    case GE_ORIGINAL_MODEL62_ALLOCATION_FAILED:return "allocation failed";
    default:return "unknown";
    }
}

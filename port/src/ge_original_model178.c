#include "ge_original_model178.h"

#include <stdlib.h>
#include <string.h>

#define GE_MODEL178_BASE 0x05000000U

static uint32_t read_be32(const uint8_t *p)
{ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static uint16_t read_be16(const uint8_t *p)
{ return (uint16_t)(((uint16_t)p[0]<<8)|p[1]); }
static float read_bef32(const uint8_t *p)
{ uint32_t bits=read_be32(p); float v; memcpy(&v,&bits,sizeof(v)); return v; }
static int seg(uint32_t address, size_t offset)
{ return address==GE_MODEL178_BASE+(uint32_t)offset; }
static void read_vertex(Vertex *v, const uint8_t *p)
{
    v->coord.x=(s16)read_be16(p); v->coord.y=(s16)read_be16(p+2U);
    v->coord.z=(s16)read_be16(p+4U); v->index=(s16)read_be16(p+6U);
    v->s=(s16)read_be16(p+8U); v->t=(s16)read_be16(p+10U);
    v->r=p[12]; v->g=p[13]; v->b=p[14]; v->a=p[15];
}

GeOriginalModel178Status ge_original_model178_relocate(
    GeOriginalModel178 *r, const void *source_blob, size_t source_size)
{
    static const size_t noff[3]={0x24U,0x3cU,0x54U};
    static const uint16_t op[3]={MODELNODE_OPCODE_GROUP,
        MODELNODE_OPCODE_BBOX,MODELNODE_OPCODE_DLCOLLISION};
    const uint8_t *b=source_blob; size_t i;
    if (r==NULL||b==NULL) return GE_ORIGINAL_MODEL178_INVALID_ARGUMENT;
    memset(r,0,sizeof(*r));
    if (source_size!=GE_ORIGINAL_MODEL178_BLOB_SIZE)
        return GE_ORIGINAL_MODEL178_INVALID_SIZE;
    for(i=0;i<3U;i++) if(read_be16(b+noff[i])!=op[i])
        return GE_ORIGINAL_MODEL178_INVALID_LAYOUT;
    if(!seg(read_be32(b+0x28U),0x6cU)
       ||!seg(read_be32(b+0x38U),0x3cU)
       ||!seg(read_be32(b+0x40U),0x88U)
       ||!seg(read_be32(b+0x50U),0x54U)
       ||!seg(read_be32(b+0x58U),0x500U)
       ||!seg(read_be32(b+0x508U),0xa8U)
       ||!seg(read_be32(b+0x510U),0x368U)
       ||!seg(read_be32(b+0x514U),0x4a8U))
        return GE_ORIGINAL_MODEL178_INVALID_LAYOUT;
    r->source_blob=b; r->source_size=source_size;
    for(i=0;i<2U;i++) {
        const uint8_t *p=b+i*12U;
        r->textures[i].TextureID=read_be32(p); r->textures[i].Width=p[4];
        r->textures[i].Height=p[5]; r->textures[i].MipMapTiles=p[6];
        r->textures[i].Type=p[7]; r->textures[i].RenderDepth=p[8];
        r->textures[i].sflags=p[9]; r->textures[i].tflags=p[10];
    }
    r->group_data.Group.Origin.x=read_bef32(b+0x6cU);
    r->group_data.Group.Origin.y=read_bef32(b+0x70U);
    r->group_data.Group.Origin.z=read_bef32(b+0x74U);
    r->group_data.Group.JointID=read_be16(b+0x78U);
    r->group_data.Group.MatrixID0=(s16)read_be16(b+0x7aU);
    r->group_data.Group.MatrixID1=(s16)read_be16(b+0x7cU);
    r->group_data.Group.MatrixID2=(s16)read_be16(b+0x7eU);
    r->group_data.Group.BoundingVolumeRadius=read_bef32(b+0x84U);
    r->bbox_data.BoundingBox.ModelNumber=read_be32(b+0x88U);
    r->bbox_data.BoundingBox.Bounds.xmin=read_bef32(b+0x8cU);
    r->bbox_data.BoundingBox.Bounds.xmax=read_bef32(b+0x90U);
    r->bbox_data.BoundingBox.Bounds.ymin=read_bef32(b+0x94U);
    r->bbox_data.BoundingBox.Bounds.ymax=read_bef32(b+0x98U);
    r->bbox_data.BoundingBox.Bounds.zmin=read_bef32(b+0x9cU);
    r->bbox_data.BoundingBox.Bounds.zmax=read_bef32(b+0xa0U);
    for(i=0;i<GE_ORIGINAL_MODEL178_VERTEX_COUNT;i++) {
        read_vertex(&r->vertices[i],b+0xa8U+i*16U);
        r->point_usage[i]=(s16)read_be16(b+0x4a8U+i*2U);
    }
    for(i=0;i<GE_ORIGINAL_MODEL178_COLLISION_VERTEX_COUNT;i++)
        read_vertex(&r->collision_vertices[i],b+0x368U+i*16U);
    r->collision_data.DisplayListCollisions.Primary=(Gfx *)(uintptr_t)(b+0x520U);
    r->collision_data.DisplayListCollisions.Secondary=NULL;
    r->collision_data.DisplayListCollisions.Vertices=r->vertices;
    r->collision_data.DisplayListCollisions.numVertices=(s16)read_be16(b+0x50cU);
    r->collision_data.DisplayListCollisions.numCollisionVertices=(s16)read_be16(b+0x50eU);
    r->collision_data.DisplayListCollisions.CollisionVertices=r->collision_vertices;
    r->collision_data.DisplayListCollisions.PointUsage=r->point_usage;
    r->collision_data.DisplayListCollisions.ModelType=(s16)read_be16(b+0x518U);
    r->collision_data.DisplayListCollisions.RwDataIndex=read_be16(b+0x51aU);
    r->collision_data.DisplayListCollisions.BaseAddr=(void *)(uintptr_t)b;
    r->nodes[0]=(ModelNode){op[0],&r->group_data,NULL,NULL,NULL,&r->nodes[1]};
    r->nodes[1]=(ModelNode){op[1],&r->bbox_data,&r->nodes[0],NULL,NULL,&r->nodes[2]};
    r->nodes[2]=(ModelNode){op[2],&r->collision_data,&r->nodes[1],NULL,NULL,NULL};
    r->joints[0].NodeType=0x15U; r->joints[0].mtxA=0; r->joints[0].mtxB=0;
    r->joints[1].NodeType=0x15U; r->joints[1].mtxA=1; r->joints[1].mtxB=1;
    r->skeleton.numjoints=2; r->skeleton.Joints=r->joints;
    r->header.RootNode=&r->nodes[0]; r->header.Skeleton=&r->skeleton;
    r->header.numMatrices=1; r->header.BoundingVolumeRadius=r->group_data.Group.BoundingVolumeRadius;
    r->header.numtextures=2; r->header.Textures=r->textures;
    r->model.rwdatalen=1; r->model.obj=&r->header;
    r->model.render_pos=r->render_positions;
    r->model.datas=(union ModelRwData **)(void *)r->rwdata_words;
    r->model.scale=1.0f;
    return GE_ORIGINAL_MODEL178_OK;
}

GeOriginalModel178 *ge_original_model178_create(const void *blob,size_t size,
    GeOriginalModel178Status *status)
{
    GeOriginalModel178 *r=malloc(sizeof(*r)); GeOriginalModel178Status s;
    if(!r){if(status)*status=GE_ORIGINAL_MODEL178_ALLOCATION_FAILED;return NULL;}
    s=ge_original_model178_relocate(r,blob,size); if(status)*status=s;
    if(s!=GE_ORIGINAL_MODEL178_OK){free(r);return NULL;} return r;
}
void ge_original_model178_destroy(GeOriginalModel178 *r){free(r);}
int32_t ge_original_model178_model_load(void *context,int32_t id)
{ GeOriginalModel178 *r=context; return r&&r->source_blob&&id==178; }
int ge_original_model178_resolve_instance(void *context,int32_t id,
    void **header,void **model,float *scale)
{
    GeOriginalModel178 *r=context;
    if(!r||!r->source_blob||id!=178||!header||!model||!scale)return 0;
    *header=&r->header; *model=&r->model; *scale=1.0f; return 1;
}

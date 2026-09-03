#include "ge_original_character_models.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrobjdata.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GE_CHR_SEGMENT_BASE UINT32_C(0x05000000)
#define GE_CHR_TEXTURE_BYTES 12U
#define GE_CHR_NODE_BYTES 24U
#define GE_CHR_PREFIX "converted/models/characters/"
#define GE_CHR_SUFFIX ".bin"
#define GE_CHR_PATH_CAPACITY 128U

typedef struct GeCharacterDependency {
    uint8_t model_id;
    uint8_t roles;
} GeCharacterDependency;

enum { GE_CHR_ROLE_BODY = 1, GE_CHR_ROLE_HEAD = 2 };

/* Exact union of all 20 solo setup dependencies, both unchanged random-head
 * pools, initializeGunBarrelIntro, and MENU_DISPLAY_CAST.  The 71-record
 * list is independently derived and hash-locked by
 * scripts/stage_3ds_character_models.py. */
static const GeCharacterDependency ge_character_dependencies[] = {
    {0,1},{1,1},{2,1},{3,1},{4,1},{5,1},{6,1},{7,1},{8,1},{9,1},
    {10,1},{11,1},{12,1},{13,1},{14,1},{15,1},{16,1},{17,1},{18,1},
    {19,1},{20,1},{21,1},{22,1},{23,1},{24,1},{25,1},{28,1},{29,1},
    {32,1},{33,1},{34,1},{35,1},{36,1},{37,1},{38,1},{39,1},{40,1},
    {42,2},{43,2},{44,2},{45,2},{46,2},{47,2},{48,2},{49,2},{50,2},
    {51,2},{52,2},{53,2},{54,2},{55,2},{56,2},{57,2},{58,2},{59,2},
    {62,2},{63,2},{64,2},{65,2},{66,2},{67,2},{68,2},{69,2},{70,2},
    {71,2},{72,2},{73,2},{74,2},{75,2},{78,2},{79,1}
};

typedef struct GeCharacterCollision {
    Vertex *vertices;
    Vertex *collision_vertices;
    s16 *point_usage;
    size_t bytes;
    uint32_t primary_offset;
    uint32_t secondary_offset;
    uint32_t vertices_offset;
} GeCharacterCollision;

typedef struct GeCharacterResource {
    int32_t model_id;
    uint8_t *blob;
    size_t blob_size;
    uint32_t *node_offsets;
    uint32_t *rodata_offsets;
    size_t node_count;
    ModelNode *nodes;
    union ModelRoData *rodatas;
    GeCharacterCollision *collision;
    ModelNode **switches;
    ModelFileTextures *textures;
    ModelFileHeader header;
    size_t rw_words;
    size_t native_bytes;
} GeCharacterResource;

typedef struct GeCharacterInstance {
    Model model;
    RenderPosView *render_positions;
    uintptr_t *rwdata;
    GeCharacterResource *body;
    GeCharacterResource *head;
    ModelFileHeader body_header;
    ModelFileHeader head_header;
    ModelNode *body_nodes;
    ModelNode *head_nodes;
    union ModelRoData *body_rodatas;
    union ModelRoData *head_rodatas;
    ModelNode **body_switches;
    ModelNode **head_switches;
    int32_t body_id;
    int32_t head_id;
    size_t bytes;
} GeCharacterInstance;

struct GeOriginalCharacterModelProvider {
    /* All storage is fixed by the caller-provided capacities; resources are
     * only populated on first use of an authored body/head dependency. */
    GeAssetPack *pack;
    GeCharacterResource *resources;
    GeCharacterInstance *instances;
    size_t resource_capacity, resource_count;
    size_t instance_capacity, instance_count;
    size_t source_blob_bytes, native_resource_bytes, native_instance_bytes;
    GeOriginalCharacterModelStatus last_status;
    uint16_t last_unsupported_opcode;
};

static uint16_t be16(const uint8_t *p)
{ return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
       | ((uint32_t)p[2] << 8) | p[3]; }
static float bef32(const uint8_t *p)
{ uint32_t bits=be32(p); float value; memcpy(&value,&bits,4); return value; }

static int range_valid(size_t size, size_t offset, size_t count, size_t stride)
{
    return stride == 0U || (count <= (SIZE_MAX-offset)/stride
        && offset + count*stride <= size);
}

static int seg_offset(uint32_t address, size_t size, size_t required,
                      uint32_t *offset)
{
    uint32_t local;
    if ((address & UINT32_C(0xff000000)) != GE_CHR_SEGMENT_BASE) return 0;
    local=address & UINT32_C(0x00ffffff);
    if (!range_valid(size,local,required,1U)) return 0;
    if (offset) *offset=local;
    return 1;
}

static size_t rodata_size(uint16_t opcode)
{
    switch (opcode & 0xffU) {
    case MODELNODE_OPCODE_HEADER: return 0x10U;
    case MODELNODE_OPCODE_GROUP: return 0x1cU;
    case MODELNODE_OPCODE_LOD: return 0x10U;
    case MODELNODE_OPCODE_BSP: return 0x24U;
    case MODELNODE_OPCODE_BBOX: return 0x1cU;
    case MODELNODE_OPCODE_SHADOW: return 0x20U;
    case MODELNODE_OPCODE_SWITCH: return 0x08U;
    case MODELNODE_OPCODE_HEAD: return 0x04U;
    case MODELNODE_OPCODE_DLCOLLISION: return 0x20U;
    default: return 0U;
    }
}

static const GeCharacterDependency *dependency_for_id(int32_t model_id)
{
    size_t i;
    for (i=0;i<sizeof(ge_character_dependencies)/sizeof(ge_character_dependencies[0]);++i)
        if (ge_character_dependencies[i].model_id == model_id)
            return &ge_character_dependencies[i];
    return NULL;
}

static size_t find_offset(const uint32_t *offsets,size_t count,uint32_t wanted)
{
    size_t i; for(i=0;i<count;++i) if(offsets[i]==wanted) return i;
    return SIZE_MAX;
}

static int append_node(GeOriginalCharacterModelProvider *provider,
                       GeCharacterResource *resource, size_t *capacity,
                       uint32_t offset)
{
    uint32_t *next; uint16_t opcode; size_t next_capacity;
    if(find_offset(resource->node_offsets,resource->node_count,offset)!=SIZE_MAX)
        return 1;
    if(!range_valid(resource->blob_size,offset,GE_CHR_NODE_BYTES,1U)) return 0;
    opcode=be16(resource->blob+offset)&0xffU;
    if(rodata_size(opcode)==0U){provider->last_unsupported_opcode=opcode;return -1;}
    if(resource->node_count==*capacity){
        next_capacity=*capacity?*capacity*2U:16U;
        if(next_capacity<*capacity||next_capacity>SIZE_MAX/sizeof(*next))return 0;
        next=realloc(resource->node_offsets,next_capacity*sizeof(*next));
        if(!next)return 0;
        resource->node_offsets=next;
        *capacity=next_capacity;
    }
    resource->node_offsets[resource->node_count++]=offset;
    return 1;
}

static GeOriginalCharacterModelStatus collect_nodes(
    GeOriginalCharacterModelProvider *provider, GeCharacterResource *resource,
    uint32_t root_offset)
{
    size_t capacity=0,cursor,field; int result;
    result=append_node(provider,resource,&capacity,root_offset);
    if(result<=0)return result<0?GE_ORIGINAL_CHARACTER_MODEL_UNSUPPORTED_OPCODE:
        GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    for(cursor=0;cursor<resource->node_count;++cursor){
        const uint8_t *raw=resource->blob+resource->node_offsets[cursor];
        for(field=8;field<=20;field+=4){
            uint32_t address=be32(raw+field),offset;
            if(!address)continue;
            if(!seg_offset(address,resource->blob_size,GE_CHR_NODE_BYTES,&offset))
                return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
            result=append_node(provider,resource,&capacity,offset);
            if(result<=0)return result<0?GE_ORIGINAL_CHARACTER_MODEL_UNSUPPORTED_OPCODE:
                GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        }
    }
    return GE_ORIGINAL_CHARACTER_MODEL_OK;
}

static ModelNode *node_address(GeCharacterResource *r,uint32_t address)
{
    uint32_t offset; size_t i;
    if(!address)return NULL;
    if(!seg_offset(address,r->blob_size,GE_CHR_NODE_BYTES,&offset))return NULL;
    i=find_offset(r->node_offsets,r->node_count,offset);
    return i==SIZE_MAX?NULL:&r->nodes[i];
}

static void *blob_address(GeCharacterResource *r,uint32_t address,size_t required)
{
    uint32_t offset;if(!address)return NULL;
    return seg_offset(address,r->blob_size,required,&offset)?r->blob+offset:NULL;
}

static void read_vertex(Vertex *out,const uint8_t *raw)
{
    memset(out,0,sizeof(*out));out->coord.x=(s16)be16(raw);
    out->coord.y=(s16)be16(raw+2);out->coord.z=(s16)be16(raw+4);
    out->index=(s16)be16(raw+6);out->s=(s16)be16(raw+8);
    out->t=(s16)be16(raw+10);out->r=raw[12];out->g=raw[13];
    out->b=raw[14];out->a=raw[15];
}

static GeOriginalCharacterModelStatus decode_dl(GeCharacterResource *r,
                                                 size_t index,const uint8_t *raw)
{
    ModelRoData_DisplayList_CollisionRecord *out=&r->rodatas[index].DisplayListCollisions;
    GeCharacterCollision *store=&r->collision[index];
    int16_t nv=(int16_t)be16(raw+12),nc=(int16_t)be16(raw+14);
    uint32_t vo,co,uo;size_t i;
    if(nv<0||nc<0||!seg_offset(be32(raw+8),r->blob_size,(size_t)nv*16,&vo)
       ||!seg_offset(be32(raw+16),r->blob_size,(size_t)nc*16,&co)
       ||!seg_offset(be32(raw+20),r->blob_size,(size_t)nv*2,&uo))
        return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    store->vertices=calloc(nv?nv:1,sizeof(*store->vertices));
    store->collision_vertices=calloc(nc?nc:1,sizeof(*store->collision_vertices));
    store->point_usage=calloc(nv?nv:1,sizeof(*store->point_usage));
    if(!store->vertices||!store->collision_vertices||!store->point_usage)
        return GE_ORIGINAL_CHARACTER_MODEL_ALLOCATION_FAILED;
    for(i=0;i<(size_t)nv;++i){read_vertex(&store->vertices[i],r->blob+vo+i*16);
        store->point_usage[i]=(s16)be16(r->blob+uo+i*2);}
    for(i=0;i<(size_t)nc;++i){const uint8_t *v=r->blob+co+i*16;uint32_t related=be32(v+8);
        read_vertex(&store->collision_vertices[i],v);
        store->collision_vertices[i].CollisionRelatedNode=node_address(r,related);
        if(related&&!store->collision_vertices[i].CollisionRelatedNode)
            return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        store->collision_vertices[i].CollisionRelatedIndex=(s16)be16(v+12);
        store->collision_vertices[i].CollisionReserved=(s16)be16(v+14);}
    out->Primary=blob_address(r,be32(raw),sizeof(Gfx));
    out->Secondary=blob_address(r,be32(raw+4),sizeof(Gfx));
    if((be32(raw)&&!out->Primary)||(be32(raw+4)&&!out->Secondary))
        return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    out->Vertices=store->vertices;out->numVertices=nv;
    out->numCollisionVertices=nc;out->CollisionVertices=store->collision_vertices;
    out->PointUsage=store->point_usage;out->ModelType=(s16)be16(raw+24);
    out->BaseAddr=r->blob;store->primary_offset=be32(raw)&0xffffffU;
    store->secondary_offset=be32(raw+4)?(be32(raw+4)&0xffffffU):UINT32_MAX;
    store->vertices_offset=vo;
    store->bytes=(size_t)(nv+nc)*sizeof(Vertex)+(size_t)nv*sizeof(s16);
    return GE_ORIGINAL_CHARACTER_MODEL_OK;
}

static GeOriginalCharacterModelStatus decode_rodata(GeCharacterResource *r,
                                                     size_t index)
{
    const uint8_t *node=r->blob+r->node_offsets[index];
    const uint8_t *raw=r->blob+r->rodata_offsets[index];
    uint16_t opcode=be16(node)&0xffU;
    switch(opcode){
    case MODELNODE_OPCODE_HEADER:{ModelRoData_HeaderRecord *o=&r->rodatas[index].Header;
        o->AnimPart=be16(raw);o->MatrixIndex=(s16)be16(raw+2);
        o->FirstGroupNode=node_address(r,be32(raw+4));
        if(be32(raw+4)&&!o->FirstGroupNode)return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        o->Group1=be16(raw+8);o->Group2=be16(raw+10);o->reserved=be16(raw+14);break;}
    case MODELNODE_OPCODE_GROUP:{ModelRoData_GroupRecord *o=&r->rodatas[index].Group;
        o->Origin.x=bef32(raw);o->Origin.y=bef32(raw+4);o->Origin.z=bef32(raw+8);
        o->JointID=be16(raw+12);o->MatrixID0=(s16)be16(raw+14);
        o->MatrixID1=(s16)be16(raw+16);o->MatrixID2=(s16)be16(raw+18);
        o->ChildGroup=(ModelRoData_GroupRecord*)node_address(r,be32(raw+20));
        if(be32(raw+20)&&!o->ChildGroup)return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        o->BoundingVolumeRadius=bef32(raw+24);break;}
    case MODELNODE_OPCODE_LOD:{ModelRoData_LODRecord *o=&r->rodatas[index].LOD;
        o->MinDistance=bef32(raw);o->MaxDistance=bef32(raw+4);
        o->Affects=node_address(r,be32(raw+8));
        if(be32(raw+8)&&!o->Affects)return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        o->reserved=be16(raw+14);break;}
    case MODELNODE_OPCODE_BSP:{ModelRoData_BSPRecord *o=&r->rodatas[index].BSP;
        o->Point.x=bef32(raw);o->Point.y=bef32(raw+4);o->Point.z=bef32(raw+8);
        o->Vector.x=bef32(raw+12);o->Vector.y=bef32(raw+16);o->Vector.z=bef32(raw+20);
        o->leftChild=node_address(r,be32(raw+24));o->rightChild=node_address(r,be32(raw+28));
        if((be32(raw+24)&&!o->leftChild)||(be32(raw+28)&&!o->rightChild))
            return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        o->reserved=(s16)be16(raw+32);break;}
    case MODELNODE_OPCODE_BBOX:{ModelRoData_BoundingBoxRecord *o=&r->rodatas[index].BoundingBox;
        o->ModelNumber=be32(raw);o->Bounds.xmin=bef32(raw+4);o->Bounds.xmax=bef32(raw+8);
        o->Bounds.ymin=bef32(raw+12);o->Bounds.ymax=bef32(raw+16);
        o->Bounds.zmin=bef32(raw+20);o->Bounds.zmax=bef32(raw+24);break;}
    case MODELNODE_OPCODE_SHADOW:{ModelRoData_ShadowRecord *o=&r->rodatas[index].Shadow;
        o->pos.x=bef32(raw);o->pos.y=bef32(raw+4);o->size.x=bef32(raw+8);o->size.y=bef32(raw+12);
        o->image=blob_address(r,be32(raw+16),1);o->HeaderNode=node_address(r,be32(raw+20));
        if((be32(raw+16)&&!o->image)||(be32(raw+20)&&!o->HeaderNode))
            return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        o->Scale=bef32(raw+24);o->BaseAddr=r->blob;break;}
    case MODELNODE_OPCODE_SWITCH:{ModelRoData_SwitchRecord *o=&r->rodatas[index].Switch;
        o->Controls=node_address(r,be32(raw));if(be32(raw)&&!o->Controls)
            return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        o->reserved=be16(raw+6);break;}
    case MODELNODE_OPCODE_HEAD:r->rodatas[index].HeadPlaceholder.RwDataIndex=0;break;
    case MODELNODE_OPCODE_DLCOLLISION:return decode_dl(r,index,raw);
    default:return GE_ORIGINAL_CHARACTER_MODEL_UNSUPPORTED_OPCODE;
    }
    return GE_ORIGINAL_CHARACTER_MODEL_OK;
}

static void reorder_bsp(ModelNode *node,int visible)
{
    ModelRoData_BSPRecord *r=&node->Data->BSP;
    ModelNode *a=visible?r->leftChild:r->rightChild;
    ModelNode *b=visible?r->rightChild:r->leftChild;
    ModelNode *p;
    if(a){
        node->Child=a;a->Prev=NULL;p=a;
        while(p->Next&&p->Next!=b)p=p->Next;
        p->Next=b;
        if(b){
            b->Prev=p;p=b;
            while(p->Next&&p->Next!=a)p=p->Next;
            p->Next=NULL;
        }
    }else{
        node->Child=b;
        if(b)b->Prev=NULL;
    }
}

static GeOriginalCharacterModelStatus assign_rw(GeCharacterResource *r)
{
    ModelNode *node=r->header.RootNode;size_t words=0,visited=0;
    while(node&&visited++<=r->node_count*2U){
        switch(node->Opcode&0xffU){
        case MODELNODE_OPCODE_HEADER:node->Data->Header.RwDataIndex=(u16)words;
            words+=sizeof(ModelRwData_HeaderRecord)/4U;break;
        case MODELNODE_OPCODE_LOD:node->Data->LOD.RwDataIndex=(u16)words++;
            node->Child=node->Data->LOD.Affects;break;
        case MODELNODE_OPCODE_BSP:node->Data->BSP.RwDataIndex=(u16)words++;
            reorder_bsp(node,0);break;
        case MODELNODE_OPCODE_SWITCH:node->Data->Switch.RwDataIndex=(u16)words++;
            node->Child=node->Data->Switch.Controls;break;
        case MODELNODE_OPCODE_HEAD:node->Data->HeadPlaceholder.RwDataIndex=(u16)words;
            words+=2U;node->Child=NULL;break;
        case MODELNODE_OPCODE_DLCOLLISION:node->Data->DisplayListCollisions.RwDataIndex=(u16)words;
            words+=2U;break;
        default:break;}
        if(words>INT16_MAX)return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        if(node->Child)node=node->Child;else{while(node&&!node->Next)node=node->Parent;
            if(node)node=node->Next;}
    }
    if(node||!visited)return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    r->rw_words=words;r->header.numRecords=(s16)words;
    return GE_ORIGINAL_CHARACTER_MODEL_OK;
}

static void free_resource(GeCharacterResource *r)
{
    size_t i;if(!r)return;if(r->collision)for(i=0;i<r->node_count;++i){
        free(r->collision[i].vertices);free(r->collision[i].collision_vertices);
        free(r->collision[i].point_usage);}
    free(r->blob);free(r->node_offsets);free(r->rodata_offsets);free(r->nodes);
    free(r->rodatas);free(r->collision);free(r->switches);free(r->textures);
    memset(r,0,sizeof(*r));
}

static GeOriginalCharacterModelStatus relocate(
    GeOriginalCharacterModelProvider *provider,GeCharacterResource *r,
    const ChrModelFileRecord *item)
{
    const ModelFileHeader *meta=item->header;size_t sw,tex,root,i;
    GeOriginalCharacterModelStatus status;
    if(!meta||meta->numSwitches<0||meta->numMatrices<=0||meta->numtextures<0)
        return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    sw=(size_t)meta->numSwitches;tex=(size_t)meta->numtextures;
    root=sw*4U+tex*GE_CHR_TEXTURE_BYTES;
    if(!range_valid(r->blob_size,root,GE_CHR_NODE_BYTES,1))
        return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    status=collect_nodes(provider,r,(uint32_t)root);if(status)return status;
    r->rodata_offsets=calloc(r->node_count,sizeof(*r->rodata_offsets));
    r->nodes=calloc(r->node_count,sizeof(*r->nodes));r->rodatas=calloc(r->node_count,sizeof(*r->rodatas));
    r->collision=calloc(r->node_count,sizeof(*r->collision));
    r->switches=calloc(sw?sw:1,sizeof(*r->switches));r->textures=calloc(tex?tex:1,sizeof(*r->textures));
    if(!r->rodata_offsets||!r->nodes||!r->rodatas||!r->collision||!r->switches||!r->textures)
        return GE_ORIGINAL_CHARACTER_MODEL_ALLOCATION_FAILED;
    for(i=0;i<r->node_count;++i){const uint8_t *raw=r->blob+r->node_offsets[i];uint32_t off;
        uint16_t opcode=be16(raw);if(!seg_offset(be32(raw+4),r->blob_size,rodata_size(opcode),&off))
            return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
        r->rodata_offsets[i]=off;r->nodes[i].Opcode=opcode;r->nodes[i].Data=&r->rodatas[i];}
    for(i=0;i<r->node_count;++i){const uint8_t *raw=r->blob+r->node_offsets[i];ModelNode *o=&r->nodes[i];
        o->Parent=node_address(r,be32(raw+8));o->Next=node_address(r,be32(raw+12));
        o->Prev=node_address(r,be32(raw+16));o->Child=node_address(r,be32(raw+20));
        if((be32(raw+8)&&!o->Parent)||(be32(raw+12)&&!o->Next)||(be32(raw+16)&&!o->Prev)||(be32(raw+20)&&!o->Child))
            return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;}
    for(i=0;i<tex;++i){const uint8_t *raw=r->blob+sw*4U+i*12U;ModelFileTextures *o=&r->textures[i];
        o->TextureID=be32(raw);o->Width=raw[4];o->Height=raw[5];o->MipMapTiles=raw[6];o->Type=raw[7];
        o->RenderDepth=raw[8];o->sflags=raw[9];o->tflags=raw[10];}
    for(i=0;i<sw;++i){uint32_t address=be32(r->blob+i*4);r->switches[i]=node_address(r,address);
        if(address&&!r->switches[i])return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;}
    for(i=0;i<r->node_count;++i){status=decode_rodata(r,i);if(status)return status;}
    r->header=*meta;r->header.RootNode=&r->nodes[0];r->header.Switches=r->switches;r->header.Textures=r->textures;
#if !defined(VERSION_EU)
    r->header.isLoaded=1;
#endif
    status=assign_rw(r);if(status)return status;
    r->native_bytes=r->node_count*(sizeof(*r->node_offsets)+sizeof(*r->rodata_offsets)+sizeof(*r->nodes)+sizeof(*r->rodatas)+sizeof(*r->collision))
        +(sw?sw:1)*sizeof(*r->switches)+(tex?tex:1)*sizeof(*r->textures);
    for(i=0;i<r->node_count;++i)r->native_bytes+=r->collision[i].bytes;
    return GE_ORIGINAL_CHARACTER_MODEL_OK;
}

static GeCharacterResource *find_resource(GeOriginalCharacterModelProvider *p,int32_t id)
{size_t i;for(i=0;i<p->resource_count;++i)if(p->resources[i].model_id==id)return &p->resources[i];return NULL;}

static union ModelRwData *instance_rw_at(uintptr_t *data, size_t index)
{
    union ModelRwData **words = (union ModelRwData **)(void *)data;
    return (union ModelRwData *)(void *)&words[index];
}

static void initialize_resource_rw(GeCharacterResource *r,ModelNode *nodes,
                                   uintptr_t *data)
{
    size_t n;
    for (n=0;n<r->node_count;++n) {
        ModelNode *node=&nodes[n];
        union ModelRwData *rw;
        switch(node->Opcode&0xffU) {
        case MODELNODE_OPCODE_HEADER:
            rw=instance_rw_at(data,node->Data->Header.RwDataIndex);
            memset(&rw->Header,0,sizeof(rw->Header));
            break;
        case MODELNODE_OPCODE_LOD:
            rw=instance_rw_at(data,node->Data->LOD.RwDataIndex);
            rw->LOD.visible=FALSE;
            node->Child=node->Data->LOD.Affects;
            break;
        case MODELNODE_OPCODE_BSP:
            rw=instance_rw_at(data,node->Data->BSP.RwDataIndex);
            rw->BSP.visible=FALSE;
            reorder_bsp(node,0);
            break;
        case MODELNODE_OPCODE_SWITCH:
            rw=instance_rw_at(data,node->Data->Switch.RwDataIndex);
            rw->Switch.visible=TRUE;
            node->Child=node->Data->Switch.Controls;
            break;
        case MODELNODE_OPCODE_HEAD:
            rw=instance_rw_at(data,node->Data->HeadPlaceholder.RwDataIndex);
            rw->HeadPlaceholder.ModelFileHeader=NULL;
            rw->HeadPlaceholder.RwDatas=NULL;
            node->Child=NULL;
            break;
        case MODELNODE_OPCODE_DLCOLLISION:
            rw=instance_rw_at(data,
                node->Data->DisplayListCollisions.RwDataIndex);
            rw->DisplayListCollisions.Vertices=
                node->Data->DisplayListCollisions.Vertices;
            rw->DisplayListCollisions.gdl=
                node->Data->DisplayListCollisions.Primary;
            break;
        default:
            break;
        }
    }
}

static ModelNode *resource_head_placeholder(GeCharacterResource *r,
                                            ModelNode *nodes)
{
    size_t n;
    for(n=0;n<r->node_count;++n)
        if((nodes[n].Opcode&0xffU)==MODELNODE_OPCODE_HEAD)
            return &nodes[n];
    return NULL;
}

static ModelNode *clone_node_pointer(GeCharacterResource *resource,
                                     ModelNode *nodes,ModelNode *source)
{
    uintptr_t address,base,bytes;size_t index;
    if(source==NULL)return NULL;
    address=(uintptr_t)(void *)source;
    base=(uintptr_t)(void *)resource->nodes;
    if(resource->node_count>SIZE_MAX/sizeof(*resource->nodes))return NULL;
    bytes=resource->node_count*sizeof(*resource->nodes);
    if(address<base||address-base>=bytes
            ||(address-base)%sizeof(*resource->nodes)!=0U)return NULL;
    index=(size_t)((address-base)/sizeof(*resource->nodes));
    return &nodes[index];
}

static int clone_resource_topology(GeCharacterResource *resource,
    ModelFileHeader *header,ModelNode **nodes_out,
    union ModelRoData **rodatas_out,ModelNode ***switches_out)
{
    ModelNode *nodes=NULL;union ModelRoData *rodatas=NULL;
    ModelNode **switches=NULL;size_t index;
    nodes=calloc(resource->node_count,sizeof(*nodes));
    rodatas=calloc(resource->node_count,sizeof(*rodatas));
    switches=calloc(resource->header.numSwitches>0
        ?(size_t)resource->header.numSwitches:1U,sizeof(*switches));
    if(nodes==NULL||rodatas==NULL||switches==NULL)goto fail;
    memcpy(rodatas,resource->rodatas,
           resource->node_count*sizeof(*rodatas));
    for(index=0U;index<resource->node_count;++index){
        nodes[index]=resource->nodes[index];
        nodes[index].Data=&rodatas[index];
    }
    for(index=0U;index<resource->node_count;++index){
        ModelNode *node=&nodes[index];
        node->Parent=clone_node_pointer(resource,nodes,
                                        resource->nodes[index].Parent);
        node->Next=clone_node_pointer(resource,nodes,
                                      resource->nodes[index].Next);
        node->Prev=clone_node_pointer(resource,nodes,
                                      resource->nodes[index].Prev);
        node->Child=clone_node_pointer(resource,nodes,
                                       resource->nodes[index].Child);
        switch(node->Opcode&0xffU){
        case MODELNODE_OPCODE_HEADER:
            node->Data->Header.FirstGroupNode=clone_node_pointer(
                resource,nodes,resource->rodatas[index].Header.FirstGroupNode);
            break;
        case MODELNODE_OPCODE_GROUP:
            node->Data->Group.ChildGroupNode=clone_node_pointer(
                resource,nodes,resource->rodatas[index].Group.ChildGroupNode);
            break;
        case MODELNODE_OPCODE_LOD:
            node->Data->LOD.Affects=clone_node_pointer(
                resource,nodes,resource->rodatas[index].LOD.Affects);
            break;
        case MODELNODE_OPCODE_BSP:
            node->Data->BSP.leftChild=clone_node_pointer(
                resource,nodes,resource->rodatas[index].BSP.leftChild);
            node->Data->BSP.rightChild=clone_node_pointer(
                resource,nodes,resource->rodatas[index].BSP.rightChild);
            break;
        case MODELNODE_OPCODE_SHADOW:
            node->Data->Shadow.HeaderNode=clone_node_pointer(
                resource,nodes,resource->rodatas[index].Shadow.HeaderNode);
            break;
        case MODELNODE_OPCODE_SWITCH:
            node->Data->Switch.Controls=clone_node_pointer(
                resource,nodes,resource->rodatas[index].Switch.Controls);
            break;
        default:break;
        }
    }
    for(index=0U;index<(size_t)resource->header.numSwitches;++index){
        switches[index]=clone_node_pointer(
            resource,nodes,resource->header.Switches[index]);
        if(resource->header.Switches[index]!=NULL&&switches[index]==NULL)
            goto fail;
    }
    *header=resource->header;
    header->RootNode=clone_node_pointer(
        resource,nodes,resource->header.RootNode);
    header->Switches=switches;
    if(header->RootNode==NULL)goto fail;
    *nodes_out=nodes;*rodatas_out=rodatas;*switches_out=switches;
    return 1;
fail:
    free(switches);free(rodatas);free(nodes);return 0;
}

static void free_instance_topology(GeCharacterInstance *instance)
{
    free(instance->head_switches);free(instance->head_rodatas);
    free(instance->head_nodes);free(instance->body_switches);
    free(instance->body_rodatas);free(instance->body_nodes);
    instance->head_switches=NULL;instance->head_rodatas=NULL;
    instance->head_nodes=NULL;instance->body_switches=NULL;
    instance->body_rodatas=NULL;instance->body_nodes=NULL;
}

static GeOriginalCharacterModelStatus create_instance(
    GeOriginalCharacterModelProvider *p,GeCharacterResource *body,
    GeCharacterResource *head,int sunglasses,GeCharacterInstance **result)
{
    GeCharacterInstance *i;size_t words=body->rw_words;
    if(head!=NULL && head->rw_words>SIZE_MAX-words)
        return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    if(head!=NULL)words+=head->rw_words;
    if(words>(size_t)INT16_MAX)
        return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    if(p->instance_count>=p->instance_capacity)
        return GE_ORIGINAL_CHARACTER_MODEL_CAPACITY_EXHAUSTED;
    i=&p->instances[p->instance_count];memset(i,0,sizeof(*i));
    if(!clone_resource_topology(body,&i->body_header,&i->body_nodes,
            &i->body_rodatas,&i->body_switches))goto allocation;
    if(head!=NULL&&!clone_resource_topology(head,&i->head_header,
            &i->head_nodes,&i->head_rodatas,&i->head_switches))goto allocation;
    i->render_positions=calloc((size_t)body->header.numMatrices,
                               sizeof(*i->render_positions));
    i->rwdata=calloc(words?words:1,sizeof(*i->rwdata));
    if(!i->render_positions||!i->rwdata){
        goto allocation;
    }
    i->body=body;i->head=head;i->body_id=body->model_id;
    i->head_id=head!=NULL?head->model_id:-1;
    i->model.obj=&i->body_header;i->model.render_pos=i->render_positions;
    i->model.datas=(union ModelRwData **)(void *)i->rwdata;
    i->model.rwdatalen=(s16)words;i->model.scale=1.0f;
    i->model.anim=NULL;i->model.anim2=NULL;i->model.animlooping=0;
    i->model.animflipfunc=0;i->model.unk9c=0;i->model.unka0=0;
    i->model.unk2c=0.0f;i->model.timespeed=0.0f;i->model.unk5c=0.0f;
    i->model.unk7c=0.0f;i->model.unk84=0.0f;i->model.unk88=0.0f;
    i->model.unkb0=0.0f;i->model.speed=1.0f;i->model.speed2=1.0f;
    i->model.playspeed=1.0f;i->model.anim_translation_scale=1.0f;
    i->model.endframe=-1.0f;i->model.unk6c=-1.0f;
    initialize_resource_rw(body,i->body_nodes,i->rwdata);
    if(head!=NULL) {
        ModelNode *placeholder=resource_head_placeholder(body,i->body_nodes);
        uintptr_t *head_data=i->rwdata+body->rw_words;
        ModelRwData_HeadPlaceholderRecord *head_rw;
        ModelNode *root;
        if(placeholder==NULL)goto invalid;
        initialize_resource_rw(head,i->head_nodes,head_data);
        head_rw=&instance_rw_at(i->rwdata,
            placeholder->Data->HeadPlaceholder.RwDataIndex)->HeadPlaceholder;
        head_rw->ModelFileHeader=&i->head_header;
        head_rw->RwDatas=(union ModelRwData **)(void *)head_data;
        placeholder->Child=i->head_header.RootNode;
        for(root=placeholder->Child;root!=NULL;root=root->Next)
            root->Parent=placeholder;
        if(!sunglasses && i->head_header.numSwitches>0
                && i->head_header.Switches[0]!=NULL) {
            ModelNode *node=i->head_header.Switches[0];
            if((node->Opcode&0xffU)!=MODELNODE_OPCODE_SWITCH)goto invalid;
            instance_rw_at(head_data,node->Data->Switch.RwDataIndex)
                ->Switch.visible=FALSE;
            node->Child=NULL;
        }
    }
    i->model.scale=c_item_entries[body->model_id].scale*0.10000001f;
    i->model.anim_translation_scale=c_item_entries[body->model_id].pov;
    i->bytes=(size_t)body->header.numMatrices*sizeof(*i->render_positions)
        +(words?words:1)*sizeof(*i->rwdata)
        +body->node_count*(sizeof(*i->body_nodes)+sizeof(*i->body_rodatas))
        +(body->header.numSwitches>0
            ?(size_t)body->header.numSwitches:1U)*sizeof(*i->body_switches);
    if(head!=NULL)i->bytes+=head->node_count
            *(sizeof(*i->head_nodes)+sizeof(*i->head_rodatas))
        +(head->header.numSwitches>0
            ?(size_t)head->header.numSwitches:1U)*sizeof(*i->head_switches);
    ++p->instance_count;p->native_instance_bytes+=i->bytes;*result=i;
    return GE_ORIGINAL_CHARACTER_MODEL_OK;
invalid:
    free(i->render_positions);free(i->rwdata);free_instance_topology(i);
    memset(i,0,sizeof(*i));
    return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
allocation:
    free(i->render_positions);free(i->rwdata);free_instance_topology(i);
    memset(i,0,sizeof(*i));
    return GE_ORIGINAL_CHARACTER_MODEL_ALLOCATION_FAILED;
}

static GeOriginalCharacterModelStatus load_resource(GeOriginalCharacterModelProvider *p,int32_t id,GeCharacterResource **result)
{
    const ChrModelFileRecord *item;const GeAssetPackEntry *entry;GeCharacterResource next;
    GeOriginalCharacterModelStatus status;char path[GE_CHR_PATH_CAPACITY];int n;
    if(!p||!p->pack||!result)return GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT;
    if(!dependency_for_id(id))return GE_ORIGINAL_CHARACTER_MODEL_INVALID_ID;
    *result=find_resource(p,id);if(*result)return GE_ORIGINAL_CHARACTER_MODEL_OK;
    if(p->resource_count>=p->resource_capacity)return GE_ORIGINAL_CHARACTER_MODEL_CAPACITY_EXHAUSTED;
    item=&c_item_entries[id];if(!item->header||!item->filename||item->scale<=0)
        return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    n=snprintf(path,sizeof(path),"%s%s%s",GE_CHR_PREFIX,item->filename,GE_CHR_SUFFIX);
    if(n<0||(size_t)n>=sizeof(path))return GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT;
    entry=ge_asset_pack_find(p->pack,path);if(!entry||!entry->data_size||entry->data_size>SIZE_MAX)
        return GE_ORIGINAL_CHARACTER_MODEL_NOT_FOUND;
    memset(&next,0,sizeof(next));next.model_id=id;next.blob_size=(size_t)entry->data_size;
    next.blob=malloc(next.blob_size);if(!next.blob)return GE_ORIGINAL_CHARACTER_MODEL_ALLOCATION_FAILED;
    if(ge_asset_pack_read(p->pack,path,next.blob,next.blob_size,NULL)!=GE_ASSET_PACK_OK){free_resource(&next);return GE_ORIGINAL_CHARACTER_MODEL_NOT_FOUND;}
    status=relocate(p,&next,item);if(status){free_resource(&next);return status;}
    p->resources[p->resource_count]=next;*result=&p->resources[p->resource_count++];
    p->source_blob_bytes+=next.blob_size;p->native_resource_bytes+=next.native_bytes;
    return GE_ORIGINAL_CHARACTER_MODEL_OK;
}

GeOriginalCharacterModelProvider *ge_original_character_model_provider_create(GeAssetPack *pack,size_t mc,size_t ic,GeOriginalCharacterModelStatus *out)
{
    GeOriginalCharacterModelProvider *p=NULL;GeOriginalCharacterModelStatus s=GE_ORIGINAL_CHARACTER_MODEL_OK;
    if(!pack||!mc||!ic)s=GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT;else{
        p=calloc(1,sizeof(*p));if(!p)s=GE_ORIGINAL_CHARACTER_MODEL_ALLOCATION_FAILED;else{
            p->resources=calloc(mc,sizeof(*p->resources));p->instances=calloc(ic,sizeof(*p->instances));
            if(!p->resources||!p->instances){free(p->resources);free(p->instances);free(p);p=NULL;s=GE_ORIGINAL_CHARACTER_MODEL_ALLOCATION_FAILED;}
            else{p->pack=pack;p->resource_capacity=mc;p->instance_capacity=ic;p->last_status=s;}}}
    if(out)*out=s;
    return p;
}

void ge_original_character_model_provider_destroy(GeOriginalCharacterModelProvider *p)
{size_t i;if(!p)return;for(i=0;i<p->resource_count;++i)free_resource(&p->resources[i]);
 for(i=0;i<p->instance_count;++i){free(p->instances[i].render_positions);free(p->instances[i].rwdata);free_instance_topology(&p->instances[i]);}
 free(p->resources);free(p->instances);free(p);}

int ge_original_character_models_visit_texture_ids(
    const GeOriginalCharacterModelProvider *provider, void *context,
    int (*visitor)(void *context, uint16_t image_id))
{
    size_t resource_index, texture_index;
    if (provider == NULL || visitor == NULL) return 0;
    /* Validate before the first callback so malformed native IDs cannot
     * partially publish a resource dependency set. Tables remain ROM-owned. */
    for (resource_index = 0U; resource_index < provider->resource_count;
            ++resource_index) {
        const GeCharacterResource *resource = &provider->resources[resource_index];
        if (resource->header.numtextures < 0
                || resource->header.Textures != resource->textures) return 0;
        for (texture_index = 0U;
                texture_index < (size_t)resource->header.numtextures;
                ++texture_index)
            if (resource->textures[texture_index].TextureID > UINT16_MAX)
                return 0;
    }
    for (resource_index = 0U; resource_index < provider->resource_count;
            ++resource_index) {
        const GeCharacterResource *resource = &provider->resources[resource_index];
        for (texture_index = 0U;
                texture_index < (size_t)resource->header.numtextures;
                ++texture_index)
            if (!visitor(context,
                    (uint16_t)resource->textures[texture_index].TextureID))
                return 0;
    }
    return 1;
}

size_t ge_original_character_model_dependency_count(void)
{return sizeof(ge_character_dependencies)/sizeof(ge_character_dependencies[0]);}

int ge_original_character_model_dependency_metadata(size_t index,GeOriginalCharacterModelMetadata *out)
{
    const GeCharacterDependency *d;const ChrModelFileRecord *r;if(!out||index>=ge_original_character_model_dependency_count())return 0;
    d=&ge_character_dependencies[index];r=&c_item_entries[d->model_id];memset(out,0,sizeof(*out));
    out->model_id=d->model_id;out->name=r->filename;out->is_body_dependency=(d->roles&1)!=0;
    out->is_head_dependency=(d->roles&2)!=0;out->is_male=r->isMale;out->has_integrated_head=r->hasHead;
    out->scale=r->scale;out->pov=r->pov;return 1;
}

int ge_original_character_model_load(void *context,int32_t id)
{GeOriginalCharacterModelProvider *p=context;GeCharacterResource *r=NULL;GeOriginalCharacterModelStatus s=load_resource(p,id,&r);if(p)p->last_status=s;return s==0;}
int ge_original_character_model_available(void *context,int32_t id)
{return ge_original_character_model_load(context,id);}

int ge_original_character_model_resolve_instance(void *context,int32_t id,void **hout,void **mout,float *scale,float *pov)
{
    GeOriginalCharacterModelProvider *p=context;GeCharacterResource *r=NULL;GeCharacterInstance *i=NULL;
    GeOriginalCharacterModelStatus s;if(!p||!hout||!mout||!scale||!pov){if(p)p->last_status=GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT;return 0;}
    s=load_resource(p,id,&r);if(!s)s=create_instance(p,r,NULL,0,&i);
    if(s){p->last_status=s;return 0;}
    *hout=&i->body_header;*mout=&i->model;*scale=c_item_entries[id].scale;*pov=c_item_entries[id].pov;
    p->last_status=GE_ORIGINAL_CHARACTER_MODEL_OK;return 1;
}

int ge_original_character_model_resolve_pair(void *context,int32_t body_id,
    int32_t head_id,int sunglasses,GeOriginalCharacterModelPair *pair)
{
    GeOriginalCharacterModelProvider *p=context;GeCharacterResource *body=NULL,*head=NULL;
    GeCharacterInstance *instance=NULL;const GeCharacterDependency *bd,*hd;
    GeOriginalCharacterModelStatus status;
    if(!p||!pair||(sunglasses!=0&&sunglasses!=1)){
        if(p)p->last_status=GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT;
        return 0;
    }
    memset(pair,0,sizeof(*pair));pair->body_id=body_id;pair->head_id=head_id;
    bd=dependency_for_id(body_id);
    if(!bd||(bd->roles&GE_CHR_ROLE_BODY)==0){p->last_status=GE_ORIGINAL_CHARACTER_MODEL_INVALID_ID;return 0;}
    if(c_item_entries[body_id].hasHead){
        if(head_id!=-1){p->last_status=GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT;return 0;}
    }else{
        hd=dependency_for_id(head_id);
        if(head_id<0||!hd||(hd->roles&GE_CHR_ROLE_HEAD)==0){p->last_status=GE_ORIGINAL_CHARACTER_MODEL_INVALID_ID;return 0;}
    }
    status=load_resource(p,body_id,&body);
    if(!status&&head_id>=0)status=load_resource(p,head_id,&head);
    if(!status)status=create_instance(p,body,head,sunglasses,&instance);
    if(status){p->last_status=status;return 0;}
    pair->model_header=&instance->body_header;pair->model_instance=&instance->model;
    pair->scale=c_item_entries[body_id].scale;
    pair->pov=c_item_entries[body_id].pov;
    pair->matrix_count=(size_t)body->header.numMatrices;
    p->last_status=GE_ORIGINAL_CHARACTER_MODEL_OK;return 1;
}

int ge_original_character_model_instance_set_root(void *model_instance,
    const float position[3],float angle)
{
    Model *model=model_instance;ModelNode *root;ModelRwData_HeaderRecord *rw;
    if(!model||!position||!isfinite(position[0])||!isfinite(position[1])
            ||!isfinite(position[2])||!isfinite(angle)||!model->obj
            ||!(root=model->obj->RootNode)
            ||(root->Opcode&0xffU)!=MODELNODE_OPCODE_HEADER)return 0;
    rw=&instance_rw_at((uintptr_t *)(void *)model->datas,
        root->Data->Header.RwDataIndex)->Header;
    rw->pos.x=position[0];rw->pos.y=position[1];rw->pos.z=position[2];
    rw->unk14=angle;rw->unk20=angle;rw->unk30=angle;return 1;
}

static GeCharacterInstance *find_instance(
    const GeOriginalCharacterModelProvider *p,const void *model_instance)
{
    size_t n;
    if(!p||!model_instance)return NULL;
    for(n=0;n<p->instance_count;++n) {
        if(&p->instances[n].model==model_instance)return &p->instances[n];
    }
    return NULL;
}

static void apply_resource_relations(GeCharacterResource *resource,
                                     ModelNode *nodes,uintptr_t *data)
{
    size_t index;
    for(index=0;index<resource->node_count;++index){
        ModelNode *node=&nodes[index];
        union ModelRwData *rw;
        switch(node->Opcode&0xffU){
        case MODELNODE_OPCODE_LOD:
            rw=instance_rw_at(data,node->Data->LOD.RwDataIndex);
            node->Child=rw->LOD.visible?node->Data->LOD.Affects:NULL;
            break;
        case MODELNODE_OPCODE_BSP:
            rw=instance_rw_at(data,node->Data->BSP.RwDataIndex);
            reorder_bsp(node,rw->BSP.visible!=0);
            break;
        case MODELNODE_OPCODE_SWITCH:
            rw=instance_rw_at(data,node->Data->Switch.RwDataIndex);
            node->Child=rw->Switch.visible?node->Data->Switch.Controls:NULL;
            break;
        case MODELNODE_OPCODE_HEAD:
            rw=instance_rw_at(data,node->Data->HeadPlaceholder.RwDataIndex);
            node->Child=rw->HeadPlaceholder.ModelFileHeader!=NULL
                ?rw->HeadPlaceholder.ModelFileHeader->RootNode:NULL;
            if(node->Child!=NULL){
                ModelNode *root;
                for(root=node->Child;root!=NULL;root=root->Next)
                    root->Parent=node;
            }
            break;
        default:
            break;
        }
    }
}

static void apply_instance_relations(GeCharacterInstance *instance)
{
    apply_resource_relations(instance->body,instance->body_nodes,
                             instance->rwdata);
    if(instance->head!=NULL){
        apply_resource_relations(instance->head,instance->head_nodes,
                                 instance->rwdata+instance->body->rw_words);
        /* The head resource relation pass may have inherited another body's
         * parent from a preceding instance. Reassert this instance's exact
         * per-model HEAD relation last, matching modelApplyHeadRelations. */
        apply_resource_relations(instance->body,instance->body_nodes,
                                 instance->rwdata);
    }
}

int ge_original_character_model_prepare_instance_relations(
    GeOriginalCharacterModelProvider *provider,void *model_instance)
{
    GeCharacterInstance *instance=find_instance(provider,model_instance);
    if(instance==NULL){
        if(provider!=NULL)
            provider->last_status=GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT;
        return 0;
    }
    apply_instance_relations(instance);
    provider->last_status=GE_ORIGINAL_CHARACTER_MODEL_OK;
    return 1;
}

static int resource_part_for_node(GeCharacterResource *r,
    const ModelNode *nodes,const ModelNode *node,
    GeOriginalCharacterModelScenePart *out)
{
    size_t n;
    if(!r||!node)return 0;
    for(n=0;n<r->node_count;++n) {
        if(&nodes[n]==node) {
            if((node->Opcode&0xffU)!=MODELNODE_OPCODE_DLCOLLISION)return 0;
            if(out) {
                GeCharacterCollision *c=&r->collision[n];
                out->node=node;
                out->blob=r->blob;out->blob_size=r->blob_size;
                out->primary_offset=c->primary_offset;
                out->secondary_offset=c->secondary_offset;
                out->segment4_offset=c->vertices_offset;
                out->model_type=
                    node->Data->DisplayListCollisions.ModelType;
            }
            return 1;
        }
    }
    return 0;
}

static size_t instance_scene_part(const GeOriginalCharacterModelProvider *p,
    const void *model_instance,size_t wanted,GeOriginalCharacterModelScenePart *out)
{
    GeCharacterInstance *i=find_instance(p,model_instance);ModelNode *node;
    size_t count=0,visited=0,limit;
    if(!i||!i->model.obj)return 0;
    apply_instance_relations(i);
    node=i->model.obj->RootNode;
    limit=(i->body?i->body->node_count:0)+(i->head?i->head->node_count:0)+1U;
    while(node&&visited++<limit){
        if(resource_part_for_node(i->body,i->body_nodes,node,NULL)
                ||resource_part_for_node(i->head,i->head_nodes,node,NULL)){
            if(count==wanted){
                if(out&&!resource_part_for_node(
                        i->body,i->body_nodes,node,out))
                    (void)resource_part_for_node(
                        i->head,i->head_nodes,node,out);
                return count+1U;
            }
            ++count;
        }
        if(node->Child)node=node->Child;else{while(node&&!node->Next)node=node->Parent;if(node)node=node->Next;}
    }
    return node==NULL?count:0;
}

int ge_original_character_model_instance_scene_parts(
    const GeOriginalCharacterModelProvider *provider,
    const void *model_instance,
    GeOriginalCharacterModelScenePart *parts, size_t part_capacity,
    size_t *part_count)
{
    GeCharacterInstance *instance;
    ModelNode *node;
    size_t count = 0U;
    size_t visited = 0U;
    size_t limit;

    if (part_count == NULL
            || (parts == NULL) != (part_capacity == 0U)
            || (instance = find_instance(provider, model_instance)) == NULL
            || instance->model.obj == NULL) return 0;
    apply_instance_relations(instance);
    node = instance->model.obj->RootNode;
    limit = (instance->body != NULL ? instance->body->node_count : 0U)
        + (instance->head != NULL ? instance->head->node_count : 0U) + 1U;
    while (node != NULL && visited++ < limit) {
        GeOriginalCharacterModelScenePart part;
        int found = resource_part_for_node(
            instance->body, instance->body_nodes, node, &part);
        if (!found)
            found = resource_part_for_node(
                instance->head, instance->head_nodes, node, &part);
        if (found) {
            if (parts != NULL) {
                if (count >= part_capacity) return 0;
                parts[count] = part;
            }
            ++count;
        }
        if (node->Child != NULL) {
            node = node->Child;
        } else {
            while (node != NULL && node->Next == NULL) node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
    if (node != NULL) return 0;
    *part_count = count;
    return 1;
}

size_t ge_original_character_model_instance_scene_part_count(
    const GeOriginalCharacterModelProvider *p,const void *model_instance)
{
    size_t count = 0U;
    return ge_original_character_model_instance_scene_parts(
        p, model_instance, NULL, 0U, &count) ? count : 0U;
}

int ge_original_character_model_instance_scene_part(
    const GeOriginalCharacterModelProvider *p,const void *model_instance,
    size_t part_index,GeOriginalCharacterModelScenePart *part)
{return part!=NULL&&instance_scene_part(p,model_instance,part_index,part)==part_index+1U;}

static int instance_node_matrix_index(ModelNode *node)
{
    while(node!=NULL){
        switch(node->Opcode&0xffU){
        case MODELNODE_OPCODE_HEADER:return node->Data->Header.MatrixIndex;
        case MODELNODE_OPCODE_GROUP:return node->Data->Group.MatrixIDs[0];
        case MODELNODE_OPCODE_GROUPSIMPLE:return node->Data->GroupSimple.Group1;
        default:node=node->Parent;break;
        }
    }
    return -1;
}

static GeCharacterResource *instance_resource_for_node(
    GeCharacterInstance *instance,ModelNode *node,ModelNode **nodes_out,
    uintptr_t **rwdata_out)
{
    GeCharacterResource *resources[2]={instance->body,instance->head};
    ModelNode *nodes[2]={instance->body_nodes,instance->head_nodes};
    uintptr_t *data[2]={instance->rwdata,
        instance->head!=NULL?instance->rwdata+instance->body->rw_words:NULL};
    size_t resource_index;
    for(resource_index=0U;resource_index<2U;++resource_index){
        GeCharacterResource *resource=resources[resource_index];
        uintptr_t address,base,bytes;
        if(resource==NULL||nodes[resource_index]==NULL)continue;
        address=(uintptr_t)(void *)node;
        base=(uintptr_t)(void *)nodes[resource_index];
        bytes=resource->node_count*sizeof(*nodes[resource_index]);
        if(address>=base&&address-base<bytes
                &&(address-base)%sizeof(*nodes[resource_index])==0U){
            if(nodes_out!=NULL)*nodes_out=nodes[resource_index];
            if(rwdata_out!=NULL)*rwdata_out=data[resource_index];
            return resource;
        }
    }
    return NULL;
}

static size_t instance_shadow(const GeOriginalCharacterModelProvider *p,
    const void *model_instance,size_t wanted,
    GeOriginalCharacterModelShadow *out)
{
    GeCharacterInstance *instance=find_instance(p,model_instance);
    ModelNode *node;size_t count=0U,visited=0U,limit;
    if(instance==NULL||instance->model.obj==NULL)return 0U;
    apply_instance_relations(instance);node=instance->model.obj->RootNode;
    limit=(instance->body?instance->body->node_count:0U)
        +(instance->head?instance->head->node_count:0U)+1U;
    while(node!=NULL&&visited++<limit){
        if((node->Opcode&0xffU)==MODELNODE_OPCODE_SHADOW){
            if(count==wanted){
                GeCharacterResource *resource;uintptr_t *rwdata;
                ModelRoData_ShadowRecord *record=&node->Data->Shadow;
                ModelNode *resource_nodes;ModelNode *header=record->HeaderNode;
                uintptr_t header_address,nodes_address;
                union ModelRwData *header_rw;
                int matrix_index=instance_node_matrix_index(node);
                resource=instance_resource_for_node(instance,node,
                    &resource_nodes,&rwdata);
                if(out==NULL||resource==NULL||header==NULL||matrix_index<0
                        ||(size_t)matrix_index
                            >=(size_t)instance->model.obj->numMatrices)
                    return 0U;
                header_address=(uintptr_t)(void *)header;
                nodes_address=(uintptr_t)(void *)resource_nodes;
                if(header_address<nodes_address
                        ||header_address-nodes_address
                            >=resource->node_count*sizeof(*resource_nodes)
                        ||(header_address-nodes_address)
                            %sizeof(*resource_nodes)!=0U
                        ||(header->Opcode&0xffU)!=MODELNODE_OPCODE_HEADER)
                    return 0U;
                header_rw=instance_rw_at(
                    rwdata,header->Data->Header.RwDataIndex);
                memset(out,0,sizeof(*out));
                out->position[0]=record->pos.x;out->position[1]=record->pos.y;
                out->size[0]=record->size.x;out->size[1]=record->size.y;
                out->scale=record->Scale;
                out->height_above_ground=
                    header_rw->Header.pos.y-header_rw->Header.ground;
                out->matrix_index=matrix_index;
                if(record->image!=NULL){
                    uintptr_t image=(uintptr_t)record->image;
                    uintptr_t base=(uintptr_t)resource->blob;
                    if(image<base||image-base>resource->blob_size
                            ||resource->blob_size-(size_t)(image-base)<12U)
                        return 0U;
                    out->image_id=be32((const uint8_t *)record->image);
                    out->image_width=((const uint8_t *)record->image)[4];
                    out->image_height=((const uint8_t *)record->image)[5];
                }
                return count+1U;
            }
            ++count;
        }
        if(node->Child!=NULL)node=node->Child;
        else{while(node!=NULL&&node->Next==NULL)node=node->Parent;
            if(node!=NULL)node=node->Next;}
    }
    return node==NULL?count:0U;
}

size_t ge_original_character_model_instance_shadow_count(
    const GeOriginalCharacterModelProvider *p,const void *model_instance)
{return instance_shadow(p,model_instance,SIZE_MAX,NULL);}

int ge_original_character_model_instance_shadow(
    const GeOriginalCharacterModelProvider *p,const void *model_instance,
    size_t shadow_index,GeOriginalCharacterModelShadow *shadow)
{return shadow!=NULL&&instance_shadow(p,model_instance,shadow_index,shadow)
    ==shadow_index+1U;}

size_t ge_original_character_model_scene_part_count(const GeOriginalCharacterModelProvider *p,int32_t id)
{size_t i,n=0;GeCharacterResource *r;if(!p)return 0;r=find_resource((GeOriginalCharacterModelProvider*)p,id);if(!r)return 0;
 for(i=0;i<r->node_count;++i)
     if((r->nodes[i].Opcode&0xffU)==MODELNODE_OPCODE_DLCOLLISION)++n;
 return n;}

int ge_original_character_model_scene_part(const GeOriginalCharacterModelProvider *p,int32_t id,size_t wanted,GeOriginalCharacterModelScenePart *out)
{
    size_t i,n=0;GeCharacterResource *r;if(!p||!out)return 0;r=find_resource((GeOriginalCharacterModelProvider*)p,id);if(!r)return 0;
    for(i=0;i<r->node_count;++i)
        if((r->nodes[i].Opcode&0xffU)==MODELNODE_OPCODE_DLCOLLISION){GeCharacterCollision *c=&r->collision[i];
        if(n++==wanted){out->blob=r->blob;out->blob_size=r->blob_size;out->primary_offset=c->primary_offset;
            out->secondary_offset=c->secondary_offset;out->segment4_offset=c->vertices_offset;return 1;}}
    return 0;
}

GeOriginalCharacterModelStatus ge_original_character_model_last_status(const GeOriginalCharacterModelProvider *p)
{return p?p->last_status:GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT;}
void ge_original_character_model_get_stats(const GeOriginalCharacterModelProvider *p,GeOriginalCharacterModelStats *s)
{if(!s)return;memset(s,0,sizeof(*s));if(!p)return;s->model_capacity=p->resource_capacity;s->loaded_models=p->resource_count;
 s->instance_capacity=p->instance_capacity;s->instantiated_models=p->instance_count;s->source_blob_bytes=p->source_blob_bytes;
 s->native_resource_bytes=p->native_resource_bytes;s->native_instance_bytes=p->native_instance_bytes;s->last_unsupported_opcode=p->last_unsupported_opcode;}
const char *ge_original_character_model_status_name(GeOriginalCharacterModelStatus s)
{switch(s){case GE_ORIGINAL_CHARACTER_MODEL_OK:return "ok";case GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT:return "invalid argument";
case GE_ORIGINAL_CHARACTER_MODEL_INVALID_ID:return "invalid character dependency id";case GE_ORIGINAL_CHARACTER_MODEL_NOT_FOUND:return "asset not found";
case GE_ORIGINAL_CHARACTER_MODEL_CAPACITY_EXHAUSTED:return "capacity exhausted";case GE_ORIGINAL_CHARACTER_MODEL_ALLOCATION_FAILED:return "allocation failed";
case GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT:return "invalid character model layout";case GE_ORIGINAL_CHARACTER_MODEL_UNSUPPORTED_OPCODE:return "unsupported opcode";default:return "unknown";}}

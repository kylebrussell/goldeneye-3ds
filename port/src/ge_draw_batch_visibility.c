#include "ge_draw_batch_visibility.h"

#include <math.h>
#include <stdint.h>

enum {
    GE_DRAW_CLIP_LEFT = 1U << 0,
    GE_DRAW_CLIP_RIGHT = 1U << 1,
    GE_DRAW_CLIP_BOTTOM = 1U << 2,
    GE_DRAW_CLIP_TOP = 1U << 3,
    GE_DRAW_CLIP_NEAR = 1U << 4,
    GE_DRAW_CLIP_FAR = 1U << 5,
    GE_DRAW_CLIP_ALL = (1U << 6) - 1U
};

static uint8_t ge_draw_clip_outcode(const float clip[4])
{
    const float x=clip[0],y=clip[1],z=clip[2],w=clip[3];
    uint8_t outcode=0U;
    if(x < -w)outcode|=GE_DRAW_CLIP_LEFT;
    if(x > w)outcode|=GE_DRAW_CLIP_RIGHT;
    if(y < -w)outcode|=GE_DRAW_CLIP_BOTTOM;
    if(y > w)outcode|=GE_DRAW_CLIP_TOP;
    if(z < -w)outcode|=GE_DRAW_CLIP_NEAR;
    if(z > w)outcode|=GE_DRAW_CLIP_FAR;
    return outcode;
}

static int ge_draw_clip_finite(const float clip[4])
{
    size_t component;
    for(component=0U;component<4U;++component)
        if(!isfinite(clip[component]))return 0;
    return 1;
}

int ge_draw_batch_may_intersect_clip_frustum(
    const GeDamRoomWorldVertex *vertices,size_t vertex_count,
    const GeDamRoomDrawBatch *batch)
{
    uint8_t common=GE_DRAW_CLIP_ALL;
    size_t index;
    if(vertices==NULL||batch==NULL||batch->first_vertex>vertex_count
            ||batch->vertex_count>vertex_count-batch->first_vertex
            ||batch->vertex_count==0U)return 1;
    for(index=batch->first_vertex;
            index<batch->first_vertex+batch->vertex_count;++index){
        const float *clip=vertices[index].processed.clip;
        if(!ge_draw_clip_finite(clip))return 1;
        common=(uint8_t)(common&ge_draw_clip_outcode(clip));
        if(common==0U)return 1;
    }
    return common==0U;
}

int ge_draw_batch_world_may_intersect_clip_frustum(
    const GeDamRoomWorldVertex *vertices,size_t vertex_count,
    const GeDamRoomDrawBatch *batch,const float world_to_clip[4][4])
{
    uint8_t common=GE_DRAW_CLIP_ALL;
    size_t row,column,index;
    if(vertices==NULL||batch==NULL||world_to_clip==NULL
            ||batch->first_vertex>vertex_count
            ||batch->vertex_count>vertex_count-batch->first_vertex
            ||batch->vertex_count==0U)return 1;
    for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
        if(!isfinite(world_to_clip[row][column]))return 1;
    for(index=batch->first_vertex;
            index<batch->first_vertex+batch->vertex_count;++index){
        const float *world=vertices[index].world;
        float clip[4];
        if(!isfinite(world[0])||!isfinite(world[1])||!isfinite(world[2]))
            return 1;
        for(column=0U;column<4U;++column)
            clip[column]=world[0]*world_to_clip[0][column]
                +world[1]*world_to_clip[1][column]
                +world[2]*world_to_clip[2][column]
                +world_to_clip[3][column];
        if(!ge_draw_clip_finite(clip))return 1;
        common=(uint8_t)(common&ge_draw_clip_outcode(clip));
        if(common==0U)return 1;
    }
    return common==0U;
}

#include "ge_draw_batch_visibility.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void set_clip(GeDamRoomWorldVertex *vertex,
                     float x,float y,float z,float w)
{
    memset(vertex,0,sizeof(*vertex));
    vertex->processed.clip[0]=x;
    vertex->processed.clip[1]=y;
    vertex->processed.clip[2]=z;
    vertex->processed.clip[3]=w;
}

static void test_invalid_ranges_fail_open(void)
{
    GeDamRoomWorldVertex vertices[3];
    GeDamRoomDrawBatch batch={0};
    set_clip(&vertices[0],0.0f,0.0f,0.0f,1.0f);
    assert(ge_draw_batch_may_intersect_clip_frustum(NULL,3U,&batch));
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,3U,NULL));
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,3U,&batch));
    batch.first_vertex=4U;batch.vertex_count=1U;
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,3U,&batch));
    batch.first_vertex=2U;batch.vertex_count=SIZE_MAX;
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,3U,&batch));
}

static void test_each_unanimous_plane_rejects(void)
{
    static const float outside[6][4]={
        {-2.0f,0.0f,0.0f,1.0f},{2.0f,0.0f,0.0f,1.0f},
        {0.0f,-2.0f,0.0f,1.0f},{0.0f,2.0f,0.0f,1.0f},
        {0.0f,0.0f,-2.0f,1.0f},{0.0f,0.0f,2.0f,1.0f}
    };
    GeDamRoomWorldVertex vertices[3];
    GeDamRoomDrawBatch batch={0};
    size_t plane,index;
    batch.vertex_count=3U;
    for(plane=0U;plane<6U;++plane){
        for(index=0U;index<3U;++index)set_clip(&vertices[index],
            outside[plane][0],outside[plane][1],outside[plane][2],
            outside[plane][3]);
        assert(!ge_draw_batch_may_intersect_clip_frustum(
            vertices,3U,&batch));
    }
}

static void test_tangent_and_intersecting_ranges_survive(void)
{
    GeDamRoomWorldVertex vertices[4],before[4];
    GeDamRoomDrawBatch batch={0};
    set_clip(&vertices[0],-1.0f,0.0f,0.0f,1.0f);
    set_clip(&vertices[1],-1.0f,0.5f,0.0f,1.0f);
    set_clip(&vertices[2],-1.0f,-0.5f,0.0f,1.0f);
    batch.vertex_count=3U;
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,4U,&batch));
    set_clip(&vertices[0],-2.0f,0.0f,0.0f,1.0f);
    set_clip(&vertices[1],-2.0f,0.5f,0.0f,1.0f);
    set_clip(&vertices[2],0.0f,0.0f,0.0f,1.0f);
    memcpy(before,vertices,sizeof(vertices));
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,4U,&batch));
    assert(memcmp(before,vertices,sizeof(vertices))==0);
    /* No common plane: each point is outside a different plane. */
    set_clip(&vertices[0],-2.0f,0.0f,0.0f,1.0f);
    set_clip(&vertices[1],2.0f,0.0f,0.0f,1.0f);
    set_clip(&vertices[2],0.0f,2.0f,0.0f,1.0f);
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,4U,&batch));
}

static void test_range_and_nonfinite_semantics(void)
{
    GeDamRoomWorldVertex vertices[5];
    GeDamRoomDrawBatch batch={0};
    size_t index;
    set_clip(&vertices[0],0.0f,0.0f,0.0f,1.0f);
    for(index=1U;index<4U;++index)
        set_clip(&vertices[index],2.0f,0.0f,0.0f,1.0f);
    set_clip(&vertices[4],0.0f,0.0f,0.0f,1.0f);
    batch.first_vertex=1U;batch.vertex_count=3U;
    assert(!ge_draw_batch_may_intersect_clip_frustum(vertices,5U,&batch));
    batch.first_vertex=0U;batch.vertex_count=4U;
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,5U,&batch));
    batch.first_vertex=1U;batch.vertex_count=3U;
    vertices[2].processed.clip[1]=NAN;
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,5U,&batch));
    vertices[2].processed.clip[1]=INFINITY;
    assert(ge_draw_batch_may_intersect_clip_frustum(vertices,5U,&batch));
}

static void identity(float matrix[4][4])
{
    size_t row,column;
    memset(matrix,0,sizeof(float)*16U);
    for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
        if(row==column)matrix[row][column]=1.0f;
}

static void test_world_matrix_transform_and_tangency(void)
{
    GeDamRoomWorldVertex vertices[3];
    GeDamRoomDrawBatch batch={0};
    float matrix[4][4];
    size_t index;
    batch.vertex_count=3U;
    identity(matrix);
    for(index=0U;index<3U;++index){
        memset(&vertices[index],0,sizeof(vertices[index]));
        vertices[index].world[0]=-1.0f;
        vertices[index].world[1]=(float)index*0.25f;
    }
    /* Row-vector scale then translation: -1*2+3 is exactly x=w. */
    matrix[0][0]=2.0f;matrix[3][0]=3.0f;
    assert(ge_draw_batch_world_may_intersect_clip_frustum(
        vertices,3U,&batch,matrix));
    matrix[3][0]=3.0001f;
    assert(!ge_draw_batch_world_may_intersect_clip_frustum(
        vertices,3U,&batch,matrix));
    vertices[2].world[0]=-2.0f;
    assert(ge_draw_batch_world_may_intersect_clip_frustum(
        vertices,3U,&batch,matrix));
}

static void test_world_matrix_invalid_data_fails_open(void)
{
    GeDamRoomWorldVertex vertices[3];
    GeDamRoomDrawBatch batch={0};
    float matrix[4][4];
    size_t index;
    batch.vertex_count=3U;identity(matrix);
    for(index=0U;index<3U;++index){
        memset(&vertices[index],0,sizeof(vertices[index]));
        vertices[index].world[0]=2.0f;
    }
    assert(!ge_draw_batch_world_may_intersect_clip_frustum(
        vertices,3U,&batch,matrix));
    matrix[2][1]=NAN;
    assert(ge_draw_batch_world_may_intersect_clip_frustum(
        vertices,3U,&batch,matrix));
    identity(matrix);vertices[1].world[2]=INFINITY;
    assert(ge_draw_batch_world_may_intersect_clip_frustum(
        vertices,3U,&batch,matrix));
    vertices[1].world[2]=0.0f;vertices[0].world[0]=FLT_MAX;
    matrix[0][0]=FLT_MAX;
    assert(ge_draw_batch_world_may_intersect_clip_frustum(
        vertices,3U,&batch,matrix));
    assert(ge_draw_batch_world_may_intersect_clip_frustum(
        vertices,3U,&batch,NULL));
}

int main(void)
{
    test_invalid_ranges_fail_open();
    test_each_unanimous_plane_rejects();
    test_tangent_and_intersecting_ranges_survive();
    test_range_and_nonfinite_semantics();
    test_world_matrix_transform_and_tangency();
    test_world_matrix_invalid_data_fails_open();
    puts("draw-batch homogeneous visibility tests passed");
    return 0;
}

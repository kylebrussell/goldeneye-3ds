#include "ge_draw_batch_visibility.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* The pre-optimization renderer composed these three public operations. */
static GeDrawBatchVisibility reference_visibility(
    const GeDamRoomWorldVertex *vertices, size_t count,
    const GeDamRoomDrawBatch *batch, const GeDrawBatchWorldBounds *bounds,
    const float matrix[4][4])
{
    if (ge_draw_batch_world_first_vertex_visible(vertices, count, batch, matrix))
        return GE_DRAW_BATCH_FIRST_VERTEX_VISIBLE;
    const GeDrawBatchBoundsVisibility bounded =
        ge_draw_batch_world_bounds_classify(bounds, matrix);
    if (bounded == GE_DRAW_BATCH_BOUNDS_INSIDE) return GE_DRAW_BATCH_BOUNDS_VISIBLE;
    if (bounded == GE_DRAW_BATCH_BOUNDS_OUTSIDE) return GE_DRAW_BATCH_BOUNDS_CULLED;
    return ge_draw_batch_world_may_intersect_clip_frustum(vertices, count, batch, matrix)
        ? GE_DRAW_BATCH_VERTICES_VISIBLE : GE_DRAW_BATCH_VERTICES_CULLED;
}

static void check_prepared(const GeDamRoomWorldVertex *vertices, size_t count,
    const GeDamRoomDrawBatch *batch, const GeDrawBatchWorldBounds *bounds,
    const float matrix[4][4])
{
    GeDrawBatchClipContext context;
    ge_draw_batch_clip_context_init(&context, matrix);
    assert(ge_draw_batch_world_visibility_prepared(vertices, count, batch, bounds,
        &context) == reference_visibility(vertices, count, batch, bounds, matrix));
    assert(ge_draw_batch_world_visibility_prepared(vertices, count, batch, NULL,
        &context) == reference_visibility(vertices, count, batch, NULL, matrix));
}

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

static float random_coordinate(uint32_t *seed)
{
    *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223);
    return ((float)(*seed >> 8U) / 16777216.0f - 0.5f) * 8.0f;
}

static void test_bounds_match_exact_vertex_decisions(void)
{
    GeDamRoomWorldVertex vertices[24] = {0};
    GeDamRoomDrawBatch batch = {0};
    GeDrawBatchWorldBounds bounds;
    float matrix[4][4];
    uint32_t seed = 91U;
    size_t sample, index, axis, column, inside = 0U, outside = 0U;
    batch.first_vertex = 2U;
    batch.vertex_count = 21U;
    for (sample = 0U; sample < 20000U; ++sample) {
        float center[3];
        GeDrawBatchBoundsVisibility classified;
        int exact;
        for (axis = 0U; axis < 3U; ++axis)
            center[axis] = random_coordinate(&seed) * 100.0f;
        for (index = 0U; index < 24U; ++index)
            for (axis = 0U; axis < 3U; ++axis)
                vertices[index].world[axis] = center[axis]
                    + random_coordinate(&seed);
        for (axis = 0U; axis < 4U; ++axis)
            for (column = 0U; column < 4U; ++column)
                matrix[axis][column] = random_coordinate(&seed);
        assert(ge_draw_batch_world_bounds_build(vertices, 24U, &batch, &bounds));
        classified = ge_draw_batch_world_bounds_classify(&bounds, matrix);
        exact = ge_draw_batch_world_may_intersect_clip_frustum(
            vertices, 24U, &batch, matrix);
        check_prepared(vertices, 24U, &batch, &bounds, matrix);
        /* The early point test may accept, but can never reject on its own.
         * Composition with the existing bounds/scalar path is byte-for-byte
         * the same visibility decision, including points behind the eye. */
        assert((ge_draw_batch_world_first_vertex_visible(
                    vertices, 24U, &batch, matrix)
                || classified == GE_DRAW_BATCH_BOUNDS_INSIDE
                || (classified == GE_DRAW_BATCH_BOUNDS_UNCERTAIN && exact))
            == exact);
        if (classified == GE_DRAW_BATCH_BOUNDS_INSIDE) {
            ++inside;
            assert(exact);
        } else if (classified == GE_DRAW_BATCH_BOUNDS_OUTSIDE) {
            ++outside;
            assert(!exact);
        }
    }
    assert(inside > 0U && outside > 0U);
    printf("bounded frustum: 20000 exact comparisons, %zu inside, %zu outside\n",
           inside, outside);
}

static void test_bounds_tangency_and_invalidation(void)
{
    GeDamRoomWorldVertex vertices[3] = {0};
    GeDamRoomDrawBatch batch = {0};
    GeDrawBatchWorldBounds bounds;
    float matrix[4][4];
    size_t axis, index;
    identity(matrix);
    batch.vertex_count = 3U;
    for (axis = 0U; axis < 3U; ++axis) {
        memset(vertices, 0, sizeof(vertices));
        for (index = 0U; index < 3U; ++index) vertices[index].world[axis] = 1.0f;
        assert(ge_draw_batch_world_bounds_build(vertices, 3U, &batch, &bounds));
        assert(ge_draw_batch_world_bounds_classify(&bounds, matrix)
               == GE_DRAW_BATCH_BOUNDS_INSIDE);
        assert(ge_draw_batch_world_first_vertex_visible(vertices, 3U, &batch, matrix));
        for (index = 0U; index < 3U; ++index)
            vertices[index].world[axis] = nextafterf(1.0f, INFINITY);
        assert(ge_draw_batch_world_bounds_build(vertices, 3U, &batch, &bounds));
        assert(ge_draw_batch_world_bounds_classify(&bounds, matrix)
               == GE_DRAW_BATCH_BOUNDS_OUTSIDE);
        for (index = 0U; index < 3U; ++index)
            vertices[index].world[axis] = nextafterf(-1.0f, -INFINITY);
        assert(ge_draw_batch_world_bounds_build(vertices, 3U, &batch, &bounds));
        assert(ge_draw_batch_world_bounds_classify(&bounds, matrix)
               == GE_DRAW_BATCH_BOUNDS_OUTSIDE);
    }
    matrix[3][3] = NAN;
    assert(ge_draw_batch_world_first_vertex_visible(vertices, 3U, &batch, matrix));
    assert(ge_draw_batch_world_bounds_classify(&bounds, matrix)
           == GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    identity(matrix);
    matrix[2][2] = FLT_MAX;
    assert(ge_draw_batch_world_bounds_classify(&bounds, matrix)
           == GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    identity(matrix);
    vertices[1].world[1] = INFINITY;
    assert(!ge_draw_batch_world_bounds_build(vertices, 3U, &batch, &bounds));
    assert(!bounds.valid);
    assert(ge_draw_batch_world_bounds_classify(&bounds, matrix)
           == GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    assert(!ge_draw_batch_world_bounds_build(NULL, 3U, &batch, &bounds));
    batch.first_vertex = SIZE_MAX;
    assert(!ge_draw_batch_world_bounds_build(vertices, 3U, &batch, &bounds));
    assert(ge_draw_batch_world_bounds_classify(NULL, matrix)
           == GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    assert(ge_draw_batch_world_first_vertex_visible(vertices, 3U, &batch, matrix));
    assert(ge_draw_batch_world_first_vertex_visible(NULL, 3U, &batch, matrix));
    assert(ge_draw_batch_world_first_vertex_visible(vertices, 3U, NULL, matrix));
    assert(ge_draw_batch_world_first_vertex_visible(vertices, 3U, &batch, NULL));
}

static void test_prepared_invalid_inputs_and_snapshot_lifetime(void)
{
    GeDamRoomWorldVertex vertices[3] = {0};
    GeDamRoomDrawBatch batch = {.vertex_count = 3U};
    GeDrawBatchWorldBounds bounds;
    GeDrawBatchClipContext context;
    float matrix[4][4];
    identity(matrix);
    for (size_t i = 0U; i < 3U; ++i) vertices[i].world[0] = 2.0f;
    assert(ge_draw_batch_world_bounds_build(vertices, 3U, &batch, &bounds));
    ge_draw_batch_clip_context_init(&context, matrix);
    assert(context.finite);
    assert(ge_draw_batch_world_visibility_prepared(vertices, 3U, &batch, &bounds,
        &context) == GE_DRAW_BATCH_BOUNDS_CULLED);
    matrix[3][0] = -2.0f;
    assert(ge_draw_batch_world_visibility_prepared(vertices, 3U, &batch, &bounds,
        &context) == GE_DRAW_BATCH_BOUNDS_CULLED); /* Snapshot, not pointer. */
    ge_draw_batch_clip_context_init(&context, matrix);
    assert(ge_draw_batch_world_visibility_prepared(vertices, 3U, &batch, &bounds,
        &context) == GE_DRAW_BATCH_FIRST_VERTEX_VISIBLE);
    for (size_t row = 0U; row < 4U; ++row) {
        for (size_t column = 0U; column < 4U; ++column) {
            identity(matrix);
            matrix[row][column] = NAN;
            check_prepared(vertices, 3U, &batch, &bounds, matrix);
            matrix[row][column] = -INFINITY;
            check_prepared(vertices, 3U, &batch, &bounds, matrix);
        }
    }
    identity(matrix);
    bounds.valid = 0;
    for (size_t i = 0U; i < 3U; ++i) {
        vertices[i].world[1] = INFINITY;
        check_prepared(vertices, 3U, &batch, &bounds, matrix);
        vertices[i].world[1] = NAN;
        check_prepared(vertices, 3U, &batch, &bounds, matrix);
        vertices[i].world[1] = 0.0f;
    }
    vertices[0].world[0] = FLT_MAX;
    matrix[0][0] = FLT_MAX;
    check_prepared(vertices, 3U, &batch, &bounds, matrix);
    check_prepared(NULL, 3U, &batch, &bounds, matrix);
    check_prepared(vertices, 3U, NULL, &bounds, matrix);
    check_prepared(vertices, 3U, &batch, &bounds, NULL);
    batch.first_vertex = SIZE_MAX;
    check_prepared(vertices, 3U, &batch, &bounds, matrix);
    batch.first_vertex = 1U;
    batch.vertex_count = SIZE_MAX;
    check_prepared(vertices, 3U, &batch, &bounds, matrix);
    batch.vertex_count = 0U;
    check_prepared(vertices, 3U, &batch, &bounds, matrix);
    ge_draw_batch_clip_context_init(&context, NULL);
    assert(!context.finite);
    ge_draw_batch_clip_context_init(NULL, matrix);
    assert(ge_draw_batch_world_visibility_prepared(vertices, 3U, &batch, &bounds,
        NULL) == GE_DRAW_BATCH_FIRST_VERTEX_VISIBLE);
}

static void test_prepared_clip_boundary_bits(void)
{
    static const float coordinates[] = {
        -FLT_MAX, -2.0f, -1.0f, -FLT_MIN, -0.0f, 0.0f, FLT_MIN, 1.0f, 2.0f, FLT_MAX,
    };
    GeDamRoomWorldVertex vertices[3] = {0};
    GeDamRoomDrawBatch batch = {.vertex_count = 3U};
    GeDrawBatchWorldBounds bounds;
    float matrix[4][4];
    for (size_t i = 0U; i < sizeof(coordinates) / sizeof(coordinates[0]); ++i) {
        for (size_t j = 0U; j < sizeof(coordinates) / sizeof(coordinates[0]); ++j) {
            identity(matrix);
            matrix[3][3] = coordinates[j];
            for (size_t axis = 0U; axis < 3U; ++axis) {
                vertices[0].world[axis] = coordinates[i];
                vertices[1].world[axis] = nextafterf(coordinates[i], INFINITY);
                vertices[2].world[axis] = nextafterf(coordinates[i], -INFINITY);
                (void)ge_draw_batch_world_bounds_build(vertices, 3U, &batch, &bounds);
                check_prepared(vertices, 3U, &batch, &bounds, matrix);
            }
        }
    }
}

#ifdef GE_DRAW_BATCH_VISIBILITY_BENCH
static void benchmark_prepared_visibility(void)
{
    GeDamRoomWorldVertex vertices[24] = {0};
    GeDamRoomDrawBatch batch = {.vertex_count = 24U};
    GeDrawBatchWorldBounds bounds;
    GeDrawBatchClipContext context;
    float matrix[4][4];
    const size_t iterations = 1000000U;
    volatile size_t checksum = 0U;
    for (size_t mode = 0U; mode < 3U; ++mode) {
        identity(matrix);
        for (size_t i = 0U; i < 24U; ++i) {
            vertices[i].world[0] = mode == 0U ? 0.5f : 2.0f;
            vertices[i].world[1] = 0.0f;
        }
        /* Third case has an outside first vertex and needs the full walk. */
        if (mode == 2U) vertices[23].world[0] = 0.0f;
        assert(ge_draw_batch_world_bounds_build(vertices, 24U, &batch, &bounds));
        if (mode == 2U) bounds.valid = 0;
        ge_draw_batch_clip_context_init(&context, matrix);
        clock_t begin = clock();
        for (size_t i = 0U; i < iterations; ++i)
            checksum += (size_t)reference_visibility(vertices, 24U, &batch, &bounds, matrix);
        clock_t middle = clock();
        for (size_t i = 0U; i < iterations; ++i)
            checksum += (size_t)ge_draw_batch_world_visibility_prepared(
                vertices, 24U, &batch, &bounds, &context);
        clock_t end = clock();
        printf("visibility mode %zu: original %.2f ms, prepared %.2f ms (%zu)\n",
            mode, 1000.0 * (double)(middle - begin) / CLOCKS_PER_SEC,
            1000.0 * (double)(end - middle) / CLOCKS_PER_SEC, (size_t)checksum);
    }
}
#endif

int main(void)
{
    test_invalid_ranges_fail_open();
    test_each_unanimous_plane_rejects();
    test_tangent_and_intersecting_ranges_survive();
    test_range_and_nonfinite_semantics();
    test_world_matrix_transform_and_tangency();
    test_world_matrix_invalid_data_fails_open();
    test_bounds_match_exact_vertex_decisions();
    test_bounds_tangency_and_invalidation();
    test_prepared_invalid_inputs_and_snapshot_lifetime();
    test_prepared_clip_boundary_bits();
#ifdef GE_DRAW_BATCH_VISIBILITY_BENCH
    benchmark_prepared_visibility();
#endif
    puts("draw-batch homogeneous visibility tests passed");
    return 0;
}

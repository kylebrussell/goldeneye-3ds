#include "ge_draw_batch_visibility.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

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

static int ge_draw_world_outcode(const float world[3],
    const float world_to_clip[4][4], uint8_t *outcode)
{
    float clip[4];
    size_t column;
    if (!isfinite(world[0]) || !isfinite(world[1]) || !isfinite(world[2]))
        return 0;
    for (column = 0U; column < 4U; ++column)
        clip[column] = world[0] * world_to_clip[0][column]
            + world[1] * world_to_clip[1][column]
            + world[2] * world_to_clip[2][column] + world_to_clip[3][column];
    if (!ge_draw_clip_finite(clip)) return 0;
    *outcode = ge_draw_clip_outcode(clip);
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
        uint8_t outcode;
        if (!ge_draw_world_outcode(vertices[index].world, world_to_clip, &outcode))
            return 1;
        common=(uint8_t)(common&outcode);
        if(common==0U)return 1;
    }
    return common==0U;
}

int ge_draw_batch_world_first_vertex_visible(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batch, const float world_to_clip[4][4])
{
    uint8_t outcode;
    size_t row, column;
    if (vertices == NULL || batch == NULL || world_to_clip == NULL
            || batch->first_vertex > vertex_count || batch->vertex_count == 0U
            || batch->vertex_count > vertex_count - batch->first_vertex)
        return 1;
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            if (!isfinite(world_to_clip[row][column])) return 1;
    return !ge_draw_world_outcode(vertices[batch->first_vertex].world,
        world_to_clip, &outcode) || outcode == 0U;
}

int ge_draw_batch_world_bounds_build(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batch, GeDrawBatchWorldBounds *bounds)
{
    size_t index, axis;
    if (bounds == NULL) return 0;
    bounds->valid = 0;
    if (vertices == NULL || batch == NULL || batch->vertex_count == 0U
            || batch->first_vertex > vertex_count
            || batch->vertex_count > vertex_count - batch->first_vertex)
        return 0;
    for (axis = 0U; axis < 3U; ++axis) {
        bounds->minimum[axis] = vertices[batch->first_vertex].world[axis];
        bounds->maximum[axis] = bounds->minimum[axis];
    }
    for (index = batch->first_vertex;
            index < batch->first_vertex + batch->vertex_count; ++index) {
        for (axis = 0U; axis < 3U; ++axis) {
            const float value = vertices[index].world[axis];
            if (!isfinite(value)) return 0;
            if (value < bounds->minimum[axis]) bounds->minimum[axis] = value;
            if (value > bounds->maximum[axis]) bounds->maximum[axis] = value;
        }
    }
    bounds->valid = 1;
    return 1;
}

static GeDrawBatchBoundsVisibility ge_draw_batch_world_bounds_classify_impl(
    const GeDrawBatchWorldBounds *bounds, const float world_to_clip[4][4],
    const uint8_t negative[3][4], int *finite_interval)
{
    float minimum[4], maximum[4];
    size_t column, axis;
    int inside = 1;
    if (finite_interval != NULL) *finite_interval = 0;
    if (bounds == NULL || !bounds->valid || world_to_clip == NULL)
        return GE_DRAW_BATCH_BOUNDS_UNCERTAIN;
    for (axis = 0U; axis < 3U; ++axis)
        if (!isfinite(bounds->minimum[axis])
                || !isfinite(bounds->maximum[axis])
                || bounds->minimum[axis] > bounds->maximum[axis])
            return GE_DRAW_BATCH_BOUNDS_UNCERTAIN;
    for (column = 0U; column < 4U; ++column) {
        float lower[3], upper[3];
        for (axis = 0U; axis < 3U; ++axis) {
            const float coefficient = world_to_clip[axis][column];
            if (negative == NULL && !isfinite(coefficient))
                return GE_DRAW_BATCH_BOUNDS_UNCERTAIN;
            /* Every bound in this pass uses the same camera coefficients.
             * Snapshot their comparisons once; retain each multiply/add and
             * its grouping, including signed-zero and overflow behavior. */
            const int reverse = negative != NULL
                ? negative[axis][column] : coefficient < 0.0f;
            lower[axis] = coefficient * (reverse
                ? bounds->maximum[axis] : bounds->minimum[axis]);
            upper[axis] = coefficient * (reverse
                ? bounds->minimum[axis] : bounds->maximum[axis]);
        }
        if (negative == NULL && !isfinite(world_to_clip[3][column]))
            return GE_DRAW_BATCH_BOUNDS_UNCERTAIN;
        /* Floating-point addition and multiplication are monotonic here.
         * Keep the exact scalar transform's grouping: combining matrix
         * columns into planes first could disagree at a clip boundary. */
        minimum[column] = lower[0] + lower[1] + lower[2]
            + world_to_clip[3][column];
        maximum[column] = upper[0] + upper[1] + upper[2]
            + world_to_clip[3][column];
        if (!isfinite(minimum[column]) || !isfinite(maximum[column]))
            return GE_DRAW_BATCH_BOUNDS_UNCERTAIN;
    }
    if (finite_interval != NULL) *finite_interval = 1;
    for (axis = 0U; axis < 3U; ++axis) {
        if (maximum[axis] < -maximum[3] || minimum[axis] > maximum[3])
            return GE_DRAW_BATCH_BOUNDS_OUTSIDE;
        if (minimum[axis] < -minimum[3] || maximum[axis] > minimum[3])
            inside = 0;
    }
    return inside ? GE_DRAW_BATCH_BOUNDS_INSIDE
                  : GE_DRAW_BATCH_BOUNDS_UNCERTAIN;
}

GeDrawBatchBoundsVisibility ge_draw_batch_world_bounds_classify(
    const GeDrawBatchWorldBounds *bounds, const float world_to_clip[4][4])
{
    return ge_draw_batch_world_bounds_classify_impl(bounds, world_to_clip, NULL, NULL);
}

void ge_draw_batch_clip_context_init(
    GeDrawBatchClipContext *context, const float world_to_clip[4][4])
{
    size_t row, column;
    if (context == NULL) return;
    context->finite = 0;
    if (world_to_clip == NULL) return;
    /* Copy rather than retain the caller's mutable camera matrix. */
    memcpy(context->world_to_clip, world_to_clip, sizeof(context->world_to_clip));
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            if (!isfinite(context->world_to_clip[row][column])) return;
    for (row = 0U; row < 3U; ++row)
        for (column = 0U; column < 4U; ++column)
            context->negative[row][column] = (uint8_t)(
                context->world_to_clip[row][column] < 0.0f);
    context->finite = 1;
}

GeDrawBatchBoundsVisibility ge_draw_batch_world_bounds_classify_prepared(
    const GeDrawBatchWorldBounds *bounds, const GeDrawBatchClipContext *context)
{
    if (context == NULL || !context->finite)
        return GE_DRAW_BATCH_BOUNDS_UNCERTAIN;
    return ge_draw_batch_world_bounds_classify_impl(
        bounds, context->world_to_clip, context->negative, NULL);
}

/* Only used after the containing bounds' exact interval transform proves all
 * four clip coordinates finite for every vertex. An outcode AND can only lose
 * bits: axes with no remaining common plane cannot affect its result. Keep
 * the scalar arithmetic for W and each still-relevant axis unchanged. Without
 * that finite proof, an omitted axis might overflow and require fail-open. */
static uint8_t ge_draw_world_common_outcode(const float world[3],
    const float matrix[4][4], uint8_t common)
{
    const float w = world[0] * matrix[0][3] + world[1] * matrix[1][3]
        + world[2] * matrix[2][3] + matrix[3][3];
    uint8_t outcode = 0U;
    for (size_t axis = 0U; axis < 3U; ++axis) {
        const uint8_t low = (uint8_t)(1U << (axis * 2U));
        const uint8_t high = (uint8_t)(low << 1U);
        if ((common & (low | high)) != 0U) {
            const float clip = world[0] * matrix[0][axis]
                + world[1] * matrix[1][axis]
                + world[2] * matrix[2][axis] + matrix[3][axis];
            if (clip < -w) outcode = (uint8_t)(outcode | low);
            if (clip > w) outcode = (uint8_t)(outcode | high);
        }
    }
    return (uint8_t)(common & outcode);
}

GeDrawBatchVisibility ge_draw_batch_world_visibility_prepared(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batch, const GeDrawBatchWorldBounds *bounds,
    const GeDrawBatchClipContext *context)
{
    uint8_t common;
    size_t index;
    int finite_interval;
    GeDrawBatchBoundsVisibility bounded;
    if (vertices == NULL || batch == NULL || context == NULL || !context->finite
            || batch->first_vertex > vertex_count || batch->vertex_count == 0U
            || batch->vertex_count > vertex_count - batch->first_vertex)
        return GE_DRAW_BATCH_FIRST_VERTEX_VISIBLE;
    if (!ge_draw_world_outcode(vertices[batch->first_vertex].world,
            context->world_to_clip, &common) || common == 0U)
        return GE_DRAW_BATCH_FIRST_VERTEX_VISIBLE;
    bounded = ge_draw_batch_world_bounds_classify_impl(
        bounds, context->world_to_clip, context->negative, &finite_interval);
    if (bounded == GE_DRAW_BATCH_BOUNDS_INSIDE)
        return GE_DRAW_BATCH_BOUNDS_VISIBLE;
    if (bounded == GE_DRAW_BATCH_BOUNDS_OUTSIDE)
        return GE_DRAW_BATCH_BOUNDS_CULLED;
    /* Continue the original outcode AND from the first vertex; recomputing
     * that transform cannot add information. Retain traversal/FP order. */
    if (finite_interval) {
        for (index = batch->first_vertex + 1U;
                index < batch->first_vertex + batch->vertex_count; ++index) {
            common = ge_draw_world_common_outcode(vertices[index].world,
                context->world_to_clip, common);
            if (common == 0U) return GE_DRAW_BATCH_VERTICES_VISIBLE;
        }
        return GE_DRAW_BATCH_VERTICES_CULLED;
    }
    for (index = batch->first_vertex + 1U;
            index < batch->first_vertex + batch->vertex_count; ++index) {
        uint8_t outcode;
        if (!ge_draw_world_outcode(vertices[index].world,
                context->world_to_clip, &outcode))
            return GE_DRAW_BATCH_VERTICES_VISIBLE;
        common = (uint8_t)(common & outcode);
        if (common == 0U) return GE_DRAW_BATCH_VERTICES_VISIBLE;
    }
    return GE_DRAW_BATCH_VERTICES_CULLED;
}

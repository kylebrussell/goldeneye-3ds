#ifndef GE_DRAW_BATCH_VISIBILITY_H
#define GE_DRAW_BATCH_VISIBILITY_H

#include "ge_dam_room.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns nonzero unless an authored batch is provably wholly outside one
 * canonical homogeneous clip plane. Invalid ranges and nonfinite clip data
 * fail open. This is renderer publication only; it does not alter rooms,
 * batches, vertices, portal state, collision, or gameplay visibility. */
int ge_draw_batch_may_intersect_clip_frustum(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batch);

/* Variant for GPU-world source vertices whose processed.clip has not been
 * published. Matrices follow the decomp/libultra row-vector convention:
 * clip[j] = world.x*M[0][j] + world.y*M[1][j]
 *         + world.z*M[2][j] + M[3][j].
 * The source and matrix are read only. Invalid/nonfinite inputs fail open. */
int ge_draw_batch_world_may_intersect_clip_frustum(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batch,
    const float world_to_clip[4][4]);

/* A real on-screen vertex proves the batch cannot have a unanimous outside
 * clip plane. Test this before the more expensive bounds interval transform.
 * Invalid/nonfinite first vertices or matrices fail open just like the full
 * scalar walk. A zero result needs bounds/the full walk, never rejection. */
int ge_draw_batch_world_first_vertex_visible(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batch, const float world_to_clip[4][4]);

/* Renderer-only bounds, rebuilt when the corresponding GPU vertex range is
 * published (not when the camera moves). Invalid bounds always fail open. */
typedef struct GeDrawBatchWorldBounds {
    float minimum[3];
    float maximum[3];
    int valid;
} GeDrawBatchWorldBounds;

typedef enum GeDrawBatchBoundsVisibility {
    GE_DRAW_BATCH_BOUNDS_UNCERTAIN = 0,
    GE_DRAW_BATCH_BOUNDS_OUTSIDE,
    GE_DRAW_BATCH_BOUNDS_INSIDE
} GeDrawBatchBoundsVisibility;

int ge_draw_batch_world_bounds_build(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batch, GeDrawBatchWorldBounds *bounds);

/* Interval transforms preserve the scalar vertex transform's operation order.
 * OUTSIDE proves a unanimous clip outcode; INSIDE proves every vertex inside.
 * An intersecting box is UNCERTAIN: use the existing exact vertex test, so a
 * loose box never changes draw merging or the original rejection decisions. */
GeDrawBatchBoundsVisibility ge_draw_batch_world_bounds_classify(
    const GeDrawBatchWorldBounds *bounds, const float world_to_clip[4][4]);

/* Immutable per-render-pass matrix snapshot. Initialize once per coordinate
 * space after camera publication; do not reuse it across camera changes. */
typedef struct GeDrawBatchClipContext {
    float world_to_clip[4][4];
    int finite;
} GeDrawBatchClipContext;

typedef enum GeDrawBatchVisibility {
    GE_DRAW_BATCH_FIRST_VERTEX_VISIBLE = 0,
    GE_DRAW_BATCH_BOUNDS_VISIBLE,
    GE_DRAW_BATCH_BOUNDS_CULLED,
    GE_DRAW_BATCH_VERTICES_VISIBLE,
    GE_DRAW_BATCH_VERTICES_CULLED
} GeDrawBatchVisibility;

void ge_draw_batch_clip_context_init(
    GeDrawBatchClipContext *context, const float world_to_clip[4][4]);

/* Conservative whole-range proof using the same per-pass camera snapshot.
 * UNCERTAIN requires the existing per-batch path; never means invisible. */
GeDrawBatchBoundsVisibility ge_draw_batch_world_bounds_classify_prepared(
    const GeDrawBatchWorldBounds *bounds, const GeDrawBatchClipContext *context);

/* Exact composition of first-vertex acceptance, optional bounds, and scalar
 * outcode intersection. Reuses matrix validation and the first outcode; no
 * plane extraction/reassociation, approximate rejection, or altered order.
 * Bounds must describe this vertex range (NULL is supported). Invalid input
 * fails open. The reason preserves the renderer's profiling counters. */
GeDrawBatchVisibility ge_draw_batch_world_visibility_prepared(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batch, const GeDrawBatchWorldBounds *bounds,
    const GeDrawBatchClipContext *context);

#ifdef __cplusplus
}
#endif

#endif

#ifndef GE_GBI_CLIP_H
#define GE_GBI_CLIP_H

#include "ge_gbi_vertex.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A triangle clipped by the six canonical homogeneous frustum planes plus
 * the positive-W guard cannot exceed these bounds.  The deliberately loose
 * public bounds keep the implementation allocation-free and make malformed
 * input fail closed instead of writing past caller storage.
 */
#define GE_GBI_CLIP_MAX_POLYGON_VERTICES 12U
#define GE_GBI_CLIP_MAX_TRIANGLES 10U

typedef struct GeGbiClippedTriangle {
    GeGbiProcessedVertex vertices[3];
} GeGbiClippedTriangle;

typedef struct GeGbiClipResult {
    GeGbiClippedTriangle triangles[GE_GBI_CLIP_MAX_TRIANGLES];
    size_t triangle_count;
} GeGbiClipResult;

typedef enum GeGbiClipStatus {
    GE_GBI_CLIP_OK,
    GE_GBI_CLIP_INVALID_ARGUMENT,
    GE_GBI_CLIP_NONFINITE_INPUT,
    GE_GBI_CLIP_CAPACITY_EXCEEDED
} GeGbiClipStatus;

/*
 * Clip one processed triangle to -w <= x,y,z <= w and w > 0, preserving
 * winding and linearly interpolating vertex attributes at new clip edges.
 * NDC is recomputed and screen coordinates are invalidated for every emitted
 * vertex so viewport projection cannot accidentally use pre-clip values.
 * A fully rejected triangle is a successful result with triangle_count == 0.
 */
GeGbiClipStatus ge_gbi_clip_triangle(
    const GeGbiProcessedVertex input[3],
    GeGbiClipResult *result);

#ifdef __cplusplus
}
#endif

#endif

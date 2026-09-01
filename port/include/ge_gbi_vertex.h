#ifndef GE_GBI_VERTEX_H
#define GE_GBI_VERTEX_H

#include "ge_gbi_state.h"
#include "ge_gbi_traverse.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_GBI_GEOMETRY_LIGHTING           UINT32_C(0x00020000)
#define GE_GBI_GEOMETRY_TEXTURE_GEN        UINT32_C(0x00040000)
#define GE_GBI_GEOMETRY_TEXTURE_GEN_LINEAR UINT32_C(0x00080000)

typedef enum GeGbiClipFlag {
    GE_GBI_CLIP_LEFT = 1U << 0,
    GE_GBI_CLIP_RIGHT = 1U << 1,
    GE_GBI_CLIP_BOTTOM = 1U << 2,
    GE_GBI_CLIP_TOP = 1U << 3,
    GE_GBI_CLIP_NEAR = 1U << 4,
    GE_GBI_CLIP_FAR = 1U << 5,
    GE_GBI_CLIP_NONPOSITIVE_W = 1U << 6
} GeGbiClipFlag;

typedef struct GeGbiProcessedVertex {
    float object[4];
    float eye[4];
    float clip[4];
    float ndc[3];
    float screen[3];
    float normal[3];
    float texture[2];
    uint8_t rgba[4];
    uint8_t clip_flags;
    uint8_t has_ndc;
    uint8_t has_screen;
    uint8_t texture_generated;
} GeGbiProcessedVertex;

typedef enum GeGbiVertexProcessStatus {
    GE_GBI_VERTEX_PROCESS_OK,
    GE_GBI_VERTEX_PROCESS_INVALID_ARGUMENT,
    GE_GBI_VERTEX_PROCESS_INVALID_STATE
} GeGbiVertexProcessStatus;

GeGbiVertexProcessStatus ge_gbi_vertex_process(
    const GeGbiRenderState *state,
    const GeGbiVertex *vertex,
    GeGbiProcessedVertex *processed);

#ifdef __cplusplus
}
#endif

#endif

#ifndef GE_3DS_ORIGINAL_FRONTEND_CAST_H
#define GE_3DS_ORIGINAL_FRONTEND_CAST_H

#include <stddef.h>

/* Projects the exact constructor_menu18_displaycast camera-space output into
 * the 3DS top-screen orthographic vertex space.  The original VI is top-down;
 * Citro3D's Mtx_OrthoTilt input is bottom-up, so the two Y conversions must
 * both be retained. */
int ge_3ds_original_frontend_cast_project(
    const float camera_space[3], float projected[3]);

typedef struct Ge3dsOriginalFrontendCastClipVertex {
    float camera_space[3];
    float texture[2];
    float rgba[4];
} Ge3dsOriginalFrontendCastClipVertex;

typedef struct Ge3dsOriginalFrontendCastProjectedVertex {
    float projected[3];
    float texture[2];
    float rgba[4];
} Ge3dsOriginalFrontendCastProjectedVertex;

/* Clips one triangle against constructor_menu18_displaycast's exact near=10
 * plane, then triangulates the retained polygon. Returns 0, 3, or 6 vertices;
 * output capacity is always six. Attributes are linearly interpolated at the
 * generated intersections, preserving the source display-list material. */
size_t ge_3ds_original_frontend_cast_clip_project_triangle(
    const Ge3dsOriginalFrontendCastClipVertex input[3],
    Ge3dsOriginalFrontendCastProjectedVertex output[6]);

#endif

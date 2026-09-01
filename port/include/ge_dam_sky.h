#ifndef GE_DAM_SKY_H
#define GE_DAM_SKY_H

#include <stddef.h>
#include <stdint.h>

#include "ge_original_stage_environment.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GE_DAM_SKY_MAX_POLYGON_VERTICES 5U
#define GE_DAM_SKY_MAX_TRIANGLE_VERTICES 9U

typedef struct GeDamSkyCamera {
    float position[3];
    float look[3];
    float up[3];
    float viewport_left;
    float viewport_top;
    float viewport_width;
    float viewport_height;
    float vertical_fov_degrees;
    float aspect;
} GeDamSkyCamera;

typedef struct GeDamSkyVertex {
    float screen_x;
    float screen_y;
    float texture_u;
    float texture_v;
    float red;
    float green;
    float blue;
    float alpha;
} GeDamSkyVertex;

typedef struct GeDamSkyScene {
    GeDamSkyVertex vertices[GE_DAM_SKY_MAX_TRIANGLE_VERTICES];
    size_t vertex_count;
    size_t polygon_vertex_count;
} GeDamSkyScene;

/* Exact skyTick offset rule from src/game/sky.c. */
void ge_dam_sky_tick(float *cloud_offset, int32_t clock_timer);

/* Portable PICA handoff for the canonical skyRender cloud-plane math. The
 * N64 body clips a screen rectangle against the world-space cloud plane and
 * emits 3-5 vertices through custom microcode; this routine retains the same
 * plane intersection, 300000-unit cap, UV scale, horizon fade, and triangle
 * topology while returning ordinary screen vertices for the 3DS renderer. */
int ge_dam_sky_build(const GeDamSkyCamera *camera, float cloud_offset,
                     GeDamSkyScene *scene);
int ge_dam_sky_build_environment(
    const GeDamSkyCamera *camera,
    const GeOriginalStageEnvironment *environment,
    float cloud_offset, GeDamSkyScene *scene);

#ifdef __cplusplus
}
#endif

#endif

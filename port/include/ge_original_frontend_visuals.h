#ifndef GE_ORIGINAL_FRONTEND_VISUALS_H
#define GE_ORIGINAL_FRONTEND_VISUALS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CPU realization of the fixed-function vertex semantics shared by
 * constructor_menu04_goldeneyelogo and load_display_rare_logo.  The packed
 * vertex RGB bytes are normals while G_LIGHTING is enabled; they are not
 * authored colours. */
typedef struct GeOriginalFrontendGeneratedVertex {
    float normal[3];
    float generated_uv[2];
    uint8_t lit_rgba[4];
} GeOriginalFrontendGeneratedVertex;

/* Per-publication snapshots; keep canonical presentation values/ordering,
 * but prepare constant light direction and rotation only once. */
typedef struct GeOriginalFrontendLightingContext {
    float cosine, sine, direction[3];
    uint8_t ambient[3], diffuse[3], valid;
} GeOriginalFrontendLightingContext;
typedef struct GeOriginalFrontendProjectionContext {
    float cosine, sine, focal, camera_eye_z;
    uint8_t valid;
} GeOriginalFrontendProjectionContext;

int ge_original_frontend_lighting_prepare(float rotation_y_radians,
    const uint8_t ambient_rgb[3], const uint8_t diffuse_rgb[3],
    const int8_t light_direction[3], GeOriginalFrontendLightingContext *context);
int ge_original_frontend_generate_lit_vertex_prepared(
    const uint8_t packed_normal[3], uint8_t alpha,
    const GeOriginalFrontendLightingContext *context,
    GeOriginalFrontendGeneratedVertex *output);
void ge_original_frontend_rareware_projection_prepare(float rotation_y_degrees,
    float camera_eye_z, GeOriginalFrontendProjectionContext *context);
void ge_original_frontend_rareware_project_prepared(const float authored[3],
    const GeOriginalFrontendProjectionContext *context, float projected[3]);

int ge_original_frontend_generate_lit_vertex(
    const uint8_t packed_normal[3], uint8_t alpha,
    float rotation_y_radians, const uint8_t ambient_rgb[3],
    const uint8_t diffuse_rgb[3], const int8_t light_direction[3],
    GeOriginalFrontendGeneratedVertex *output);

enum {
    GE_ORIGINAL_RAREWARE_FRONT_TEXTURE_OFFSET = 0x4fe8,
    GE_ORIGINAL_RAREWARE_BODY_TEXTURE_OFFSET = 0x5ff0,
    GE_ORIGINAL_RAREWARE_REFLECTION_TEXTURE_WIDTH = 32,
    GE_ORIGINAL_RAREWARE_REFLECTION_TEXTURE_HEIGHT = 32,
    GE_ORIGINAL_RAREWARE_REFLECTION_TEXTURE_BYTES = 32 * 32 * 2,
    GE_ORIGINAL_RAREWARE_BODY_TEXTURE_SCALE_S = 0x1c81,
    GE_ORIGINAL_RAREWARE_BODY_TEXTURE_SCALE_T = 0x1426,
    GE_ORIGINAL_RAREWARE_BODY_TILE_ULS = 46,
    GE_ORIGINAL_RAREWARE_BODY_TILE_ULT = 116,
    GE_ORIGINAL_RAREWARE_BODY_TILE_LRS = 124,
    GE_ORIGINAL_RAREWARE_BODY_TILE_LRT = 124
};

/* D_02004758 uses generated coordinates, the scale and tile window above,
 * and a repeating 32x32 image.  Return normalized texture coordinates ready
 * for a repeating native sampler. */
void ge_original_frontend_rareware_body_uv(
    const GeOriginalFrontendGeneratedVertex *vertex, float uv[2]);

void ge_original_frontend_rareware_project(
    const float authored[3], float rotation_y_degrees, float camera_eye_z,
    float projected[3]);

#ifdef __cplusplus
}
#endif

#endif

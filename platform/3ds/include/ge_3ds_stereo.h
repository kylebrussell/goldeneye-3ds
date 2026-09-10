#ifndef GE_3DS_STEREO_H
#define GE_3DS_STEREO_H

/* Stereoscopic top-screen support.
 *
 * The gameplay renderer projects world/eye-space vertices on the GPU with
 * matrices whose homogeneous clip w equals the original camera depth. An
 * off-axis stereo camera differs from the mono camera only by a horizontal
 * screen shift that is affine in that depth: for a convergence distance D and
 * a far-plane parallax of P pixels, the left eye moves a vertex at depth W by
 * P * (D / W - 1) pixels and the right eye by the negation. Multiplying by W
 * gives the homogeneous form P*D - P*W, so the vertex shader adds
 * `constant + coefficient * w` to the projected position before the perspective
 * divide. That is exactly the projection an off-axis camera would produce, so
 * rasterization and depth stay correct without a second CPU projection.
 *
 * The constant/coefficient live in screen pixels along the tilted framebuffer
 * axis; ge_3ds_stereo_uniforms maps them through the caller's screen-to-clip
 * column so the shader stays independent of the tilt convention. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Ge3dsStereoLayer {
    /* Perspective world/runtime-space geometry (rooms, props, guards). */
    GE_3DS_STEREO_LAYER_WORLD = 0,
    /* Perspective eye-space first-person weapon/hands. */
    GE_3DS_STEREO_LAYER_FIRST_PERSON,
    /* Orthographic screen-space overlays drawn at a fixed pop-out depth. */
    GE_3DS_STEREO_LAYER_HUD,
    /* Orthographic screen-space backdrop that must sit at infinity. */
    GE_3DS_STEREO_LAYER_SKY,
    /* Orthographic full-screen fills or anything that must not shift. */
    GE_3DS_STEREO_LAYER_FLAT,
    GE_3DS_STEREO_LAYER_COUNT
} Ge3dsStereoLayer;

typedef struct Ge3dsStereoConfig {
    bool enabled;
    /* Far-plane parallax in top-screen pixels at a full 3D slider. */
    float parallax_px;
    /* World depth (original runtime units) rendered at the screen plane. */
    float convergence;
    float first_person_parallax_px;
    float first_person_convergence;
    /* Constant pop-out of screen-space HUD elements, in pixels. */
    float hud_parallax_px;
    /* Horizontal clip margin per pixel of parallax. The renderer floors this
     * at 1 / viewport_half_width so overrides cannot discard eye-visible
     * geometry; larger values may deliberately retain extra geometry. */
    float cull_margin_per_px;
} Ge3dsStereoConfig;

typedef struct Ge3dsStereoUniforms {
    /* Added to clip xy after scaling by clip w. */
    float coefficient[2];
    /* Added to clip xy unconditionally. */
    float constant[2];
} Ge3dsStereoUniforms;

void ge_3ds_stereo_config_defaults(Ge3dsStereoConfig *config);

/* Optional overrides, one `key value` pair per line: enabled, parallax_px,
 * convergence, first_person_parallax_px, first_person_convergence,
 * hud_parallax_px, cull_margin_per_px. Unknown keys are ignored; invalid
 * values keep the default. Returns true when the stream parsed cleanly. */
bool ge_3ds_stereo_config_load(Ge3dsStereoConfig *config, FILE *stream);

/* Homogeneous screen-pixel shift for one eye and layer at the given slider
 * position: shift_h = constant + coefficient * w. `eye` is -1 (left) or +1
 * (right). Zero slider or a disabled config yields zero shifts. */
void ge_3ds_stereo_pixel_shift(const Ge3dsStereoConfig *config, float slider,
                               int eye, Ge3dsStereoLayer layer,
                               float *constant, float *coefficient);

/* Maps the pixel shift through the screen-x column of the screen-to-clip
 * (tilted orthographic) matrix so it can be added to clip xy. */
void ge_3ds_stereo_uniforms(const Ge3dsStereoConfig *config, float slider,
                            int eye, Ge3dsStereoLayer layer,
                            const float screen_x_column[2],
                            Ge3dsStereoUniforms *uniforms);

/* Largest horizontal displacement in pixels for a layer at depths >=
 * near_depth. An invalid near depth returns infinity for perspective layers
 * with nonzero parallax, allowing horizontal culling to fail open. */
float ge_3ds_stereo_max_shift_px(const Ge3dsStereoConfig *config,
    float slider, Ge3dsStereoLayer layer, float near_depth);

/* World-layer shorthand, retained for callers that only draw world geometry. */
float ge_3ds_stereo_max_world_shift_px(const Ge3dsStereoConfig *config,
                                       float slider, float near_depth);

#ifdef __cplusplus
}
#endif

#endif

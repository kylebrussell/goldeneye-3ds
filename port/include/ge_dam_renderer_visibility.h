#ifndef GE_DAM_RENDERER_VISIBILITY_H
#define GE_DAM_RENDERER_VISIBILITY_H

#include "ge_dam_visibility_runtime.h"
#include "ge_dam_preload_queue.h"

/* Renderer-only output. Membership bits: 1 canonical, 2 left, 4 right.
 * It must never be used for AI, active prop lists, or gameplay on-screen flags.
 * Instances execute sequentially on the render thread. No canonical globals
 * or provider callbacks are touched. The immutable program is borrowed. */
typedef struct GeDamRendererVisibility {
    GeOriginalBgVisibilityResult visible;
    uint8_t membership[GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS];
    uint8_t preload[GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS];
    uint8_t stereo;
} GeDamRendererVisibility;

typedef struct GeRendererEyeShift {
    float constant_px;      /* displacement = constant / camera depth + coefficient */
    float coefficient_px;
} GeRendererEyeShift;

/* Construct one eye's off-axis visibility input, preserving the supplied
 * local player's room and authored policy. camera must use an orthonormal
 * view and the original perspective ABI (clip W = -eye Z). No live player
 * lookup or publication occurs, so a multiplayer caller selects the camera. */
int ge_dam_renderer_visibility_eye_input(
    const GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera, const float position[3],
    const GeRendererEyeShift *shift, GeOriginalBgVisibilityInput *input);

/* Zero shifts copy canonical byte-for-byte and perform no interpreter pass.
 * Each eye gets a private preload queue snapshot, preserving authored preload
 * conditionals without invoking caller callbacks. Commit requests explicitly
 * through queue_request after success; failures leave output untouched. */
GeOriginalBgVisibilityStatus ge_dam_renderer_visibility_run(
    const GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera, const float position[3],
    const GeOriginalBgVisibilityProviders *providers,
    const GeDamPreloadQueue *queue,
    const GeOriginalBgVisibilityResult *canonical,
    const GeRendererEyeShift shifts[2], GeDamRendererVisibility *result);

#endif

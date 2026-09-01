#ifndef GE_3DS_MATERIAL_H
#define GE_3DS_MATERIAL_H

#include <citro3d.h>
#include <stddef.h>
#include <stdint.h>

#include "ge_pica_apply.h"
#include "ge_pica_material.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Ge3dsMaterialStatus {
    GE_3DS_MATERIAL_OK = 0,
    GE_3DS_MATERIAL_INVALID_ARGUMENT
} Ge3dsMaterialStatus;

/*
 * REPLACE is an explicit bring-up escape hatch for decoded materials whose
 * original texture is not available yet.  It keeps previews visible while the
 * result's VISIBILITY_OVERRIDE bit makes the approximation auditable.
 */
typedef enum Ge3dsMaterialTextureFallback {
    GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE = 0,
    GE_3DS_MATERIAL_TEXTURE_FALLBACK_REPLACE
} Ge3dsMaterialTextureFallback;

typedef struct Ge3dsMaterialBinding {
    C3D_Tex *texture0;
    Ge3dsMaterialTextureFallback missing_texture_fallback;
} Ge3dsMaterialBinding;

typedef struct Ge3dsMaterialResult {
    GePicaApplyState state;
    uint8_t texture_bound;
} Ge3dsMaterialResult;

enum { GE_3DS_TEXTURE_RECTANGLE_VERTEX_COUNT = 6 };

typedef struct Ge3dsTextureRectangleSubmission {
    GePicaTextureRectangleDraw draw;
    Ge3dsMaterialResult material;
    uint8_t draw_submitted;
} Ge3dsTextureRectangleSubmission;

/* Split compilation from command-buffer submission so immutable authored
 * materials can retain their exact compiled PICA state across frames. */
Ge3dsMaterialStatus ge_3ds_material_prepare(
    const GePicaMaterial *material,
    const Ge3dsMaterialBinding *binding,
    Ge3dsMaterialResult *result);

Ge3dsMaterialStatus ge_3ds_material_apply_prepared(
    const Ge3dsMaterialResult *prepared,
    const Ge3dsMaterialBinding *binding);

Ge3dsMaterialStatus ge_3ds_material_apply_prepared_delta(
    const Ge3dsMaterialResult *prepared,
    const Ge3dsMaterialBinding *binding,
    const Ge3dsMaterialResult *previous,
    const Ge3dsMaterialBinding *previous_binding);

Ge3dsMaterialStatus ge_3ds_material_apply(
    const GePicaMaterial *material,
    const Ge3dsMaterialBinding *binding,
    Ge3dsMaterialResult *result);

/* Consume the portable typed Fast3D action at the PICA boundary. The caller
 * supplies its already-bound linear vertex range and the exact texture affine
 * transform; this function publishes six vertices, applies canonical material
 * state, and submits one authored rectangle draw without touching pass order. */
Ge3dsMaterialStatus ge_3ds_texture_rectangle_submit(
    const GeGbiRenderState *state,
    const GeGbiStateAction *action,
    const GePicaTextureBindingTransform *coordinate_binding,
    const Ge3dsMaterialBinding *material_binding,
    GePicaScreenVertex *vertex_destination,
    size_t vertex_capacity,
    int first_vertex,
    Ge3dsTextureRectangleSubmission *submission);

#ifdef __cplusplus
}
#endif

#endif

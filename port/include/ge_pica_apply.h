#ifndef GE_PICA_APPLY_H
#define GE_PICA_APPLY_H

#include <stdint.h>

#include "ge_pica_material.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Backend-neutral PICA200 draw state.  This keeps the material policy and its
 * fallback accounting host-testable; platform adapters only translate these
 * small enums into their graphics API's constants.
 */
typedef enum GePicaApplyStatus {
    GE_PICA_APPLY_OK = 0,
    GE_PICA_APPLY_INVALID_ARGUMENT
} GePicaApplyStatus;

typedef enum GePicaApplySource {
    GE_PICA_APPLY_SOURCE_PRIMARY = 0,
    GE_PICA_APPLY_SOURCE_TEXTURE0,
    GE_PICA_APPLY_SOURCE_CONSTANT
} GePicaApplySource;

typedef enum GePicaApplyCombine {
    GE_PICA_APPLY_REPLACE = 0,
    GE_PICA_APPLY_MODULATE
} GePicaApplyCombine;

typedef enum GePicaApplyCull {
    GE_PICA_APPLY_CULL_NONE = 0,
    GE_PICA_APPLY_CULL_FRONT,
    GE_PICA_APPLY_CULL_BACK
} GePicaApplyCull;

typedef enum GePicaApplyFallback {
    GE_PICA_APPLY_FALLBACK_NONE = 0,
    GE_PICA_APPLY_FALLBACK_INVALID_ENUM = UINT32_C(1) << 0,
    GE_PICA_APPLY_FALLBACK_CULL_BOTH = UINT32_C(1) << 1,
    GE_PICA_APPLY_FALLBACK_TEXTURE_UNBOUND = UINT32_C(1) << 2,
    GE_PICA_APPLY_FALLBACK_VISIBILITY_OVERRIDE = UINT32_C(1) << 3,
    GE_PICA_APPLY_FALLBACK_DEPTH_MODE = UINT32_C(1) << 4,
    GE_PICA_APPLY_FALLBACK_FOG = UINT32_C(1) << 5
} GePicaApplyFallback;

typedef struct GePicaApplyChannel {
    GePicaApplySource source0;
    GePicaApplySource source1;
    GePicaApplyCombine combine;
} GePicaApplyChannel;

typedef struct GePicaApplyState {
    uint32_t material_fallback_flags;
    uint32_t apply_fallback_flags;
    GePicaApplyChannel color;
    GePicaApplyChannel alpha;
    GePicaColor constant_color;
    GePicaApplyCull cull;
    GePicaTextureWrap wrap_s;
    GePicaTextureWrap wrap_t;
    GePicaTextureFilter min_filter;
    GePicaTextureFilter mag_filter;
    uint8_t texture_required;
    uint8_t draw_enabled;
    uint8_t depth_test_enabled;
    uint8_t depth_write_enabled;
    uint8_t alpha_test_enabled;
    uint8_t alpha_threshold;
    uint8_t blend_enabled;
} GePicaApplyState;

GePicaApplyStatus ge_pica_apply_compile(const GePicaMaterial *material,
                                        GePicaApplyState *state);

#ifdef __cplusplus
}
#endif

#endif

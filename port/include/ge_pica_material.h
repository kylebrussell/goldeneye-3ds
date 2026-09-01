#ifndef GE_PICA_MATERIAL_H
#define GE_PICA_MATERIAL_H

#include <stdint.h>

#include "ge_gbi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A bounded, platform-neutral description of the PICA200 state needed by the
 * 3DS renderer. This is deliberately not an RDP emulator: every approximation
 * is reported in fallback_flags so a caller can audit or replace it.
 */
typedef enum GePicaMaterialStatus {
    GE_PICA_MATERIAL_OK = 0,
    GE_PICA_MATERIAL_INVALID_ARGUMENT
} GePicaMaterialStatus;

typedef enum GePicaCullMode {
    GE_PICA_CULL_NONE = 0,
    GE_PICA_CULL_FRONT,
    GE_PICA_CULL_BACK,
    GE_PICA_CULL_BOTH
} GePicaCullMode;

typedef enum GePicaTextureWrap {
    GE_PICA_WRAP_REPEAT = 0,
    GE_PICA_WRAP_MIRROR,
    GE_PICA_WRAP_CLAMP
} GePicaTextureWrap;

typedef enum GePicaTextureFilter {
    GE_PICA_FILTER_NEAREST = 0,
    GE_PICA_FILTER_LINEAR
} GePicaTextureFilter;

typedef enum GePicaTextureSource {
    GE_PICA_TEXTURE_SOURCE_NONE = 0,
    GE_PICA_TEXTURE_SOURCE_RARE_ID,
    GE_PICA_TEXTURE_SOURCE_GBI_IMAGE
} GePicaTextureSource;

typedef enum GePicaCombineMode {
    GE_PICA_COMBINE_SHADE = 0,
    GE_PICA_COMBINE_PRIMITIVE,
    GE_PICA_COMBINE_ENVIRONMENT,
    GE_PICA_COMBINE_TEXTURE0,
    GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE,
    GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE
} GePicaCombineMode;

typedef enum GePicaAlphaMode {
    GE_PICA_ALPHA_ONE = 0,
    GE_PICA_ALPHA_SHADE,
    GE_PICA_ALPHA_PRIMITIVE,
    GE_PICA_ALPHA_ENVIRONMENT,
    GE_PICA_ALPHA_TEXTURE0,
    GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE,
    GE_PICA_ALPHA_TEXTURE0_MODULATE_PRIMITIVE
} GePicaAlphaMode;

typedef enum GePicaAlphaTest {
    GE_PICA_ALPHA_TEST_DISABLED = 0,
    GE_PICA_ALPHA_TEST_THRESHOLD
} GePicaAlphaTest;

typedef enum GePicaDepthMode {
    GE_PICA_DEPTH_OPAQUE = 0,
    GE_PICA_DEPTH_INTERPENETRATING,
    GE_PICA_DEPTH_TRANSLUCENT,
    GE_PICA_DEPTH_DECAL
} GePicaDepthMode;

typedef enum GePicaCycleType {
    GE_PICA_CYCLE_ONE = 0,
    GE_PICA_CYCLE_TWO,
    GE_PICA_CYCLE_COPY,
    GE_PICA_CYCLE_FILL
} GePicaCycleType;

typedef enum GePicaMaterialFallback {
    GE_PICA_FALLBACK_NONE = 0,
    GE_PICA_FALLBACK_COMBINER = UINT32_C(1) << 0,
    GE_PICA_FALLBACK_TWO_CYCLE = UINT32_C(1) << 1,
    GE_PICA_FALLBACK_COPY_FILL_CYCLE = UINT32_C(1) << 2,
    GE_PICA_FALLBACK_ALPHA_DITHER = UINT32_C(1) << 3,
    GE_PICA_FALLBACK_PRIMITIVE_DEPTH = UINT32_C(1) << 4,
    GE_PICA_FALLBACK_TEXTURE_DETAIL = UINT32_C(1) << 5,
    GE_PICA_FALLBACK_TEXTURE_LOD = UINT32_C(1) << 6,
    GE_PICA_FALLBACK_TEXTURE_LUT = UINT32_C(1) << 7,
    GE_PICA_FALLBACK_COLOR_DITHER = UINT32_C(1) << 8,
    GE_PICA_FALLBACK_CULL_BOTH = UINT32_C(1) << 9,
    GE_PICA_FALLBACK_BLENDER = UINT32_C(1) << 10,
    GE_PICA_FALLBACK_TEXTURE_AVERAGE = UINT32_C(1) << 11,
    GE_PICA_FALLBACK_DETAIL_TEXTURE = UINT32_C(1) << 12,
    GE_PICA_FALLBACK_RARE_TEXTURE_TYPE = UINT32_C(1) << 13,
    GE_PICA_FALLBACK_MISSING_TEXTURE = UINT32_C(1) << 14
} GePicaMaterialFallback;

typedef struct GePicaColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} GePicaColor;

typedef struct GePicaMaterial {
    uint32_t fallback_flags;
    uint32_t combine_mux0;
    uint32_t combine_mux1;
    uint32_t texture_image_address;
    uint16_t texture_id;
    uint16_t detail_texture_id;
    uint16_t texture_image_width;
    uint16_t texture_scale_s;
    uint16_t texture_scale_t;
    int16_t fog_multiplier;
    int16_t fog_offset;
    GePicaColor primitive_color;
    GePicaColor environment_color;
    GePicaColor blend_color;
    GePicaColor fog_color;
    GePicaCullMode cull_mode;
    GePicaTextureWrap wrap_s;
    GePicaTextureWrap wrap_t;
    GePicaTextureFilter min_filter;
    GePicaTextureFilter mag_filter;
    GePicaTextureSource texture_source;
    GePicaCombineMode color_combine;
    GePicaAlphaMode alpha_combine;
    GePicaAlphaTest alpha_test;
    GePicaDepthMode depth_mode;
    GePicaCycleType cycle_type;
    uint8_t texture_enabled;
    uint8_t texture_tile;
    uint8_t texture_type;
    uint8_t texture_min_level;
    uint8_t texture_shift_s;
    uint8_t texture_shift_t;
    uint8_t texture_image_format;
    uint8_t texture_image_size;
    uint8_t texture_perspective;
    uint8_t lighting_enabled;
    uint8_t smooth_shading;
    uint8_t fog_enabled;
    uint8_t depth_test_enabled;
    uint8_t depth_write_enabled;
    uint8_t blend_enabled;
    uint8_t alpha_threshold;
} GePicaMaterial;

/* C3D-ready screen vertex layout shared by command consumers. Texture
 * coordinates are produced through an explicit affine binding so callers can
 * represent a full texture, a Tex3DS subtexture, or vertically inverted
 * storage without changing the canonical RDP rectangle calculation. */
typedef struct GePicaScreenVertex {
    float x;
    float y;
    float z;
    float texture_u;
    float texture_v;
    float red;
    float green;
    float blue;
    float alpha;
} GePicaScreenVertex;

typedef struct GePicaTextureBindingTransform {
    float u_scale;
    float v_scale;
    float u_bias;
    float v_bias;
} GePicaTextureBindingTransform;

typedef struct GePicaTextureRectangleDraw {
    GePicaMaterial material;
    GePicaScreenVertex vertices[6];
    uint8_t tile;
    uint8_t flipped;
} GePicaTextureRectangleDraw;

GePicaMaterialStatus ge_pica_material_translate(
    const GeGbiRenderState *state,
    GePicaMaterial *material);

/* Translate one fully assembled Fast3D texture-rectangle action. Screen
 * bounds retain their 10.2 precision; S/T and derivatives retain signed
 * 10.5/5.10 semantics. COPY mode's four-pixel horizontal issue rate is
 * accounted for exactly. */
GePicaMaterialStatus ge_pica_texture_rectangle_translate(
    const GeGbiRenderState *state,
    const GeGbiTextureRectangle *rectangle,
    const GePicaTextureBindingTransform *binding,
    GePicaTextureRectangleDraw *draw);

GePicaMaterialStatus ge_pica_texture_rectangle_translate_action(
    const GeGbiRenderState *state,
    const GeGbiStateAction *action,
    const GePicaTextureBindingTransform *binding,
    GePicaTextureRectangleDraw *draw);

#ifdef __cplusplus
}
#endif

#endif

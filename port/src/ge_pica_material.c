#include "ge_pica_material.h"

#include <stddef.h>
#include <string.h>

enum {
    GEOMETRY_ZBUFFER = 0x00000001,
    GEOMETRY_SHADE = 0x00000004,
    GEOMETRY_SHADING_SMOOTH = 0x00000200,
    GEOMETRY_CULL_FRONT = 0x00001000,
    GEOMETRY_CULL_BACK = 0x00002000,
    GEOMETRY_FOG = 0x00010000,
    GEOMETRY_LIGHTING = 0x00020000,
    OTHER_LOW_ALPHA_COMPARE_MASK = 0x00000003,
    OTHER_LOW_DEPTH_SOURCE = 0x00000004,
    OTHER_LOW_DEPTH_COMPARE = 0x00000010,
    OTHER_LOW_DEPTH_WRITE = 0x00000020,
    OTHER_LOW_DEPTH_MODE_MASK = 0x00000c00,
    OTHER_LOW_FORCE_BLEND = 0x00004000,
    OTHER_LOW_COVERAGE_TIMES_ALPHA = 0x00001000,
    OTHER_HIGH_ALPHA_DITHER_MASK = 0x00000030,
    OTHER_HIGH_COLOR_DITHER_MASK = 0x000000c0,
    OTHER_HIGH_TEXTURE_FILTER_MASK = 0x00003000,
    OTHER_HIGH_TEXTURE_LUT_MASK = 0x0000c000,
    OTHER_HIGH_TEXTURE_LOD = 0x00010000,
    OTHER_HIGH_TEXTURE_DETAIL_MASK = 0x00060000,
    OTHER_HIGH_TEXTURE_PERSPECTIVE = 0x00080000,
    OTHER_HIGH_CYCLE_MASK = 0x00300000
};

typedef struct GePicaCombineCycle {
    uint8_t color_a;
    uint8_t color_b;
    uint8_t color_c;
    uint8_t color_d;
    uint8_t alpha_a;
    uint8_t alpha_b;
    uint8_t alpha_c;
    uint8_t alpha_d;
} GePicaCombineCycle;

static GePicaColor ge_pica_unpack_color(uint32_t packed)
{
    GePicaColor color;

    color.red = (uint8_t)(packed >> 24);
    color.green = (uint8_t)(packed >> 16);
    color.blue = (uint8_t)(packed >> 8);
    color.alpha = (uint8_t)packed;
    return color;
}

static GePicaTextureWrap ge_pica_wrap(uint8_t clamp_mirror)
{
    if ((clamp_mirror & UINT8_C(2)) != 0U) {
        return GE_PICA_WRAP_CLAMP;
    }
    if ((clamp_mirror & UINT8_C(1)) != 0U) {
        return GE_PICA_WRAP_MIRROR;
    }
    return GE_PICA_WRAP_REPEAT;
}

static GePicaCombineCycle ge_pica_decode_combine(
    const GeGbiRenderState *state,
    unsigned int cycle)
{
    GePicaCombineCycle result;
    const uint32_t mux0 = state->combine_mux0;
    const uint32_t mux1 = state->combine_mux1;

    if (cycle == 0U) {
        result.color_a = (uint8_t)((mux0 >> 20) & UINT32_C(0x0f));
        result.color_b = (uint8_t)((mux1 >> 28) & UINT32_C(0x0f));
        result.color_c = (uint8_t)((mux0 >> 15) & UINT32_C(0x1f));
        result.color_d = (uint8_t)((mux1 >> 15) & UINT32_C(0x07));
        result.alpha_a = (uint8_t)((mux0 >> 12) & UINT32_C(0x07));
        result.alpha_b = (uint8_t)((mux1 >> 12) & UINT32_C(0x07));
        result.alpha_c = (uint8_t)((mux0 >> 9) & UINT32_C(0x07));
        result.alpha_d = (uint8_t)((mux1 >> 9) & UINT32_C(0x07));
    } else {
        result.color_a = (uint8_t)((mux0 >> 5) & UINT32_C(0x0f));
        result.color_b = (uint8_t)((mux1 >> 24) & UINT32_C(0x0f));
        result.color_c = (uint8_t)(mux0 & UINT32_C(0x1f));
        result.color_d = (uint8_t)((mux1 >> 6) & UINT32_C(0x07));
        result.alpha_a = (uint8_t)((mux1 >> 21) & UINT32_C(0x07));
        result.alpha_b = (uint8_t)((mux1 >> 3) & UINT32_C(0x07));
        result.alpha_c = (uint8_t)((mux1 >> 18) & UINT32_C(0x07));
        result.alpha_d = (uint8_t)(mux1 & UINT32_C(0x07));
    }
    return result;
}

static int ge_pica_color_direct(const GePicaCombineCycle *cycle,
                                uint8_t source)
{
    return cycle->color_a == UINT8_C(15)
        && cycle->color_b == UINT8_C(15)
        && cycle->color_c == UINT8_C(31) && cycle->color_d == source;
}

static int ge_pica_color_modulate(const GePicaCombineCycle *cycle,
                                  uint8_t first,
                                  uint8_t second)
{
    return cycle->color_a == first && cycle->color_b == UINT8_C(15)
        && cycle->color_c == second && cycle->color_d == UINT8_C(7);
}

static int ge_pica_alpha_direct(const GePicaCombineCycle *cycle,
                                uint8_t source)
{
    return cycle->alpha_a == UINT8_C(7)
        && cycle->alpha_b == UINT8_C(7)
        && cycle->alpha_c == UINT8_C(7) && cycle->alpha_d == source;
}

static int ge_pica_alpha_modulate(const GePicaCombineCycle *cycle,
                                  uint8_t first,
                                  uint8_t second)
{
    return cycle->alpha_a == first && cycle->alpha_b == UINT8_C(7)
        && cycle->alpha_c == second && cycle->alpha_d == UINT8_C(7);
}

static int ge_pica_translate_color_combine(const GePicaCombineCycle *cycle,
                                           uint8_t combined_is_texture,
                                           GePicaCombineMode *mode)
{
    const uint8_t texture_source = combined_is_texture != 0U ? 0U : 1U;

    if (ge_pica_color_direct(cycle, 4U)) {
        *mode = GE_PICA_COMBINE_SHADE;
    } else if (ge_pica_color_direct(cycle, 3U)) {
        *mode = GE_PICA_COMBINE_PRIMITIVE;
    } else if (ge_pica_color_direct(cycle, 5U)) {
        *mode = GE_PICA_COMBINE_ENVIRONMENT;
    } else if (ge_pica_color_direct(cycle, texture_source)) {
        *mode = GE_PICA_COMBINE_TEXTURE0;
    } else if (ge_pica_color_modulate(cycle, texture_source, 4U)) {
        *mode = GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE;
    } else if (ge_pica_color_modulate(cycle, texture_source, 3U)) {
        *mode = GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE;
    } else if (ge_pica_color_modulate(cycle, texture_source, 5U)) {
        *mode = GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT;
    } else {
        return 0;
    }
    return 1;
}

static int ge_pica_translate_alpha_combine(const GePicaCombineCycle *cycle,
                                           uint8_t combined_is_texture,
                                           GePicaAlphaMode *mode)
{
    const uint8_t texture_source = combined_is_texture != 0U ? 0U : 1U;

    if (ge_pica_alpha_direct(cycle, 6U)) {
        *mode = GE_PICA_ALPHA_ONE;
    } else if (ge_pica_alpha_direct(cycle, 4U)) {
        *mode = GE_PICA_ALPHA_SHADE;
    } else if (ge_pica_alpha_direct(cycle, 3U)) {
        *mode = GE_PICA_ALPHA_PRIMITIVE;
    } else if (ge_pica_alpha_direct(cycle, 5U)) {
        *mode = GE_PICA_ALPHA_ENVIRONMENT;
    } else if (ge_pica_alpha_direct(cycle, texture_source)) {
        *mode = GE_PICA_ALPHA_TEXTURE0;
    } else if (ge_pica_alpha_modulate(cycle, texture_source, 4U)) {
        *mode = GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE;
    } else if (ge_pica_alpha_modulate(cycle, texture_source, 3U)) {
        *mode = GE_PICA_ALPHA_TEXTURE0_MODULATE_PRIMITIVE;
    } else if (ge_pica_alpha_modulate(cycle, texture_source, 5U)) {
        *mode = GE_PICA_ALPHA_TEXTURE0_MODULATE_ENVIRONMENT;
    } else {
        return 0;
    }
    return 1;
}

static int ge_pica_color_mode_uses_texture(GePicaCombineMode mode)
{
    return mode == GE_PICA_COMBINE_TEXTURE0
        || mode == GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE
        || mode == GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE
        || mode == GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT;
}

static int ge_pica_alpha_mode_uses_texture(GePicaAlphaMode mode)
{
    return mode == GE_PICA_ALPHA_TEXTURE0
        || mode == GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE
        || mode == GE_PICA_ALPHA_TEXTURE0_MODULATE_PRIMITIVE
        || mode == GE_PICA_ALPHA_TEXTURE0_MODULATE_ENVIRONMENT;
}

static int ge_pica_translate_two_cycle_color(
    const GePicaCombineCycle *first,
    const GePicaCombineCycle *second,
    uint8_t texture_enabled,
    GePicaCombineMode *mode)
{
    GePicaCombineMode first_mode = texture_enabled != 0U
        ? GE_PICA_COMBINE_TEXTURE0 : GE_PICA_COMBINE_SHADE;
    int first_exact = ge_pica_translate_color_combine(
        first, UINT8_C(0), &first_mode);

    /* Cycle two's COMBINED input is cycle one's complete result. Preserve a
     * directly passed shade/constant result rather than assuming COMBINED is
     * a texture merely because gSPTexture remains enabled. Dam's untextured
     * portal/occlusion surfaces depend on this distinction. */
    if (ge_pica_color_direct(second, UINT8_C(0))) {
        *mode = first_mode;
        return first_exact;
    }
    /* A second modulation cannot silently discard the environment factor
     * introduced by the new one-cycle texture/environment mapping. */
    if (first_mode == GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT)
        first_exact = 0;
    if (ge_pica_color_modulate(second, UINT8_C(0), UINT8_C(5))) {
        *mode = GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT;
        return first_exact && first_mode == GE_PICA_COMBINE_TEXTURE0;
    }
    if (ge_pica_color_modulate(second, UINT8_C(0), UINT8_C(4))) {
        *mode = ge_pica_color_mode_uses_texture(first_mode)
            ? GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE
            : GE_PICA_COMBINE_SHADE;
        return first_exact;
    }
    if (ge_pica_color_modulate(second, UINT8_C(0), UINT8_C(3))) {
        *mode = ge_pica_color_mode_uses_texture(first_mode)
            ? GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE
            : GE_PICA_COMBINE_PRIMITIVE;
        return first_exact;
    }
    return ge_pica_translate_color_combine(second,
        ge_pica_color_mode_uses_texture(first_mode) ? UINT8_C(1)
                                                    : UINT8_C(0), mode)
        && first_exact;
}

static int ge_pica_translate_two_cycle_alpha(
    const GePicaCombineCycle *first,
    const GePicaCombineCycle *second,
    uint8_t texture_enabled,
    GePicaAlphaMode *mode)
{
    GePicaAlphaMode first_mode = texture_enabled != 0U
        ? GE_PICA_ALPHA_TEXTURE0 : GE_PICA_ALPHA_SHADE;
    int first_exact = ge_pica_translate_alpha_combine(
        first, UINT8_C(0), &first_mode);

    if (ge_pica_alpha_direct(second, UINT8_C(0))) {
        *mode = first_mode;
        return first_exact;
    }
    /* modelApplyRenderModeType4's ordinary/tinted glass alpha:
     * clamp(COMBINED.a * SHADE.a + PRIMITIVE.a). Keep the authored
     * opacity even when the first-cycle trilinear texture is approximated. */
    if (second->alpha_a == 0U && second->alpha_b == 7U
            && second->alpha_c == 4U && second->alpha_d == 3U
            && first_mode == GE_PICA_ALPHA_TEXTURE0) {
        *mode = GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE_ADD_PRIMITIVE;
        return first_exact;
    }
    if (first_mode == GE_PICA_ALPHA_TEXTURE0_MODULATE_ENVIRONMENT)
        first_exact = 0;
    if (ge_pica_alpha_modulate(second, UINT8_C(0), UINT8_C(5))) {
        *mode = GE_PICA_ALPHA_TEXTURE0_MODULATE_ENVIRONMENT;
        return first_exact && first_mode == GE_PICA_ALPHA_TEXTURE0;
    }
    if (ge_pica_alpha_modulate(second, UINT8_C(0), UINT8_C(4))) {
        *mode = ge_pica_alpha_mode_uses_texture(first_mode)
            ? GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE
            : GE_PICA_ALPHA_SHADE;
        return first_exact;
    }
    if (ge_pica_alpha_modulate(second, UINT8_C(0), UINT8_C(3))) {
        *mode = ge_pica_alpha_mode_uses_texture(first_mode)
            ? GE_PICA_ALPHA_TEXTURE0_MODULATE_PRIMITIVE
            : GE_PICA_ALPHA_PRIMITIVE;
        return first_exact;
    }
    return ge_pica_translate_alpha_combine(second,
        ge_pica_alpha_mode_uses_texture(first_mode) ? UINT8_C(1)
                                                    : UINT8_C(0), mode)
        && first_exact;
}

static void ge_pica_translate_combiner(const GeGbiRenderState *state,
                                       GePicaMaterial *material)
{
    const int two_cycle = material->cycle_type == GE_PICA_CYCLE_TWO;
    const GePicaCombineCycle first = ge_pica_decode_combine(state, 0U);
    const GePicaCombineCycle second = ge_pica_decode_combine(state, 1U);
    int color_exact;
    int alpha_exact;

    if (material->texture_enabled != 0U) {
        material->color_combine = GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE;
        material->alpha_combine = GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE;
    } else {
        material->color_combine = GE_PICA_COMBINE_SHADE;
        material->alpha_combine = GE_PICA_ALPHA_SHADE;
    }
    color_exact = two_cycle
        ? ge_pica_translate_two_cycle_color(
            &first, &second, material->texture_enabled,
            &material->color_combine)
        : ge_pica_translate_color_combine(
            &first, UINT8_C(0), &material->color_combine);
    alpha_exact = two_cycle
        ? ge_pica_translate_two_cycle_alpha(
            &first, &second, material->texture_enabled,
            &material->alpha_combine)
        : ge_pica_translate_alpha_combine(
            &first, UINT8_C(0), &material->alpha_combine);
    if (!color_exact) {
        material->fallback_flags |= GE_PICA_FALLBACK_COMBINER;
    }
    if (!alpha_exact) {
        material->fallback_flags |= GE_PICA_FALLBACK_COMBINER;
    }
    if (material->texture_enabled == 0U
            && material->color_combine >= GE_PICA_COMBINE_TEXTURE0) {
        material->color_combine = GE_PICA_COMBINE_SHADE;
        material->fallback_flags |= GE_PICA_FALLBACK_COMBINER;
    }
    if (material->texture_enabled == 0U
            && material->alpha_combine >= GE_PICA_ALPHA_TEXTURE0) {
        material->alpha_combine = GE_PICA_ALPHA_SHADE;
        material->fallback_flags |= GE_PICA_FALLBACK_COMBINER;
    }
}

GePicaMaterialStatus ge_pica_material_translate(
    const GeGbiRenderState *state,
    GePicaMaterial *material)
{
    uint32_t cull_bits;
    uint32_t filter;
    uint32_t alpha_compare;

    if (state == NULL || material == NULL) {
        return GE_PICA_MATERIAL_INVALID_ARGUMENT;
    }
    memset(material, 0, sizeof(*material));
    material->combine_mux0 = state->combine_mux0;
    material->combine_mux1 = state->combine_mux1;
    material->texture_id = state->rare_texture.texture_id;
    material->detail_texture_id = state->rare_texture.detail_texture_id;
    material->texture_image_address = state->texture_image.address.raw;
    material->texture_image_width = state->texture_image.width;
    material->texture_image_format = state->texture_image.format;
    material->texture_image_size = state->texture_image.size;
    material->texture_scale_s = state->texture.scale_s;
    material->texture_scale_t = state->texture.scale_t;
    material->texture_tile = state->rare_texture.tile;
    material->texture_type = state->rare_texture.type;
    material->texture_min_level = state->rare_texture.min_level;
    material->texture_shift_s = state->rare_texture.shift_s;
    material->texture_shift_t = state->rare_texture.shift_t;
    material->texture_enabled = state->texture.enabled != 0U ? 1U : 0U;
    if (state->active_texture_binding == GE_GBI_TEXTURE_BINDING_RARE_ID) {
        material->texture_source = GE_PICA_TEXTURE_SOURCE_RARE_ID;
    } else if (state->active_texture_binding == GE_GBI_TEXTURE_BINDING_IMAGE) {
        material->texture_source = GE_PICA_TEXTURE_SOURCE_GBI_IMAGE;
    } else {
        material->texture_source = GE_PICA_TEXTURE_SOURCE_NONE;
    }
    if (material->texture_enabled != 0U
            && material->texture_source == GE_PICA_TEXTURE_SOURCE_NONE) {
        material->fallback_flags |= GE_PICA_FALLBACK_MISSING_TEXTURE;
    }
    material->wrap_s = ge_pica_wrap(state->rare_texture.clamp_mirror_s);
    material->wrap_t = ge_pica_wrap(state->rare_texture.clamp_mirror_t);
    material->primitive_color = ge_pica_unpack_color(state->primitive_color);
    material->environment_color = ge_pica_unpack_color(state->environment_color);
    material->blend_color = ge_pica_unpack_color(state->blend_color);
    material->fog_color = ge_pica_unpack_color(state->fog_color);
    material->fog_multiplier = state->fog_multiplier;
    material->fog_offset = state->fog_offset;

    cull_bits = state->geometry_mode
        & (GEOMETRY_CULL_FRONT | GEOMETRY_CULL_BACK);
    if (cull_bits == (GEOMETRY_CULL_FRONT | GEOMETRY_CULL_BACK)) {
        material->cull_mode = GE_PICA_CULL_BOTH;
        material->fallback_flags |= GE_PICA_FALLBACK_CULL_BOTH;
    } else if (cull_bits == GEOMETRY_CULL_FRONT) {
        material->cull_mode = GE_PICA_CULL_FRONT;
    } else if (cull_bits == GEOMETRY_CULL_BACK) {
        material->cull_mode = GE_PICA_CULL_BACK;
    } else {
        material->cull_mode = GE_PICA_CULL_NONE;
    }
    material->lighting_enabled = (state->geometry_mode & GEOMETRY_LIGHTING)
        != 0U ? 1U : 0U;
    material->texture_gen_enabled =
        (state->geometry_mode & UINT32_C(0x00040000)) != 0U ? 1U : 0U;
    material->texture_gen_linear =
        (state->geometry_mode & UINT32_C(0x00080000)) != 0U ? 1U : 0U;
    material->smooth_shading = (state->geometry_mode
        & (GEOMETRY_SHADE | GEOMETRY_SHADING_SMOOTH))
        == (GEOMETRY_SHADE | GEOMETRY_SHADING_SMOOTH) ? 1U : 0U;
    material->fog_enabled = (state->geometry_mode & GEOMETRY_FOG) != 0U
        ? 1U : 0U;

    material->cycle_type = (GePicaCycleType)((state->other_mode_high
        & OTHER_HIGH_CYCLE_MASK) >> 20);
    material->texture_perspective = (state->other_mode_high
        & OTHER_HIGH_TEXTURE_PERSPECTIVE) != 0U ? 1U : 0U;
    filter = state->other_mode_high & OTHER_HIGH_TEXTURE_FILTER_MASK;
    material->min_filter = filter == 0U
        ? GE_PICA_FILTER_NEAREST : GE_PICA_FILTER_LINEAR;
    material->mag_filter = material->min_filter;
    if (filter == OTHER_HIGH_TEXTURE_FILTER_MASK) {
        material->fallback_flags |= GE_PICA_FALLBACK_TEXTURE_AVERAGE;
    }
    if ((state->other_mode_high & OTHER_HIGH_TEXTURE_DETAIL_MASK) != 0U) {
        material->fallback_flags |= GE_PICA_FALLBACK_TEXTURE_DETAIL;
    }
    if ((state->other_mode_high & OTHER_HIGH_TEXTURE_LOD) != 0U) {
        material->fallback_flags |= GE_PICA_FALLBACK_TEXTURE_LOD;
    }
    if ((state->other_mode_high & OTHER_HIGH_TEXTURE_LUT_MASK) != 0U) {
        material->fallback_flags |= GE_PICA_FALLBACK_TEXTURE_LUT;
    }
    if ((state->other_mode_high & OTHER_HIGH_COLOR_DITHER_MASK) != 0U) {
        material->fallback_flags |= GE_PICA_FALLBACK_COLOR_DITHER;
    }
    if (material->texture_enabled != 0U
            && state->active_texture_binding
                == GE_GBI_TEXTURE_BINDING_RARE_ID
            && state->rare_texture.detail_texture_id != 0U) {
        material->fallback_flags |= GE_PICA_FALLBACK_DETAIL_TEXTURE;
    }
    if (material->texture_enabled != 0U
            && state->active_texture_binding
                == GE_GBI_TEXTURE_BINDING_RARE_ID
            && state->rare_texture.type < UINT8_C(3)) {
        material->fallback_flags |= GE_PICA_FALLBACK_RARE_TEXTURE_TYPE;
    }
    if (material->cycle_type == GE_PICA_CYCLE_TWO) {
        material->fallback_flags |= GE_PICA_FALLBACK_TWO_CYCLE;
    } else if (material->cycle_type == GE_PICA_CYCLE_COPY
               || material->cycle_type == GE_PICA_CYCLE_FILL) {
        material->fallback_flags |= GE_PICA_FALLBACK_COPY_FILL_CYCLE;
    }

    alpha_compare = state->other_mode_low & OTHER_LOW_ALPHA_COMPARE_MASK;
    material->alpha_test = alpha_compare == 0U
        ? GE_PICA_ALPHA_TEST_DISABLED : GE_PICA_ALPHA_TEST_THRESHOLD;
    material->alpha_threshold = material->blend_color.alpha;
    if ((state->other_mode_low & OTHER_LOW_COVERAGE_TIMES_ALPHA) != 0U) {
        /* RDP coverage-times-alpha suppresses zero-alpha fragments even
         * with G_AC_NONE. PICA has no coverage buffer: retain that endpoint
         * through alpha testing and report the fractional-coverage gap. */
        if (alpha_compare == 0U) {
            material->alpha_test = GE_PICA_ALPHA_TEST_THRESHOLD;
            material->alpha_threshold = 0U;
        }
        material->fallback_flags |= GE_PICA_FALLBACK_COVERAGE;
    }
    if (alpha_compare == OTHER_LOW_ALPHA_COMPARE_MASK
            || (state->other_mode_high & OTHER_HIGH_ALPHA_DITHER_MASK)
                != 0U) {
        material->fallback_flags |= GE_PICA_FALLBACK_ALPHA_DITHER;
    }
    if ((state->other_mode_low & OTHER_LOW_DEPTH_SOURCE) != 0U) {
        material->fallback_flags |= GE_PICA_FALLBACK_PRIMITIVE_DEPTH;
    }
    material->depth_mode = (GePicaDepthMode)((state->other_mode_low
        & OTHER_LOW_DEPTH_MODE_MASK) >> 10);
    material->depth_test_enabled = (state->geometry_mode & GEOMETRY_ZBUFFER)
            != 0U && (state->other_mode_low & OTHER_LOW_DEPTH_COMPARE) != 0U
        ? 1U : 0U;
    material->depth_write_enabled = (state->geometry_mode & GEOMETRY_ZBUFFER)
            != 0U && (state->other_mode_low & OTHER_LOW_DEPTH_WRITE) != 0U
        ? 1U : 0U;
    material->blend_enabled = (state->other_mode_low & OTHER_LOW_FORCE_BLEND)
            != 0U || material->depth_mode == GE_PICA_DEPTH_TRANSLUCENT
        ? 1U : 0U;
    if (material->blend_enabled != 0U) {
        material->fallback_flags |= GE_PICA_FALLBACK_BLENDER;
    }

    ge_pica_translate_combiner(state, material);
    return GE_PICA_MATERIAL_OK;
}

static void ge_pica_rectangle_vertex(
    GePicaScreenVertex *vertex, float x, float y,
    float texture_s, float texture_t,
    const GePicaTextureBindingTransform *binding)
{
    vertex->x = x;
    vertex->y = y;
    vertex->z = 0.5f;
    vertex->texture_u = texture_s * binding->u_scale + binding->u_bias;
    vertex->texture_v = texture_t * binding->v_scale + binding->v_bias;
    /* Texture rectangles have no RSP shade vertex. White is the neutral
     * primary input; authored primitive/environment colors remain in the
     * translated material and are selected by the original combiner. */
    vertex->red = 1.0f;
    vertex->green = 1.0f;
    vertex->blue = 1.0f;
    vertex->alpha = 1.0f;
}

GePicaMaterialStatus ge_pica_texture_rectangle_translate(
    const GeGbiRenderState *state,
    const GeGbiTextureRectangle *rectangle,
    const GePicaTextureBindingTransform *binding,
    GePicaTextureRectangleDraw *draw)
{
    const GeGbiTileState *tile;
    float left;
    float top;
    float right;
    float bottom;
    float width;
    float height;
    float s0;
    float t0;
    float s_right;
    float t_right;
    float s_bottom;
    float t_bottom;
    float horizontal_rate_scale;

    if (state == NULL || rectangle == NULL || binding == NULL || draw == NULL
            || rectangle->tile >= 8U || binding->u_scale == 0.0f
            || binding->v_scale == 0.0f)
        return GE_PICA_MATERIAL_INVALID_ARGUMENT;
    tile = &state->tiles[rectangle->tile];
    /* Texture shifts alter the RDP coordinate domain and need a distinct
     * sampler transform. Watch/text rectangles author zero shifts; reject a
     * different domain rather than silently approximate it. */
    if (tile->shift_s != 0U || tile->shift_t != 0U)
        return GE_PICA_MATERIAL_INVALID_ARGUMENT;
    if (ge_pica_material_translate(state, &draw->material)
            != GE_PICA_MATERIAL_OK)
        return GE_PICA_MATERIAL_INVALID_ARGUMENT;

    draw->material.texture_tile = rectangle->tile;
    draw->material.wrap_s = ge_pica_wrap(tile->clamp_mirror_s);
    draw->material.wrap_t = ge_pica_wrap(tile->clamp_mirror_t);
    draw->tile = rectangle->tile;
    draw->flipped = rectangle->flipped;

    left = (float)rectangle->screen.upper_x * 0.25f;
    top = (float)rectangle->screen.upper_y * 0.25f;
    right = (float)rectangle->screen.lower_x * 0.25f;
    bottom = (float)rectangle->screen.lower_y * 0.25f;
    width = right - left;
    height = bottom - top;
    s0 = (float)rectangle->s * (1.0f / 32.0f)
        - (float)tile->upper_s * 0.25f;
    t0 = (float)rectangle->t * (1.0f / 32.0f)
        - (float)tile->upper_t * 0.25f;
    horizontal_rate_scale = draw->material.cycle_type == GE_PICA_CYCLE_COPY
        ? 0.25f : 1.0f;

    if (rectangle->flipped == 0U) {
        s_right = s0 + width * (float)rectangle->dsdx
            * (1.0f / 1024.0f) * horizontal_rate_scale;
        t_right = t0;
        s_bottom = s0;
        t_bottom = t0 + height * (float)rectangle->dtdy
            * (1.0f / 1024.0f);
    } else {
        s_right = s0;
        t_right = t0 + width * (float)rectangle->dtdy
            * (1.0f / 1024.0f) * horizontal_rate_scale;
        s_bottom = s0 + height * (float)rectangle->dsdx
            * (1.0f / 1024.0f);
        t_bottom = t0;
    }

    ge_pica_rectangle_vertex(&draw->vertices[0], left, top,
        s0, t0, binding);
    ge_pica_rectangle_vertex(&draw->vertices[1], right, top,
        s_right, t_right, binding);
    ge_pica_rectangle_vertex(&draw->vertices[2], right, bottom,
        s_right + s_bottom - s0, t_right + t_bottom - t0, binding);
    draw->vertices[3] = draw->vertices[0];
    draw->vertices[4] = draw->vertices[2];
    ge_pica_rectangle_vertex(&draw->vertices[5], left, bottom,
        s_bottom, t_bottom, binding);
    return GE_PICA_MATERIAL_OK;
}

GePicaMaterialStatus ge_pica_texture_rectangle_translate_action(
    const GeGbiRenderState *state,
    const GeGbiStateAction *action,
    const GePicaTextureBindingTransform *binding,
    GePicaTextureRectangleDraw *draw)
{
    if (action == NULL
            || action->kind
                != GE_GBI_STATE_ACTION_DRAW_TEXTURE_RECTANGLE)
        return GE_PICA_MATERIAL_INVALID_ARGUMENT;
    return ge_pica_texture_rectangle_translate(
        state, &action->data.texture_rectangle, binding, draw);
}

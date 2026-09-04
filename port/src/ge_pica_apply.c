#include "ge_pica_apply.h"

#include <stddef.h>
#include <string.h>

static GePicaColor ge_pica_apply_white(void)
{
    GePicaColor color;

    color.red = UINT8_MAX;
    color.green = UINT8_MAX;
    color.blue = UINT8_MAX;
    color.alpha = UINT8_MAX;
    return color;
}

static void ge_pica_apply_color(const GePicaMaterial *material,
                                GePicaApplyState *state)
{
    GePicaApplyChannel *channel = &state->color;

    channel->source0 = GE_PICA_APPLY_SOURCE_PRIMARY;
    channel->source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
    channel->combine = GE_PICA_APPLY_REPLACE;
    switch (material->color_combine) {
    case GE_PICA_COMBINE_SHADE:
        break;
    case GE_PICA_COMBINE_PRIMITIVE:
        channel->source0 = GE_PICA_APPLY_SOURCE_CONSTANT;
        state->constant_color.red = material->primitive_color.red;
        state->constant_color.green = material->primitive_color.green;
        state->constant_color.blue = material->primitive_color.blue;
        break;
    case GE_PICA_COMBINE_ENVIRONMENT:
        channel->source0 = GE_PICA_APPLY_SOURCE_CONSTANT;
        state->constant_color.red = material->environment_color.red;
        state->constant_color.green = material->environment_color.green;
        state->constant_color.blue = material->environment_color.blue;
        break;
    case GE_PICA_COMBINE_TEXTURE0:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        state->texture_required = UINT8_C(1);
        break;
    case GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        channel->source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
        channel->combine = GE_PICA_APPLY_MODULATE;
        state->texture_required = UINT8_C(1);
        break;
    case GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        channel->source1 = GE_PICA_APPLY_SOURCE_CONSTANT;
        channel->combine = GE_PICA_APPLY_MODULATE;
        state->constant_color.red = material->primitive_color.red;
        state->constant_color.green = material->primitive_color.green;
        state->constant_color.blue = material->primitive_color.blue;
        state->texture_required = UINT8_C(1);
        break;
    case GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        channel->source1 = GE_PICA_APPLY_SOURCE_CONSTANT;
        channel->combine = GE_PICA_APPLY_MODULATE;
        state->constant_color.red = material->environment_color.red;
        state->constant_color.green = material->environment_color.green;
        state->constant_color.blue = material->environment_color.blue;
        state->texture_required = UINT8_C(1);
        break;
    default:
        state->apply_fallback_flags |= GE_PICA_APPLY_FALLBACK_INVALID_ENUM;
        break;
    }
}

static void ge_pica_apply_alpha(const GePicaMaterial *material,
                                GePicaApplyState *state)
{
    GePicaApplyChannel *channel = &state->alpha;

    channel->source0 = GE_PICA_APPLY_SOURCE_PRIMARY;
    channel->source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
    channel->combine = GE_PICA_APPLY_REPLACE;
    switch (material->alpha_combine) {
    case GE_PICA_ALPHA_ONE:
        channel->source0 = GE_PICA_APPLY_SOURCE_CONSTANT;
        state->constant_color.alpha = UINT8_MAX;
        break;
    case GE_PICA_ALPHA_SHADE:
        break;
    case GE_PICA_ALPHA_PRIMITIVE:
        channel->source0 = GE_PICA_APPLY_SOURCE_CONSTANT;
        state->constant_color.alpha = material->primitive_color.alpha;
        break;
    case GE_PICA_ALPHA_ENVIRONMENT:
        channel->source0 = GE_PICA_APPLY_SOURCE_CONSTANT;
        state->constant_color.alpha = material->environment_color.alpha;
        break;
    case GE_PICA_ALPHA_TEXTURE0:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        state->texture_required = UINT8_C(1);
        break;
    case GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        channel->source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
        channel->combine = GE_PICA_APPLY_MODULATE;
        state->texture_required = UINT8_C(1);
        break;
    case GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE_ADD_PRIMITIVE:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        channel->source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
        channel->combine = GE_PICA_APPLY_MULTIPLY_ADD;
        state->constant_color.alpha = material->primitive_color.alpha;
        state->texture_required = UINT8_C(1);
        break;
    case GE_PICA_ALPHA_TEXTURE0_MODULATE_PRIMITIVE:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        channel->source1 = GE_PICA_APPLY_SOURCE_CONSTANT;
        channel->combine = GE_PICA_APPLY_MODULATE;
        state->constant_color.alpha = material->primitive_color.alpha;
        state->texture_required = UINT8_C(1);
        break;
    case GE_PICA_ALPHA_TEXTURE0_MODULATE_ENVIRONMENT:
        channel->source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
        channel->source1 = GE_PICA_APPLY_SOURCE_CONSTANT;
        channel->combine = GE_PICA_APPLY_MODULATE;
        state->constant_color.alpha = material->environment_color.alpha;
        state->texture_required = UINT8_C(1);
        break;
    default:
        state->apply_fallback_flags |= GE_PICA_APPLY_FALLBACK_INVALID_ENUM;
        break;
    }
}

GePicaApplyStatus ge_pica_apply_compile(const GePicaMaterial *material,
                                        GePicaApplyState *state)
{
    if (material == NULL || state == NULL) {
        return GE_PICA_APPLY_INVALID_ARGUMENT;
    }
    memset(state, 0, sizeof(*state));
    state->constant_color = ge_pica_apply_white();
    state->material_fallback_flags = material->fallback_flags;
    state->draw_enabled = UINT8_C(1);
    state->wrap_s = material->wrap_s;
    state->wrap_t = material->wrap_t;
    state->min_filter = material->min_filter;
    state->mag_filter = material->mag_filter;
    state->depth_test_enabled = material->depth_test_enabled;
    state->depth_write_enabled = material->depth_write_enabled;
    state->alpha_test_enabled = material->alpha_test
        == GE_PICA_ALPHA_TEST_THRESHOLD ? UINT8_C(1) : UINT8_C(0);
    state->alpha_threshold = material->alpha_threshold;
    state->blend_enabled = material->blend_enabled;
    if (material->depth_mode != GE_PICA_DEPTH_OPAQUE) {
        state->apply_fallback_flags |= GE_PICA_APPLY_FALLBACK_DEPTH_MODE;
    }
    if (material->fog_enabled != 0U) {
        state->apply_fallback_flags |= GE_PICA_APPLY_FALLBACK_FOG;
    }

    switch (material->cull_mode) {
    case GE_PICA_CULL_NONE:
        state->cull = GE_PICA_APPLY_CULL_NONE;
        break;
    case GE_PICA_CULL_FRONT:
        state->cull = GE_PICA_APPLY_CULL_FRONT;
        break;
    case GE_PICA_CULL_BACK:
        state->cull = GE_PICA_APPLY_CULL_BACK;
        break;
    case GE_PICA_CULL_BOTH:
        state->cull = GE_PICA_APPLY_CULL_NONE;
        state->draw_enabled = UINT8_C(0);
        state->apply_fallback_flags |= GE_PICA_APPLY_FALLBACK_CULL_BOTH;
        break;
    default:
        state->cull = GE_PICA_APPLY_CULL_NONE;
        state->apply_fallback_flags |= GE_PICA_APPLY_FALLBACK_INVALID_ENUM;
        break;
    }

    if (state->wrap_s > GE_PICA_WRAP_CLAMP
            || state->wrap_t > GE_PICA_WRAP_CLAMP
            || state->min_filter > GE_PICA_FILTER_LINEAR
            || state->mag_filter > GE_PICA_FILTER_LINEAR) {
        state->wrap_s = GE_PICA_WRAP_CLAMP;
        state->wrap_t = GE_PICA_WRAP_CLAMP;
        state->min_filter = GE_PICA_FILTER_NEAREST;
        state->mag_filter = GE_PICA_FILTER_NEAREST;
        state->apply_fallback_flags |= GE_PICA_APPLY_FALLBACK_INVALID_ENUM;
    }
    ge_pica_apply_color(material, state);
    ge_pica_apply_alpha(material, state);
    return GE_PICA_APPLY_OK;
}

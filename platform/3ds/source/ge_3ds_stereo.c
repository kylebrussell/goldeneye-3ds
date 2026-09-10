#include "ge_3ds_stereo.h"

#include <math.h>
#include <string.h>

void ge_3ds_stereo_config_defaults(Ge3dsStereoConfig *config)
{
    if (config == NULL) return;
    config->enabled = true;
    /* About 2% of the 400-pixel screen at infinity, comfortably below the
     * usual handheld guideline, so the slider's upper range stays usable. */
    config->parallax_px = 8.0f;
    /* Original runtime units: the player's eye stands roughly 150 units
     * above the floor, so the screen plane sits about one body length ahead
     * and near walls pop out without extreme disparity. */
    config->convergence = 180.0f;
    config->first_person_parallax_px = 3.0f;
    config->first_person_convergence = 45.0f;
    config->hud_parallax_px = 2.0f;
    config->cull_margin_per_px = 1.0f / 160.0f;
}

bool ge_3ds_stereo_config_load(Ge3dsStereoConfig *config, FILE *stream)
{
    char line[128];
    bool clean = true;
    if (config == NULL) return false;
    ge_3ds_stereo_config_defaults(config);
    if (stream == NULL) return false;
    while (fgets(line, sizeof(line), stream) != NULL) {
        char key[48];
        float value;
        if (line[0] == '#' || sscanf(line, "%47s %f", key, &value) != 2) continue;
        if (!isfinite(value)) {
            clean = false;
            continue;
        }
        if (strcmp(key, "enabled") == 0) config->enabled = value != 0.0f;
        else if (strcmp(key, "parallax_px") == 0 && value >= 0.0f && value <= 40.0f)
            config->parallax_px = value;
        else if (strcmp(key, "convergence") == 0 && value > 0.0f)
            config->convergence = value;
        else if (strcmp(key, "first_person_parallax_px") == 0 && value >= 0.0f
                && value <= 40.0f)
            config->first_person_parallax_px = value;
        else if (strcmp(key, "first_person_convergence") == 0 && value > 0.0f)
            config->first_person_convergence = value;
        else if (strcmp(key, "hud_parallax_px") == 0 && value >= -40.0f
                && value <= 40.0f)
            config->hud_parallax_px = value;
        else if (strcmp(key, "cull_margin_per_px") == 0 && value >= 0.0f)
            config->cull_margin_per_px = value;
        else clean = false;
    }
    return clean;
}

void ge_3ds_stereo_pixel_shift(const Ge3dsStereoConfig *config, float slider,
                               int eye, Ge3dsStereoLayer layer,
                               float *constant, float *coefficient)
{
    float parallax;
    float convergence;
    float sign;
    if (constant == NULL || coefficient == NULL) return;
    *constant = 0.0f;
    *coefficient = 0.0f;
    if (config == NULL || !config->enabled || !(slider > 0.0f) || eye == 0)
        return;
    if (slider > 1.0f) slider = 1.0f;
    /* Left eye: near geometry (W < D) shifts right, far geometry left. */
    sign = eye < 0 ? 1.0f : -1.0f;
    switch (layer) {
    case GE_3DS_STEREO_LAYER_WORLD:
        parallax = config->parallax_px * slider;
        convergence = config->convergence;
        *constant = sign * parallax * convergence;
        *coefficient = -sign * parallax;
        return;
    case GE_3DS_STEREO_LAYER_FIRST_PERSON:
        parallax = config->first_person_parallax_px * slider;
        convergence = config->first_person_convergence;
        *constant = sign * parallax * convergence;
        *coefficient = -sign * parallax;
        return;
    case GE_3DS_STEREO_LAYER_HUD:
        /* Screen-space vertices carry w = 1; a constant is a pure pop-out. */
        *constant = sign * config->hud_parallax_px * slider;
        return;
    case GE_3DS_STEREO_LAYER_SKY:
        *constant = -sign * config->parallax_px * slider;
        return;
    case GE_3DS_STEREO_LAYER_FLAT:
    default:
        return;
    }
}

void ge_3ds_stereo_uniforms(const Ge3dsStereoConfig *config, float slider,
                            int eye, Ge3dsStereoLayer layer,
                            const float screen_x_column[2],
                            Ge3dsStereoUniforms *uniforms)
{
    float constant;
    float coefficient;
    if (uniforms == NULL) return;
    memset(uniforms, 0, sizeof(*uniforms));
    if (screen_x_column == NULL) return;
    ge_3ds_stereo_pixel_shift(config, slider, eye, layer, &constant, &coefficient);
    uniforms->constant[0] = screen_x_column[0] * constant;
    uniforms->constant[1] = screen_x_column[1] * constant;
    uniforms->coefficient[0] = screen_x_column[0] * coefficient;
    uniforms->coefficient[1] = screen_x_column[1] * coefficient;
}

float ge_3ds_stereo_max_shift_px(const Ge3dsStereoConfig *config,
    float slider, Ge3dsStereoLayer layer, float near_depth)
{
    float constant;
    float coefficient;
    float far_shift;
    float near_shift;
    ge_3ds_stereo_pixel_shift(config, slider, -1, layer,
        &constant, &coefficient);
    if (layer != GE_3DS_STEREO_LAYER_WORLD
            && layer != GE_3DS_STEREO_LAYER_FIRST_PERSON)
        return fabsf(constant + coefficient);
    far_shift = fabsf(coefficient);
    if (constant == 0.0f) return far_shift;
    if (!(near_depth > 0.0f) || !isfinite(near_depth)) return INFINITY;
    /* |constant / W + coefficient| attains its supremum at the near
     * endpoint or infinity. Do not clamp W: even small nearby props can
     * enter an eye's image from outside the mono frustum. */
    near_shift = fabsf(constant / near_depth + coefficient);
    return near_shift > far_shift ? near_shift : far_shift;
}

float ge_3ds_stereo_max_world_shift_px(const Ge3dsStereoConfig *config,
                                       float slider, float near_depth)
{
    return ge_3ds_stereo_max_shift_px(config, slider,
        GE_3DS_STEREO_LAYER_WORLD, near_depth);
}

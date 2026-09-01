#include "ge_3ds_fade_overlay.h"

static float ge_fade_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

int ge_3ds_fade_overlay_from_snapshot(
    const GeOriginalDamMissionExitSnapshot *snapshot,
    Ge3dsFadeOverlay *overlay)
{
    if (snapshot == 0 || overlay == 0) return 0;
    overlay->red = ge_fade_clamp((float)snapshot->fade_red, 0.0f, 255.0f)
        / 255.0f;
    overlay->green = ge_fade_clamp(
        (float)snapshot->fade_green, 0.0f, 255.0f) / 255.0f;
    overlay->blue = ge_fade_clamp(
        (float)snapshot->fade_blue, 0.0f, 255.0f) / 255.0f;
    overlay->alpha = ge_fade_clamp(snapshot->fade_fraction, 0.0f, 1.0f);
    overlay->visible = overlay->alpha > 0.0f;
    return 1;
}

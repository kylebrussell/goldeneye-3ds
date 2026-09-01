#include "ge_3ds_fade_overlay.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    GeOriginalDamMissionExitSnapshot snapshot;
    Ge3dsFadeOverlay overlay;

    memset(&snapshot, 0, sizeof(snapshot));
    memset(&overlay, 0, sizeof(overlay));
    snapshot.fade_red = 0;
    snapshot.fade_green = 64;
    snapshot.fade_blue = 255;
    snapshot.fade_fraction = 0.5f;
    assert(ge_3ds_fade_overlay_from_snapshot(&snapshot, &overlay));
    assert(overlay.red == 0.0f);
    assert(fabsf(overlay.green - 64.0f / 255.0f) < 0.0001f);
    assert(overlay.blue == 1.0f);
    assert(overlay.alpha == 0.5f);
    assert(overlay.visible == 1U);

    /* Exact colour-screen values published by the canonical damage flash
     * body after Bond dies.  The platform must preserve the red primitive
     * colour rather than treating it as a texture modulation input. */
    snapshot.fade_red = 150;
    snapshot.fade_green = 0;
    snapshot.fade_blue = 0;
    snapshot.fade_fraction = 0.7058824f;
    assert(ge_3ds_fade_overlay_from_snapshot(&snapshot, &overlay));
    assert(fabsf(overlay.red - 150.0f / 255.0f) < 0.0001f);
    assert(overlay.green == 0.0f);
    assert(overlay.blue == 0.0f);
    assert(fabsf(overlay.alpha - 180.0f / 255.0f) < 0.0001f);
    assert(overlay.visible == 1U);

    snapshot.fade_red = -20;
    snapshot.fade_green = 300;
    snapshot.fade_fraction = 2.0f;
    assert(ge_3ds_fade_overlay_from_snapshot(&snapshot, &overlay));
    assert(overlay.red == 0.0f);
    assert(overlay.green == 1.0f);
    assert(overlay.alpha == 1.0f);

    snapshot.fade_fraction = -1.0f;
    assert(ge_3ds_fade_overlay_from_snapshot(&snapshot, &overlay));
    assert(overlay.alpha == 0.0f);
    assert(overlay.visible == 0U);
    assert(!ge_3ds_fade_overlay_from_snapshot(NULL, &overlay));
    assert(!ge_3ds_fade_overlay_from_snapshot(&snapshot, NULL));

    puts("3DS fade overlay consumes canonical colour-screen RGBA state");
    return 0;
}

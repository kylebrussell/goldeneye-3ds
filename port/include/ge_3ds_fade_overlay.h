#ifndef GE_3DS_FADE_OVERLAY_H
#define GE_3DS_FADE_OVERLAY_H

#include <stdint.h>

#include "ge_original_dam_mission_exit_services.h"

typedef struct Ge3dsFadeOverlay {
    float red;
    float green;
    float blue;
    float alpha;
    uint8_t visible;
} Ge3dsFadeOverlay;

/* Converts the original player's colour-screen state into the PICA primary
 * colour consumed by the final fullscreen pass. */
int ge_3ds_fade_overlay_from_snapshot(
    const GeOriginalDamMissionExitSnapshot *snapshot,
    Ge3dsFadeOverlay *overlay);

#endif

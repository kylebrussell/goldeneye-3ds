#include "ge_original_sfx_volume.h"

#include <stddef.h>
#include <stdint.h>

/* Canonical music.c owns these pointers; sndNew normally allocates seven
 * entries and initializes both tables to 0x7fff. The port's PCM mixer does
 * not run sndNew, but retained Watch/settings code still reads its tables. */
extern uint16_t *g_sndSfxSlotNaturalVolume;
extern int16_t *g_sndSfxSlotVolume;

static uint16_t natural_volume[7] = {
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff
};
static int16_t effective_volume[7];

void ge_original_sfx_volume_init(void)
{
    if (g_sndSfxSlotNaturalVolume == NULL)
        g_sndSfxSlotNaturalVolume = natural_volume;
    if (g_sndSfxSlotVolume == NULL) {
        for (size_t i = 0; i < 7U; ++i)
            effective_volume[i] = (int16_t)g_sndSfxSlotNaturalVolume[i];
        g_sndSfxSlotVolume = effective_volume;
    }
}

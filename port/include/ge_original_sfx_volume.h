#ifndef GE_ORIGINAL_SFX_VOLUME_H
#define GE_ORIGINAL_SFX_VOLUME_H

/* Supply sndNew's slot-volume storage when the native mixer replaces sndNew.
 * Repeated calls retain existing volume settings and external owners. */
void ge_original_sfx_volume_init(void);

#endif

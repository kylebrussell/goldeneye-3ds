#ifndef GE_ORIGINAL_DAM_GUARD_CHR_SCHEDULER_H
#define GE_ORIGINAL_DAM_GUARD_CHR_SCHEDULER_H

#include <bondtypes.h>

/* Unchanged chr.c bodies retained under isolated names. They are deliberately
 * not installed in the live frame loop until their full production boundary
 * and the reachable action graph have both been closed. */
void ge_original_dam_guard_chr_detect_rooms_exact(ChrRecord *chr);
void ge_original_dam_guard_chr_update_anim_exact(ChrRecord *chr,
                                                  s32 tickamount);
s32 ge_original_dam_guard_chr_tick_exact(PropRecord *prop);

/* Unchanged chrlvAllChrTick body. It advances the setup's synthetic
 * background mission actors before active prop rendering/ticks, exactly as
 * the original frame scheduler does. */
void ge_original_dam_guard_all_chr_tick_exact(void);

/* Unchanged chrprop.c propsTick body. With the authored active prop list this
 * is the canonical tail-to-head dispatcher that calls chrTick once per guard. */
void ge_original_dam_guard_props_tick_exact(void);

#endif

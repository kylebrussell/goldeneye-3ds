#ifndef GE_ORIGINAL_DAM_GUARD_DEATH_LINK_H
#define GE_ORIGINAL_DAM_GUARD_DEATH_LINK_H

/* Retain the unchanged canonical death handlers in the native image without
 * executing the not-yet-complete all-actions scheduler. */
void ge_original_dam_guard_death_force_link(void);

#endif

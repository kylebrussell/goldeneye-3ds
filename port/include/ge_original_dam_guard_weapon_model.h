#ifndef GE_ORIGINAL_DAM_GUARD_WEAPON_MODEL_H
#define GE_ORIGINAL_DAM_GUARD_WEAPON_MODEL_H

#include "ge_original_dam_guard_model.h"

#include <stddef.h>

#define GE_ORIGINAL_DAM_GUARD_WEAPON_MODEL_BLOB_SIZE 2352U

/* Exact generated PchrkalashZ model used by Dam setup collectables 59..62. */
int ge_original_dam_guard_weapon_model_prepare(void);
void *ge_original_dam_guard_weapon_model_header(void);
size_t ge_original_dam_guard_weapon_model_rw_words(void);
const GeOriginalDamGuardDisplayList *
ge_original_dam_guard_weapon_model_display_list(void);

#endif

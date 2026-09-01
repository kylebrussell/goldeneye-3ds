#ifndef GE_ORIGINAL_BUG_MODEL_H
#define GE_ORIGINAL_BUG_MODEL_H

#include <stdint.h>

/* Native linkage for the decompiled Pchrbug model used by Dam's authored
 * covert-modem loadout. The model geometry and display list remain the exact
 * generated asset definitions. */
int ge_original_bug_model_prepare(void);
void *ge_original_bug_model_header(void);
int32_t ge_original_bug_model_id(void);

#endif

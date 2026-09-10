#ifndef GE_ORIGINAL_PROP_RENDER_SEMANTICS_H
#define GE_ORIGINAL_PROP_RENDER_SEMANTICS_H
#include <stdint.h>
#include "ge_original_stage_guard_runtime.h"
/* Original BG room/pass/prop/node order, before first-person gun production.
 * Mutates canonical render-side CPU state once, while retaining float model
 * banks for the platform renderer. Repeated display draws only read caches. */
int ge_original_prop_render_semantics_prepare(GeOriginalStageGuardRuntime *guards,
    uint64_t tick,unsigned viewport);
#endif

#ifndef GE_ORIGINAL_DAM_MISSION_STAGE_STORAGE_H
#define GE_ORIGINAL_DAM_MISSION_STAGE_STORAGE_H

#include <stddef.h>

/*
 * Resets the native MEMPOOL_STAGE adapter to the start of its currently
 * bound arena.  The 3DS bootstrap normally binds g_mempPools[MEMPOOL_STAGE]
 * before the first allocation; focused host tests use the owned fallback.
 */
void ge_original_dam_stage_storage_reset(void);

/* Focused allocation-boundary introspection. */
size_t ge_original_dam_stage_storage_used(void);
size_t ge_original_dam_stage_storage_capacity(void);

#endif

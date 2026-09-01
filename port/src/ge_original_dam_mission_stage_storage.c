#include <stddef.h>
#include <stdint.h>

#include "bondtypes.h"
#include "ge_original_dam_mission_stage_storage.h"
#include "memp.h"

ChrRecord *g_ActiveChrs;
s32 g_ActiveChrsCount;

#ifndef GE_PLATFORM_3DS
enum {
    /* Host-only setup/mission tests do not bind the canonical pool table.
     * Surface II authors 18 background actors, so the former Dam-sized 8 KiB
     * fallback was not a campaign-wide equivalent of MEMPOOL_STAGE. */
    GE_DAM_STAGE_FALLBACK_CAPACITY = 65536
};

static union {
    max_align_t alignment;
    uint8_t bytes[GE_DAM_STAGE_FALLBACK_CAPACITY];
} dam_stage_fallback_storage;

static MemoryPool dam_stage_fallback_pool;
#endif

/* The full 3DS link owns this symbol in the exact guard-damage support slice.
 * Small mission-flow tests intentionally do not, and use the fallback above. */
#ifdef GE_PLATFORM_3DS
extern MemoryPool g_mempPools[MEMPOOL_COUNT];
#endif

static MemoryPool *ge_dam_bound_stage_pool(void)
{
#ifdef GE_PLATFORM_3DS
    MemoryPool *pool;

    {
        pool = &g_mempPools[MEMPOOL_STAGE];
        if (pool->start != NULL && pool->pos != NULL && pool->end != NULL
                && pool->start <= pool->pos && pool->pos <= pool->end)
            return pool;
    }
#else
    if (dam_stage_fallback_pool.start == NULL) {
        dam_stage_fallback_pool.start = dam_stage_fallback_storage.bytes;
        dam_stage_fallback_pool.pos = dam_stage_fallback_storage.bytes;
        dam_stage_fallback_pool.end = dam_stage_fallback_storage.bytes
            + sizeof(dam_stage_fallback_storage.bytes);
        dam_stage_fallback_pool.prevpos = NULL;
    }
    return &dam_stage_fallback_pool;
#endif

    /* A live 3DS caller before the stage-pool bind is a startup-order bug.
     * Do not silently redirect it into host test storage. */
    return NULL;
}

void ge_original_dam_stage_storage_reset(void)
{
    MemoryPool *pool = ge_dam_bound_stage_pool();

    if (pool == NULL) return;
    pool->pos = pool->start;
    pool->prevpos = NULL;
}

size_t ge_original_dam_stage_storage_used(void)
{
    MemoryPool *pool = ge_dam_bound_stage_pool();
    if (pool == NULL) return 0U;
    return (size_t)(pool->pos - pool->start);
}

size_t ge_original_dam_stage_storage_capacity(void)
{
    MemoryPool *pool = ge_dam_bound_stage_pool();
    if (pool == NULL) return 0U;
    return (size_t)(pool->end - pool->start);
}

/*
 * Native bounded form of the original stage bump allocator.  The former
 * adapter returned dam_stage_ai_storage.bytes for every request, aliasing the
 * four canonical vtxstore allocations with g_ActiveChrs.  Preserve the N64
 * stage lifetime and monotonically advancing ownership instead.
 */
void *mempAllocBytesInBank(u32 bytes, u8 bank)
{
    MemoryPool *pool;
    uintptr_t current;
    uintptr_t aligned;
    size_t remaining;

    if (bank != MEMPOOL_STAGE) return NULL;
    pool = ge_dam_bound_stage_pool();
    if (pool == NULL) return NULL;
    current = (uintptr_t)pool->pos;
    aligned = (current + 15U) & ~(uintptr_t)15U;
    if (aligned < current || aligned > (uintptr_t)pool->end) return NULL;
    remaining = (size_t)((uintptr_t)pool->end - aligned);
    if ((size_t)bytes > remaining) return NULL;

    pool->prevpos = (u8 *)aligned;
    pool->pos = (u8 *)(aligned + (size_t)bytes);
    return pool->prevpos;
}

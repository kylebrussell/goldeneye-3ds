#include "ge_original_embedment_pool.h"

#include <ultra64.h>
#include <bondtypes.h>
#include "game/chrai.h"

extern Embedment g_Embedments[EMBEDMENT_ARR_MAX];

void ge_original_embedment_pool_reset_exact(void)
{
    size_t index;

    /* Exact relevant initobjects body. embedmentAllocate consumes bit zero
     * and embedmentFree restores it after the attached object is released. */
    for (index = 0U; index < EMBEDMENT_ARR_MAX; ++index)
        g_Embedments[index].flags = EMBEDMENTFLAG_FREE;
}

size_t ge_original_embedment_pool_capacity(void)
{
    return EMBEDMENT_ARR_MAX;
}

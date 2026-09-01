#include "ge_original_embedment_pool.h"

#include <ultra64.h>
#include <bondtypes.h>
#include "game/chrai.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

Embedment g_Embedments[EMBEDMENT_ARR_MAX];

int main(void)
{
    size_t index;

    memset(g_Embedments, 0xa5, sizeof(g_Embedments));
    ge_original_embedment_pool_reset_exact();
    assert(ge_original_embedment_pool_capacity() == EMBEDMENT_ARR_MAX);
    for (index = 0U; index < EMBEDMENT_ARR_MAX; ++index)
        assert(g_Embedments[index].flags == EMBEDMENTFLAG_FREE);
    puts("canonical embedment pool reset: every objEmbed slot is free");
    return 0;
}

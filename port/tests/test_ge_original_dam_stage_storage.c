#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ge_original_dam_mission_stage_storage.h"
#include "memp.h"

MemoryPool g_mempPools[MEMPOOL_COUNT];

static _Alignas(16) uint8_t stage_arena[65536];

static int ranges_disjoint(const uint8_t *a, size_t a_size,
                           const uint8_t *b, size_t b_size)
{
    return a + a_size <= b || b + b_size <= a;
}

int main(void)
{
    uint8_t *metadata_a;
    uint8_t *vertices_a;
    uint8_t *metadata_b;
    uint8_t *vertices_b;
    uint8_t *background_ai;
    size_t used;

    memset(g_mempPools, 0, sizeof(g_mempPools));
    g_mempPools[MEMPOOL_STAGE].start = stage_arena;
    g_mempPools[MEMPOOL_STAGE].pos = stage_arena;
    g_mempPools[MEMPOOL_STAGE].end = stage_arena + sizeof(stage_arena);
    ge_original_dam_stage_storage_reset();

    /* Exact single-player vtxstore allocation order, followed by the exact
     * eight-record Dam background-AI allocation boundary. */
    metadata_a = mempAllocBytesInBank(20U * 20U, MEMPOOL_STAGE);
    vertices_a = mempAllocBytesInBank(500U * 16U, MEMPOOL_STAGE);
    metadata_b = mempAllocBytesInBank(40U * 20U, MEMPOOL_STAGE);
    vertices_b = mempAllocBytesInBank(1500U * 16U, MEMPOOL_STAGE);
    background_ai = mempAllocBytesInBank(3824U, MEMPOOL_STAGE);
    assert(metadata_a != NULL && vertices_a != NULL && metadata_b != NULL
           && vertices_b != NULL && background_ai != NULL);
    assert(((uintptr_t)metadata_a & 15U) == 0U);
    assert(((uintptr_t)vertices_a & 15U) == 0U);
    assert(((uintptr_t)metadata_b & 15U) == 0U);
    assert(((uintptr_t)vertices_b & 15U) == 0U);
    assert(((uintptr_t)background_ai & 15U) == 0U);
    assert(ranges_disjoint(metadata_a, 400U, vertices_a, 8000U));
    assert(ranges_disjoint(metadata_a, 400U, metadata_b, 800U));
    assert(ranges_disjoint(metadata_a, 400U, vertices_b, 24000U));
    assert(ranges_disjoint(metadata_a, 400U, background_ai, 3824U));
    assert(ranges_disjoint(vertices_a, 8000U, metadata_b, 800U));
    assert(ranges_disjoint(vertices_a, 8000U, vertices_b, 24000U));
    assert(ranges_disjoint(vertices_a, 8000U, background_ai, 3824U));
    assert(ranges_disjoint(metadata_b, 800U, vertices_b, 24000U));
    assert(ranges_disjoint(metadata_b, 800U, background_ai, 3824U));
    assert(ranges_disjoint(vertices_b, 24000U, background_ai, 3824U));

    memset(metadata_a, 0xa5, 400U);
    memset(background_ai, 0x5a, 3824U);
    assert(metadata_a[0] == 0xa5U && metadata_a[399] == 0xa5U);
    assert(background_ai[0] == 0x5aU && background_ai[3823] == 0x5aU);
    used = ge_original_dam_stage_storage_used();
    assert(used == 37024U);
    assert(ge_original_dam_stage_storage_capacity() == sizeof(stage_arena));
    assert(mempAllocBytesInBank(1U, MEMPOOL_PERMANENT) == NULL);
    assert(mempAllocBytesInBank((u32)sizeof(stage_arena), MEMPOOL_STAGE)
           == NULL);
    assert(ge_original_dam_stage_storage_used() == used);

    ge_original_dam_stage_storage_reset();
    assert(ge_original_dam_stage_storage_used() == 0U);
    assert(mempAllocBytesInBank(400U, MEMPOOL_STAGE) == metadata_a);

    puts("Dam stage allocator: vtxstore/background AI disjoint and reset-stable");
    return 0;
}

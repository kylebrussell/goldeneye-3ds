#include "ge_original_gun_frame_arena.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void *dynAllocate(int size);

int main(void)
{
    unsigned char storage[160];
    unsigned char preserved[48];
    GeOriginalDynFrameAudit audit;
    void *first;
    void *second;
    void *guard_matrices;
    const uintptr_t aligned = ((uintptr_t)(storage + 1) + 15U)
        & ~(uintptr_t)15U;
    const size_t expected_capacity = (size_t)(
        (uintptr_t)(storage + 1 + 128U) - aligned);

    assert(ge_original_gun_frame_arena_begin(storage + 1, 128U));
    assert(ge_original_gun_frame_arena_active());
    first = dynAllocate(1);
    second = dynAllocate(17);
    assert(((uintptr_t)first & 15U) == 0U);
    assert((unsigned char *)second - (unsigned char *)first == 16);
    assert(ge_original_gun_frame_arena_used() == 48U);
    memset(first, 0x5a, sizeof(preserved));
    memcpy(preserved, first, sizeof(preserved));

    /* The later canonical chrTick consumer shares dynAllocate and therefore
     * appends after the gun matrices instead of resetting their storage. */
    guard_matrices = dynAllocate(32);
    assert((unsigned char *)guard_matrices
        == (unsigned char *)first + sizeof(preserved));
    memset(guard_matrices, 0xa5, 32U);
    assert(memcmp(first, preserved, sizeof(preserved)) == 0);
    assert(ge_original_gun_frame_arena_finalize(&audit));
    assert(!ge_original_gun_frame_arena_active());
    assert(audit.generation == 1U);
    assert(audit.within_bounds);
    assert(audit.capacity == expected_capacity);
    assert(audit.used == 80U);

    assert(ge_original_gun_frame_arena_begin(storage + 1, 128U));
    assert(dynAllocate(1) == first);
    assert(ge_original_gun_frame_arena_finalize(&audit));
    assert(audit.generation == 2U && audit.used == 16U);
    puts("shared gun->chr dynAllocate/frame lifetime: ok");
    return 0;
}

#include "ge_original_rom_copy.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ultra64.h"

void romCopy(void *target, void *source, u32 size);

int main(void)
{
    static const uint8_t region_a[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    static const uint8_t region_b[] = {0xa0, 0xb1, 0xc2, 0xd3};
    uint8_t output[4] = {0};

    ge_original_rom_copy_reset();
    assert(ge_original_rom_copy_bind((uintptr_t)0x1000U, region_a,
                                     sizeof(region_a)) == 0);
    assert(ge_original_rom_copy_bind((uintptr_t)0x4000U, region_b,
                                     sizeof(region_b)) == 0);
    romCopy(output, (void *)(uintptr_t)0x1001U, 4U);
    assert(memcmp(output, region_a + 1, sizeof(output)) == 0);
    romCopy(output, (void *)(uintptr_t)0x4000U, 4U);
    assert(memcmp(output, region_b, sizeof(output)) == 0);
    assert(ge_original_rom_copy_last_error() == 0);
    return 0;
}

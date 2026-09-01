#include "ge_original_rom_copy.h"

#include <string.h>

#include "ultra64.h"

typedef struct GeOriginalRomRegion {
    uintptr_t canonical_start;
    const u8 *native_bytes;
    size_t byte_count;
} GeOriginalRomRegion;

static GeOriginalRomRegion g_regions[GE_ORIGINAL_ROM_COPY_MAX_REGIONS];
static size_t g_region_count;
static int g_last_error;

void ge_original_rom_copy_reset(void)
{
    memset(g_regions, 0, sizeof(g_regions));
    g_region_count = 0U;
    g_last_error = 0;
}

int ge_original_rom_copy_bind(uintptr_t canonical_start,
                              const void *native_bytes,
                              size_t byte_count)
{
    GeOriginalRomRegion *region;

    if (native_bytes == NULL || byte_count == 0U
            || g_region_count == GE_ORIGINAL_ROM_COPY_MAX_REGIONS
            || canonical_start > UINTPTR_MAX - byte_count) {
        return -1;
    }
    region = &g_regions[g_region_count++];
    region->canonical_start = canonical_start;
    region->native_bytes = native_bytes;
    region->byte_count = byte_count;
    return 0;
}

int ge_original_rom_copy_last_error(void)
{
    return g_last_error;
}

/* Native implementation of the original synchronous ROM-copy contract.  The
 * canonical source address is resolved through asset regions registered by
 * the platform layer; an unbound/out-of-range request fails closed. */
void romCopy(void *target, void *source, u32 size)
{
    uintptr_t address = (uintptr_t)source;
    size_t index;

    g_last_error = 0;
    if (target == NULL || size == 0U) {
        g_last_error = -1;
        __builtin_trap();
    }

    for (index = 0U; index < g_region_count; index++) {
        const GeOriginalRomRegion *region = &g_regions[index];
        size_t offset;

        if (address < region->canonical_start) continue;
        offset = (size_t)(address - region->canonical_start);
        if (offset <= region->byte_count
                && (size_t)size <= region->byte_count - offset) {
            memcpy(target, region->native_bytes + offset, (size_t)size);
            return;
        }
    }

    g_last_error = -2;
    __builtin_trap();
}

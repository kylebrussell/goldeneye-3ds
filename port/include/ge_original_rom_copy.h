#ifndef GE_ORIGINAL_ROM_COPY_H
#define GE_ORIGINAL_ROM_COPY_H

#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_ROM_COPY_MAX_REGIONS 8U

/* Bind a native asset buffer to the canonical linker address used by original
 * romCopy callers.  The buffer must remain alive until reset. */
int ge_original_rom_copy_bind(uintptr_t canonical_start,
                              const void *native_bytes,
                              size_t byte_count);
void ge_original_rom_copy_reset(void);
int ge_original_rom_copy_last_error(void);

#endif

#ifndef GE_ORIGINAL_LANGUAGE_TEXT_H
#define GE_ORIGINAL_LANGUAGE_TEXT_H

#include <stdint.h>

/* Native pointer-table view of the decomp-owned NTSC-U language assets.
 * Canonical text IDs remain (bank << 10) | slot; this boundary only replaces
 * language.c's N64 file-loader/32-bit offset relocation on the 3DS. */
const char *ge_original_language_text(uint16_t text_id);
const char *ge_original_language_text_by_bank(
    uint16_t bank, uint16_t slot);

#endif

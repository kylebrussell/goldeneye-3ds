#ifndef GE_ORIGINAL_MUSIC_BANK_H
#define GE_ORIGINAL_MUSIC_BANK_H

#include <stddef.h>
#include <stdint.h>

#include <libaudio.h>

typedef struct GeOriginalMusicBank GeOriginalMusicBank;

typedef struct GeOriginalMusicBankStats {
    uint32_t banks;
    uint32_t instruments;
    uint32_t sounds;
    uint32_t wavetables;
    uint32_t adpcm_books;
    uint32_t adpcm_loops;
    uint32_t raw_loops;
} GeOriginalMusicBankStats;

/* Relocate one authored N64 .ctl bank into native libaudio structures.  The
 * returned graph owns only structure storage; wavetable bases point directly
 * into the caller-owned .tbl bytes, which must outlive the graph. */
GeOriginalMusicBank *ge_original_music_bank_open(
        const uint8_t *ctl, size_t ctl_size,
        const uint8_t *tbl, size_t tbl_size,
        uint32_t bank_index);
void ge_original_music_bank_close(GeOriginalMusicBank *bank);
ALBank *ge_original_music_bank_native(GeOriginalMusicBank *bank);
void ge_original_music_bank_stats(const GeOriginalMusicBank *bank,
        GeOriginalMusicBankStats *stats);

#endif

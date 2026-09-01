#include "ge_original_music_bank.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *file = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)length, file) == (size_t)length);
    assert(fclose(file) == 0);
    *size_out = (size_t)length;
    return bytes;
}

int main(int argc, char **argv)
{
    size_t ctl_size, tbl_size;
    uint8_t *ctl, *tbl;
    GeOriginalMusicBank *owner;
    GeOriginalMusicBankStats stats;
    ALBank *bank;
    ALInstrument *instrument;
    ALSound *sound;

    assert(argc == 3);
    ctl = read_file(argv[1], &ctl_size);
    tbl = read_file(argv[2], &tbl_size);
    owner = ge_original_music_bank_open(ctl, ctl_size, tbl, tbl_size, 0U);
    assert(owner != NULL);
    bank = ge_original_music_bank_native(owner);
    assert(bank != NULL);
    assert(bank->instCount == 75);
    assert(bank->sampleRate == 22050);
    assert(bank->percussion == NULL);
    instrument = bank->instArray[0];
    assert(instrument != NULL);
    assert(instrument->volume == 0x7fU);
    assert(instrument->pan == 0x40U);
    assert(instrument->priority == 5U);
    assert(instrument->bendRange == 200);
    assert(instrument->soundCount == 1);
    sound = instrument->soundArray[0];
    assert(sound != NULL);
    assert(sound->samplePan == 0x40U);
    assert(sound->sampleVolume == 0x64U);
    assert(sound->wavetable != NULL);
    assert(sound->wavetable->type == AL_ADPCM_WAVE);
    assert(sound->wavetable->base == tbl + 0x508U);
    assert(sound->wavetable->len == 0x129a);
    assert(sound->wavetable->waveInfo.adpcmWave.book != NULL);
    assert(sound->wavetable->waveInfo.adpcmWave.book->order == 2);
    assert(sound->wavetable->waveInfo.adpcmWave.book->npredictors == 1);

    ge_original_music_bank_stats(owner, &stats);
    assert(stats.banks == 1U);
    assert(stats.instruments > 60U);
    assert(stats.sounds > stats.instruments);
    assert(stats.wavetables > 0U);
    assert(stats.wavetables <= stats.sounds);
    assert(stats.adpcm_books > 0U);
    printf("original instrument bank relocated: %u instruments, %u sounds, %u waves, %u books\n",
            stats.instruments, stats.sounds, stats.wavetables,
            stats.adpcm_books);

    ge_original_music_bank_close(owner);
    free(tbl);
    free(ctl);
    return 0;
}

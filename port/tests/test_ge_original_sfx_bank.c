#include "ge_original_sfx_bank.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void store_be16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8U);
    bytes[1] = (uint8_t)value;
}

static void store_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24U);
    bytes[1] = (uint8_t)(value >> 16U);
    bytes[2] = (uint8_t)(value >> 8U);
    bytes[3] = (uint8_t)value;
}

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    uint8_t *data;
    long length;
    if (stream == NULL || fseek(stream, 0L, SEEK_END) != 0
            || (length = ftell(stream)) <= 0L
            || fseek(stream, 0L, SEEK_SET) != 0) {
        if (stream != NULL) fclose(stream);
        return NULL;
    }
    data = malloc((size_t)length);
    if (data == NULL || fread(data, 1U, (size_t)length, stream)
            != (size_t)length) {
        free(data);
        fclose(stream);
        return NULL;
    }
    fclose(stream);
    *size = (size_t)length;
    return data;
}

static void test_original_assets(const char *control_path,
                                 const char *samples_path)
{
    size_t control_size = 0U;
    size_t samples_size = 0U;
    uint8_t *control = read_file(control_path, &control_size);
    uint8_t *samples = read_file(samples_path, &samples_size);
    GeOriginalSfxBank bank;
    GeOriginalSfxInfo info;
    int16_t *pcm;
    size_t frames = 0U;
    size_t nonzero = 0U;
    size_t index;
    assert(control != NULL && samples != NULL);
    assert(ge_original_sfx_bank_init(
        &bank, control, control_size, samples, samples_size)
        == GE_ORIGINAL_SFX_BANK_OK);
    assert(bank.sample_rate == 22050U && bank.sound_count == 261U);
    assert(ge_original_sfx_bank_decode(
        &bank, 46, NULL, 0U, &frames, &info)
        == GE_ORIGINAL_SFX_BANK_OUTPUT_TOO_SMALL);
    assert(frames == 3984U);
    assert(info.pan == 64U && info.volume == 100U && !info.has_loop);
    assert(fabsf(info.pitch_ratio - powf(2.0f, -550.0f / 1200.0f))
        < 0.0001f);
    pcm = malloc(frames * sizeof(*pcm));
    assert(pcm != NULL);
    assert(ge_original_sfx_bank_decode(
        &bank, 46, pcm, frames, &frames, &info)
        == GE_ORIGINAL_SFX_BANK_OK);
    for (index = 0U; index < frames; index++)
        if (pcm[index] != 0) nonzero++;
    assert(nonzero > frames / 2U);
    free(pcm);
    free(control);
    free(samples);
}

int main(int argc, char **argv)
{
    enum {
        BANK = 8,
        INSTRUMENT = 24,
        SOUND = 44,
        KEYMAP = 60,
        WAVE = 66,
        BOOK = 86
    };
    uint8_t control[126] = {0};
    uint8_t samples[9] = {0, 0x11, 0x11, 0x11, 0x11,
                          0x11, 0x11, 0x11, 0x11};
    GeOriginalSfxBank bank;
    GeOriginalSfxInfo info;
    int16_t pcm[16];
    size_t frames = 0U;
    size_t index;

    store_be16(control, 0x4231);
    store_be16(control + 2U, 1U);
    store_be32(control + 4U, BANK);
    store_be16(control + BANK, 1U);
    store_be32(control + BANK + 4U, 22050U);
    store_be32(control + BANK + 12U, INSTRUMENT);
    control[INSTRUMENT] = 127U;
    control[INSTRUMENT + 1U] = 64U;
    store_be16(control + INSTRUMENT + 14U, 1U);
    store_be32(control + INSTRUMENT + 16U, SOUND);
    store_be32(control + SOUND + 4U, KEYMAP);
    store_be32(control + SOUND + 8U, WAVE);
    control[SOUND + 12U] = 64U;
    control[SOUND + 13U] = 90U;
    control[KEYMAP + 4U] = 60U;
    store_be32(control + WAVE, 0U);
    store_be32(control + WAVE + 4U, sizeof(samples));
    control[WAVE + 8U] = 0U;
    store_be32(control + WAVE + 16U, BOOK);
    store_be32(control + BOOK, 2U);
    store_be32(control + BOOK + 4U, 1U);

    assert(ge_original_sfx_bank_init(
        &bank, control, sizeof(control), samples, sizeof(samples))
        == GE_ORIGINAL_SFX_BANK_OK);
    assert(bank.sample_rate == 22050U);
    assert(bank.sound_count == 1U);
    assert(ge_original_sfx_bank_decode(
        &bank, 0, NULL, 0U, &frames, &info)
        == GE_ORIGINAL_SFX_BANK_OUTPUT_TOO_SMALL);
    assert(frames == 16U);
    assert(info.source_rate == 22050U);
    assert(info.source_frames == 16U);
    assert(fabsf(info.pitch_ratio - 1.0f) < 0.0001f);
    assert(info.pan == 64U && info.volume == 90U && !info.has_loop);
    assert(ge_original_sfx_bank_decode(
        &bank, 0, pcm, 16U, &frames, &info)
        == GE_ORIGINAL_SFX_BANK_OK);
    for (index = 0U; index < frames; index++) assert(pcm[index] == 1);
    assert(ge_original_sfx_bank_decode(
        &bank, 1, pcm, 16U, &frames, &info)
        == GE_ORIGINAL_SFX_BANK_SOUND_UNAVAILABLE);
    control[WAVE + 8U] = 1U;
    assert(ge_original_sfx_bank_decode(
        &bank, 0, pcm, 16U, &frames, &info)
        == GE_ORIGINAL_SFX_BANK_UNSUPPORTED_WAVE);
    if (argc == 3) test_original_assets(argv[1], argv[2]);
    printf("original GoldenEye SFX bank parser/ADPCM decoder passed\n");
    return 0;
}

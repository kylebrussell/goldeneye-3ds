#ifndef GE_ORIGINAL_SFX_BANK_H
#define GE_ORIGINAL_SFX_BANK_H

#include <stddef.h>
#include <stdint.h>

typedef enum GeOriginalSfxBankStatus {
    GE_ORIGINAL_SFX_BANK_OK = 0,
    GE_ORIGINAL_SFX_BANK_INVALID_ARGUMENT,
    GE_ORIGINAL_SFX_BANK_INVALID_FORMAT,
    GE_ORIGINAL_SFX_BANK_SOUND_UNAVAILABLE,
    GE_ORIGINAL_SFX_BANK_UNSUPPORTED_WAVE,
    GE_ORIGINAL_SFX_BANK_OUTPUT_TOO_SMALL
} GeOriginalSfxBankStatus;

typedef struct GeOriginalSfxBank {
    const uint8_t *control;
    size_t control_size;
    const uint8_t *samples;
    size_t samples_size;
    uint32_t bank_offset;
    uint32_t instrument_offset;
    uint32_t sample_rate;
    uint16_t sound_count;
} GeOriginalSfxBank;

typedef struct GeOriginalSfxInfo {
    uint32_t source_rate;
    uint32_t source_frames;
    uint32_t loop_start;
    uint32_t loop_end;
    uint32_t loop_count;
    float pitch_ratio;
    uint8_t pan;
    uint8_t volume;
    uint8_t has_loop;
} GeOriginalSfxInfo;

GeOriginalSfxBankStatus ge_original_sfx_bank_init(
    GeOriginalSfxBank *bank,
    const void *control, size_t control_size,
    const void *samples, size_t samples_size);

/* Decodes the original mono N64 ADPCM sample without resampling.  The caller
 * applies info.pitch_ratio and spatial pan while mixing into the platform
 * output rate.  On OUTPUT_TOO_SMALL, frames_written reports the required
 * source-frame capacity. */
GeOriginalSfxBankStatus ge_original_sfx_bank_decode(
    const GeOriginalSfxBank *bank, int32_t sound_id,
    int16_t *pcm, size_t pcm_capacity_frames,
    size_t *frames_written, GeOriginalSfxInfo *info);

const char *ge_original_sfx_bank_status_name(
    GeOriginalSfxBankStatus status);

#endif

#include "ge_original_sfx_bank.h"

#include <math.h>
#include <string.h>
#if defined(__ARM_FEATURE_SAT)
#include <arm_acle.h>
#endif

enum {
    GE_BANK_VERSION = 0x4231,
    GE_ADPCM_WAVE = 0,
    GE_ADPCM_FRAME_BYTES = 9,
    GE_ADPCM_FRAME_SAMPLES = 16,
    GE_ADPCM_PREDICTOR_SAMPLES = 16
};

static int range_ok(size_t size, uint32_t offset, size_t length)
{
    return offset <= size && length <= size - offset;
}

static uint16_t load_be16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] << 8U | bytes[1]);
}

static int16_t load_be_s16(const uint8_t *bytes)
{
    return (int16_t)load_be16(bytes);
}

static uint32_t load_be32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 24U
        | (uint32_t)bytes[1] << 16U
        | (uint32_t)bytes[2] << 8U
        | bytes[3];
}

static int16_t saturate_s64(int64_t value)
{
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
}

static int64_t floor_divide_2048(int64_t value)
{
    /* Fixed-point ADPCM requires floor division. All supported native/host
     * targets use arithmetic right shift, including both ARM11 models. */
    _Static_assert((-INT64_C(1) >> 1) == -INT64_C(1),
        "ADPCM requires arithmetic signed right shift");
    return value >> 11U;
}

static int16_t residual(uint8_t nibble, uint8_t scale)
{
    const int16_t signed_nibble = (nibble & 8U) != 0U
        ? (int16_t)nibble - 16 : (int16_t)nibble;
    const unsigned effective_scale = scale < 12U ? scale : 12U;
    return (int16_t)(signed_nibble * (int16_t)(1U << effective_scale));
}

static void decode_group(int16_t *destination, const int16_t *residuals,
                         const int16_t *book,
                         int16_t last_first, int16_t last_second)
{
    size_t sample;
    for (sample = 0U; sample < 8U; sample++) {
        int64_t accumulator = (int64_t)residuals[sample] * 2048;
        size_t previous;
        accumulator += (int32_t)book[sample] * last_first;
        accumulator += (int32_t)book[8U + sample] * last_second;
        for (previous = 0U; previous < sample; previous++)
            accumulator += (int32_t)book[8U + previous]
                * residuals[sample - 1U - previous];
        destination[sample] = saturate_s64(
            floor_divide_2048(accumulator));
    }
}

static int16_t saturate_s32(int32_t value)
{
#if defined(__ARM_FEATURE_SAT)
    return (int16_t)__ssat(value, 16);
#else
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
#endif
}

static int narrow_predictor(const int16_t *codebook)
{
    uint32_t feedback_sum = 0U;
    for (size_t sample = 0U; sample < 8U; ++sample) {
        const int32_t first = codebook[sample];
        const int32_t second = codebook[8U + sample];
        const uint32_t first_abs = (uint32_t)(first < 0 ? -first : first);
        const uint32_t second_abs = (uint32_t)(second < 0 ? -second : second);
        /* Every history/residual sample has magnitude <=32768. Include
         * the residual's 2048 scale and every product in each partial sum:
         * 32768 * (2048 + 63487) < INT32_MAX. Unusual/extreme codebooks
         * retain the original 64-bit accumulator. */
        if (first_abs + second_abs + feedback_sum > 63487U) return 0;
        feedback_sum += second_abs;
    }
    return 1;
}

static void decode_group_narrow(
        int16_t *destination, const int16_t *residuals,
        const int16_t *codebook, int16_t last_first, int16_t last_second)
{
    for (size_t sample = 0U; sample < 8U; ++sample) {
        int32_t accumulator = (int32_t)residuals[sample] * 2048;
        accumulator += (int32_t)codebook[sample] * last_first;
        accumulator += (int32_t)codebook[8U + sample] * last_second;
        for (size_t previous = 0U; previous < sample; ++previous)
            accumulator += (int32_t)codebook[8U + previous]
                * residuals[sample - 1U - previous];
        destination[sample] = saturate_s32(accumulator >> 11U);
    }
}

static int memory_overlaps(const void *left, size_t left_size,
                           const void *right, size_t right_size)
{
    const uintptr_t a = (uintptr_t)left, b = (uintptr_t)right;
    if (left_size == 0U || right_size == 0U) return 0;
    return a >= b ? a - b < right_size : b - a < left_size;
}

GeOriginalSfxBankStatus ge_original_sfx_bank_init(
    GeOriginalSfxBank *bank,
    const void *control, size_t control_size,
    const void *samples, size_t samples_size)
{
    const uint8_t *ctl = control;
    uint32_t bank_offset;
    uint32_t instrument_offset;
    if (bank == NULL || ctl == NULL || samples == NULL)
        return GE_ORIGINAL_SFX_BANK_INVALID_ARGUMENT;
    memset(bank, 0, sizeof(*bank));
    if (!range_ok(control_size, 0U, 8U)
            || load_be16(ctl) != GE_BANK_VERSION
            || load_be16(ctl + 2U) == 0U)
        return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
    bank_offset = load_be32(ctl + 4U);
    if (!range_ok(control_size, bank_offset, 16U)
            || load_be16(ctl + bank_offset) == 0U)
        return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
    instrument_offset = load_be32(ctl + bank_offset + 12U);
    if (!range_ok(control_size, instrument_offset, 16U)
            || load_be16(ctl + instrument_offset + 14U) == 0U)
        return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
    bank->control = ctl;
    bank->control_size = control_size;
    bank->samples = samples;
    bank->samples_size = samples_size;
    bank->bank_offset = bank_offset;
    bank->instrument_offset = instrument_offset;
    bank->sample_rate = load_be32(ctl + bank_offset + 4U);
    bank->sound_count = load_be16(ctl + instrument_offset + 14U);
    if (bank->sample_rate == 0U
            || !range_ok(control_size, instrument_offset + 16U,
                         (size_t)bank->sound_count * 4U)) {
        memset(bank, 0, sizeof(*bank));
        return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
    }
    return GE_ORIGINAL_SFX_BANK_OK;
}

GeOriginalSfxBankStatus ge_original_sfx_bank_decode(
    const GeOriginalSfxBank *bank, int32_t sound_id,
    int16_t *pcm, size_t pcm_capacity_frames,
    size_t *frames_written, GeOriginalSfxInfo *info)
{
    const uint8_t *ctl;
    uint32_t sound_offset;
    uint32_t keymap_offset;
    uint32_t wave_offset;
    uint32_t sample_offset;
    uint32_t sample_bytes;
    uint32_t book_offset;
    uint32_t loop_offset;
    uint32_t order;
    uint32_t predictors;
    size_t frame_count;
    size_t output_frames;
    int16_t history[GE_ADPCM_FRAME_SAMPLES] = {0};
    size_t frame;
    if (frames_written != NULL) *frames_written = 0U;
    if (info != NULL) memset(info, 0, sizeof(*info));
    if (bank == NULL || bank->control == NULL || bank->samples == NULL
            || frames_written == NULL)
        return GE_ORIGINAL_SFX_BANK_INVALID_ARGUMENT;
    if (sound_id < 0 || sound_id >= bank->sound_count)
        return GE_ORIGINAL_SFX_BANK_SOUND_UNAVAILABLE;
    ctl = bank->control;
    sound_offset = load_be32(ctl + bank->instrument_offset + 16U
                             + (size_t)sound_id * 4U);
    if (!range_ok(bank->control_size, sound_offset, 16U))
        return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
    keymap_offset = load_be32(ctl + sound_offset + 4U);
    wave_offset = load_be32(ctl + sound_offset + 8U);
    if (!range_ok(bank->control_size, keymap_offset, 6U)
            || !range_ok(bank->control_size, wave_offset, 20U))
        return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
    if (ctl[wave_offset + 8U] != GE_ADPCM_WAVE)
        return GE_ORIGINAL_SFX_BANK_UNSUPPORTED_WAVE;
    sample_offset = load_be32(ctl + wave_offset);
    sample_bytes = load_be32(ctl + wave_offset + 4U);
    book_offset = load_be32(ctl + wave_offset + 16U);
    loop_offset = load_be32(ctl + wave_offset + 12U);
    if (!range_ok(bank->samples_size, sample_offset, sample_bytes)
            || !range_ok(bank->control_size, book_offset, 8U))
        return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
    order = load_be32(ctl + book_offset);
    predictors = load_be32(ctl + book_offset + 4U);
    if (order != 2U || predictors == 0U
            || predictors > UINT32_MAX / GE_ADPCM_PREDICTOR_SAMPLES
            || !range_ok(bank->control_size, book_offset + 8U,
                         (size_t)predictors
                             * GE_ADPCM_PREDICTOR_SAMPLES * 2U))
        return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
    frame_count = sample_bytes / GE_ADPCM_FRAME_BYTES;
    output_frames = frame_count * GE_ADPCM_FRAME_SAMPLES;
    *frames_written = output_frames;
    if (info != NULL) {
        const int8_t detune = (int8_t)ctl[keymap_offset + 5U];
        int cents = (int)ctl[keymap_offset + 4U] * 100 - 0x1770;
        if ((ctl[keymap_offset + 3U] & 0xf0U) != 0x20U)
            cents += detune;
        info->source_rate = bank->sample_rate;
        info->source_frames = output_frames > UINT32_MAX
            ? UINT32_MAX : (uint32_t)output_frames;
        info->pitch_ratio = powf(2.0f, (float)cents / 1200.0f);
        info->pan = ctl[sound_offset + 12U];
        info->volume = ctl[sound_offset + 13U];
        if (loop_offset != 0U) {
            if (!range_ok(bank->control_size, loop_offset, 12U))
                return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
            info->loop_start = load_be32(ctl + loop_offset);
            info->loop_end = load_be32(ctl + loop_offset + 4U);
            info->loop_count = load_be32(ctl + loop_offset + 8U);
            info->has_loop = 1U;
        }
    }
    if (pcm == NULL || pcm_capacity_frames < output_frames)
        return GE_ORIGINAL_SFX_BANK_OUTPUT_TOO_SMALL;
    /* Only the current predictor is retained: constant stack space and no
     * allocation. A caller may decode over its control storage, so disable
     * reuse in that case and observe every original per-block reload. */
    int16_t book[GE_ADPCM_PREDICTOR_SAMPLES];
    uint32_t last_predictor = UINT32_MAX;
    int narrow = 0;
    const int book_reusable = output_frames <= SIZE_MAX / sizeof(*pcm)
        && !memory_overlaps(pcm,
        output_frames * sizeof(*pcm), ctl, bank->control_size);
    for (frame = 0U; frame < frame_count; frame++) {
        const uint8_t *encoded = bank->samples + sample_offset
            + frame * GE_ADPCM_FRAME_BYTES;
        const uint8_t scale = encoded[0] >> 4U;
        const uint32_t predictor = encoded[0] & 0x0fU;
        int16_t residuals[GE_ADPCM_FRAME_SAMPLES];
        size_t sample;
        if (predictor >= predictors)
            return GE_ORIGINAL_SFX_BANK_INVALID_FORMAT;
        if (!book_reusable || predictor != last_predictor) {
            for (sample = 0U; sample < GE_ADPCM_PREDICTOR_SAMPLES; sample++)
                book[sample] = load_be_s16(ctl + book_offset + 8U
                    + (predictor * GE_ADPCM_PREDICTOR_SAMPLES + sample) * 2U);
            narrow = narrow_predictor(book);
            last_predictor = predictor;
        }
        for (sample = 0U; sample < GE_ADPCM_FRAME_SAMPLES; sample++) {
            const uint8_t packed = encoded[1U + sample / 2U];
            residuals[sample] = residual(
                (sample & 1U) == 0U ? packed >> 4U : packed & 0x0fU,
                scale);
        }
        if (narrow) {
            decode_group_narrow(history, residuals, book, history[14], history[15]);
            decode_group_narrow(history + 8U, residuals + 8U, book, history[6], history[7]);
        } else {
            decode_group(history, residuals, book, history[14], history[15]);
            decode_group(history + 8U, residuals + 8U, book, history[6], history[7]);
        }
        memcpy(pcm + frame * GE_ADPCM_FRAME_SAMPLES,
               history, sizeof(history));
    }
    return GE_ORIGINAL_SFX_BANK_OK;
}

const char *ge_original_sfx_bank_status_name(GeOriginalSfxBankStatus status)
{
    switch (status) {
    case GE_ORIGINAL_SFX_BANK_OK: return "ok";
    case GE_ORIGINAL_SFX_BANK_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_SFX_BANK_INVALID_FORMAT: return "invalid format";
    case GE_ORIGINAL_SFX_BANK_SOUND_UNAVAILABLE: return "sound unavailable";
    case GE_ORIGINAL_SFX_BANK_UNSUPPORTED_WAVE: return "unsupported wave";
    case GE_ORIGINAL_SFX_BANK_OUTPUT_TOO_SMALL: return "output too small";
    default: return "unknown";
    }
}

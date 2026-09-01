#include "ge_audio_abi.h"

#include <limits.h>
#include <string.h>

enum {
    GE_ABI_SPNOOP = 0,
    GE_ABI_ADPCM = 1,
    GE_ABI_CLEARBUFF = 2,
    GE_ABI_ENVMIXER = 3,
    GE_ABI_LOADBUFF = 4,
    GE_ABI_RESAMPLE = 5,
    GE_ABI_SAVEBUFF = 6,
    GE_ABI_SEGMENT = 7,
    GE_ABI_SETBUFF = 8,
    GE_ABI_SETVOL = 9,
    GE_ABI_DMEMMOVE = 10,
    GE_ABI_LOADADPCM = 11,
    GE_ABI_MIXER = 12,
    GE_ABI_INTERLEAVE = 13,
    GE_ABI_POLEF = 14,
    GE_ABI_SETLOOP = 15,
    GE_ABI_FLAG_INIT = 1,
    GE_ABI_FLAG_LOOP = 2,
    GE_ABI_ADPCM_FRAME_BYTES = 9,
    GE_ABI_ADPCM_FRAME_SAMPLES = 16,
    GE_ABI_ADPCM_FRAME_OUTPUT_BYTES = 32,
    GE_ABI_ADPCM_STATE_BYTES = 32,
    GE_ABI_ADPCM_PREDICTOR_SAMPLES = 16,
    GE_ABI_RESAMPLE_HISTORY_SAMPLES = 4,
    GE_ABI_RESAMPLE_STATE_BYTES = 10,
    GE_ABI_ENVMIX_STATE_BYTES = 80,
    GE_ABI_POLEF_STATE_BYTES = 8,
    GE_ABI_FLAG_LEFT = 2,
    GE_ABI_FLAG_VOLUME = 4,
    GE_ABI_FLAG_AUX = 8
};

/* 64 four-tap rows from the reconstructed NTSC-U ABI1 microcode. */
static const int16_t ge_audio_abi_resample_lut[64U * 4U] = {
    3129, 26285, 3398, -33, 2873, 26262, 3679, -40,
    2628, 26217, 3971, -48, 2394, 26150, 4276, -56,
    2173, 26061, 4592, -65, 1963, 25950, 4920, -74,
    1764, 25817, 5260, -84, 1576, 25663, 5611, -95,
    1399, 25487, 5974, -106, 1233, 25291, 6347, -118,
    1077, 25075, 6732, -130, 932, 24838, 7127, -143,
    796, 24583, 7532, -156, 671, 24309, 7947, -170,
    554, 24016, 8371, -184, 446, 23706, 8804, -198,
    347, 23379, 9246, -212, 257, 23036, 9696, -226,
    174, 22678, 10153, -240, 99, 22304, 10618, -254,
    31, 21917, 11088, -268, -30, 21517, 11564, -280,
    -84, 21104, 12045, -293, -132, 20679, 12531, -304,
    -173, 20244, 13020, -314, -210, 19799, 13512, -323,
    -241, 19345, 14006, -330, -267, 18882, 14501, -336,
    -289, 18413, 14997, -340, -306, 17937, 15493, -341,
    -320, 17456, 15988, -340, -330, 16970, 16480, -337,
    -337, 16480, 16970, -330, -340, 15988, 17456, -320,
    -341, 15493, 17937, -306, -340, 14997, 18413, -289,
    -336, 14501, 18882, -267, -330, 14006, 19345, -241,
    -323, 13512, 19799, -210, -314, 13020, 20244, -173,
    -304, 12531, 20679, -132, -293, 12045, 21104, -84,
    -280, 11564, 21517, -30, -268, 11088, 21917, 31,
    -254, 10618, 22304, 99, -240, 10153, 22678, 174,
    -226, 9696, 23036, 257, -212, 9246, 23379, 347,
    -198, 8804, 23706, 446, -184, 8371, 24016, 554,
    -170, 7947, 24309, 671, -156, 7532, 24583, 796,
    -143, 7127, 24838, 932, -130, 6732, 25075, 1077,
    -118, 6347, 25291, 1233, -106, 5974, 25487, 1399,
    -95, 5611, 25663, 1576, -84, 5260, 25817, 1764,
    -74, 4920, 25950, 1963, -65, 4592, 26061, 2173,
    -56, 4276, 26150, 2394, -48, 3971, 26217, 2628,
    -40, 3679, 26262, 2873, -33, 3398, 26285, 3129
};

static int ge_audio_abi_dmem_range(uint32_t offset, size_t length)
{
    return offset <= GE_AUDIO_ABI_DMEM_BYTES
            && length <= GE_AUDIO_ABI_DMEM_BYTES - offset;
}

static uint32_t ge_audio_abi_address(
        const GeAudioAbiState *state,
        uint32_t address)
{
    uint32_t segment = address >> 24U;

    if (state->direct_addresses != 0U) {
        return address;
    }
    if (segment != 0U && segment < GE_AUDIO_ABI_SEGMENTS) {
        return state->segments[segment] + (address & 0x00ffffffU);
    }
    return address;
}

static uint16_t ge_audio_abi_load_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] << 8U)
            | (uint16_t)bytes[1];
}

static int16_t ge_audio_abi_load_s16(const uint8_t *bytes)
{
    return (int16_t)ge_audio_abi_load_u16(bytes);
}

static void ge_audio_abi_store_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8U);
    bytes[1] = (uint8_t)value;
}

static uint32_t ge_audio_abi_load_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 24U
            | (uint32_t)bytes[1] << 16U
            | (uint32_t)bytes[2] << 8U
            | (uint32_t)bytes[3];
}

static void ge_audio_abi_store_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24U);
    bytes[1] = (uint8_t)(value >> 16U);
    bytes[2] = (uint8_t)(value >> 8U);
    bytes[3] = (uint8_t)value;
}

static void ge_audio_abi_store_s16(uint8_t *bytes, int16_t value)
{
    ge_audio_abi_store_u16(bytes, (uint16_t)value);
}

static int16_t ge_audio_abi_saturate(int32_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

static int16_t ge_audio_abi_saturate_s64(int64_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

typedef struct GeAudioAbiRamp {
    int64_t value;
    int64_t step;
    int64_t target;
} GeAudioAbiRamp;

static int16_t ge_audio_abi_ramp_step(GeAudioAbiRamp *ramp)
{
    ramp->value += ramp->step;
    if ((ramp->step <= 0 && ramp->value <= ramp->target)
            || (ramp->step > 0 && ramp->value >= ramp->target)) {
        ramp->value = ramp->target;
        ramp->step = 0;
    }
    return (int16_t)(ramp->value >> 16U);
}

static GeAudioAbiResult ge_audio_abi_envmixer(
        GeAudioAbiState *state,
        uint8_t flags,
        uint32_t state_address,
        GeAudioAbiResolve resolve,
        void *resolve_context)
{
    const size_t sample_count = state->count_bytes / sizeof(int16_t);
    const int auxiliary = (flags & GE_ABI_FLAG_AUX) != 0U;
    const uint16_t outputs[4] = {
        state->dmem_output,
        state->dmem_dry_right,
        state->dmem_wet_left,
        state->dmem_wet_right,
    };
    uint32_t resolved_state_address = ge_audio_abi_address(
            state, state_address);
    uint8_t *saved_state;
    GeAudioAbiRamp ramps[2];
    int16_t dry = state->envelope_dry;
    int16_t wet = state->envelope_wet;
    size_t output_count = auxiliary ? 4U : 2U;
    size_t sample;
    size_t output;

    if (!ge_audio_abi_dmem_range(state->dmem_input, state->count_bytes)) {
        return GE_AUDIO_ABI_DMEM_RANGE;
    }
    for (output = 0U; output < output_count; ++output) {
        if (!ge_audio_abi_dmem_range(outputs[output], state->count_bytes)) {
            return GE_AUDIO_ABI_DMEM_RANGE;
        }
    }
    if (resolve == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }
    saved_state = resolve(resolve_context, resolved_state_address,
            GE_ABI_ENVMIX_STATE_BYTES);
    if (saved_state == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }

    if ((flags & GE_ABI_FLAG_INIT) != 0U) {
        for (output = 0U; output < 2U; ++output) {
            ramps[output].value = (int64_t)state->envelope_volume[output]
                    * 65536;
            ramps[output].target = (int64_t)state->envelope_target[output]
                    * 65536;
            ramps[output].step = state->envelope_rate[output] / 8;
        }
    } else {
        wet = ge_audio_abi_load_s16(saved_state + 0U);
        dry = ge_audio_abi_load_s16(saved_state + 4U);
        ramps[0].target = (int32_t)ge_audio_abi_load_u32(saved_state + 8U);
        ramps[1].target = (int32_t)ge_audio_abi_load_u32(saved_state + 12U);
        ramps[0].step = (int32_t)ge_audio_abi_load_u32(saved_state + 16U);
        ramps[1].step = (int32_t)ge_audio_abi_load_u32(saved_state + 20U);
        ramps[0].value = (int32_t)ge_audio_abi_load_u32(saved_state + 32U);
        ramps[1].value = (int32_t)ge_audio_abi_load_u32(saved_state + 36U);
    }

    for (sample = 0U; sample < sample_count; ++sample) {
        const int16_t source = ge_audio_abi_load_s16(state->dmem
                + state->dmem_input + sample * sizeof(int16_t));
        const int16_t left_volume = ge_audio_abi_ramp_step(&ramps[0]);
        const int16_t right_volume = ge_audio_abi_ramp_step(&ramps[1]);
        const int16_t gains[4] = {
            ge_audio_abi_saturate(((int32_t)left_volume * dry + 0x4000)
                    >> 15U),
            ge_audio_abi_saturate(((int32_t)right_volume * dry + 0x4000)
                    >> 15U),
            ge_audio_abi_saturate(((int32_t)left_volume * wet + 0x4000)
                    >> 15U),
            ge_audio_abi_saturate(((int32_t)right_volume * wet + 0x4000)
                    >> 15U),
        };

        for (output = 0U; output < output_count; ++output) {
            uint8_t *destination = state->dmem + outputs[output]
                    + sample * sizeof(int16_t);
            const int32_t current = ge_audio_abi_load_s16(destination);
            const int32_t mixed = current
                    + (((int32_t)source * gains[output]) >> 15U);

            ge_audio_abi_store_s16(destination,
                    ge_audio_abi_saturate(mixed));
        }
    }

    ge_audio_abi_store_s16(saved_state + 0U, wet);
    ge_audio_abi_store_s16(saved_state + 4U, dry);
    ge_audio_abi_store_u32(saved_state + 8U, (uint32_t)ramps[0].target);
    ge_audio_abi_store_u32(saved_state + 12U, (uint32_t)ramps[1].target);
    ge_audio_abi_store_u32(saved_state + 16U, (uint32_t)ramps[0].step);
    ge_audio_abi_store_u32(saved_state + 20U, (uint32_t)ramps[1].step);
    ge_audio_abi_store_u32(saved_state + 32U, (uint32_t)ramps[0].value);
    ge_audio_abi_store_u32(saved_state + 36U, (uint32_t)ramps[1].value);
    return GE_AUDIO_ABI_OK;
}

static GeAudioAbiResult ge_audio_abi_polef(
        GeAudioAbiState *state,
        uint8_t flags,
        uint16_t gain,
        uint32_t state_address,
        GeAudioAbiResolve resolve,
        void *resolve_context)
{
    const size_t filtered_bytes = ((size_t)state->count_bytes + 15U)
            & ~(size_t)15U;
    uint32_t resolved_state_address;
    uint8_t *saved_state;
    int16_t scaled_feedback[8];
    int16_t previous_first = 0;
    int16_t previous_second = 0;
    size_t frame_offset;
    size_t coefficient;

    if (state->count_bytes == 0U) {
        return GE_AUDIO_ABI_OK;
    }
    if (state->adpcm_codebook_bytes < 32U) {
        return GE_AUDIO_ABI_CODEBOOK_RANGE;
    }
    if (!ge_audio_abi_dmem_range(state->dmem_input, filtered_bytes)
            || !ge_audio_abi_dmem_range(state->dmem_output,
                    filtered_bytes)) {
        return GE_AUDIO_ABI_DMEM_RANGE;
    }
    if (resolve == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }
    resolved_state_address = ge_audio_abi_address(state, state_address);
    saved_state = resolve(resolve_context, resolved_state_address,
            GE_ABI_POLEF_STATE_BYTES);
    if (saved_state == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }
    if ((flags & GE_ABI_FLAG_INIT) == 0U) {
        previous_first = ge_audio_abi_load_s16(saved_state + 4U);
        previous_second = ge_audio_abi_load_s16(saved_state + 6U);
    }
    for (coefficient = 0U; coefficient < 8U; ++coefficient) {
        scaled_feedback[coefficient] = (int16_t)(
                ((int32_t)state->adpcm_codebook[8U + coefficient] * gain)
                >> 14U);
    }

    for (frame_offset = 0U; frame_offset < filtered_bytes;
            frame_offset += 16U) {
        int16_t frame[8];
        size_t sample;

        for (sample = 0U; sample < 8U; ++sample) {
            frame[sample] = ge_audio_abi_load_s16(state->dmem
                    + state->dmem_input + frame_offset
                    + sample * sizeof(int16_t));
        }
        for (sample = 0U; sample < 8U; ++sample) {
            int64_t accumulator = (int32_t)frame[sample] * gain
                    + (int32_t)state->adpcm_codebook[sample]
                        * previous_first
                    + (int32_t)state->adpcm_codebook[8U + sample]
                        * previous_second;
            size_t previous;

            for (previous = 0U; previous < sample; ++previous) {
                accumulator += (int32_t)scaled_feedback[previous]
                        * frame[sample - 1U - previous];
            }
            ge_audio_abi_store_s16(state->dmem + state->dmem_output
                    + frame_offset + sample * sizeof(int16_t),
                    ge_audio_abi_saturate_s64(accumulator >> 14U));
        }
        previous_first = ge_audio_abi_load_s16(state->dmem
                + state->dmem_output + frame_offset + 12U);
        previous_second = ge_audio_abi_load_s16(state->dmem
                + state->dmem_output + frame_offset + 14U);
    }

    memcpy(saved_state, state->dmem + state->dmem_output
            + filtered_bytes - GE_ABI_POLEF_STATE_BYTES,
            GE_ABI_POLEF_STATE_BYTES);
    return GE_AUDIO_ABI_OK;
}

static int64_t ge_audio_abi_floor_divide_2048(int64_t value)
{
    if (value >= 0) {
        return value / 2048;
    }
    return -((-value + 2047) / 2048);
}

static int64_t ge_audio_abi_floor_divide_32768(int64_t value)
{
    if (value >= 0) {
        return value / 32768;
    }
    return -((-value + 32767) / 32768);
}

static int ge_audio_abi_resample_input_range(
        const GeAudioAbiState *state,
        int64_t relative_sample)
{
    uint64_t byte_offset;

    if (relative_sample < -GE_ABI_RESAMPLE_HISTORY_SAMPLES) {
        return 0;
    }
    if (relative_sample < 0) {
        return 1;
    }
    byte_offset = (uint64_t)state->dmem_input
            + (uint64_t)relative_sample * sizeof(int16_t);
    return byte_offset <= UINT32_MAX
            && ge_audio_abi_dmem_range((uint32_t)byte_offset,
                    sizeof(int16_t));
}

static int16_t ge_audio_abi_resample_input(
        const GeAudioAbiState *state,
        const int16_t *history,
        int64_t relative_sample)
{
    if (relative_sample < 0) {
        return history[relative_sample + GE_ABI_RESAMPLE_HISTORY_SAMPLES];
    }
    return ge_audio_abi_load_s16(state->dmem + state->dmem_input
            + (size_t)relative_sample * sizeof(int16_t));
}

static GeAudioAbiResult ge_audio_abi_resample(
        GeAudioAbiState *state,
        uint8_t flags,
        uint16_t pitch,
        uint32_t state_address,
        GeAudioAbiResolve resolve,
        void *resolve_context)
{
    size_t output_bytes = ((size_t)state->count_bytes + 15U)
            & ~(size_t)15U;
    size_t output_samples = output_bytes / sizeof(int16_t);
    uint32_t resolved_state_address = ge_audio_abi_address(
            state, state_address);
    uint8_t *saved_state;
    int16_t history[GE_ABI_RESAMPLE_HISTORY_SAMPLES];
    uint32_t initial_phase = 0U;
    uint32_t pitch_step = (uint32_t)pitch << 1U;
    uint32_t phase;
    int64_t input_position;
    size_t output_sample;

    if (!ge_audio_abi_dmem_range(state->dmem_output, output_bytes)) {
        return GE_AUDIO_ABI_DMEM_RANGE;
    }
    if (resolve == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }
    saved_state = resolve(resolve_context, resolved_state_address,
            GE_ABI_RESAMPLE_STATE_BYTES);
    if (saved_state == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }

    if ((flags & GE_ABI_FLAG_INIT) != 0U) {
        memset(history, 0, sizeof(history));
    } else {
        size_t sample;

        for (sample = 0U; sample < GE_ABI_RESAMPLE_HISTORY_SAMPLES;
                ++sample) {
            history[sample] = ge_audio_abi_load_s16(
                    saved_state + sample * sizeof(int16_t));
        }
        initial_phase = ge_audio_abi_load_u16(saved_state + 8U);
    }

    /* Preflight all filter taps and the four samples persisted afterward. */
    phase = initial_phase;
    input_position = -GE_ABI_RESAMPLE_HISTORY_SAMPLES;
    for (output_sample = 0U; output_sample < output_samples;
            ++output_sample) {
        size_t tap;
        uint32_t advanced_phase;

        for (tap = 0U; tap < 4U; ++tap) {
            if (!ge_audio_abi_resample_input_range(
                    state, input_position + (int64_t)tap)) {
                return GE_AUDIO_ABI_DMEM_RANGE;
            }
        }
        advanced_phase = phase + pitch_step;
        input_position += (int64_t)(advanced_phase >> 16U);
        phase = advanced_phase & 0xffffU;
    }
    for (output_sample = 0U;
            output_sample < GE_ABI_RESAMPLE_HISTORY_SAMPLES;
            ++output_sample) {
        if (!ge_audio_abi_resample_input_range(
                state, input_position + (int64_t)output_sample)) {
            return GE_AUDIO_ABI_DMEM_RANGE;
        }
    }

    phase = initial_phase;
    input_position = -GE_ABI_RESAMPLE_HISTORY_SAMPLES;
    for (output_sample = 0U; output_sample < output_samples;
            ++output_sample) {
        size_t coefficient = ((phase & 0xfc00U) >> 8U);
        int64_t accumulator = 0;
        uint32_t advanced_phase;
        size_t tap;

        for (tap = 0U; tap < 4U; ++tap) {
            accumulator += (int32_t)ge_audio_abi_resample_input(
                    state, history, input_position + (int64_t)tap)
                    * ge_audio_abi_resample_lut[coefficient + tap];
        }
        ge_audio_abi_store_s16(
                state->dmem + state->dmem_output
                        + output_sample * sizeof(int16_t),
                ge_audio_abi_saturate_s64(
                        ge_audio_abi_floor_divide_32768(accumulator)));

        advanced_phase = phase + pitch_step;
        input_position += (int64_t)(advanced_phase >> 16U);
        phase = advanced_phase & 0xffffU;
    }

    for (output_sample = 0U;
            output_sample < GE_ABI_RESAMPLE_HISTORY_SAMPLES;
            ++output_sample) {
        ge_audio_abi_store_s16(
                saved_state + output_sample * sizeof(int16_t),
                ge_audio_abi_resample_input(state, history,
                        input_position + (int64_t)output_sample));
    }
    ge_audio_abi_store_u16(saved_state + 8U, (uint16_t)phase);
    return GE_AUDIO_ABI_OK;
}

static int16_t ge_audio_abi_adpcm_residual(uint8_t nibble, uint8_t scale)
{
    int16_t signed_nibble = (nibble & 8U) != 0U
            ? (int16_t)nibble - 16
            : (int16_t)nibble;
    unsigned effective_scale = scale < 12U ? scale : 12U;

    return (int16_t)(signed_nibble * (int16_t)(1U << effective_scale));
}

static void ge_audio_abi_adpcm_group(
        int16_t *destination,
        const int16_t *residuals,
        const int16_t *codebook,
        int16_t last_first,
        int16_t last_second)
{
    size_t sample;

    for (sample = 0U; sample < 8U; ++sample) {
        int64_t accumulator = (int64_t)residuals[sample] * 2048;
        size_t previous;

        accumulator += (int32_t)codebook[sample] * last_first;
        accumulator += (int32_t)codebook[8U + sample] * last_second;
        for (previous = 0U; previous < sample; ++previous) {
            accumulator += (int32_t)codebook[8U + previous]
                    * residuals[sample - 1U - previous];
        }
        destination[sample] = ge_audio_abi_saturate_s64(
                ge_audio_abi_floor_divide_2048(accumulator));
    }
}

static GeAudioAbiResult ge_audio_abi_adpcm(
        GeAudioAbiState *state,
        uint8_t flags,
        uint32_t state_address,
        GeAudioAbiResolve resolve,
        void *resolve_context)
{
    size_t decoded_bytes = ((size_t)state->count_bytes + 31U) & ~(size_t)31U;
    size_t frame_count = decoded_bytes / GE_ABI_ADPCM_FRAME_OUTPUT_BYTES;
    size_t compressed_bytes = frame_count * GE_ABI_ADPCM_FRAME_BYTES;
    size_t output_bytes = GE_ABI_ADPCM_STATE_BYTES + decoded_bytes;
    uint32_t resolved_state_address = ge_audio_abi_address(
            state, state_address);
    uint8_t *saved_state;
    uint8_t *initial_state = NULL;
    int16_t history[GE_ABI_ADPCM_FRAME_SAMPLES];
    size_t frame;

    if (!ge_audio_abi_dmem_range(state->dmem_input, compressed_bytes)
            || !ge_audio_abi_dmem_range(state->dmem_output, output_bytes)) {
        return GE_AUDIO_ABI_DMEM_RANGE;
    }
    if (resolve == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }
    saved_state = resolve(resolve_context, resolved_state_address,
            GE_ABI_ADPCM_STATE_BYTES);
    if (saved_state == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }
    if ((flags & GE_ABI_FLAG_INIT) == 0U) {
        uint32_t initial_address = (flags & GE_ABI_FLAG_LOOP) != 0U
                ? state->adpcm_loop_address
                : resolved_state_address;

        initial_state = resolve(resolve_context, initial_address,
                GE_ABI_ADPCM_STATE_BYTES);
        if (initial_state == NULL) {
            return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
        }
    }

    /* Validate every predictor before the output or saved state is changed. */
    for (frame = 0U; frame < frame_count; ++frame) {
        uint8_t header = state->dmem[state->dmem_input
                + frame * GE_ABI_ADPCM_FRAME_BYTES];
        size_t predictor_end = ((size_t)(header & 0x0fU) + 1U)
                * GE_ABI_ADPCM_PREDICTOR_SAMPLES * sizeof(int16_t);

        if (predictor_end > state->adpcm_codebook_bytes) {
            return GE_AUDIO_ABI_CODEBOOK_RANGE;
        }
    }

    if ((flags & GE_ABI_FLAG_INIT) != 0U) {
        memset(history, 0, sizeof(history));
    } else {
        for (frame = 0U; frame < GE_ABI_ADPCM_FRAME_SAMPLES; ++frame) {
            history[frame] = ge_audio_abi_load_s16(
                    initial_state + frame * sizeof(int16_t));
        }
    }
    for (frame = 0U; frame < GE_ABI_ADPCM_FRAME_SAMPLES; ++frame) {
        ge_audio_abi_store_s16(
                state->dmem + state->dmem_output
                        + frame * sizeof(int16_t),
                history[frame]);
    }

    for (frame = 0U; frame < frame_count; ++frame) {
        size_t input = state->dmem_input
                + frame * GE_ABI_ADPCM_FRAME_BYTES;
        size_t output = state->dmem_output + GE_ABI_ADPCM_STATE_BYTES
                + frame * GE_ABI_ADPCM_FRAME_OUTPUT_BYTES;
        uint8_t header = state->dmem[input];
        uint8_t scale = header >> 4U;
        size_t predictor = header & 0x0fU;
        const int16_t *codebook = state->adpcm_codebook
                + predictor * GE_ABI_ADPCM_PREDICTOR_SAMPLES;
        int16_t residuals[GE_ABI_ADPCM_FRAME_SAMPLES];
        size_t sample;

        for (sample = 0U; sample < GE_ABI_ADPCM_FRAME_SAMPLES; ++sample) {
            uint8_t packed = state->dmem[input + 1U + sample / 2U];
            uint8_t nibble = (sample & 1U) == 0U
                    ? packed >> 4U
                    : packed & 0x0fU;

            residuals[sample] = ge_audio_abi_adpcm_residual(nibble, scale);
        }
        ge_audio_abi_adpcm_group(history, residuals, codebook,
                history[14], history[15]);
        ge_audio_abi_adpcm_group(history + 8U, residuals + 8U, codebook,
                history[6], history[7]);

        for (sample = 0U; sample < GE_ABI_ADPCM_FRAME_SAMPLES; ++sample) {
            ge_audio_abi_store_s16(
                    state->dmem + output + sample * sizeof(int16_t),
                    history[sample]);
        }
    }

    for (frame = 0U; frame < GE_ABI_ADPCM_FRAME_SAMPLES; ++frame) {
        ge_audio_abi_store_s16(
                saved_state + frame * sizeof(int16_t), history[frame]);
    }
    return GE_AUDIO_ABI_OK;
}

void ge_audio_abi_init(GeAudioAbiState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

GeAudioAbiResult ge_audio_abi_execute(
        GeAudioAbiState *state,
        const GeAudioAbiCommand *commands,
        size_t command_count,
        GeAudioAbiResolve resolve,
        void *resolve_context)
{
    size_t index;

    if (state == NULL || (command_count != 0U && commands == NULL)) {
        return GE_AUDIO_ABI_INVALID_ARGUMENT;
    }

    for (index = 0U; index < command_count; ++index) {
        uint32_t word0 = commands[index].word0;
        uint32_t word1 = commands[index].word1;
        uint8_t opcode = (uint8_t)(word0 >> 24U);

        switch (opcode) {
        case GE_ABI_SPNOOP:
            break;

        case GE_ABI_ADPCM: {
            GeAudioAbiResult result = ge_audio_abi_adpcm(
                    state,
                    (uint8_t)(word0 >> 16U),
                    word1,
                    resolve,
                    resolve_context);

            if (result != GE_AUDIO_ABI_OK) {
                return result;
            }
            break;
        }

        case GE_ABI_ENVMIXER: {
            GeAudioAbiResult result = ge_audio_abi_envmixer(
                    state,
                    (uint8_t)(word0 >> 16U),
                    word1,
                    resolve,
                    resolve_context);

            if (result != GE_AUDIO_ABI_OK) {
                return result;
            }
            break;
        }

        case GE_ABI_SETBUFF:
            if (((word0 >> 16U) & GE_ABI_FLAG_AUX) != 0U) {
                state->dmem_dry_right = (uint16_t)word0;
                state->dmem_wet_left = (uint16_t)(word1 >> 16U);
                state->dmem_wet_right = (uint16_t)word1;
            } else {
                state->dmem_input = (uint16_t)(word0 & 0xffffU);
                state->dmem_output = (uint16_t)(word1 >> 16U);
                state->count_bytes = (uint16_t)(word1 & 0xffffU);
            }
            break;

        case GE_ABI_SETVOL: {
            const uint8_t flags = (uint8_t)(word0 >> 16U);

            if ((flags & GE_ABI_FLAG_AUX) != 0U) {
                state->envelope_dry = (int16_t)word0;
                state->envelope_wet = (int16_t)word1;
            } else {
                const size_t channel = (flags & GE_ABI_FLAG_LEFT) != 0U
                        ? 0U : 1U;

                if ((flags & GE_ABI_FLAG_VOLUME) != 0U) {
                    state->envelope_volume[channel] = (int16_t)word0;
                } else {
                    state->envelope_target[channel] = (int16_t)word0;
                    state->envelope_rate[channel] = (int32_t)word1;
                }
            }
            break;
        }

        case GE_ABI_CLEARBUFF: {
            uint32_t destination = word0 & 0x00ffffffU;
            size_t count = (size_t)(word1 & 0xffffU);

            if (!ge_audio_abi_dmem_range(destination, count)) {
                return GE_AUDIO_ABI_DMEM_RANGE;
            }
            memset(state->dmem + destination, 0, count);
            break;
        }

        case GE_ABI_LOADBUFF: {
            size_t count = state->count_bytes;
            void *source;

            if (!ge_audio_abi_dmem_range(state->dmem_input, count)) {
                return GE_AUDIO_ABI_DMEM_RANGE;
            }
            if (resolve == NULL) {
                return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
            }
            source = resolve(resolve_context,
                    ge_audio_abi_address(state, word1), count);
            if (source == NULL) {
                return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
            }
            memcpy(state->dmem + state->dmem_input, source, count);
            break;
        }

        case GE_ABI_RESAMPLE: {
            GeAudioAbiResult result = ge_audio_abi_resample(
                    state,
                    (uint8_t)(word0 >> 16U),
                    (uint16_t)word0,
                    word1,
                    resolve,
                    resolve_context);

            if (result != GE_AUDIO_ABI_OK) {
                return result;
            }
            break;
        }

        case GE_ABI_SAVEBUFF: {
            size_t count = state->count_bytes;
            void *destination;

            if (!ge_audio_abi_dmem_range(state->dmem_output, count)) {
                return GE_AUDIO_ABI_DMEM_RANGE;
            }
            if (resolve == NULL) {
                return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
            }
            destination = resolve(resolve_context,
                    ge_audio_abi_address(state, word1), count);
            if (destination == NULL) {
                return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
            }
            memcpy(destination, state->dmem + state->dmem_output, count);
            break;
        }

        case GE_ABI_SEGMENT: {
            uint32_t segment = (word1 >> 24U) & 0x0fU;

            state->segments[segment] = word1 & 0x00ffffffU;
            break;
        }

        case GE_ABI_DMEMMOVE: {
            uint32_t source = word0 & 0x00ffffffU;
            uint32_t destination = word1 >> 16U;
            size_t count = (size_t)(word1 & 0xffffU);

            if (!ge_audio_abi_dmem_range(source, count)
                    || !ge_audio_abi_dmem_range(destination, count)) {
                return GE_AUDIO_ABI_DMEM_RANGE;
            }
            memmove(state->dmem + destination, state->dmem + source, count);
            break;
        }

        case GE_ABI_LOADADPCM: {
            size_t requested_bytes = word0 & 0x00ffffffU;
            size_t loaded_bytes = (requested_bytes + 7U) & ~(size_t)7U;
            uint8_t *source;
            size_t coefficient;

            if ((requested_bytes & 1U) != 0U
                    || loaded_bytes > sizeof(state->adpcm_codebook)) {
                return GE_AUDIO_ABI_CODEBOOK_RANGE;
            }
            if (loaded_bytes == 0U) {
                state->adpcm_codebook_bytes = 0U;
                break;
            }
            if (resolve == NULL) {
                return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
            }
            source = resolve(resolve_context,
                    ge_audio_abi_address(state, word1), loaded_bytes);
            if (source == NULL) {
                return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
            }
            for (coefficient = 0U;
                    coefficient < loaded_bytes / sizeof(int16_t);
                    ++coefficient) {
                state->adpcm_codebook[coefficient] = ge_audio_abi_load_s16(
                        source + coefficient * sizeof(int16_t));
            }
            state->adpcm_codebook_bytes = loaded_bytes;
            break;
        }

        case GE_ABI_MIXER: {
            int16_t gain = (int16_t)(word0 & 0xffffU);
            uint32_t source = word1 >> 16U;
            uint32_t destination = word1 & 0xffffU;
            size_t count = state->count_bytes;
            size_t byte_offset;

            if (!ge_audio_abi_dmem_range(source, count)
                    || !ge_audio_abi_dmem_range(destination, count)) {
                return GE_AUDIO_ABI_DMEM_RANGE;
            }
            for (byte_offset = 0U; byte_offset + 1U < count;
                    byte_offset += 2U) {
                int32_t input = ge_audio_abi_load_s16(
                        state->dmem + source + byte_offset);
                int32_t current = ge_audio_abi_load_s16(
                        state->dmem + destination + byte_offset);
                int32_t mixed = current + ((input * gain) >> 15);

                ge_audio_abi_store_s16(
                        state->dmem + destination + byte_offset,
                        ge_audio_abi_saturate(mixed));
            }
            break;
        }

        case GE_ABI_INTERLEAVE: {
            uint32_t left = word1 >> 16U;
            uint32_t right = word1 & 0xffffU;
            size_t mono_bytes = state->count_bytes;
            size_t stereo_bytes = mono_bytes * 2U;
            size_t byte_offset;

            if (!ge_audio_abi_dmem_range(left, mono_bytes)
                    || !ge_audio_abi_dmem_range(right, mono_bytes)
                    || !ge_audio_abi_dmem_range(
                            state->dmem_output, stereo_bytes)) {
                return GE_AUDIO_ABI_DMEM_RANGE;
            }
            for (byte_offset = 0U; byte_offset + 1U < mono_bytes;
                    byte_offset += 2U) {
                size_t frame_offset = byte_offset * 2U;

                ge_audio_abi_store_s16(
                        state->dmem + state->dmem_output + frame_offset,
                        ge_audio_abi_load_s16(state->dmem + left + byte_offset));
                ge_audio_abi_store_s16(
                        state->dmem + state->dmem_output + frame_offset + 2U,
                        ge_audio_abi_load_s16(state->dmem + right + byte_offset));
            }
            break;
        }

        case GE_ABI_POLEF: {
            GeAudioAbiResult result = ge_audio_abi_polef(
                    state,
                    (uint8_t)(word0 >> 16U),
                    (uint16_t)word0,
                    word1,
                    resolve,
                    resolve_context);

            if (result != GE_AUDIO_ABI_OK) {
                return result;
            }
            break;
        }

        case GE_ABI_SETLOOP:
            state->adpcm_loop_address = ge_audio_abi_address(state, word1);
            break;

        default:
            state->unsupported_opcode = opcode;
            return GE_AUDIO_ABI_UNSUPPORTED_COMMAND;
        }

        state->commands_executed++;
    }
    return GE_AUDIO_ABI_OK;
}

GeAudioAbiResult ge_audio_abi_execute_and_queue(
        GeAudioAbiState *state,
        const GeAudioAbiCommand *commands,
        size_t command_count,
        GeAudioAbiResolve resolve,
        void *resolve_context,
        uint32_t output_address,
        size_t frame_count,
        GeAudioOutput *output)
{
    GeAudioAbiResult result;
    uint8_t *samples;
    int16_t converted[128U * 2U];
    size_t converted_frames = 0U;
    size_t frame_offset = 0U;

    if (output == NULL || resolve == NULL
            || frame_count > SIZE_MAX / (2U * sizeof(int16_t))) {
        return GE_AUDIO_ABI_INVALID_ARGUMENT;
    }
    if (ge_audio_output_free(output) < frame_count) {
        return GE_AUDIO_ABI_OUTPUT_FULL;
    }
    result = ge_audio_abi_execute(state, commands, command_count,
            resolve, resolve_context);
    if (result != GE_AUDIO_ABI_OK) {
        return result;
    }
    samples = resolve(resolve_context, output_address,
            frame_count * 2U * sizeof(int16_t));
    if (samples == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }
    while (frame_offset < frame_count) {
        size_t index;

        converted_frames = frame_count - frame_offset;
        if (converted_frames > 128U) {
            converted_frames = 128U;
        }
        for (index = 0U; index < converted_frames * 2U; ++index) {
            converted[index] = ge_audio_abi_load_s16(
                    samples + (frame_offset * 2U + index) * sizeof(int16_t));
        }
        if (ge_audio_output_write(output, converted, converted_frames)
                != converted_frames) {
            return GE_AUDIO_ABI_OUTPUT_FULL;
        }
        frame_offset += converted_frames;
    }
    return GE_AUDIO_ABI_OK;
}

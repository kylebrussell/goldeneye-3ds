#include "ge_audio_output.h"

#include <limits.h>
#include <string.h>

#define GE_AUDIO_MIN_SAMPLE_RATE 8000U
#define GE_AUDIO_MAX_SAMPLE_RATE 48000U

static uint64_t ge_audio_counter_add(uint64_t counter, size_t amount)
{
    if ((uint64_t)amount > UINT64_MAX - counter) {
        return UINT64_MAX;
    }
    return counter + (uint64_t)amount;
}

static int ge_audio_output_is_valid(const GeAudioOutput *output)
{
    return output != NULL && output->samples != NULL
        && output->capacity_frames != 0U;
}

/* Every transfer is bounded by one ring capacity. This also avoids a
 * division on ARM for each PCM frame and does not require power-of-two
 * capacities. Subtraction first keeps the cursor update overflow-free. */
static size_t ge_audio_ring_advance(size_t cursor, size_t frames, size_t capacity)
{
    const size_t remaining = capacity - cursor;
    return frames < remaining ? cursor + frames : frames - remaining;
}

int ge_audio_output_init(
        GeAudioOutput *output,
        int16_t *stereo_storage,
        size_t capacity_frames,
        uint32_t sample_rate)
{
    if (output == NULL || stereo_storage == NULL || capacity_frames == 0U
            || capacity_frames > SIZE_MAX / 2U
            || sample_rate < GE_AUDIO_MIN_SAMPLE_RATE
            || sample_rate > GE_AUDIO_MAX_SAMPLE_RATE) {
        return -1;
    }

    memset(output, 0, sizeof(*output));
    output->samples = stereo_storage;
    output->capacity_frames = capacity_frames;
    output->sample_rate = sample_rate;
    return 0;
}

void ge_audio_output_reset(GeAudioOutput *output)
{
    if (!ge_audio_output_is_valid(output)) {
        return;
    }

    output->read_frame = 0U;
    output->write_frame = 0U;
    output->queued_frames = 0U;
    output->frames_written = 0U;
    output->frames_read = 0U;
    output->frames_discarded = 0U;
    output->frames_dropped = 0U;
    output->frames_underflowed = 0U;
}

int ge_audio_output_set_sample_rate(GeAudioOutput *output, uint32_t sample_rate)
{
    if (!ge_audio_output_is_valid(output)
            || sample_rate < GE_AUDIO_MIN_SAMPLE_RATE
            || sample_rate > GE_AUDIO_MAX_SAMPLE_RATE) {
        return -1;
    }
    output->sample_rate = sample_rate;
    return 0;
}

size_t ge_audio_output_write(
        GeAudioOutput *output,
        const int16_t *interleaved_stereo,
        size_t frame_count)
{
    size_t accepted;
    size_t contiguous;

    if (!ge_audio_output_is_valid(output)
            || (interleaved_stereo == NULL && frame_count != 0U)
            || frame_count > SIZE_MAX / (2U * sizeof(int16_t))) {
        return 0U;
    }

    accepted = ge_audio_output_free(output);
    if (accepted > frame_count) {
        accepted = frame_count;
    }
    contiguous = output->capacity_frames - output->write_frame;
    if (contiguous > accepted) contiguous = accepted;
    if (contiguous != 0U) {
        memcpy(output->samples + output->write_frame * 2U,
                interleaved_stereo, contiguous * 2U * sizeof(int16_t));
    }
    if (accepted > contiguous) {
        memcpy(output->samples, interleaved_stereo + contiguous * 2U,
                (accepted - contiguous) * 2U * sizeof(int16_t));
    }
    output->write_frame = ge_audio_ring_advance(
            output->write_frame, accepted, output->capacity_frames);
    output->queued_frames += accepted;
    output->frames_written = ge_audio_counter_add(
            output->frames_written,
            accepted);
    output->frames_dropped = ge_audio_counter_add(
            output->frames_dropped,
            frame_count - accepted);
    return accepted;
}

size_t ge_audio_output_read(
        GeAudioOutput *output,
        int16_t *interleaved_stereo,
        size_t frame_count)
{
    size_t available;
    size_t contiguous;

    if (!ge_audio_output_is_valid(output)
            || (interleaved_stereo == NULL && frame_count != 0U)
            || frame_count > SIZE_MAX / (2U * sizeof(int16_t))) {
        return 0U;
    }

    available = output->queued_frames;
    if (available > frame_count) {
        available = frame_count;
    }
    contiguous = output->capacity_frames - output->read_frame;
    if (contiguous > available) contiguous = available;
    if (contiguous != 0U) {
        memcpy(interleaved_stereo, output->samples + output->read_frame * 2U,
                contiguous * 2U * sizeof(int16_t));
    }
    if (available > contiguous) {
        memcpy(interleaved_stereo + contiguous * 2U, output->samples,
                (available - contiguous) * 2U * sizeof(int16_t));
    }
    output->read_frame = ge_audio_ring_advance(
            output->read_frame, available, output->capacity_frames);
    if (frame_count > available) {
        memset(
                interleaved_stereo + available * 2U,
                0,
                (frame_count - available) * 2U * sizeof(int16_t));
    }

    output->queued_frames -= available;
    output->frames_read = ge_audio_counter_add(output->frames_read, available);
    output->frames_underflowed = ge_audio_counter_add(
            output->frames_underflowed,
            frame_count - available);
    return available;
}

size_t ge_audio_output_discard(
        GeAudioOutput *output,
        size_t frame_count)
{
    size_t discarded;

    if (!ge_audio_output_is_valid(output)) {
        return 0U;
    }
    discarded = output->queued_frames;
    if (discarded > frame_count) {
        discarded = frame_count;
    }
    output->read_frame = ge_audio_ring_advance(
            output->read_frame, discarded, output->capacity_frames);
    output->queued_frames -= discarded;
    output->frames_discarded = ge_audio_counter_add(
            output->frames_discarded,
            discarded);
    return discarded;
}

size_t ge_audio_output_queued(const GeAudioOutput *output)
{
    return ge_audio_output_is_valid(output) ? output->queued_frames : 0U;
}

size_t ge_audio_output_free(const GeAudioOutput *output)
{
    return ge_audio_output_is_valid(output)
        ? output->capacity_frames - output->queued_frames
        : 0U;
}

static int16_t ge_audio_saturate_s16(int64_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

void ge_audio_mix_stereo_s16(
        int16_t *destination,
        const int16_t *source,
        size_t frame_count,
        int32_t left_gain_q15,
        int32_t right_gain_q15)
{
    size_t frame;

    if (destination == NULL || source == NULL || frame_count > SIZE_MAX / 2U) {
        return;
    }

    for (frame = 0U; frame < frame_count; frame++) {
        int64_t left = destination[frame * 2U]
            + ((int64_t)source[frame * 2U] * left_gain_q15) / 32768;
        int64_t right = destination[frame * 2U + 1U]
            + ((int64_t)source[frame * 2U + 1U] * right_gain_q15) / 32768;

        destination[frame * 2U] = ge_audio_saturate_s16(left);
        destination[frame * 2U + 1U] = ge_audio_saturate_s16(right);
    }
}

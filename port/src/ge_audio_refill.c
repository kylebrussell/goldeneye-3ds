#include "ge_audio_refill.h"

#include <limits.h>
#include <string.h>

static uint64_t ge_audio_refill_counter_add(uint64_t value, size_t amount)
{
    if ((uint64_t)amount > UINT64_MAX - value) {
        return UINT64_MAX;
    }
    return value + (uint64_t)amount;
}

void ge_audio_refill_init(GeAudioRefillState *state, uint32_t sample_rate)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->sample_rate = sample_rate;
}

int ge_audio_refill_update_rate(
        GeAudioRefillState *state,
        uint32_t sample_rate)
{
    if (state == NULL || sample_rate == 0U) {
        return -1;
    }
    if (state->sample_rate == sample_rate) {
        return 0;
    }
    state->sample_rate = sample_rate;
    state->sample_rate_changes = ge_audio_refill_counter_add(
            state->sample_rate_changes,
            1U);
    return 1;
}

size_t ge_audio_refill_block(
        GeAudioRefillState *state,
        GeAudioOutput *output,
        int16_t *interleaved_stereo,
        size_t frame_count)
{
    size_t source_frames;

    if (state == NULL || output == NULL
            || (interleaved_stereo == NULL && frame_count != 0U)) {
        return 0U;
    }

    source_frames = ge_audio_output_read(
            output,
            interleaved_stereo,
            frame_count);
    state->blocks_prepared = ge_audio_refill_counter_add(
            state->blocks_prepared,
            1U);
    state->source_frames = ge_audio_refill_counter_add(
            state->source_frames,
            source_frames);
    state->silent_frames = ge_audio_refill_counter_add(
            state->silent_frames,
            frame_count - source_frames);
    return source_frames;
}

#ifndef GE_AUDIO_OUTPUT_H
#define GE_AUDIO_OUTPUT_H

/* Allocation-free interleaved stereo PCM boundary for an ndsp frontend. */

#include <stddef.h>
#include <stdint.h>

typedef struct GeAudioOutput {
    int16_t *samples;
    size_t capacity_frames;
    size_t read_frame;
    size_t write_frame;
    size_t queued_frames;
    uint32_t sample_rate;
    uint64_t frames_written;
    uint64_t frames_read;
    uint64_t frames_discarded;
    uint64_t frames_dropped;
    uint64_t frames_underflowed;
} GeAudioOutput;

int ge_audio_output_init(
        GeAudioOutput *output,
        int16_t *stereo_storage,
        size_t capacity_frames,
        uint32_t sample_rate);
void ge_audio_output_reset(GeAudioOutput *output);
int ge_audio_output_set_sample_rate(GeAudioOutput *output, uint32_t sample_rate);

/* Returns frames accepted. Excess complete frames are dropped and counted. */
size_t ge_audio_output_write(
        GeAudioOutput *output,
        const int16_t *interleaved_stereo,
        size_t frame_count);

/*
 * Returns real frames read. Any requested shortage is zero-filled, providing
 * a complete click-free block for an ndsp wave buffer, and counted.
 */
size_t ge_audio_output_read(
        GeAudioOutput *output,
        int16_t *interleaved_stereo,
        size_t frame_count);

/*
 * Advances past queued frames without copying them.  This keeps the original
 * mixer clock running for diagnostics when a platform sink is unavailable;
 * discarded frames are tracked separately from rendered reads and overflow.
 */
size_t ge_audio_output_discard(
        GeAudioOutput *output,
        size_t frame_count);

size_t ge_audio_output_queued(const GeAudioOutput *output);
size_t ge_audio_output_free(const GeAudioOutput *output);

/* Saturating stereo accumulation with signed Q15 per-channel gains. */
void ge_audio_mix_stereo_s16(
        int16_t *destination,
        const int16_t *source,
        size_t frame_count,
        int32_t left_gain_q15,
        int32_t right_gain_q15);

#endif

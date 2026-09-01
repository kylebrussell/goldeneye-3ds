#ifndef GE_AUDIO_REFILL_H
#define GE_AUDIO_REFILL_H

#include "ge_audio_output.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Platform-independent accounting used by pull-style audio frontends.  The
 * destination is always completely initialized: ge_audio_output_read()
 * supplies source frames and zero-fills any underrun.
 */
typedef struct GeAudioRefillState {
    uint32_t sample_rate;
    uint64_t blocks_prepared;
    uint64_t source_frames;
    uint64_t silent_frames;
    uint64_t sample_rate_changes;
} GeAudioRefillState;

void ge_audio_refill_init(GeAudioRefillState *state, uint32_t sample_rate);

/* Returns 1 when the rate changed, 0 when it did not, and -1 on bad input. */
int ge_audio_refill_update_rate(
        GeAudioRefillState *state,
        uint32_t sample_rate);

/* Returns the number of real (non-padding) source frames copied. */
size_t ge_audio_refill_block(
        GeAudioRefillState *state,
        GeAudioOutput *output,
        int16_t *interleaved_stereo,
        size_t frame_count);

#endif

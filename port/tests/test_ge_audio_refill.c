#include "ge_audio_refill.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    static const int16_t source[] = {
        1, -1,
        2, -2,
        3, -3,
    };
    int16_t ring_storage[8] = {0};
    int16_t block[8] = {99, 99, 99, 99, 99, 99, 99, 99};
    GeAudioOutput output;
    GeAudioRefillState refill;

    assert(ge_audio_output_init(&output, ring_storage, 4U, 32000U) == 0);
    ge_audio_refill_init(&refill, output.sample_rate);
    assert(ge_audio_output_write(&output, source, 3U) == 3U);

    assert(ge_audio_refill_block(&refill, &output, block, 4U) == 3U);
    assert(block[0] == 1 && block[1] == -1);
    assert(block[2] == 2 && block[3] == -2);
    assert(block[4] == 3 && block[5] == -3);
    assert(block[6] == 0 && block[7] == 0);
    assert(refill.blocks_prepared == 1U);
    assert(refill.source_frames == 3U);
    assert(refill.silent_frames == 1U);

    assert(ge_audio_refill_update_rate(&refill, 32000U) == 0);
    assert(ge_audio_refill_update_rate(&refill, 44100U) == 1);
    assert(ge_audio_refill_update_rate(&refill, 0U) == -1);
    assert(refill.sample_rate == 44100U);
    assert(refill.sample_rate_changes == 1U);

    assert(ge_audio_refill_block(&refill, &output, block, 4U) == 0U);
    assert(block[0] == 0 && block[7] == 0);
    assert(refill.blocks_prepared == 2U);
    assert(refill.source_frames == 3U);
    assert(refill.silent_frames == 5U);

    puts("ge_audio_refill tests passed");
    return 0;
}

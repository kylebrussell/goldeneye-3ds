#include "ge_audio_output.h"
#include "ge_libultra_audio.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_ring_wrap_drop_and_underflow(void)
{
    int16_t storage[8];
    const int16_t first[6] = {1, -1, 2, -2, 3, -3};
    const int16_t second[6] = {4, -4, 5, -5, 6, -6};
    int16_t output[8];
    GeAudioOutput audio;

    assert(ge_audio_output_init(&audio, storage, 4U, 22050U) == 0);
    assert(ge_audio_output_write(&audio, first, 3U) == 3U);
    assert(ge_audio_output_write(&audio, second, 3U) == 1U);
    assert(ge_audio_output_queued(&audio) == 4U);
    assert(audio.frames_dropped == 2U);
    assert(ge_audio_output_read(&audio, output, 2U) == 2U);
    assert(output[0] == 1 && output[1] == -1);
    assert(output[2] == 2 && output[3] == -2);

    assert(ge_audio_output_write(&audio, second + 2, 2U) == 2U);
    memset(output, 0x7f, sizeof(output));
    assert(ge_audio_output_read(&audio, output, 4U) == 4U);
    assert(output[0] == 3 && output[1] == -3);
    assert(output[2] == 4 && output[3] == -4);
    assert(output[4] == 5 && output[5] == -5);
    assert(output[6] == 6 && output[7] == -6);

    memset(output, 0x7f, sizeof(output));
    assert(ge_audio_output_read(&audio, output, 2U) == 0U);
    assert(output[0] == 0 && output[1] == 0);
    assert(output[2] == 0 && output[3] == 0);
    assert(audio.frames_underflowed == 2U);
}

static void test_saturating_mixer(void)
{
    int16_t destination[4] = {30000, -30000, 1000, -1000};
    const int16_t source[4] = {10000, -10000, 4000, 4000};

    ge_audio_mix_stereo_s16(
            destination, source, 2U, 32768, 16384);
    assert(destination[0] == INT16_MAX);
    assert(destination[1] == INT16_MIN);
    assert(destination[2] == 5000);
    assert(destination[3] == 1000);
}

static void test_explicit_sink_unavailable_discard(void)
{
    int16_t storage[8];
    const int16_t samples[8] = {1, -1, 2, -2, 3, -3, 4, -4};
    int16_t consumed[4];
    GeAudioOutput audio;

    assert(ge_audio_output_init(&audio, storage, 4U, 32000U) == 0);
    assert(ge_audio_output_write(&audio, samples, 4U) == 4U);
    assert(ge_audio_output_discard(&audio, 3U) == 3U);
    assert(ge_audio_output_queued(&audio) == 1U);
    assert(audio.frames_read == 0U);
    assert(audio.frames_discarded == 3U);
    assert(audio.frames_dropped == 0U);
    assert(ge_audio_output_read(&audio, consumed, 1U) == 1U);
    assert(consumed[0] == 4 && consumed[1] == -4);
    ge_audio_output_reset(&audio);
    assert(audio.frames_discarded == 0U);
}

static void test_ai_compatibility(void)
{
    int16_t storage[8];
    int16_t pcm[6] = {1, 2, 3, 4, 5, 6};
    int16_t consumed[4];
    GeAudioOutput audio;

    assert(ge_audio_output_init(&audio, storage, 4U, 22050U) == 0);
    ge_libultra_audio_bind(&audio);
    assert(osAiSetFrequency(32000U) == 32000);
    assert(audio.sample_rate == 32000U);
    assert(osAiSetFrequency(1000U) == -1);
    assert(osAiSetNextBuffer(pcm, sizeof(pcm)) == 0);
    assert(osAiGetLength() == sizeof(pcm));
    assert(osAiGetStatus() == AI_STATUS_DMA_BUSY);
    assert(osAiSetNextBuffer(pcm, 8U) == -1);
    assert(osAiGetLength() == sizeof(pcm));
    assert(osAiSetNextBuffer(pcm, 4U) == 0);
    assert((osAiGetStatus() & AI_STATUS_FIFO_FULL) != 0U);
    assert(osAiSetNextBuffer(pcm, 4U) == -1);

    assert(ge_audio_output_read(&audio, consumed, 2U) == 2U);
    assert(osAiGetLength() == 8U);
    assert((osAiGetStatus() & AI_STATUS_FIFO_FULL) == 0U);
    ge_libultra_audio_bind(NULL);
    assert(osAiSetNextBuffer(pcm, sizeof(pcm)) == -1);
    assert(osAiGetStatus() == 0U);
}

int main(void)
{
    test_ring_wrap_drop_and_underflow();
    test_saturating_mixer();
    test_explicit_sink_unavailable_discard();
    test_ai_compatibility();
    puts("portable PCM output, mixer, and AI compatibility tests passed");
    return 0;
}

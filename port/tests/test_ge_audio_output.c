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

static uint32_t ring_random(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

/* Compare the platform queue with its former sample-at-a-time behavior,
 * including arbitrary (not just power-of-two) capacities and empty calls. */
static void test_ring_matches_scalar(void)
{
    static const size_t capacities[] = {1U, 2U, 3U, 7U, 64U, 511U, 512U, 1025U};
    int16_t storage[2050], reference[2050], source[2100];
    int16_t actual[2100], expected[2100];
    uint32_t random = UINT32_C(0x0073d5);
    size_t capacity_index;
    for (capacity_index = 0U;
            capacity_index < sizeof(capacities) / sizeof(capacities[0]);
            ++capacity_index) {
        const size_t capacity = capacities[capacity_index];
        size_t read = 0U, write = 0U, queued = 0U, iteration;
        uint64_t written = 0U, consumed = 0U, dropped = 0U;
        uint64_t underflowed = 0U, discarded = 0U;
        GeAudioOutput audio;
        memset(storage, 0, sizeof(storage));
        memset(reference, 0, sizeof(reference));
        assert(ge_audio_output_init(&audio, storage, capacity, 32000U) == 0);
        for (iteration = 0U; iteration < 5000U; ++iteration) {
            const size_t requested = ring_random(&random) % (capacity + 25U);
            const uint32_t operation = ring_random(&random) % 3U;
            size_t count, frame;
            if (operation == 0U) {
                count = requested < capacity - queued ? requested : capacity - queued;
                for (frame = 0U; frame < requested * 2U; ++frame)
                    source[frame] = (int16_t)((int32_t)(ring_random(&random) & 65535U) - 32768);
                for (frame = 0U; frame < count; ++frame) {
                    reference[write * 2U] = source[frame * 2U];
                    reference[write * 2U + 1U] = source[frame * 2U + 1U];
                    write = (write + 1U) % capacity;
                }
                assert(ge_audio_output_write(&audio,
                    requested != 0U ? source : NULL, requested) == count);
                queued += count;
                written += count;
                dropped += requested - count;
            } else {
                count = requested < queued ? requested : queued;
                if (operation == 1U) {
                    memset(actual, 0x5a, sizeof(actual));
                    memset(expected, 0x5a, sizeof(expected));
                    for (frame = 0U; frame < count; ++frame) {
                        expected[frame * 2U] = reference[read * 2U];
                        expected[frame * 2U + 1U] = reference[read * 2U + 1U];
                        read = (read + 1U) % capacity;
                    }
                    memset(expected + count * 2U, 0,
                        (requested - count) * 2U * sizeof(*expected));
                    assert(ge_audio_output_read(&audio,
                        requested != 0U ? actual : NULL, requested) == count);
                    assert(memcmp(actual, expected, sizeof(actual)) == 0);
                    consumed += count;
                    underflowed += requested - count;
                } else {
                    read = (read + count) % capacity;
                    assert(ge_audio_output_discard(&audio, requested) == count);
                    discarded += count;
                }
                queued -= count;
            }
            assert(audio.read_frame == read && audio.write_frame == write);
            assert(audio.queued_frames == queued);
            assert(audio.frames_written == written && audio.frames_read == consumed);
            assert(audio.frames_dropped == dropped && audio.frames_underflowed == underflowed);
            assert(audio.frames_discarded == discarded);
            assert(memcmp(storage, reference, sizeof(storage)) == 0);
        }
    }
    puts("PCM ring matches scalar samples, cursors and counters across 40,000 operations");
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
    test_ring_matches_scalar();
    test_saturating_mixer();
    test_explicit_sink_unavailable_discard();
    test_ai_compatibility();
    puts("portable PCM output, mixer, and AI compatibility tests passed");
    return 0;
}

#include "ge_3ds_audio.h"
#include <3ds.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAVE_FRAMES 512U
#define WAVE_SAMPLES (WAVE_FRAMES * 2U)
#define RING_FRAMES 4096U

static struct {
    Result init_result;
    bool alloc_fail;
    unsigned flush_fail_at;
    bool initialized;
    void *allocation;
    unsigned inits, exits, allocs, frees, clears, resets, adds, flushes;
    float rate;
    ndspWaveBuf *queue[4];
    int16_t queued_samples[4][WAVE_SAMPLES];
    size_t queue_count;
} mock;

static GeAudioOutput primary, secondary;
static int16_t primary_storage[RING_FRAMES * 2U];
static int16_t secondary_storage[RING_FRAMES * 2U];
static int16_t source[RING_FRAMES * 2U];

static void check_owned_samples(void)
{
    size_t i;
    for (i = 0; i < mock.queue_count; ++i) {
        assert(mock.queue[i]->status == NDSP_WBUF_QUEUED
                || mock.queue[i]->status == NDSP_WBUF_PLAYING);
        assert(memcmp(mock.queue[i]->data_vaddr, mock.queued_samples[i],
                    sizeof(mock.queued_samples[i])) == 0);
    }
}

Result ndspInit(void)
{
    assert(!mock.initialized);
    ++mock.inits;
    if (!R_FAILED(mock.init_result)) mock.initialized = true;
    return mock.init_result;
}

void ndspExit(void)
{
    assert(mock.initialized);
    assert(mock.queue_count == 0U);
    mock.initialized = false;
    ++mock.exits;
}

void *linearAlloc(size_t size)
{
    assert(mock.initialized);
    assert(mock.allocation == NULL);
    assert(size == 4U * WAVE_SAMPLES * sizeof(int16_t));
    ++mock.allocs;
    if (mock.alloc_fail) return NULL;
    mock.allocation = malloc(size);
    assert(mock.allocation != NULL);
    memset(mock.allocation, 0xa5, size);
    return mock.allocation;
}

void linearFree(void *pointer)
{
    assert(!mock.initialized); /* NDSP must relinquish DSP ownership first. */
    assert(mock.queue_count == 0U);
    assert(pointer != NULL && pointer == mock.allocation);
    free(pointer);
    mock.allocation = NULL;
    ++mock.frees;
}

Result DSP_FlushDataCache(const void *address, uint32_t size)
{
    const uintptr_t offset = (uintptr_t)address - (uintptr_t)mock.allocation;
    size_t i;
    assert(mock.initialized && mock.allocation != NULL);
    assert(size == WAVE_SAMPLES * sizeof(int16_t));
    assert(offset < 4U * size && offset % size == 0U);
    check_owned_samples();
    for (i = 0; i < mock.queue_count; ++i)
        assert(mock.queue[i]->data_vaddr != address);
    ++mock.flushes;
    return mock.flushes == mock.flush_fail_at ? -77 : 0;
}

void ndspSetOutputMode(ndspOutputMode mode)
{
    assert(mock.initialized && mode == NDSP_OUTPUT_STEREO);
}

void ndspChnReset(int id)
{
    assert(mock.initialized && id == 0 && mock.queue_count == 0U);
    ++mock.resets;
}

void ndspChnSetInterp(int id, ndspInterpType type)
{
    assert(mock.initialized && id == 0 && type == NDSP_INTERP_LINEAR);
}

void ndspChnSetRate(int id, float rate)
{
    assert(mock.initialized && id == 0);
    assert(rate >= 8000.0f && rate <= 48000.0f);
    mock.rate = rate;
}

void ndspChnSetFormat(int id, uint16_t format)
{
    assert(mock.initialized && id == 0 && format == NDSP_FORMAT_STEREO_PCM16);
}

void ndspChnWaveBufClear(int id)
{
    size_t i;
    assert(mock.initialized && id == 0);
    check_owned_samples();
    for (i = 0; i < mock.queue_count; ++i)
        mock.queue[i]->status = NDSP_WBUF_DONE;
    mock.queue_count = 0U;
    ++mock.clears;
}

void ndspChnWaveBufAdd(int id, ndspWaveBuf *wave)
{
    size_t i;
    assert(mock.initialized && id == 0 && mock.queue_count < 4U);
    assert(wave->status == NDSP_WBUF_FREE || wave->status == NDSP_WBUF_DONE);
    assert(wave->nsamples == WAVE_FRAMES && !wave->looping);
    assert(wave->adpcm_data == NULL && wave->offset == 0U);
    check_owned_samples();
    for (i = 0; i < mock.queue_count; ++i) assert(mock.queue[i] != wave);
    wave->status = NDSP_WBUF_QUEUED;
    wave->sequence_id = (uint16_t)++mock.adds;
    wave->next = NULL;
    if (mock.queue_count != 0U)
        mock.queue[mock.queue_count - 1U]->next = wave;
    mock.queue[mock.queue_count] = wave;
    memcpy(mock.queued_samples[mock.queue_count], wave->data_vaddr,
            sizeof(mock.queued_samples[0]));
    ++mock.queue_count;
}

static void complete_wave(void)
{
    check_owned_samples();
    assert(mock.queue_count != 0U);
    mock.queue[0]->status = NDSP_WBUF_DONE;
    --mock.queue_count;
    memmove(mock.queue, mock.queue + 1, mock.queue_count * sizeof(mock.queue[0]));
    memmove(mock.queued_samples, mock.queued_samples + 1,
            mock.queue_count * sizeof(mock.queued_samples[0]));
}

static void reset_test(void)
{
    ge_3ds_audio_exit();
    assert(mock.allocation == NULL && !mock.initialized);
    memset(&mock, 0, sizeof(mock));
    assert(ge_audio_output_init(&primary, primary_storage,
                RING_FRAMES, 22050U) == 0);
    assert(ge_audio_output_init(&secondary, secondary_storage,
                RING_FRAMES, 22050U) == 0);
}

static void test_init_validation(void)
{
    unsigned variant;
    reset_test();
    assert(ge_3ds_audio_init(NULL) == -1);
    for (variant = 0; variant < 10U; ++variant) {
        GeAudioOutput bad = primary;
        switch (variant) {
        case 0: bad.samples = NULL; break;
        case 1: bad.capacity_frames = 0U; break;
        case 2: bad.capacity_frames = SIZE_MAX; break;
        case 3: bad.read_frame = RING_FRAMES; break;
        case 4: bad.write_frame = RING_FRAMES; break;
        case 5: bad.queued_frames = RING_FRAMES + 1U; break;
        case 6: bad.write_frame = 1U; break; /* inconsistent ring */
        case 7: bad.sample_rate = 0U; break;
        case 8: bad.sample_rate = 7999U; break;
        case 9: bad.sample_rate = 48001U; break;
        }
        assert(ge_3ds_audio_init(&bad) == -1);
        assert(ge_3ds_audio_last_error() == -1);
    }
    assert(mock.inits == 0U && mock.allocs == 0U);
    assert(!ge_3ds_audio_is_active() && ge_3ds_audio_refill_stats() == NULL);
    ge_3ds_audio_pump();
    ge_3ds_audio_exit();
    assert(mock.exits == 0U && mock.frees == 0U);
}

static void test_failures_and_retry(void)
{
    unsigned failure;
    reset_test();
    mock.init_result = -55;
    assert(ge_3ds_audio_init(&primary) == -1);
    assert(ge_3ds_audio_last_error() == -55);
    assert(mock.inits == 1U && mock.exits == 0U && mock.allocs == 0U);
    mock.init_result = 0;
    mock.alloc_fail = true;
    assert(ge_3ds_audio_init(&primary) == -1);
    assert(ge_3ds_audio_last_error() == -2);
    assert(mock.exits == 1U && mock.frees == 0U);
    mock.alloc_fail = false;
    assert(ge_3ds_audio_init(&primary) == 0);
    assert(ge_3ds_audio_last_error() == 0);
    for (failure = 1U; failure <= 4U; ++failure) {
        reset_test();
        mock.flush_fail_at = failure;
        assert(ge_3ds_audio_init(&primary) == -1);
        assert(ge_3ds_audio_last_error() == -77);
        assert(mock.adds == failure - 1U);
        assert(!ge_3ds_audio_is_active() && mock.exits == 1U && mock.frees == 1U);
        ge_3ds_audio_exit();
        assert(ge_3ds_audio_bind_secondary(NULL) == 0);
        assert(ge_3ds_audio_last_error() == -77);
        assert(mock.exits == 1U && mock.frees == 1U);
        mock.flush_fail_at = 0U;
        assert(ge_3ds_audio_init(&primary) == 0);
    }
}

static void test_queue_ownership(void)
{
    const GeAudioRefillState *stats;
    unsigned i;
    reset_test();
    assert(ge_audio_output_write(&primary, source, RING_FRAMES) == RING_FRAMES);
    assert(ge_3ds_audio_init(&primary) == 0);
    stats = ge_3ds_audio_refill_stats();
    assert(stats != NULL && stats->blocks_prepared == 4U);
    assert(stats->source_frames == 2048U && stats->silent_frames == 0U);
    for (i = 0U; i < 4U; ++i)
        assert(memcmp(mock.queue[i]->data_vaddr, source + i * WAVE_SAMPLES,
                    WAVE_SAMPLES * sizeof(int16_t)) == 0);
    assert(ge_3ds_audio_init(&primary) == 0);
    assert(ge_3ds_audio_init(&secondary) == -1);
    assert(ge_3ds_audio_last_error() == -1);
    assert(mock.inits == 1U && mock.allocs == 1U && mock.adds == 4U);
    mock.queue[0]->status = NDSP_WBUF_PLAYING;
    /* A stalled/suspended DSP retains its queued buffers. Pumping must not
     * read more source, allocate or alter those buffers until completion. */
    for (i = 0U; i < 100U; ++i) ge_3ds_audio_pump();
    assert(stats->blocks_prepared == 4U && primary.frames_read == 2048U);
    assert(mock.adds == 4U && mock.allocs == 1U);
    complete_wave();
    ge_3ds_audio_pump();
    assert(mock.adds == 5U && stats->blocks_prepared == 5U);
    assert(memcmp(mock.queue[3]->data_vaddr, source + 2048U * 2U,
                WAVE_SAMPLES * sizeof(int16_t)) == 0);
    ge_3ds_audio_exit();
    ge_3ds_audio_exit();
    ge_3ds_audio_pump();
    assert(mock.exits == 1U && mock.frees == 1U);
    assert(!ge_3ds_audio_is_active() && ge_3ds_audio_refill_stats() == NULL);
}

static void test_secondary_binding(void)
{
    GeAudioOutput overlap;
    size_t i;
    reset_test();
    assert(ge_3ds_audio_bind_secondary(&secondary) == -1);
    assert(ge_3ds_audio_init(&primary) == 0);
    assert(ge_3ds_audio_bind_secondary(&secondary) == 0);
    assert(ge_audio_output_write(&secondary, source, WAVE_FRAMES) == WAVE_FRAMES);
    assert(ge_3ds_audio_bind_secondary(&primary) == -1);
    overlap = primary;
    assert(ge_3ds_audio_bind_secondary(&overlap) == -1);
    overlap.samples = primary_storage + 2U;
    overlap.capacity_frames = RING_FRAMES - 1U;
    assert(ge_3ds_audio_bind_secondary(&overlap) == -1);
    overlap = secondary;
    overlap.sample_rate = 32000U;
    assert(ge_3ds_audio_bind_secondary(&overlap) == -1);
    overlap = secondary;
    overlap.read_frame = RING_FRAMES;
    assert(ge_3ds_audio_bind_secondary(&overlap) == -1);
    complete_wave();
    ge_3ds_audio_pump(); /* Rejected binds preserved the valid secondary. */
    assert(secondary.frames_read == WAVE_FRAMES);
    for (i = 0U; i < WAVE_SAMPLES; ++i)
        assert(mock.queue[3]->data_pcm16[i] == (source[i] * 32767) / 32768);
    assert(ge_3ds_audio_bind_secondary(NULL) == 0);
    assert(ge_audio_output_write(&secondary, source, WAVE_FRAMES) == WAVE_FRAMES);
    complete_wave();
    ge_3ds_audio_pump();
    assert(secondary.frames_read == WAVE_FRAMES);
    for (i = 0U; i < WAVE_SAMPLES; ++i)
        assert(mock.queue[3]->data_pcm16[i] == 0);
}

static void test_runtime_validation_and_rate(void)
{
    reset_test();
    assert(ge_3ds_audio_init(&primary) == 0);
    assert(ge_3ds_audio_bind_secondary(&secondary) == 0);
    assert(ge_audio_output_write(&secondary, source, WAVE_FRAMES) == WAVE_FRAMES);
    assert(ge_audio_output_set_sample_rate(&primary, 32000U) == 0);
    ge_3ds_audio_pump();
    assert(mock.rate == 32000.0f && mock.clears == 1U && mock.adds == 8U);
    assert(ge_3ds_audio_refill_stats()->sample_rate_changes == 1U);
    assert(ge_3ds_audio_last_error() == -1 && secondary.frames_read == 0U);
    assert(ge_audio_output_set_sample_rate(&secondary, 32000U) == 0);
    complete_wave();
    ge_3ds_audio_pump();
    assert(secondary.frames_read == 0U); /* Must explicitly bind after mismatch. */
    assert(ge_3ds_audio_bind_secondary(&secondary) == 0);
    secondary.read_frame = SIZE_MAX;
    complete_wave();
    ge_3ds_audio_pump();
    assert(ge_3ds_audio_is_active() && secondary.frames_read == 0U);
    primary.queued_frames = SIZE_MAX;
    ge_3ds_audio_pump();
    assert(!ge_3ds_audio_is_active() && mock.exits == 1U && mock.frees == 1U);
    reset_test();
    assert(ge_3ds_audio_init(&primary) == 0);
    mock.flush_fail_at = 5U;
    complete_wave();
    ge_3ds_audio_pump();
    assert(!ge_3ds_audio_is_active() && mock.adds == 4U);
    assert(ge_3ds_audio_last_error() == -77 && mock.exits == 1U && mock.frees == 1U);
}

static void test_resume_discards_old_audio(void)
{
    int16_t old_samples[WAVE_SAMPLES];
    int16_t new_samples[WAVE_SAMPLES];
    size_t i, wave;
    reset_test();
    ge_3ds_audio_resume();
    assert(mock.clears == 0U && mock.inits == 0U);
    for (i = 0; i < WAVE_SAMPLES; ++i) {
        old_samples[i] = -12345;
        new_samples[i] = 10000;
    }
    for (i = 0; i < RING_FRAMES / WAVE_FRAMES; ++i) {
        assert(ge_audio_output_write(&primary, old_samples, WAVE_FRAMES)
                == WAVE_FRAMES);
        assert(ge_audio_output_write(&secondary, old_samples, WAVE_FRAMES)
                == WAVE_FRAMES);
    }
    assert(ge_3ds_audio_init(&primary) == 0);
    assert(ge_3ds_audio_bind_secondary(&secondary) == 0);
    for (wave = 0; wave < mock.queue_count; ++wave)
        assert(mock.queue[wave]->data_pcm16[0] == -12345);
    ge_3ds_audio_resume();
    assert(ge_3ds_audio_is_active() && mock.queue_count == 0U);
    assert(mock.inits == 1U && mock.allocs == 1U && mock.exits == 0U);
    assert(primary.queued_frames == 0U && secondary.queued_frames == 0U);
    assert(primary.frames_discarded == 2048U && secondary.frames_discarded == 4096U);
    assert(primary.frames_read == 2048U && secondary.frames_read == 0U);
    assert(primary.frames_written == 4096U && secondary.frames_written == 4096U);
    assert(ge_3ds_audio_refill_stats()->blocks_prepared == 4U);
    ge_3ds_audio_resume(); /* Duplicate drain has no extra source effect. */
    assert(primary.frames_discarded == 2048U && secondary.frames_discarded == 4096U);
    assert(ge_audio_output_write(&primary, new_samples, WAVE_FRAMES) == WAVE_FRAMES);
    assert(ge_audio_output_write(&secondary, new_samples, WAVE_FRAMES) == WAVE_FRAMES);
    ge_3ds_audio_pump();
    assert(mock.queue_count == 4U && mock.adds == 8U && mock.allocs == 1U);
    for (wave = 0; wave < mock.queue_count; ++wave)
        for (i = 0; i < WAVE_SAMPLES; ++i)
            assert(mock.queue[wave]->data_pcm16[i] == (wave == 0U ? 19999 : 0));
    /* Resume with no new source produces silence, never old linear contents. */
    ge_3ds_audio_resume();
    ge_3ds_audio_pump();
    for (wave = 0; wave < mock.queue_count; ++wave)
        for (i = 0; i < WAVE_SAMPLES; ++i)
            assert(mock.queue[wave]->data_pcm16[i] == 0);
    secondary.read_frame = SIZE_MAX;
    ge_3ds_audio_resume(); /* Invalid secondary is detached, not dereferenced. */
    assert(ge_3ds_audio_is_active() && mock.queue_count == 0U);
    primary.samples = NULL;
    ge_3ds_audio_resume();
    assert(!ge_3ds_audio_is_active() && mock.exits == 1U && mock.frees == 1U);
    ge_3ds_audio_resume();
    assert(mock.exits == 1U && mock.frees == 1U);
    reset_test();
    mock.init_result = -55;
    assert(ge_3ds_audio_init(&primary) == -1);
    ge_3ds_audio_resume();
    assert(mock.exits == 0U && ge_3ds_audio_last_error() == -55);
}

int main(void)
{
    size_t i;
    for (i = 0; i < RING_FRAMES * 2U; ++i) source[i] = (int16_t)(i + 1U);
    test_init_validation();
    test_failures_and_retry();
    test_queue_ownership();
    test_secondary_binding();
    test_runtime_validation_and_rate();
    test_resume_discards_old_audio();
    ge_3ds_audio_exit();
    puts("3DS audio lifecycle: validation, fault cleanup, ownership, rate and resume drain tests passed");
    return 0;
}

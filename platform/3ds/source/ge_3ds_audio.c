#include "ge_3ds_audio.h"

#include <3ds.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define GE_3DS_AUDIO_CHANNEL 0
#define GE_3DS_AUDIO_WAVE_COUNT 4U
#define GE_3DS_AUDIO_FRAMES_PER_WAVE 512U
#define GE_3DS_AUDIO_CHANNELS 2U
#define GE_3DS_AUDIO_BYTES_PER_WAVE \
    (GE_3DS_AUDIO_FRAMES_PER_WAVE * GE_3DS_AUDIO_CHANNELS * sizeof(int16_t))
#define GE_3DS_AUDIO_BUFFER_BYTES \
    (GE_3DS_AUDIO_WAVE_COUNT * GE_3DS_AUDIO_BYTES_PER_WAVE)
#define GE_3DS_AUDIO_MIN_SAMPLE_RATE 8000U
#define GE_3DS_AUDIO_MAX_SAMPLE_RATE 48000U

typedef struct Ge3dsAudioState {
    GeAudioOutput *output;
    GeAudioOutput *secondary_output;
    GeAudioRefillState refill;
    ndspWaveBuf waves[GE_3DS_AUDIO_WAVE_COUNT];
    int16_t *pcm;
    bool submitted[GE_3DS_AUDIO_WAVE_COUNT];
    bool ndsp_initialized;
    bool active;
} Ge3dsAudioState;

static Ge3dsAudioState g_audio;
static int32_t g_last_error;

static bool ge_3ds_audio_output_valid(const GeAudioOutput *output)
{
    size_t remaining;
    size_t expected_write;
    size_t bytes;

    if (output == NULL || output->samples == NULL
            || output->capacity_frames == 0U
            || output->capacity_frames > SIZE_MAX / (2U * sizeof(int16_t))
            || output->read_frame >= output->capacity_frames
            || output->write_frame >= output->capacity_frames
            || output->queued_frames > output->capacity_frames
            || output->sample_rate < GE_3DS_AUDIO_MIN_SAMPLE_RATE
            || output->sample_rate > GE_3DS_AUDIO_MAX_SAMPLE_RATE) {
        return false;
    }
    bytes = output->capacity_frames * 2U * sizeof(int16_t);
    if ((uintptr_t)output->samples > UINTPTR_MAX - bytes) {
        return false;
    }
    /* Match the producer ring invariant without an overflowing addition. */
    remaining = output->capacity_frames - output->read_frame;
    expected_write = output->queued_frames < remaining
        ? output->read_frame + output->queued_frames
        : output->queued_frames - remaining;
    return output->write_frame == expected_write;
}

/* Both rings have already been validated, including their address extents. */
static bool ge_3ds_audio_outputs_overlap(
        const GeAudioOutput *a, const GeAudioOutput *b)
{
    const uintptr_t a_begin = (uintptr_t)a->samples;
    const uintptr_t b_begin = (uintptr_t)b->samples;
    const size_t a_bytes = a->capacity_frames * 2U * sizeof(int16_t);
    const size_t b_bytes = b->capacity_frames * 2U * sizeof(int16_t);

    return a_begin < b_begin + b_bytes && b_begin < a_begin + a_bytes;
}

static void ge_3ds_audio_configure_channel(uint32_t sample_rate)
{
    ndspChnReset(GE_3DS_AUDIO_CHANNEL);
    ndspChnSetInterp(GE_3DS_AUDIO_CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetRate(GE_3DS_AUDIO_CHANNEL, (float)sample_rate);
    ndspChnSetFormat(GE_3DS_AUDIO_CHANNEL, NDSP_FORMAT_STEREO_PCM16);
}

static void ge_3ds_audio_clear_queue(void)
{
    ndspChnWaveBufClear(GE_3DS_AUDIO_CHANNEL);
    memset(g_audio.waves, 0, sizeof(g_audio.waves));
    memset(g_audio.submitted, 0, sizeof(g_audio.submitted));
}

static int ge_3ds_audio_submit(size_t index)
{
    Result result;
    ndspWaveBuf *wave = &g_audio.waves[index];
    int16_t *samples = g_audio.pcm
        + index * GE_3DS_AUDIO_FRAMES_PER_WAVE * GE_3DS_AUDIO_CHANNELS;

    (void)ge_audio_refill_block(
            &g_audio.refill,
            g_audio.output,
            samples,
            GE_3DS_AUDIO_FRAMES_PER_WAVE);
    if (g_audio.secondary_output != NULL) {
        int16_t secondary[GE_3DS_AUDIO_FRAMES_PER_WAVE * GE_3DS_AUDIO_CHANNELS];
        (void)ge_audio_output_read(g_audio.secondary_output, secondary,
                GE_3DS_AUDIO_FRAMES_PER_WAVE);
        ge_audio_mix_stereo_s16(samples, secondary,
                GE_3DS_AUDIO_FRAMES_PER_WAVE, 32767, 32767);
    }
    result = DSP_FlushDataCache(samples, GE_3DS_AUDIO_BYTES_PER_WAVE);
    if (R_FAILED(result)) {
        g_last_error = result;
        return -1;
    }

    memset(wave, 0, sizeof(*wave));
    wave->data_vaddr = samples;
    wave->nsamples = GE_3DS_AUDIO_FRAMES_PER_WAVE;
    wave->looping = false;
    ndspChnWaveBufAdd(GE_3DS_AUDIO_CHANNEL, wave);
    g_audio.submitted[index] = true;
    return 0;
}

int ge_3ds_audio_init(GeAudioOutput *output)
{
    Result result;

    g_last_error = 0;
    if (!ge_3ds_audio_output_valid(output)) {
        g_last_error = -1;
        return -1;
    }
    if (g_audio.active) {
        if (g_audio.output == output) return 0;
        g_last_error = -1;
        return -1;
    }

    memset(&g_audio, 0, sizeof(g_audio));
    result = ndspInit();
    if (R_FAILED(result)) {
        g_last_error = result;
        return -1;
    }
    g_audio.ndsp_initialized = true;

    g_audio.pcm = linearAlloc(GE_3DS_AUDIO_BUFFER_BYTES);
    if (g_audio.pcm == NULL) {
        g_last_error = -2;
        ge_3ds_audio_exit();
        return -1;
    }

    memset(g_audio.pcm, 0, GE_3DS_AUDIO_BUFFER_BYTES);
    g_audio.output = output;
    ge_audio_refill_init(&g_audio.refill, output->sample_rate);
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ge_3ds_audio_configure_channel(output->sample_rate);
    g_audio.active = true;
    ge_3ds_audio_pump();
    return g_audio.active ? 0 : -1;
}

int ge_3ds_audio_bind_secondary(GeAudioOutput *output)
{
    /* Cleanup may unbind after a failed pump; preserve that failure code. */
    if (output == NULL) {
        g_audio.secondary_output = NULL;
        return 0;
    }
    if (!g_audio.active || !ge_3ds_audio_output_valid(g_audio.output)
                || !ge_3ds_audio_output_valid(output)
                || output->sample_rate != g_audio.output->sample_rate
                || ge_3ds_audio_outputs_overlap(g_audio.output, output)) {
        g_last_error = -1;
        return -1;
    }
    g_audio.secondary_output = output;
    return 0;
}

void ge_3ds_audio_pump(void)
{
    size_t index;
    int rate_changed;

    if (!g_audio.active) {
        return;
    }
    if (!ge_3ds_audio_output_valid(g_audio.output)) {
        g_last_error = -1;
        ge_3ds_audio_exit();
        return;
    }
    if (g_audio.secondary_output != NULL
            && (!ge_3ds_audio_output_valid(g_audio.secondary_output)
                || g_audio.secondary_output->sample_rate
                    != g_audio.output->sample_rate
                || ge_3ds_audio_outputs_overlap(
                    g_audio.output, g_audio.secondary_output))) {
        /* A reset/retuned secondary must be explicitly rebound. Never read a
         * malformed ring or play it at the primary's unrelated sample rate. */
        g_audio.secondary_output = NULL;
        g_last_error = -1;
    }

    rate_changed = ge_audio_refill_update_rate(
            &g_audio.refill,
            g_audio.output->sample_rate);
    if (rate_changed > 0) {
        ge_3ds_audio_clear_queue();
        ge_3ds_audio_configure_channel(g_audio.output->sample_rate);
    }

    for (index = 0U; index < GE_3DS_AUDIO_WAVE_COUNT; index++) {
        if (!g_audio.submitted[index]
                || g_audio.waves[index].status == NDSP_WBUF_DONE) {
            if (ge_3ds_audio_submit(index) != 0) {
                ge_3ds_audio_exit();
                return;
            }
        }
    }
}

void ge_3ds_audio_exit(void)
{
    if (g_audio.ndsp_initialized) {
        ndspChnWaveBufClear(GE_3DS_AUDIO_CHANNEL);
        ndspChnReset(GE_3DS_AUDIO_CHANNEL);
        /* Stop DSP ownership before returning linear memory to the heap. */
        ndspExit();
    }
    if (g_audio.pcm != NULL) {
        linearFree(g_audio.pcm);
    }
    memset(&g_audio, 0, sizeof(g_audio));
}

bool ge_3ds_audio_is_active(void)
{
    return g_audio.active;
}

const GeAudioRefillState *ge_3ds_audio_refill_stats(void)
{
    return g_audio.active ? &g_audio.refill : NULL;
}

int32_t ge_3ds_audio_last_error(void)
{
    return g_last_error;
}

#include "ge_original_music_runtime.h"
#include "ge_original_music_bank.h"
#if defined(GE_PLATFORM_3DS)
#include "ge_3ds_music_worker.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <abi.h>
#include <libaudio.h>

#define GE_MUSIC_RUNTIME_HEAP_BYTES 0x2e000U
#define GE_MUSIC_RUNTIME_MAX_COMMANDS 3000U
#define GE_MUSIC_RUNTIME_MAX_SAMPLES 800U
#define GE_MUSIC_RUNTIME_MAX_PHYSICAL_VOICES 0x18
#define GE_MUSIC_RUNTIME_MAX_UPDATES 0x80
#define GE_MUSIC_RUNTIME_MAX_VOICES 0x10
#define GE_MUSIC_RUNTIME_MAX_EVENTS 0x40
#define GE_MUSIC_RUNTIME_MAX_CHANNELS 16
#define GE_MUSIC_RUNTIME_LAYER_COUNT 3U

typedef struct GeMusicRetiredSequence {
    uint8_t *bytes;
    struct GeMusicRetiredSequence *next;
} GeMusicRetiredSequence;

struct GeOriginalMusicRuntime {
    ALGlobals globals;
    ALHeap heap;
    ALCSPlayer player;
    ALCSeq sequence;
    ALCSPlayer extra_players[GE_MUSIC_RUNTIME_LAYER_COUNT - 1U];
    ALCSeq extra_sequences[GE_MUSIC_RUNTIME_LAYER_COUNT - 1U];
    uint8_t *heap_bytes;
    uint8_t *sequence_bytes;
    uint8_t *extra_sequence_bytes[GE_MUSIC_RUNTIME_LAYER_COUNT - 1U];
    GeMusicRetiredSequence *retired_sequences;
    Acmd *commands;
    int16_t *frame_pcm;
    int16_t *output_samples;
    GeAudioOutput music_output;
    GeAudioAbiState abi;
    GeAudioOutput *output;
    GeOriginalMusicBank *owned_bank;
    uint8_t *owned_ctl;
    uint8_t *owned_tbl;
    GeOriginalMusicRuntimeStats stats;
    int initialized;
    uint8_t retrace_phase;
    int pending;
    size_t pending_samples;
    size_t pending_command_count;
    GeAudioAbiResult pending_result;
};

static ALCSPlayer *ge_music_layer_player(
        GeOriginalMusicRuntime *runtime, unsigned layer)
{
    if (runtime == NULL || layer >= GE_MUSIC_RUNTIME_LAYER_COUNT) return NULL;
    return layer == 0U ? &runtime->player
        : &runtime->extra_players[layer - 1U];
}

static ALCSeq *ge_music_layer_sequence(
        GeOriginalMusicRuntime *runtime, unsigned layer)
{
    if (runtime == NULL || layer >= GE_MUSIC_RUNTIME_LAYER_COUNT) return NULL;
    return layer == 0U ? &runtime->sequence
        : &runtime->extra_sequences[layer - 1U];
}

static uint8_t **ge_music_layer_bytes(
        GeOriginalMusicRuntime *runtime, unsigned layer)
{
    if (runtime == NULL || layer >= GE_MUSIC_RUNTIME_LAYER_COUNT) return NULL;
    return layer == 0U ? &runtime->sequence_bytes
        : &runtime->extra_sequence_bytes[layer - 1U];
}

#define GE_MUSIC_MS *(((s32)((f32)44.1)) & ~0x7)
static s32 ge_music_custom_fx_params[50] = {
    6, 160 GE_MUSIC_MS,
    0, 4 GE_MUSIC_MS, 9830, -9830, 0, 0, 0, 0,
    4 GE_MUSIC_MS, 8 GE_MUSIC_MS, 9830, -9830, 0x2b84, 0, 0, 0x2500,
    20 GE_MUSIC_MS, 64 GE_MUSIC_MS, 16384, -16384, 0x11eb, 0, 0, 0x3000,
    80 GE_MUSIC_MS, 140 GE_MUSIC_MS, 16384, -16384, 0x11eb, 0, 0, 0x3500,
    84 GE_MUSIC_MS, 120 GE_MUSIC_MS, 8192, -8192, 0, 0, 0, 0x4000,
    0, 148 GE_MUSIC_MS, 13000, -13000, 0, 0x017c, 0x0a, 0x4500,
};
#undef GE_MUSIC_MS

static s32 ge_music_dma(s32 address, s32 length, void *unused_state)
{
    (void)unused_state;
    (void)length;
    return address;
}

static ALDMAproc ge_music_dma_new(void *state)
{
    (void)state;
    return ge_music_dma;
}

static void *ge_music_resolve(void *unused_context, uint32_t address,
        size_t size_bytes)
{
    (void)unused_context;
    (void)size_bytes;
    return (void *)(uintptr_t)address;
}

GeOriginalMusicRuntime *ge_original_music_runtime_open(
        ALBank *instrument_bank,
        const uint8_t *cseq, size_t cseq_size,
        int16_t volume,
        GeAudioOutput *output)
{
    GeOriginalMusicRuntime *runtime;
    ALSynConfig synth;
    ALSeqpConfig player;

    if (instrument_bank == NULL || cseq == NULL || cseq_size < 68U
            || output == NULL || sizeof(uintptr_t) > sizeof(uint32_t)
            || alGlobals != NULL) return NULL;
    if (ge_audio_output_set_sample_rate(
            output, GE_ORIGINAL_MUSIC_SAMPLE_RATE) != 0) return NULL;
    runtime = calloc(1U, sizeof(*runtime));
    if (runtime == NULL) return NULL;
    runtime->heap_bytes = calloc(1U, GE_MUSIC_RUNTIME_HEAP_BYTES);
    runtime->sequence_bytes = malloc(cseq_size);
    runtime->commands = calloc(GE_MUSIC_RUNTIME_MAX_COMMANDS, sizeof(Acmd));
    runtime->frame_pcm = calloc(GE_MUSIC_RUNTIME_MAX_SAMPLES * 2U,
            sizeof(int16_t));
    runtime->output_samples = calloc(GE_ORIGINAL_MUSIC_RING_SAMPLES * 2U,
            sizeof(int16_t));
    if (runtime->heap_bytes == NULL || runtime->sequence_bytes == NULL
            || runtime->commands == NULL || runtime->frame_pcm == NULL
            || runtime->output_samples == NULL
            || ge_audio_output_init(&runtime->music_output,
                runtime->output_samples, GE_ORIGINAL_MUSIC_RING_SAMPLES,
                GE_ORIGINAL_MUSIC_SAMPLE_RATE) != 0) {
        ge_original_music_runtime_close(runtime);
        return NULL;
    }
    runtime->output = &runtime->music_output;
    alHeapInit(&runtime->heap, runtime->heap_bytes,
            GE_MUSIC_RUNTIME_HEAP_BYTES);
    memset(&synth, 0, sizeof(synth));
    synth.maxVVoices = 0;
    synth.maxPVoices = GE_MUSIC_RUNTIME_MAX_PHYSICAL_VOICES;
    synth.maxUpdates = GE_MUSIC_RUNTIME_MAX_UPDATES;
    synth.maxFXbusses = 1;
    synth.dmaproc = (void *)ge_music_dma_new;
    synth.heap = &runtime->heap;
    synth.outputRate = GE_ORIGINAL_MUSIC_SAMPLE_RATE;
    synth.fxType = AL_FX_CUSTOM;
    synth.params = ge_music_custom_fx_params;
    alInit(&runtime->globals, &synth);
    runtime->initialized = 1;

    memset(&player, 0, sizeof(player));
    player.maxVoices = GE_MUSIC_RUNTIME_MAX_VOICES;
    player.maxEvents = GE_MUSIC_RUNTIME_MAX_EVENTS;
    player.maxChannels = GE_MUSIC_RUNTIME_MAX_CHANNELS;
    player.heap = &runtime->heap;
    {
        unsigned layer;
        for (layer = 0U; layer < GE_MUSIC_RUNTIME_LAYER_COUNT; ++layer) {
            ALCSPlayer *layer_player = ge_music_layer_player(runtime, layer);
            alCSPNew(layer_player, &player);
            /* Preserve music.c's original bank-event call and three-player
             * layout: main, X/theme and background ambience. */
            alSeqpSetBank((ALSeqPlayer *)layer_player, instrument_bank);
        }
    }
    memcpy(runtime->sequence_bytes, cseq, cseq_size);
    alCSeqNew(&runtime->sequence, runtime->sequence_bytes);
    alCSPSetSeq(&runtime->player, &runtime->sequence);
    alCSPSetVol(&runtime->player, volume);
    alCSPPlay(&runtime->player);
    ge_audio_abi_init(&runtime->abi);
    runtime->abi.direct_addresses = 1U;
    runtime->stats.last_abi_result = GE_AUDIO_ABI_OK;
    return runtime;
}

static int ge_original_music_runtime_set_layer(
        GeOriginalMusicRuntime *runtime, unsigned layer,
        const uint8_t *cseq, size_t cseq_size, int16_t volume)
{
    ALCSPlayer *player;
    ALCSeq *sequence;
    uint8_t **owned_bytes;
    uint8_t *replacement;
    GeMusicRetiredSequence *retired = NULL;
    if (runtime == NULL || !runtime->initialized
            || cseq == NULL || cseq_size < 68U
            || layer >= GE_MUSIC_RUNTIME_LAYER_COUNT)
        return 0;
    replacement = malloc(cseq_size);
    if (replacement == NULL) return 0;
    owned_bytes = ge_music_layer_bytes(runtime, layer);
    if (*owned_bytes != NULL) {
        retired = malloc(sizeof(*retired));
        if (retired == NULL) {
            free(replacement);
            return 0;
        }
    }
    memcpy(replacement, cseq, cseq_size);
    player = ge_music_layer_player(runtime, layer);
    sequence = ge_music_layer_sequence(runtime, layer);
    (void)ge_original_music_runtime_finish(runtime);
    alCSPStop(player);
    if (retired != NULL) {
        /* Stop is delivered through libaudio's event queue. Keep the former
         * CSeq backing alive until the shared synth is closed so an in-flight
         * voice can finish consuming it safely. */
        retired->bytes = *owned_bytes;
        retired->next = runtime->retired_sequences;
        runtime->retired_sequences = retired;
    }
    *owned_bytes = replacement;
    alCSeqNew(sequence, replacement);
    alCSPSetSeq(player, sequence);
    alCSPSetVol(player, volume);
    alCSPPlay(player);
    return 1;
}

static uint8_t *ge_music_read_asset(GeAssetPack *pack, const char *path,
        size_t *size_out)
{
    const GeAssetPackEntry *entry;
    uint8_t *bytes;
    size_t read = 0U;
    if (pack == NULL || path == NULL || size_out == NULL
            || (entry = ge_asset_pack_find(pack, path)) == NULL
            || entry->data_size == 0U || entry->data_size > SIZE_MAX) {
        return NULL;
    }
    bytes = malloc((size_t)entry->data_size);
    if (bytes == NULL || ge_asset_pack_read(pack, path, bytes,
            (size_t)entry->data_size, &read) != GE_ASSET_PACK_OK
            || read != (size_t)entry->data_size) {
        free(bytes);
        return NULL;
    }
    *size_out = read;
    return bytes;
}

GeOriginalMusicRuntime *ge_original_music_runtime_open_asset_pack(
        GeAssetPack *pack, const char *cseq_path,
        int16_t volume, GeAudioOutput *output)
{
    uint8_t *ctl = NULL, *tbl = NULL, *cseq = NULL;
    size_t ctl_size = 0U, tbl_size = 0U, cseq_size = 0U;
    GeOriginalMusicBank *bank = NULL;
    GeOriginalMusicRuntime *runtime = NULL;
    ctl = ge_music_read_asset(pack, "music/instruments.ctl", &ctl_size);
    tbl = ge_music_read_asset(pack, "music/instruments.tbl", &tbl_size);
    cseq = ge_music_read_asset(pack, cseq_path, &cseq_size);
    if (ctl == NULL || tbl == NULL || cseq == NULL) goto fail;
    bank = ge_original_music_bank_open(ctl, ctl_size, tbl, tbl_size, 0U);
    if (bank == NULL) goto fail;
    runtime = ge_original_music_runtime_open(
            ge_original_music_bank_native(bank), cseq, cseq_size,
            volume, output);
    if (runtime == NULL) goto fail;
    runtime->owned_bank = bank;
    runtime->owned_ctl = ctl;
    runtime->owned_tbl = tbl;
    free(cseq);
    return runtime;
fail:
    ge_original_music_bank_close(bank);
    free(cseq);
    free(tbl);
    free(ctl);
    return NULL;
}

int ge_original_music_runtime_set_layer_asset_pack(
        GeOriginalMusicRuntime *runtime, GeAssetPack *pack,
        unsigned layer, const char *cseq_path, int16_t volume)
{
    uint8_t *cseq;
    size_t cseq_size = 0U;
    int result;
    cseq = ge_music_read_asset(pack, cseq_path, &cseq_size);
    if (cseq == NULL) return 0;
    result = ge_original_music_runtime_set_layer(
        runtime, layer, cseq, cseq_size, volume);
    free(cseq);
    return result;
}

int ge_original_music_runtime_set_layer_volume(
        GeOriginalMusicRuntime *runtime, unsigned layer, int16_t volume)
{
    ALCSPlayer *player = ge_music_layer_player(runtime, layer);
    if (player == NULL || !runtime->initialized) return 0;
    (void)ge_original_music_runtime_finish(runtime);
    alCSPSetVol(player, volume);
    return 1;
}

void ge_original_music_runtime_stop_layer(
        GeOriginalMusicRuntime *runtime, unsigned layer)
{
    ALCSPlayer *player = ge_music_layer_player(runtime, layer);
    if (player != NULL && runtime->initialized) {
        (void)ge_original_music_runtime_finish(runtime);
        alCSPStop(player);
    }
}

void ge_original_music_runtime_close(GeOriginalMusicRuntime *runtime)
{
    GeMusicRetiredSequence *retired;
    if (runtime == NULL) return;
    (void)ge_original_music_runtime_finish(runtime);
    if (runtime->initialized) {
        unsigned layer;
        for (layer = 0U; layer < GE_MUSIC_RUNTIME_LAYER_COUNT; ++layer)
            alCSPStop(ge_music_layer_player(runtime, layer));
        alClose(&runtime->globals);
    }
    ge_original_music_bank_close(runtime->owned_bank);
    free(runtime->owned_tbl);
    free(runtime->owned_ctl);
    free(runtime->frame_pcm);
    free(runtime->output_samples);
    free(runtime->commands);
    free(runtime->sequence_bytes);
    free(runtime->extra_sequence_bytes[0]);
    free(runtime->extra_sequence_bytes[1]);
    retired = runtime->retired_sequences;
    while (retired != NULL) {
        GeMusicRetiredSequence *next = retired->next;
        free(retired->bytes);
        free(retired);
        retired = next;
    }
    free(runtime->heap_bytes);
    free(runtime);
}

GeAudioOutput *ge_original_music_runtime_output(
        GeOriginalMusicRuntime *runtime)
{
    return runtime == NULL ? NULL : &runtime->music_output;
}

static void ge_music_execute_pending(void *context)
{
    GeOriginalMusicRuntime *runtime = context;
    runtime->pending_result = ge_audio_abi_execute_and_queue(&runtime->abi,
            (const GeAudioAbiCommand *)runtime->commands,
            runtime->pending_command_count, ge_music_resolve, NULL,
            (uint32_t)(uintptr_t)runtime->frame_pcm,
            runtime->pending_samples, runtime->output);
}

GeAudioAbiResult ge_original_music_runtime_finish(GeOriginalMusicRuntime *runtime)
{
    if (runtime == NULL) return GE_AUDIO_ABI_INVALID_ARGUMENT;
    if (!runtime->pending) return GE_AUDIO_ABI_OK;
#if defined(GE_PLATFORM_3DS)
    ge_3ds_music_worker_wait();
#endif
    GeAudioAbiResult result = runtime->pending_result;
    runtime->stats.last_command_count = runtime->pending_command_count;
    runtime->stats.generated_commands += runtime->pending_command_count;
    runtime->stats.last_abi_result = result;
    runtime->stats.player_state = alCSPGetState(&runtime->player);
    if (result == GE_AUDIO_ABI_OK) {
        runtime->stats.rendered_frames++;
        runtime->stats.rendered_samples += runtime->pending_samples;
    }
    runtime->pending = 0;
    return result;
}

static GeAudioAbiResult ge_music_begin_render(
        GeOriginalMusicRuntime *runtime, size_t sample_count, int asynchronous)
{
    s32 command_count = 0;
    if (runtime == NULL || !runtime->initialized || sample_count == 0U
            || sample_count > GE_MUSIC_RUNTIME_MAX_SAMPLES
            || (sample_count & 15U) != 0U) return GE_AUDIO_ABI_INVALID_ARGUMENT;
    GeAudioAbiResult previous = ge_original_music_runtime_finish(runtime);
    if (previous != GE_AUDIO_ABI_OK) return previous;
    if (ge_audio_output_free(runtime->output) < sample_count)
        return GE_AUDIO_ABI_OUTPUT_FULL;
    memset(runtime->frame_pcm, 0, sample_count * 2U * sizeof(int16_t));
    alAudioFrame(runtime->commands, &command_count, runtime->frame_pcm,
            (s32)sample_count);
    if (command_count < 0
            || (size_t)command_count > GE_MUSIC_RUNTIME_MAX_COMMANDS) {
        runtime->stats.last_abi_result = GE_AUDIO_ABI_INVALID_ARGUMENT;
        return GE_AUDIO_ABI_INVALID_ARGUMENT;
    }
    runtime->pending_samples = sample_count;
    runtime->pending_command_count = (size_t)command_count;
    runtime->pending = 1;
#if defined(GE_PLATFORM_3DS)
    if (asynchronous && ge_3ds_music_worker_submit(
            ge_music_execute_pending, runtime)) return GE_AUDIO_ABI_OK;
#else
    (void)asynchronous;
#endif
    ge_music_execute_pending(runtime);
    return GE_AUDIO_ABI_OK;
}

GeAudioAbiResult ge_original_music_runtime_render(
        GeOriginalMusicRuntime *runtime, size_t sample_count)
{
    GeAudioAbiResult result = ge_music_begin_render(runtime, sample_count, 0);
    return result == GE_AUDIO_ABI_OK
        ? ge_original_music_runtime_finish(runtime) : result;
}

GeAudioAbiResult ge_original_music_runtime_begin_tick_60hz(
        GeOriginalMusicRuntime *runtime)
{
    if (runtime == NULL) return GE_AUDIO_ABI_INVALID_ARGUMENT;
    GeAudioAbiResult previous = ge_original_music_runtime_finish(runtime);
    if (previous != GE_AUDIO_ABI_OK) return previous;
    uint8_t phase = runtime->retrace_phase++;
    if ((phase & 1U) != 0U) return GE_AUDIO_ABI_OK;
    return ge_music_begin_render(runtime, GE_ORIGINAL_MUSIC_FRAME_SAMPLES, 1);
}

GeAudioAbiResult ge_original_music_runtime_tick_60hz(
        GeOriginalMusicRuntime *runtime)
{
    if (runtime == NULL) return GE_AUDIO_ABI_INVALID_ARGUMENT;
    uint8_t phase = runtime->retrace_phase++;
    if ((phase & 1U) != 0U) return ge_original_music_runtime_finish(runtime);
    return ge_original_music_runtime_render(runtime, GE_ORIGINAL_MUSIC_FRAME_SAMPLES);
}

void ge_original_music_runtime_stats(
        const GeOriginalMusicRuntime *runtime,
        GeOriginalMusicRuntimeStats *stats)
{
    if (stats == NULL) return;
    if (runtime == NULL) memset(stats, 0, sizeof(*stats));
    else *stats = runtime->stats;
}

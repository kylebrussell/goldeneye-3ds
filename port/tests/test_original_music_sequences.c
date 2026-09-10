#include "ge_original_music_runtime.h"

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Offline service boundary: synthesis is serial; no interrupts or audio device. */
uint32_t osVirtualToPhysical(void *pointer) { return (uint32_t)(uintptr_t)pointer; }
uint32_t osSetIntMask(uint32_t mask) { return mask; }
void osSyncPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

typedef struct SequenceEvent {
    unsigned frame, layer;
    int volume;
    char operation[16], asset[256];
} SequenceEvent;

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeAudioOutput output;
    GeOriginalMusicRuntime *runtime;
    GeOriginalMusicRuntimeStats stats;
    int16_t storage[4096 * 2], pcm[736 * 2];
    FILE *dump;
    unsigned frames, tail;
    int peak = 0, tail_peak = 0;
    uint64_t nonzero = 0, clipped = 0;
    SequenceEvent events[128];
    unsigned event_count = 0, next_event = 0;
    assert(argc == 6 || argc == 7);
    assert(sizeof(uintptr_t) == 4);
    frames = (unsigned)strtoul(argv[3], NULL, 10);
    tail = (unsigned)strtoul(argv[4], NULL, 10);
    assert(frames > 0 && frames <= 60000 && tail >= 30 && tail <= 60000);
    if (argc == 7) {
        FILE *schedule = fopen(argv[6], "r");
        assert(schedule != NULL);
        while (event_count < 128) {
            SequenceEvent *event = &events[event_count];
            int fields = fscanf(schedule, "%u %15s %u %d %255s", &event->frame,
                event->operation, &event->layer, &event->volume, event->asset);
            if (fields == EOF) break;
            assert(fields == 5 && event->layer < 3 && event->frame < frames);
            assert(event->volume >= 0 && event->volume <= 32767);
            assert(event_count == 0 || event->frame >= events[event_count - 1].frame);
            ++event_count;
        }
        assert(feof(schedule));
        assert(fclose(schedule) == 0);
    }
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    assert(ge_audio_output_init(&output, storage, 4096, 22050) == 0);
    runtime = ge_original_music_runtime_open_asset_pack(&pack, argv[2], 26212, &output);
    assert(runtime != NULL);
    dump = fopen(argv[5], "wb");
    assert(dump != NULL);
    for (unsigned frame = 0; frame < frames + tail; ++frame) {
        while (next_event < event_count && events[next_event].frame == frame) {
            const SequenceEvent *event = &events[next_event++];
            if (strcmp(event->operation, "set") == 0) {
                assert(ge_original_music_runtime_set_layer_asset_pack(runtime, &pack,
                    event->layer, event->asset, (int16_t)event->volume));
            } else if (strcmp(event->operation, "volume") == 0) {
                assert(ge_original_music_runtime_set_layer_volume(runtime, event->layer,
                    (int16_t)event->volume));
            } else {
                assert(strcmp(event->operation, "stop") == 0);
                ge_original_music_runtime_stop_layer(runtime, event->layer);
            }
        }
        if (frame == frames) {
            for (unsigned layer = 0; layer < 3; ++layer)
                ge_original_music_runtime_stop_layer(runtime, layer);
        }
        assert(ge_original_music_runtime_render(runtime, 736) == GE_AUDIO_ABI_OK);
        assert(ge_audio_output_read(ge_original_music_runtime_output(runtime), pcm, 736) == 736);
        for (size_t sample = 0; sample < 736 * 2; ++sample) {
            int value = pcm[sample], magnitude = value < 0 ? -value : value;
            uint16_t bits = (uint16_t)pcm[sample];
            assert(fputc(bits & 255, dump) != EOF);
            assert(fputc(bits >> 8, dump) != EOF);
            if (magnitude > peak) peak = magnitude;
            if (frame >= frames + tail - 30 && magnitude > tail_peak) tail_peak = magnitude;
            nonzero += value != 0;
            clipped += value == -32768 || value == 32767;
        }
    }
    assert(fclose(dump) == 0);
    ge_original_music_runtime_stats(runtime, &stats);
    assert(stats.rendered_frames == frames + tail);
    assert(stats.rendered_samples == (uint64_t)(frames + tail) * 736);
    assert(stats.player_state == 0);
    assert(next_event == event_count);
    assert(ge_original_music_runtime_output(runtime)->frames_dropped == 0);
    assert(ge_original_music_runtime_output(runtime)->frames_underflowed == 0);
    printf("{\"sequence\":\"%s\",\"rendered_frames\":%" PRIu64 ",\"commands\":%" PRIu64
        ",\"peak\":%d,\"last_second_peak\":%d,\"nonzero\":%" PRIu64 ",\"clipped\":%" PRIu64 "}\n",
        argv[2], stats.rendered_frames, stats.generated_commands, peak, tail_peak, nonzero, clipped);
    ge_original_music_runtime_close(runtime);
    ge_asset_pack_close(&pack);
    /* Detect the demonstrated high-amplitude self-oscillation, allowing the
     * small fixed-point remainder of the complete six-section reverb. This
     * numerical stop gate does not establish subjective listening quality. */
    assert(tail_peak <= 64);
    return 0;
}

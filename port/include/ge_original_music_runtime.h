#ifndef GE_ORIGINAL_MUSIC_RUNTIME_H
#define GE_ORIGINAL_MUSIC_RUNTIME_H

#include "ge_audio_abi.h"
#include "ge_asset_pack.h"

#include <stddef.h>
#include <stdint.h>

typedef struct ALBank_s ALBank;

#define GE_ORIGINAL_MUSIC_SAMPLE_RATE 22050U
#define GE_ORIGINAL_MUSIC_FRAME_SAMPLES 736U
#define GE_ORIGINAL_MUSIC_RING_SAMPLES 4096U

typedef struct GeOriginalMusicRuntime GeOriginalMusicRuntime;

typedef struct GeOriginalMusicRuntimeStats {
    uint64_t rendered_frames;
    uint64_t rendered_samples;
    uint64_t generated_commands;
    size_t last_command_count;
    GeAudioAbiResult last_abi_result;
    int player_state;
} GeOriginalMusicRuntimeStats;

GeOriginalMusicRuntime *ge_original_music_runtime_open(
        ALBank *instrument_bank,
        const uint8_t *cseq, size_t cseq_size,
        int16_t volume,
        /* Primary platform/SFX output; used to negotiate exact 22050 Hz. */
        GeAudioOutput *output);
GeOriginalMusicRuntime *ge_original_music_runtime_open_asset_pack(
        GeAssetPack *pack, const char *cseq_path,
        int16_t volume, GeAudioOutput *output);
int ge_original_music_runtime_set_layer_asset_pack(
        GeOriginalMusicRuntime *runtime, GeAssetPack *pack,
        unsigned layer, const char *cseq_path, int16_t volume);
int ge_original_music_runtime_set_layer_volume(
        GeOriginalMusicRuntime *runtime, unsigned layer, int16_t volume);
void ge_original_music_runtime_stop_layer(
        GeOriginalMusicRuntime *runtime, unsigned layer);
void ge_original_music_runtime_close(GeOriginalMusicRuntime *runtime);
/* Separate music PCM ring. Bind this as the platform sink's secondary mix. */
GeAudioOutput *ge_original_music_runtime_output(
        GeOriginalMusicRuntime *runtime);

/* Generate one canonical libaudio frame, execute its ABI1 command list on
 * the CPU, and append the resulting interleaved PCM to the shared output. */
GeAudioAbiResult ge_original_music_runtime_render(
        GeOriginalMusicRuntime *runtime, size_t sample_count);
/* GoldenEye's scheduler sends the audio client every even 60 Hz retrace. */
GeAudioAbiResult ge_original_music_runtime_tick_60hz(
        GeOriginalMusicRuntime *runtime);
void ge_original_music_runtime_stats(
        const GeOriginalMusicRuntime *runtime,
        GeOriginalMusicRuntimeStats *stats);

#endif

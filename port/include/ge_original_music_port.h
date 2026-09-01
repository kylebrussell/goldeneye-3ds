#ifndef GE_ORIGINAL_MUSIC_PORT_H
#define GE_ORIGINAL_MUSIC_PORT_H

#include <stdint.h>
#include <stddef.h>

typedef struct GeOriginalMusicPortSnapshot {
    uint64_t unavailable_play_requests;
    int32_t last_layer;
    int32_t last_track;
    uint64_t layer_generation[3];
    int32_t layer_track[3];
    uint16_t layer_volume[3];
    uint8_t layer_fading[3];
    uint32_t instrument_count;
    uint32_t sound_count;
    uint32_t wavetable_count;
    uint8_t instrument_bank_ready;
} GeOriginalMusicPortSnapshot;

int ge_original_music_port_bind_instrument_bank(
        const uint8_t *ctl, size_t ctl_size,
        const uint8_t *tbl, size_t tbl_size);
void ge_original_music_port_unbind_instrument_bank(void);
/* Mirror the original scheduler's once-per-retrace music fade service. */
void ge_original_music_port_tick(void);
void ge_original_music_port_snapshot(GeOriginalMusicPortSnapshot *snapshot);

#endif

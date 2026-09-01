/* Compile the complete canonical GoldenEye music engine for the native port.
 * The SFX bank pointer is already owned by the exact relocated 3DS SFX-bank
 * loader; rename only music.c's otherwise-duplicate storage definition. */
#include "ge_original_music_port.h"
#include "ge_original_music_bank.h"

#include <string.h>

static GeOriginalMusicPortSnapshot ge_original_music_port_state;
static GeOriginalMusicBank *ge_original_music_port_bank;

static void ge_original_music_port_unavailable(int32_t layer, int32_t track)
{
    ++ge_original_music_port_state.unavailable_play_requests;
    ge_original_music_port_state.last_layer = layer;
    ge_original_music_port_state.last_track = track;
}

#define g_musicSfxBufferPtr ge_original_music_source_unused_sfx_bank
#define GE_PORT_MUSIC_NULL_GUARD 1
#define GE_PORT_MUSIC_NULL_GUARD_NOTIFY(layer, track) \
    ge_original_music_port_unavailable((layer), (track))
#include "../../src/music.c"

int ge_original_music_port_bind_instrument_bank(
        const uint8_t *ctl, size_t ctl_size,
        const uint8_t *tbl, size_t tbl_size)
{
    GeOriginalMusicBank *owner;
    GeOriginalMusicBankStats stats;

    owner = ge_original_music_bank_open(ctl, ctl_size, tbl, tbl_size, 0U);
    if (owner == NULL) return 0;
    ge_original_music_bank_stats(owner, &stats);
    ge_original_music_bank_close(ge_original_music_port_bank);
    ge_original_music_port_bank = owner;
    g_musicInstrumentBufferPtr = ge_original_music_bank_native(owner);
    ge_original_music_port_state.instrument_count = stats.instruments;
    ge_original_music_port_state.sound_count = stats.sounds;
    ge_original_music_port_state.wavetable_count = stats.wavetables;
    ge_original_music_port_state.instrument_bank_ready = 1U;
    return 1;
}

void ge_original_music_port_unbind_instrument_bank(void)
{
    g_musicInstrumentBufferPtr = NULL;
    ge_original_music_bank_close(ge_original_music_port_bank);
    ge_original_music_port_bank = NULL;
    ge_original_music_port_state.instrument_count = 0U;
    ge_original_music_port_state.sound_count = 0U;
    ge_original_music_port_state.wavetable_count = 0U;
    ge_original_music_port_state.instrument_bank_ready = 0U;
}

void ge_original_music_port_snapshot(GeOriginalMusicPortSnapshot *snapshot)
{
    if (snapshot != NULL) *snapshot = ge_original_music_port_state;
}

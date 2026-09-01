#include "ge_original_gameplay_services.h"

#include "ge_original_covert_modem_fire.h"
#include "ge_original_pp7_fire.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <snd.h>
#include "game/file2.h"
#include "game/options.h"
#include "ge_original_bond_input_provider.h"

extern ALBank *g_musicSfxBufferPtr;

static int32_t played_music = -1;
static int32_t loaded_model = -1;
static int32_t restored_mission_state = -1;
static GeOriginalBondInputProvider input_provider;
static uint8_t persisted_music;
static uint8_t persisted_sfx;
static uint16_t persisted_options;
static unsigned persist_calls;
s32 dword_CODE_bss_8008C604;

extern s32 modelLoad(s32 modelid);
extern void sub_GAME_7F0C1340(void);
extern void deleteCurrentSelectedFolder(void);

GeOriginalBondInputProvider *ge_original_bond_input_provider(void)
{
    return &input_provider;
}

u16 get_mTrack2Vol(void) { return UINT16_C(0x2a80); }
u16 call_sndGetSfxSlotFirstNaturalVolume(void) { return UINT16_C(0x3300); }
s32 cur_player_get_autoaim(void) { return 1; }
u32 cur_player_get_sight_onscreen_control(void) { return 1; }
u32 cur_player_get_lookahead(void) { return 0; }
u32 cur_player_get_ammo_onscreen_setting(void) { return 1; }
u32 cur_player_get_screen_setting(void) { return SCREEN_SIZE_CINEMA; }
SCREEN_RATIO_OPTION get_screen_ratio(void) { return SCREEN_RATIO_16_9; }

static int persist_settings(void *context, uint8_t music, uint8_t sfx,
                            uint16_t options)
{
    assert(context == &persist_calls);
    ++persist_calls;
    persisted_music = music;
    persisted_sfx = sfx;
    persisted_options = options;
    return 1;
}

void set_missionstate(MISSION_STATE_ID state)
{
    restored_mission_state = state;
}

void musicTrack1Play(s32 track)
{
    played_music = track;
}

int32_t ge_original_bug_model_id(void) { return -2; }
int ge_original_bug_model_prepare(void) { return 0; }
int ge_original_guard_grenade_model_prepare(void) { return 0; }

static int32_t test_model_load(void *context, int32_t model_id)
{
    assert(context == &loaded_model);
    loaded_model = model_id;
    return 1;
}

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    unsigned char *bytes;
    long length;

    assert(stream != NULL);
    assert(fseek(stream, 0L, SEEK_END) == 0);
    length = ftell(stream);
    assert(length > 0L);
    assert(fseek(stream, 0L, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes != NULL);
    assert(fread(bytes, 1U, (size_t)length, stream) == (size_t)length);
    assert(fclose(stream) == 0);
    *size = (size_t)length;
    return bytes;
}

/* The focused binary retains only the gameplay service's audio/reset/tick
 * sections. These are the exact subsystem reset/tick boundaries which are
 * independently covered by their own integration tests. */
void ge_original_covert_modem_object_reset(void) {}
void ge_original_guard_grenade_object_reset(void) {}
void ge_original_covert_modem_projectile_reset(void) {}
void ge_original_covert_modem_fire_reset(void) {}
void ge_original_pp7_fire_reset(void) {}

GeOriginalCovertModemFireStatus ge_original_covert_modem_fire_tick(void)
{
    return GE_ORIGINAL_COVERT_MODEM_FIRE_IDLE;
}

GeOriginalPp7FireStatus ge_original_pp7_fire_tick(void)
{
    return GE_ORIGINAL_PP7_FIRE_IDLE;
}

int main(int argc, char **argv)
{
    unsigned char *control;
    unsigned char *samples;
    size_t control_size;
    size_t samples_size;
    GeOriginalSfxBank bank;
    GeAudioOutput output;
    int16_t ring[2048];
    ALSoundState *owner = NULL;
    ALSoundState *first;
    ALSoundState *second;
    GeOriginalGameplayServiceStats stats;
    unsigned tick;

    assert(argc == 3);
    control = read_file(argv[1], &control_size);
    samples = read_file(argv[2], &samples_size);
    assert(ge_original_sfx_bank_init(&bank,
        control, control_size, samples, samples_size)
        == GE_ORIGINAL_SFX_BANK_OK);
    assert(ge_audio_output_init(&output, ring, 1024U, 32000U) == 0);

    ge_original_gameplay_services_reset();
    memset(&input_provider, 0, sizeof(input_provider));
    input_provider.look_vertical_inverted = 1;
    input_provider.aim_control = 1;
    input_provider.control_type = 5;
    ge_original_gameplay_services_bind_model_loader(
        &loaded_model, test_model_load);
    assert(modelLoad(123) == 1 && loaded_model == 123);
    ge_original_gameplay_services_bind_audio(&bank, &output);
    ge_original_gameplay_services_play_music(M_FOLDERS);
    assert(played_music == M_FOLDERS);

    /* The exact watch-open owner saves the pre-watch mission state in this
     * shared word. Watch close must restore that value and must not count an
     * otherwise-authentic music transition as an unsupported service. */
    dword_CODE_bss_8008C604 = MISSION_STATE_5;
    sub_GAME_7F0C1340();
    assert(restored_mission_state == MISSION_STATE_5);

    /* Normal watch close calls the canonical fileClearSavefileForFolder
     * wrapper. Preserve exact fileSaveSettingsForFolder composition and send
     * only its three fields to the durable platform owner. */
    ge_original_gameplay_services_bind_settings_persistence(
        &persist_calls, persist_settings);
    deleteCurrentSelectedFolder();
    assert(persist_calls == 1U);
    assert(persisted_music == 0x55U && persisted_sfx == 0x66U);
    assert(persisted_options == (OPTION_INVERTLOOK | OPTION_AUTOAIM
        | OPTION_AIMCONTROL | OPTION_SIGHTONSCREEN | OPTION_DISPLAYAMMO
        | OPTION_SCREENCINEMA | OPTION_SCREENRATIO | 0x0500U));

    /* The authored Dam silenced PP7 sound is SFX 0x2e. Canonical gun code
     * passes &hand->audioHandle through the ALSoundState pending-link ABI. */
    first = sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr,
                       0x2e, (ALSoundState *)&owner);
    assert(first != NULL && owner == first);
    for (tick = 0U; tick < 600U && owner != NULL; tick++) {
        ge_original_gameplay_services_tick();
        (void)ge_audio_output_discard(
            &output, ge_audio_output_queued(&output));
    }
    assert(owner == NULL);
    assert(sndGetPlayingState(first) == SOUND_STATE_NONE);

    /* A completed first shot must not strand both canonical hand sound slots.
     * The second shot reuses the same owner cleanly and deactivate clears it. */
    second = sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr,
                        0x2e, (ALSoundState *)&owner);
    assert(second != NULL && owner == second);
    sndDeactivate(second);
    assert(owner == NULL);
    assert(sndGetPlayingState(second) == SOUND_STATE_NONE);

    ge_original_gameplay_services_snapshot(&stats);
    assert(stats.sound_play_calls == 2U);
    assert(stats.decoded_sound_starts == 2U);
    assert(stats.sound_decode_failures == 0U);
    assert(stats.sound_deactivate_calls == 1U);
    assert(stats.mixed_audio_frames != 0U);
    assert(stats.music_play_calls == 1U);
    assert(stats.last_music_id == M_FOLDERS);
    assert(stats.unsupported_object_calls == 0U);
    assert(stats.settings_persist_calls == 1U);
    assert(stats.settings_persist_failures == 0U);
    assert(output.frames_discarded != 0U);

    /* An unavailable durable owner remains an explicit frontier; watch close
     * must never claim that settings were saved when no stage slot is bound. */
    ge_original_gameplay_services_bind_settings_persistence(NULL, NULL);
    deleteCurrentSelectedFolder();
    ge_original_gameplay_services_snapshot(&stats);
    assert(stats.settings_persist_calls == 2U);
    assert(stats.settings_persist_failures == 1U);
    assert(stats.unsupported_object_calls == 1U);

    ge_original_gameplay_services_bind_audio(NULL, NULL);
    ge_original_gameplay_services_bind_model_loader(NULL, NULL);
    ge_original_gameplay_services_reset();
    free(samples);
    free(control);
    puts("canonical SFX owner lifecycle and sinkless decode clock: ok");
    return 0;
}

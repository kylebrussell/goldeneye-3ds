#include <limits.h>
#include <stdint.h>
#include <string.h>

#include <bondtypes.h>
#include <snd.h>

uint32_t ge_test_mission_sound_starts;
int16_t ge_test_mission_last_sound;

ALBank *g_musicSfxBufferPtr;
s32 g_ClockTimer = 1;
static ALSoundState ge_test_mission_sound;

ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 sound_index,
                         ALSoundState *pending_state)
{
    (void)bank;
    memset(&ge_test_mission_sound, 0, sizeof(ge_test_mission_sound));
    ge_test_mission_sound.playingState = SOUND_STATE_PLAYING;
    ge_test_mission_sound.vol = INT16_MAX;
    if (pending_state != NULL)
        pending_state->link.next = (ALLink *)&ge_test_mission_sound;
    ge_test_mission_sound_starts++;
    ge_test_mission_last_sound = sound_index;
    return &ge_test_mission_sound;
}

u8 sndGetPlayingState(ALSoundState *state)
{
    return state != NULL ? state->playingState : SOUND_STATE_NONE;
}

void sndDeactivate(ALSoundState *state)
{
    if (state != NULL) state->playingState = SOUND_STATE_NONE;
}

void sndCreatePostEvent(ALSoundState *state, s16 event_type, s32 value)
{
    (void)state;
    (void)event_type;
    (void)value;
}

s32 lvlGetControlsLockedFlag(void)
{
    return 0;
}

s32 sub_GAME_7F0539E4(coord3d *position)
{
    (void)position;
    return INT16_MAX;
}

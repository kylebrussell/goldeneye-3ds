/* Actual native pre-tick and music bodies; effects, binding validation,
 * background AI and regeneration are explicit resource/producer seams. */
#include "ge_original_multiplayer_runtime.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <music.h>
#include <random.h>
#include "game/mp_music.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

s32 g_ClockTimer;
f32 g_GlobalTimerDelta;
s32 g_musicXTrack1Fade, g_musicXTrack2Fade, g_musicXTrack3Fade;
extern s32 stageMusicID, mission_state, dword_CODE_bss_8008C604;
extern s32 g_CurrentStageToLoad;
extern s32 music_slot_active_0[4], music_slot_minutes_0[4], music_slot_seconds_0[4];
static unsigned phase, ai_calls, audio_calls, shuffle_calls;
static int fail_effects, fail_binding, fail_regen;
static int expected_minimum, expected_duration, expected_mission;
static int before_regen_minimum, before_regen_duration;

u16 get_mTrack2Vol(void) { return 0x6000; }
u16 call_sndGetSfxSlotFirstNaturalVolume(void) { return 0x4000; }
#define AUDIO(n) \
void musicTrack##n##Play(s32 track) { (void)track; ++audio_calls; } \
void musicTrack##n##Stop(void) { ++audio_calls; } \
void musicTrack##n##ApplySeqpVol(u16 volume) { (void)volume; ++audio_calls; } \
void musicTrack##n##FadeOut(f32 time) { (void)time; ++audio_calls; } \
void musicTrack##n##FadeIn(f32 time, u16 volume) { (void)time; (void)volume; ++audio_calls; }
AUDIO(1) AUDIO(2) AUDIO(3)
#undef AUDIO

int ge_original_effect_runtime_pre_world(void)
{ assert(phase == 0); phase = 1; return !fail_effects; }
GeOriginalStageActivePropStatus fixture_validate_live_binding(
    GeOriginalStageActiveProps *state, int pre)
{
    assert(state && pre == 1 && phase == 1); phase = 2;
    return fail_binding ? GE_ORIGINAL_STAGE_ACTIVE_PROP_NOT_BOUND : GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
}
void ge_original_dam_guard_all_chr_tick_exact(void)
{
    assert(phase == 2); ++ai_calls;
    /* A same-pass native AI music command must be consumed by this pass's
     * countdown; it must not wait for a later frame or run before this call. */
    musicPlaySlot(0, 2, 3);
    before_regen_minimum = music_slot_minutes_0[0];
    before_regen_duration = music_slot_seconds_0[0];
}
GeOriginalMultiplayerRegenStatus ge_original_multiplayer_regen_tick_all(
    int32_t tick, GeOriginalMultiplayerRegenBatch *batch)
{
    assert(batch && tick == g_ClockTimer && phase == 2); phase = 3;
    assert(music_slot_minutes_0[0] == before_regen_minimum);
    assert(music_slot_seconds_0[0] == before_regen_duration);
    return fail_regen ? GE_ORIGINAL_MP_REGEN_INVALID_ARGUMENT : GE_ORIGINAL_MP_REGEN_OK;
}
void shuffle_player_ids(void)
{
    assert(phase == 3); phase = 4; ++shuffle_calls;
    assert(music_slot_minutes_0[0] == expected_minimum);
    assert(music_slot_seconds_0[0] == expected_duration);
    assert(mission_state == expected_mission);
}

static void run(GeOriginalStageActiveProps *active, int minimum, int duration,
    int mission, GeOriginalMultiplayerRuntimeStatus status)
{
    GeOriginalMultiplayerRegenBatch batch = {0};
    uint64_t rng = g_randomSeed;
    s32 clock = g_ClockTimer;
    expected_minimum = minimum; expected_duration = duration; expected_mission = mission;
    before_regen_minimum = music_slot_minutes_0[0];
    before_regen_duration = music_slot_seconds_0[0];
    phase = 0;
    assert(ge_original_multiplayer_runtime_begin_world(active, g_ClockTimer, &batch) == status);
    assert(g_randomSeed == rng && dword_CODE_bss_8008C604 == MISSION_STATE_4);
    assert(g_ClockTimer == clock);
}

int main(void)
{
    GeOriginalStageActiveProps active = {0};
    /* Set up a valid source mission without consuming stage-selection RNG.
     * This fixture targets the world boundary, not stage initialization. */
    stageMusicID = LEVELID_CAVERNS; mission_state = MISSION_STATE_1;
    dword_CODE_bss_8008C604 = MISSION_STATE_4;
    g_ClockTimer = 1; g_GlobalTimerDelta = 1;
    run(&active, 119, 179, MISSION_STATE_2, GE_ORIGINAL_MP_RUNTIME_OK);
    assert(ai_calls == 1 && active.pre_ticks == 1 && active.pre_tick_pending);
    unsigned audio = audio_calls;
    /* Pending background AI is not repeated, but the source countdown is. */
    run(&active, 118, 178, MISSION_STATE_2, GE_ORIGINAL_MP_RUNTIME_OK);
    assert(ai_calls == 1 && active.pre_ticks == 1 && audio_calls == audio);
    /* The native zero-clock path preserves timers and suppresses transition. */
    g_ClockTimer = 0;
    run(&active, 118, 178, MISSION_STATE_2, GE_ORIGINAL_MP_RUNTIME_OK);
    assert(!active.pre_tick_pending && ai_calls == 1 && audio_calls == audio);
    /* Zero global delta suppresses AI, not the source's g_ClockTimer consumer. */
    g_ClockTimer = 3; g_GlobalTimerDelta = 0;
    run(&active, 115, 175, MISSION_STATE_2, GE_ORIGINAL_MP_RUNTIME_OK);
    assert(ai_calls == 1 && audio_calls == audio);
    music_slot_minutes_0[0] = 1; music_slot_seconds_0[0] = 2;
    run(&active, 0, 0, MISSION_STATE_1, GE_ORIGINAL_MP_RUNTIME_OK);
    assert(!music_slot_active_0[0] && audio_calls > audio);
    /* Failed earlier owners must not advance countdown or shuffle. */
    musicPlaySlot(0, 2, 3); audio = audio_calls;
    unsigned shuffled = shuffle_calls;
    fail_effects = 1;
    run(&active, 0, 0, 0, GE_ORIGINAL_MP_RUNTIME_WORLD_FAILED);
    assert(music_slot_minutes_0[0] == 120 && audio_calls == audio && shuffle_calls == shuffled);
    fail_effects = 0; fail_binding = 1;
    run(&active, 0, 0, 0, GE_ORIGINAL_MP_RUNTIME_WORLD_FAILED);
    assert(music_slot_minutes_0[0] == 120 && audio_calls == audio && shuffle_calls == shuffled);
    fail_binding = 0; fail_regen = 1;
    run(&active, 0, 0, 0, GE_ORIGINAL_MP_RUNTIME_WORLD_FAILED);
    assert(music_slot_minutes_0[0] == 120 && audio_calls == audio && shuffle_calls == shuffled && phase == 3);
    fail_regen = 0; g_CurrentStageToLoad = LEVELID_TITLE;
    run(&active, 0, 0, 0, GE_ORIGINAL_MP_RUNTIME_INVALID_ARGUMENT);
    assert(phase == 0 && music_slot_minutes_0[0] == 120 && audio_calls == audio && shuffle_calls == shuffled);
    puts("native music world: same-pass AI, pending/paused/delta-zero, expiry, failure and title-exclusion ordering passed");
    return 0;
}

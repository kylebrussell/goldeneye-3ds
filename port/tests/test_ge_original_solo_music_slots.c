/* Execute the real solo main boundary, active-prop pre-tick and native music.
 * Platform, binding validation, AI requests and audio/shuffle are test seams. */
#include "ge_original_multiplayer_runtime.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <music.h>
#include <random.h>
#include "game/mp_music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "solo music check failed: %s (line %d)\n", #condition, __LINE__); \
    exit(1); } } while (0)

extern int fixture_frame(GeOriginalStageActiveProps *, int multiplayer, int stage);
extern void fixture_native_countdown(void);
extern void sub_GAME_7F0C1310(void);
extern s32 stageMusicID, mission_state;
extern s32 music_slot_active_0[4], music_slot_minutes_0[4], music_slot_seconds_0[4];
s32 g_ClockTimer;
f32 g_GlobalTimerDelta;
s32 g_musicXTrack1Fade, g_musicXTrack2Fade, g_musicXTrack3Fade;
static unsigned ai_calls, countdown_calls, shuffle_calls, mp_calls, audio_calls;
static int emit_request, last_track[3];

u16 get_mTrack2Vol(void) { return 0x6000; }
u16 call_sndGetSfxSlotFirstNaturalVolume(void) { return 0x4000; }
#define AUDIO(n) \
void musicTrack##n##Play(s32 track) { last_track[n-1] = track; ++audio_calls; } \
void musicTrack##n##Stop(void) { ++audio_calls; } \
void musicTrack##n##ApplySeqpVol(u16 volume) { (void)volume; ++audio_calls; } \
void musicTrack##n##FadeOut(f32 time) { (void)time; ++audio_calls; } \
void musicTrack##n##FadeIn(f32 time, u16 volume) { (void)time; (void)volume; ++audio_calls; }
AUDIO(1) AUDIO(2) AUDIO(3)
#undef AUDIO

GeOriginalStageActivePropStatus fixture_validate_live_binding(
    GeOriginalStageActiveProps *state, int pre)
{
    CHECK(state && pre == 1);
    return GE_ORIGINAL_STAGE_ACTIVE_PROP_OK;
}
void ge_original_dam_guard_all_chr_tick_exact(void)
{
    ++ai_calls;
    if (emit_request) musicPlaySlot(0, 0, 1);
}
void reset_all_music_slots(void)
{
    ++countdown_calls;
    fixture_native_countdown();
}
void shuffle_player_ids(void) { ++shuffle_calls; }
uint64_t svcGetSystemTick(void) { return 0; }
void ge_dam_sky_tick(float *offset, int32_t clock) { (void)offset; (void)clock; }
GeOriginalMultiplayerRuntimeStatus ge_original_multiplayer_runtime_begin_world(
    GeOriginalStageActiveProps *active, int32_t clock,
    GeOriginalMultiplayerRegenBatch *batch)
{
    CHECK(active && batch && clock == g_ClockTimer);
    ++mp_calls;
    /* This observes exclusion of the solo owner only. The separate positive
     * multiplayer fixture executes the real MP countdown/world owner. */
    return GE_ORIGINAL_MP_RUNTIME_OK;
}

static void slots(s32 snapshot[12])
{
    memcpy(snapshot, music_slot_active_0, 4 * sizeof(s32));
    memcpy(snapshot + 4, music_slot_minutes_0, 4 * sizeof(s32));
    memcpy(snapshot + 8, music_slot_seconds_0, 4 * sizeof(s32));
}
static void setup(GeOriginalStageActiveProps *active, int stage)
{
    memset(active, 0, sizeof(*active));
    musicStopSlot(-1);
    sub_GAME_7F0C11FC(stage);
    g_ClockTimer = 1; g_GlobalTimerDelta = 1;
    ai_calls = countdown_calls = shuffle_calls = mp_calls = 0;
    emit_request = 0;
}
static void run(GeOriginalStageActiveProps *active, int multiplayer, int stage)
{
    uint64_t rng = g_randomSeed;
    s32 clock = g_ClockTimer;
    CHECK(fixture_frame(active, multiplayer, stage) == 0);
    CHECK(g_randomSeed == rng && g_ClockTimer == clock);
}

int main(void)
{
    GeOriginalStageActiveProps active;
    s32 before[12], after[12];
    randomSetSeed(0x7654321);
    setup(&active, LEVELID_DAM);
    CHECK(mission_state == MISSION_STATE_1 && last_track[0] == M_DAM);
    emit_request = 1;
    run(&active, 0, LEVELID_DAM);
    /* Missing/before-AI hook mutants must fail an observed native result. */
    CHECK(music_slot_seconds_0[0] == 59);
    CHECK(music_slot_active_0[0] && music_slot_minutes_0[0] == 0);
    CHECK(mission_state == MISSION_STATE_2 && last_track[1] == M_WIND);
    CHECK(ai_calls == 1 && active.pre_ticks == 1 && active.pre_tick_pending);
    CHECK(countdown_calls == 1 && shuffle_calls == 1);
    unsigned audio = audio_calls;
    run(&active, 0, LEVELID_DAM);
    CHECK(music_slot_seconds_0[0] == 58 && countdown_calls == 2 && shuffle_calls == 2);
    CHECK(ai_calls == 1 && active.pre_ticks == 1 && audio_calls == audio);

    /* Original watch transition and zero-clock pass preserve all slot words. */
    for (int i = 1; i < 4; ++i) musicPlaySlot(i, i, i + 1);
    sub_GAME_7F0C1310();
    CHECK(mission_state == MISSION_STATE_3 && last_track[1] == M_WATCH);
    slots(before); audio = audio_calls; g_ClockTimer = 0;
    run(&active, 0, LEVELID_DAM);
    slots(after);
    CHECK(!memcmp(before, after, sizeof(before)) && mission_state == MISSION_STATE_3);
    CHECK(countdown_calls == 3 && shuffle_calls == 3 && ai_calls == 1);
    CHECK(!active.pre_tick_pending && audio_calls == audio);
    sub_GAME_7F0C1340();
    g_ClockTimer = 1;
    run(&active, 0, LEVELID_DAM);
    CHECK(music_slot_seconds_0[0] == 57 && mission_state == MISSION_STATE_2);
    CHECK(countdown_calls == 4 && ai_calls == 2);
    /* Global delta gates AI, not the native countdown's g_ClockTimer input. */
    g_GlobalTimerDelta = 0; g_ClockTimer = 3;
    run(&active, 0, LEVELID_DAM);
    CHECK(music_slot_seconds_0[0] == 54 && countdown_calls == 5 && ai_calls == 2);

    setup(&active, LEVELID_DAM);
    musicPlaySlot(0, 2, 4);
    run(&active, 0, LEVELID_DAM);
    musicPlaySlot(0, 9, 9); /* An active slot ignores replacement requests. */
    CHECK(music_slot_minutes_0[0] == 119 && music_slot_seconds_0[0] == 239);
    musicStopSlot(0); g_ClockTimer = 60;
    run(&active, 0, LEVELID_DAM);
    CHECK(!music_slot_active_0[0] && music_slot_minutes_0[0] == 59);
    CHECK(music_slot_seconds_0[0] == 179 && mission_state == MISSION_STATE_2);
    g_ClockTimer = 59;
    run(&active, 0, LEVELID_DAM);
    CHECK(music_slot_minutes_0[0] == 0 && music_slot_seconds_0[0] == 120);
    CHECK(mission_state == MISSION_STATE_1);
    g_ClockTimer = 1;
    run(&active, 0, LEVELID_DAM);
    CHECK(music_slot_seconds_0[0] == 120); /* Native inactive contents persist. */

    setup(&active, LEVELID_SURFACE2);
    CHECK(mission_state == MISSION_STATE_4 && last_track[2] == M_WIND);
    emit_request = 1;
    run(&active, 0, LEVELID_SURFACE2);
    CHECK(mission_state == MISSION_STATE_5 && last_track[1] == M_SURFACE2END);
    CHECK(music_slot_seconds_0[0] == 59);
    g_ClockTimer = 80;
    run(&active, 0, LEVELID_SURFACE2);
    CHECK(!music_slot_active_0[0] && music_slot_seconds_0[0] == 0);
    CHECK(mission_state == MISSION_STATE_4 && last_track[2] == M_WIND);

    setup(&active, LEVELID_DAM);
    for (int i = 0; i < 4; ++i) musicPlaySlot(i, i + 1, i + 2);
    slots(before); audio = audio_calls;
    run(&active, 0, LEVELID_TITLE);
    slots(after);
    CHECK(!memcmp(before, after, sizeof(before)) && countdown_calls == 0);
    CHECK(mission_state == MISSION_STATE_1 && audio_calls == audio);
    run(&active, 1, LEVELID_DAM);
    slots(after);
    CHECK(!memcmp(before, after, sizeof(before)) && countdown_calls == 0);
    CHECK(mp_calls == 1 && shuffle_calls == 1); /* Only the preceding solo pass shuffled. */
    CHECK(mission_state == MISSION_STATE_1 && audio_calls == audio);
    puts("solo native music: same-pass AI, pending, watch/clock0, delta0, inactive timers, "
         "expiry, Dam/Surface2 transitions, title and separate MP-owner exclusion passed");
    return 0;
}

#include "ge_mp_state_music.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <music.h>
#include <random.h>
#include "game/mp_music.h"
#include "game/options.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern s32 stageMusicID, mission_state, dword_CODE_bss_8008C604;
extern s32 music_slot_active_0[4], music_slot_minutes_0[4], music_slot_seconds_0[4];
struct music_setup { s16 stage_id, main_music, bg_sound, xtrack; };
extern struct music_setup music_setup_entries[];
extern s16 random_tracks[];
void sub_GAME_7F0C1310(void);
s32 g_ClockTimer;
s32 g_musicXTrack1Fade, g_musicXTrack2Fade, g_musicXTrack3Fade;
static unsigned audio_calls;
static s32 tracks[3];
/* Resource sink only. All mission transitions, slots, timer consumers, table
 * selection and RNG are exact native source linked by the script. */
u16 get_mTrack2Vol(void) { return 0x6000; }
u16 call_sndGetSfxSlotFirstNaturalVolume(void) { return 0x4000; }
#define AUDIO(n) \
void musicTrack##n##Play(s32 track) { tracks[n-1] = track; ++audio_calls; } \
void musicTrack##n##Stop(void) { tracks[n-1] = 0; ++audio_calls; } \
void musicTrack##n##ApplySeqpVol(u16 volume) { (void)volume; ++audio_calls; } \
void musicTrack##n##FadeOut(f32 time) { (void)time; ++audio_calls; } \
void musicTrack##n##FadeIn(f32 time, u16 volume) { (void)time; (void)volume; ++audio_calls; }
AUDIO(1) AUDIO(2) AUDIO(3)
#undef AUDIO

static GeMpRegistry registry;
static GeMpRegistryEntry entries[2];
static uint64_t identity;
static GeMpStateMusicSnapshot snapshot;
static unsigned captures;
static uint32_t players;

static GeMpMusicValues native_values(void)
{
    GeMpMusicValues v = {0};
    v.stage_id = stageMusicID; v.mission_state = mission_state;
    v.saved_mission_state = dword_CODE_bss_8008C604;
    memcpy(v.slot_active, music_slot_active_0, sizeof(v.slot_active));
    memcpy(v.slot_minimum, music_slot_minutes_0, sizeof(v.slot_minimum));
    memcpy(v.slot_duration, music_slot_seconds_0, sizeof(v.slot_duration));
    for (unsigned i = 0; i < GE_MP_MUSIC_SETUP_ROWS; ++i) {
        v.setup[i][0] = music_setup_entries[i].stage_id;
        v.setup[i][1] = music_setup_entries[i].main_music;
        v.setup[i][2] = music_setup_entries[i].bg_sound;
        v.setup[i][3] = music_setup_entries[i].xtrack;
    }
    memcpy(v.random_tracks, random_tracks, sizeof(v.random_tracks));
    return v;
}

static void digest(uint8_t hash[32])
{
    GeMpMusicValues before = native_values();
    GeMpRegistryEntry registry_before[2]; memcpy(registry_before, entries, sizeof(entries));
    unsigned calls_before = audio_calls; u64 rng_before = g_randomSeed;
    assert(ge_mp_state_music_capture(&registry, identity, &snapshot) == GE_MP_STATE_OK);
    assert(snapshot.record_count == 1 && snapshot.coverage.unclassified_fields == 0);
    GeMpMusicValues after = native_values();
    assert(!memcmp(&before, &after, sizeof(before)) && !memcmp(&before, &snapshot.values, sizeof(before)));
    assert(!memcmp(registry_before, entries, sizeof(entries)));
    assert(calls_before == audio_calls && rng_before == g_randomSeed);
    GeMpFieldSchema global_schema = {256, GE_MP_STATE_U32, 0, 1, 1};
    GeMpStateField global_field = {256, GE_MP_STATE_U32, 1, &players};
    GeMpRecordSchema schemas[] = {{0, GE_MP_SYSTEM_SESSION, &global_schema, 1}, *ge_mp_state_music_schema()};
    /* Coverage attests only this isolated transition-control fixture. */
    GeMpStateSchema schema = {1, GE_MP_SYSTEM_SESSION, 1, schemas, 2};
    GeMpStateRecord records[] = {{0, 0, &global_field, 1}, snapshot.record};
    GeMpCanonicalSnapshot canonical = {.session = 83, .schema = &schema, .registry = &registry,
        .records = records, .record_count = 2, .enabled_systems = GE_MP_SYSTEM_SESSION,
        .coverage = {GE_MP_SYSTEM_SESSION, 0, 0}};
    memset(canonical.build_sha256, 1, 32); memset(canonical.content_sha256, 2, 32);
    memset(canonical.rules_sha256, 3, 32);
    assert(ge_mp_state_sha256(&canonical, hash, NULL) == GE_MP_STATE_OK);
    schema.complete = 0;
    uint8_t unchanged[32]; memcpy(unchanged, hash, 32);
    assert(ge_mp_state_sha256(&canonical, hash, NULL) == GE_MP_STATE_INCOMPLETE);
    assert(!memcmp(unchanged, hash, 32));
    ++captures;
}

static void all_fields(void)
{
    int32_t *fields[15] = {&stageMusicID, &mission_state, &dword_CODE_bss_8008C604};
    for (unsigned i = 0; i < 4; ++i) {
        fields[3+i] = &music_slot_active_0[i]; fields[7+i] = &music_slot_minutes_0[i];
        fields[11+i] = &music_slot_seconds_0[i];
    }
    const int32_t values[] = {INT32_MIN, -1, 0, 1, INT32_MAX};
    uint8_t base[32], changed[32]; digest(base);
    for (unsigned i = 0; i < 15; ++i) {
        int32_t saved = *fields[i];
        for (unsigned n = 0; n < sizeof(values)/sizeof(values[0]); ++n) {
            if (saved == values[n]) continue;
            *fields[i] = values[n]; digest(changed); assert(memcmp(base, changed, 32));
        }
        *fields[i] = saved;
    }
    for (unsigned row = 0; row < GE_MP_MUSIC_SETUP_ROWS; ++row) {
        s16 *fields16[] = {&music_setup_entries[row].stage_id, &music_setup_entries[row].main_music,
            &music_setup_entries[row].bg_sound, &music_setup_entries[row].xtrack};
        for (unsigned i = 0; i < 4; ++i) {
            s16 saved = *fields16[i]; *fields16[i] = saved ? 0 : -1;
            digest(changed); assert(memcmp(base, changed, 32)); *fields16[i] = saved;
        }
    }
    for (unsigned i = 0; i < GE_MP_MUSIC_RANDOM_TRACKS; ++i) {
        s16 saved = random_tracks[i]; random_tracks[i] = saved ? 0 : -1;
        digest(changed); assert(memcmp(base, changed, 32)); random_tracks[i] = saved;
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2); players = (uint32_t)atoi(argv[1]); assert(players == 2 || players == 4);
    assert(ge_mp_registry_init(&registry, entries, 2) == GE_MP_STATE_OK);
    uint32_t key = ge_mp_registry_key(GE_MP_MUSIC_KIND, GE_MP_MUSIC_NAMESPACE, GE_MP_MUSIC_SLOT);
    assert(ge_mp_registry_define(&registry, key) == GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry, key, &identity) == GE_MP_STATE_OK);
    sub_GAME_7F0C1364();
    g_randomSeed = UINT64_C(0x9876543212345678);
    u64 expected_rng = g_randomSeed; (void)randomGetNextFrom(&expected_rng);
    sub_GAME_7F0C11FC(LEVELID_CAVES);
    assert(mission_state == MISSION_STATE_1 && stageMusicID == LEVELID_CAVES && tracks[0]);
    assert(g_randomSeed == expected_rng); /* Exactly one real random track selection. */
    uint8_t hash[32]; digest(hash); all_fields();
    for (unsigned i = 0; i < players; ++i) musicPlaySlot((s32)i, 2, 3);
    for (unsigned i = 0; i < 240; ++i) {
        g_ClockTimer = i == 0 ? 0 : 1;
        reset_all_music_slots(); digest(hash);
        if (i == 0) assert(mission_state == MISSION_STATE_1 && music_slot_minutes_0[0] == 120);
        if (i == 1) assert(mission_state == MISSION_STATE_2 && music_slot_minutes_0[0] == 119);
    }
    assert(mission_state == MISSION_STATE_1 && !music_slot_active_0[players-1]);
    assert(g_randomSeed == expected_rng);
    sub_GAME_7F0C1310(); assert(mission_state == MISSION_STATE_3 && dword_CODE_bss_8008C604 == MISSION_STATE_1);
    digest(hash); GeMpMusicValues durable = snapshot.values;
    sub_GAME_7F0C1340(); assert(mission_state == MISSION_STATE_1);
    assert(!memcmp(&durable, &snapshot.values, sizeof(durable)));
    set_missionstate(MISSION_STATE_6); digest(hash);
    set_missionstate(MISSION_STATE_1); digest(hash);
    set_missionstate(MISSION_STATE_0); digest(hash);
    assert(g_randomSeed == expected_rng);
    /* Native stage initialization preserves saved state and slot history. */
    musicPlaySlot(3, 7, 9); sub_GAME_7F0C11FC(LEVELID_SURFACE2);
    assert(mission_state == MISSION_STATE_4 && dword_CODE_bss_8008C604 == MISSION_STATE_1);
    assert(music_slot_active_0[3] == 1 && music_slot_minutes_0[3] == 420 && music_slot_seconds_0[3] == 540);
    assert(g_randomSeed == expected_rng); digest(hash);
    musicStopSlot(-1); digest(hash);
    for (unsigned i = 0; i < 4; ++i) assert(!music_slot_active_0[i] && !music_slot_minutes_0[i] && !music_slot_seconds_0[i]);
    assert(ge_mp_state_music_capture(&registry, identity ^ (UINT64_C(1)<<48), &snapshot) == GE_MP_STATE_INVALID && !snapshot.record_count);
    uint64_t old = identity;
    assert(ge_mp_registry_release(&registry, identity) == GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry, key, &identity) == GE_MP_STATE_OK);
    assert(ge_mp_state_music_capture(&registry, old, &snapshot) == GE_MP_STATE_STALE && !snapshot.record_count);
    digest(hash);
    assert(ge_mp_registry_release(&registry, identity) == GE_MP_STATE_OK);
    assert(ge_mp_state_music_capture(&registry, identity, &snapshot) == GE_MP_STATE_STALE && !snapshot.record_count);
    printf("music %up: %u readonly captures, 15 signed fields, native track/slot/watch/death/history transitions; ", players, captures);
    for (unsigned i = 0; i < 32; ++i) printf("%02x", hash[i]);
    puts("");
    return 0;
}

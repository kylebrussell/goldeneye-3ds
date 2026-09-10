#include "ge_mp_state_music.h"
#include <string.h>

/* Exact s32 definitions owned by the retained mp_music.c source slice. */
extern int32_t stageMusicID, mission_state, dword_CODE_bss_8008C604;
extern int32_t music_slot_active_0[4], music_slot_minutes_0[4], music_slot_seconds_0[4];
struct music_setup { int16_t stage_id, main_music, bg_sound, xtrack; };
extern struct music_setup music_setup_entries[GE_MP_MUSIC_SETUP_ROWS];
extern int16_t random_tracks[GE_MP_MUSIC_RANDOM_TRACKS];
static const GeMpFieldSchema fields[] = {
    {256, GE_MP_STATE_I32, 0, 1, 1}, {257, GE_MP_STATE_I32, 0, 1, 1},
    {258, GE_MP_STATE_I32, 0, 1, 1}, {259, GE_MP_STATE_I32, 0, 4, 4},
    {260, GE_MP_STATE_I32, 0, 4, 4}, {261, GE_MP_STATE_I32, 0, 4, 4},
    {262, GE_MP_STATE_I16, 0, GE_MP_MUSIC_SETUP_ROWS*4, GE_MP_MUSIC_SETUP_ROWS*4},
    {263, GE_MP_STATE_I16, 0, GE_MP_MUSIC_RANDOM_TRACKS, GE_MP_MUSIC_RANDOM_TRACKS},
};
static const GeMpRecordSchema schema = {
    GE_MP_MUSIC_KIND, GE_MP_SYSTEM_SESSION, fields, GE_MP_MUSIC_FIELD_COUNT,
};
const GeMpRecordSchema *ge_mp_state_music_schema(void) { return &schema; }

GeMpStateStatus ge_mp_state_music_capture(const GeMpRegistry *registry,
    uint64_t actor_id, GeMpStateMusicSnapshot *out)
{
    if (!out) return GE_MP_STATE_INVALID;
    memset(out, 0, sizeof(*out));
    if (!(uint32_t)actor_id || (uint32_t)(actor_id >> 32) != ge_mp_registry_key(
            GE_MP_MUSIC_KIND, GE_MP_MUSIC_NAMESPACE, GE_MP_MUSIC_SLOT))
        return GE_MP_STATE_INVALID;
    GeMpStateStatus status = ge_mp_registry_resolve(registry, actor_id, NULL);
    if (status != GE_MP_STATE_OK) return status;
    GeMpMusicValues *v = &out->values;
    v->stage_id = stageMusicID;
    v->mission_state = mission_state;
    v->saved_mission_state = dword_CODE_bss_8008C604;
    memcpy(v->slot_active, music_slot_active_0, sizeof(v->slot_active));
    memcpy(v->slot_minimum, music_slot_minutes_0, sizeof(v->slot_minimum));
    memcpy(v->slot_duration, music_slot_seconds_0, sizeof(v->slot_duration));
    for (unsigned i = 0; i < GE_MP_MUSIC_SETUP_ROWS; ++i) {
        v->setup[i][0] = music_setup_entries[i].stage_id;
        v->setup[i][1] = music_setup_entries[i].main_music;
        v->setup[i][2] = music_setup_entries[i].bg_sound;
        v->setup[i][3] = music_setup_entries[i].xtrack;
    }
    memcpy(v->random_tracks, random_tracks, sizeof(v->random_tracks));
    out->fields[0] = (GeMpStateField){256, GE_MP_STATE_I32, 1, &v->stage_id};
    out->fields[1] = (GeMpStateField){257, GE_MP_STATE_I32, 1, &v->mission_state};
    out->fields[2] = (GeMpStateField){258, GE_MP_STATE_I32, 1, &v->saved_mission_state};
    out->fields[3] = (GeMpStateField){259, GE_MP_STATE_I32, 4, v->slot_active};
    out->fields[4] = (GeMpStateField){260, GE_MP_STATE_I32, 4, v->slot_minimum};
    out->fields[5] = (GeMpStateField){261, GE_MP_STATE_I32, 4, v->slot_duration};
    out->fields[6] = (GeMpStateField){262, GE_MP_STATE_I16, GE_MP_MUSIC_SETUP_ROWS*4, v->setup};
    out->fields[7] = (GeMpStateField){263, GE_MP_STATE_I16, GE_MP_MUSIC_RANDOM_TRACKS, v->random_tracks};
    out->record = (GeMpStateRecord){actor_id, GE_MP_MUSIC_KIND, out->fields,
        GE_MP_MUSIC_FIELD_COUNT};
    out->record_count = 1;
    out->coverage.enumerated_systems = GE_MP_SYSTEM_SESSION;
    return GE_MP_STATE_OK;
}

#include "ge_original_gameplay_services.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/bondinv.h"
#include "game/chrai.h"
#include "game/file2.h"
#include "game/gun.h"
#include "game/mp_music.h"
#include "game/options.h"
#include "game/player.h"
#include "game/propobj.h"
#include "game/stan.h"
#include "snd.h"
#include "ge_original_bond_input_internal.h"
#include "ge_original_bug_model.h"
#include "ge_original_covert_modem_object.h"
#include "ge_original_covert_modem_projectile.h"
#include "ge_original_covert_modem_fire.h"
#include "ge_original_embedment_pool.h"
#include "ge_original_pp7_fire.h"
#include "ge_original_dam_guard_death_link.h"
#include "ge_original_guard_grenade_model.h"
#include "ge_original_guard_grenade_object.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef GE_PLATFORM_3DS
#include "memp.h"
#include "game/vtxstore.h"

/* lvlStageLoad normally establishes MEMPOOL_STAGE immediately before the
 * canonical vtxstore reset.  The native 3DS stage bootstrap reaches this
 * service boundary directly, so provide the equivalent owned stage arena. */
static unsigned char ge_guard_damage_vtx_arena[65536]
    __attribute__((aligned(16)));
extern MemoryPool g_mempPools[MEMPOOL_COUNT];
extern void initWeaponAnimGroups(void);
extern void expand_ani_table_entries(s32 **entries);
extern s32 animation_table_ptrs1[];
extern struct ModelAnimation *animation_table_ptrs2[];
static bool ge_guard_animation_tables_initialized;
#endif

#define GE_SERVICE_SOUND_CAPACITY 24U
#define GE_SERVICE_VISIBLE_PROP_CAPACITY ONSCREEN_PROP_LIST_LEN
#define GE_SERVICE_SOUND_LIFETIME_TICKS 30U
#define GE_SERVICE_AUDIO_TICK_RATE 60U
#define GE_SERVICE_AUDIO_TICK_FRAMES 1024U

typedef struct GeServiceSound {
    ALSoundState state;
    uint32_t ticks_remaining;
    int16_t sound_id;
    uint8_t allocated;
    int16_t *pcm;
    size_t pcm_frames;
    uint64_t position_q16;
    uint32_t step_q16;
    uint32_t loop_start;
    uint32_t loop_end;
    uint32_t loop_count;
    uint32_t source_rate;
    float sample_pitch_ratio;
    uint8_t sample_pan;
    uint8_t sample_volume;
    int32_t left_gain_q15;
    int32_t right_gain_q15;
} GeServiceSound;

static GeServiceSound ge_service_sounds[GE_SERVICE_SOUND_CAPACITY];
static GeOriginalGameplayServiceStats ge_service_stats;
static char ge_service_hud_message[160];
static ALBank ge_service_sound_bank;
static const GeOriginalSfxBank *ge_service_sfx_bank;
static GeAudioOutput *ge_service_audio_output;
static uint32_t ge_service_audio_frame_remainder;
static bool ge_service_exact_gun_dispatch;
static void *ge_service_model_load_context;
static int32_t (*ge_service_model_load)(void *context, int32_t model_id);
static void *ge_service_settings_context;
static int (*ge_service_persist_settings)(void *context,
    uint8_t music_volume, uint8_t sfx_volume, uint16_t options);
extern void ge_original_hud_bottom_show_exact(char *message)
    __attribute__((weak));
extern void musicTrack1Play(s32 track) __attribute__((weak));

extern AmmoStats ammo_related[30];
s32 dword_CODE_bss_8008C604 __attribute__((weak));
s32 check_cur_player_ammo_amount_in_inventory(AMMOTYPE ammo_type);
s32 get_ammo_type_for_weapon(ITEM_IDS weapon);
void give_cur_player_ammo(s32 ammo_type, s32 ammo_amount);
void bondinvDetermineEquippedItem(void);
bool objTestForInteract(PropRecord *prop);

ALBank *g_musicSfxBufferPtr;
bool g_PlayerInvincible = FALSE;
PropRecord *g_OnScreenPropList[GE_SERVICE_VISIBLE_PROP_CAPACITY];
PropRecord **g_LastOnScreenProp = g_OnScreenPropList;
PropRecord *g_InteractProp;

/* Focused host binaries do not all retain the canonical mp_music owner.  A
 * strong set_missionstate from the decompiled live state slice overrides this
 * diagnostic fallback in the 3DS runtime and in tests which exercise the
 * exact watch-close transition. */
void set_missionstate(MISSION_STATE_ID state) __attribute__((weak));
void set_missionstate(MISSION_STATE_ID state)
{
    (void)state;
    ge_service_stats.unsupported_object_calls++;
}

/* Some narrow host harnesses retain this translation unit without the
 * generated options/music owners. Their durable settings callback is also
 * unbound, so conservative getter fallbacks can only feed the explicit
 * unsupported path. Strong decompiled/live symbols override every fallback
 * in the 3DS runtime and in the focused persistence test. */
u16 get_mTrack2Vol(void) __attribute__((weak));
u16 get_mTrack2Vol(void) { return 0U; }
u16 call_sndGetSfxSlotFirstNaturalVolume(void) __attribute__((weak));
u16 call_sndGetSfxSlotFirstNaturalVolume(void) { return 0U; }
s32 cur_player_get_autoaim(void) __attribute__((weak));
s32 cur_player_get_autoaim(void) { return FALSE; }
u32 cur_player_get_sight_onscreen_control(void) __attribute__((weak));
u32 cur_player_get_sight_onscreen_control(void) { return FALSE; }
u32 cur_player_get_lookahead(void) __attribute__((weak));
u32 cur_player_get_lookahead(void) { return FALSE; }
u32 cur_player_get_ammo_onscreen_setting(void) __attribute__((weak));
u32 cur_player_get_ammo_onscreen_setting(void) { return FALSE; }
u32 cur_player_get_screen_setting(void) __attribute__((weak));
u32 cur_player_get_screen_setting(void) { return SCREEN_SIZE_FULLSCREEN; }
SCREEN_RATIO_OPTION get_screen_ratio(void) __attribute__((weak));
SCREEN_RATIO_OPTION get_screen_ratio(void) { return SCREEN_RATIO_NORMAL; }

static GeServiceSound *ge_service_sound_from_state(ALSoundState *state)
{
    size_t index;
    for (index = 0U; index < GE_SERVICE_SOUND_CAPACITY; index++) {
        if (&ge_service_sounds[index].state == state)
            return &ge_service_sounds[index];
    }
    return NULL;
}

/* Exact sndUnlinkClearSound owner-link consequence.  Gun hands and other
 * canonical callers pass the address of their ALSoundState pointer as the
 * pending-state link.  Leaving that link stale after a sample finishes makes
 * both hand sound slots permanently look occupied after their first use. */
static void ge_service_sound_clear_owner(GeServiceSound *sound)
{
    ALSoundState *owner;

    if (sound == NULL) return;
    owner = sound->state.state;
    if (owner != NULL && owner->link.next == (ALLink *)&sound->state)
        owner->link.next = NULL;
    sound->state.state = NULL;
}

static void ge_service_sound_dispose(GeServiceSound *sound)
{
    if (sound == NULL) return;
    ge_service_sound_clear_owner(sound);
    free(sound->pcm);
    sound->pcm = NULL;
    sound->state.playingState = SOUND_STATE_NONE;
    sound->allocated = 0U;
}

static void ge_service_sound_apply_params(GeServiceSound *sound)
{
    int effective_pan;
    float pan;
    float volume;
    double step;
    if (sound == NULL) return;
    effective_pan = (int)sound->state.pan + (int)sound->sample_pan
        - AL_PAN_CENTER;
    if (effective_pan < AL_PAN_LEFT) effective_pan = AL_PAN_LEFT;
    if (effective_pan > AL_PAN_RIGHT) effective_pan = AL_PAN_RIGHT;
    pan = (float)effective_pan / (float)AL_PAN_RIGHT;
    volume = ((float)sound->sample_volume / 127.0f)
        * ((float)(uint16_t)sound->state.vol / 32767.0f);
    sound->left_gain_q15 = (int32_t)(
        sqrtf(1.0f - pan) * volume * 32767.0f);
    sound->right_gain_q15 = (int32_t)(
        sqrtf(pan) * volume * 32767.0f);
    if (ge_service_audio_output == NULL) return;
    step = (double)sound->source_rate * sound->sample_pitch_ratio
        * sound->state.pitch_2c / ge_service_audio_output->sample_rate;
    sound->step_q16 = (uint32_t)(step * 65536.0 + 0.5);
    if (sound->step_q16 == 0U) sound->step_q16 = 1U;
}

void ge_original_gameplay_services_reset(void)
{
    size_t index;
    for (index = 0U; index < GE_SERVICE_SOUND_CAPACITY; index++)
        ge_service_sound_dispose(&ge_service_sounds[index]);
    memset(ge_service_sounds, 0, sizeof(ge_service_sounds));
    memset(&ge_service_stats, 0, sizeof(ge_service_stats));
    memset(ge_service_hud_message, 0, sizeof(ge_service_hud_message));
    memset(g_OnScreenPropList, 0, sizeof(g_OnScreenPropList));
    memset(&ge_service_sound_bank, 0, sizeof(ge_service_sound_bank));
    g_musicSfxBufferPtr = &ge_service_sound_bank;
    g_LastOnScreenProp = g_OnScreenPropList;
    g_InteractProp = NULL;
    ge_service_exact_gun_dispatch = false;
    ge_service_settings_context = NULL;
    ge_service_persist_settings = NULL;
#ifdef GE_PLATFORM_3DS
    ge_original_embedment_pool_reset_exact();
    ge_original_dam_guard_death_force_link();
    memset(&g_mempPools[MEMPOOL_STAGE], 0,
           sizeof(g_mempPools[MEMPOOL_STAGE]));
    g_mempPools[MEMPOOL_STAGE].start = ge_guard_damage_vtx_arena;
    g_mempPools[MEMPOOL_STAGE].pos = ge_guard_damage_vtx_arena;
    g_mempPools[MEMPOOL_STAGE].end = ge_guard_damage_vtx_arena
        + sizeof(ge_guard_damage_vtx_arena);
    sub_GAME_7F09B820();
    if (!ge_guard_animation_tables_initialized) {
        expand_ani_table_entries((s32 **)&animation_table_ptrs1);
        expand_ani_table_entries((s32 **)&animation_table_ptrs2);
        initWeaponAnimGroups();
        ge_guard_animation_tables_initialized = true;
    }
#endif
    ge_original_covert_modem_object_reset();
    ge_original_guard_grenade_object_reset();
    ge_original_covert_modem_projectile_reset();
    ge_original_covert_modem_fire_reset();
    ge_original_pp7_fire_reset();
}

void ge_original_gameplay_services_bind_audio(
    const GeOriginalSfxBank *bank, GeAudioOutput *output)
{
    ge_service_sfx_bank = bank;
    ge_service_audio_output = output;
    ge_service_audio_frame_remainder = 0U;
}

void ge_original_gameplay_services_bind_model_loader(
    void *context, int32_t (*model_load)(void *context, int32_t model_id))
{
    ge_service_model_load_context = context;
    ge_service_model_load = model_load;
}

void ge_original_gameplay_services_bind_settings_persistence(
    void *context,
    int (*persist_settings)(void *context, uint8_t music_volume,
                            uint8_t sfx_volume, uint16_t options))
{
    ge_service_settings_context = context;
    ge_service_persist_settings = persist_settings;
}

void ge_original_gameplay_services_bind_visible_props(void *const *props,
                                                       size_t count)
{
    size_t index;
    if (count > GE_SERVICE_VISIBLE_PROP_CAPACITY)
        count = GE_SERVICE_VISIBLE_PROP_CAPACITY;
    memset(g_OnScreenPropList, 0, sizeof(g_OnScreenPropList));
    for (index = 0U; index < count; index++)
        g_OnScreenPropList[index] = props != NULL ? props[index] : NULL;
    g_LastOnScreenProp = g_OnScreenPropList + count;
}

void *ge_original_gameplay_services_find_interactable(
    void *const *props, size_t count)
{
    size_t onscreen_count;
    size_t onscreen_index;
    g_InteractProp = NULL;
    if (g_LastOnScreenProp == NULL
            || g_LastOnScreenProp < g_OnScreenPropList) return NULL;
    onscreen_count = (size_t)(g_LastOnScreenProp - g_OnScreenPropList);
    /* g_OnScreenPropList is far-to-near; this is the unchanged
     * propFindForInteract near-to-far traversal, restricted only to the
     * object class whose propobjInteract branch the caller owns. */
    for (onscreen_index = onscreen_count; onscreen_index > 0U;
            --onscreen_index) {
        PropRecord *prop = g_OnScreenPropList[onscreen_index - 1U];
        size_t candidate_index;
        for (candidate_index = 0U; candidate_index < count;
                ++candidate_index) {
            if (prop != (props != NULL ? props[candidate_index] : NULL))
                continue;
            (void)objTestForInteract(prop);
            break;
        }
    }
    return g_InteractProp;
}

void ge_original_gameplay_services_play_sfx(uint32_t sfx_id)
{
    (void)sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr,
                     (s16)sfx_id, NULL);
}

void ge_original_gameplay_services_play_music(int32_t music_id)
{
    ge_service_stats.last_music_id = music_id;
    ++ge_service_stats.music_play_calls;
    if (musicTrack1Play != NULL) {
        musicTrack1Play((s32)music_id);
    } else {
        ++ge_service_stats.unsupported_object_calls;
    }
}

void ge_original_gameplay_services_set_exact_gun_dispatch(int active)
{
    ge_service_exact_gun_dispatch = active != 0;
}

void ge_original_gameplay_services_tick(void)
{
    size_t index;
    uint32_t active = 0U;
    int32_t mixed[GE_SERVICE_AUDIO_TICK_FRAMES * 2U] = {0};
    int16_t interleaved[GE_SERVICE_AUDIO_TICK_FRAMES * 2U];
    size_t mix_frames = 0U;
    /* Original gunTickHandState has already published each hand's firing
     * status and first-person pose by this point in the gameplay tick. */
    if (!ge_service_exact_gun_dispatch)
        (void)ge_original_covert_modem_fire_tick();
    (void)ge_original_pp7_fire_tick();
    if (ge_service_audio_output != NULL) {
        uint32_t numerator = ge_service_audio_output->sample_rate
            + ge_service_audio_frame_remainder;
        mix_frames = numerator / GE_SERVICE_AUDIO_TICK_RATE;
        ge_service_audio_frame_remainder =
            numerator % GE_SERVICE_AUDIO_TICK_RATE;
        if (mix_frames > GE_SERVICE_AUDIO_TICK_FRAMES)
            mix_frames = GE_SERVICE_AUDIO_TICK_FRAMES;
        if (mix_frames > ge_audio_output_free(ge_service_audio_output))
            mix_frames = ge_audio_output_free(ge_service_audio_output);
    }
    for (index = 0U; index < GE_SERVICE_SOUND_CAPACITY; index++) {
        GeServiceSound *sound = &ge_service_sounds[index];
        size_t output_frame;
        if (!sound->allocated || sound->state.playingState == SOUND_STATE_NONE)
            continue;
        if (sound->pcm != NULL) {
            for (output_frame = 0U; output_frame < mix_frames; output_frame++) {
                size_t source_frame = (size_t)(sound->position_q16 >> 16U);
                uint32_t fraction = (uint32_t)sound->position_q16 & 0xffffU;
                int32_t sample;
                int32_t next;
                if (source_frame >= sound->pcm_frames) break;
                sample = sound->pcm[source_frame];
                next = source_frame + 1U < sound->pcm_frames
                    ? sound->pcm[source_frame + 1U] : sample;
                sample += (int32_t)(((int64_t)(next - sample) * fraction)
                                    >> 16U);
                mixed[output_frame * 2U] +=
                    (int32_t)(((int64_t)sample * sound->left_gain_q15)
                              >> 15U);
                mixed[output_frame * 2U + 1U] +=
                    (int32_t)(((int64_t)sample * sound->right_gain_q15)
                              >> 15U);
                sound->position_q16 += sound->step_q16;
                source_frame = (size_t)(sound->position_q16 >> 16U);
                if (sound->loop_count != 0U && sound->loop_end > sound->loop_start
                        && source_frame >= sound->loop_end) {
                    sound->position_q16 =
                        (uint64_t)sound->loop_start << 16U;
                    if (sound->loop_count != UINT32_MAX) sound->loop_count--;
                }
            }
            if ((size_t)(sound->position_q16 >> 16U) >= sound->pcm_frames) {
                ge_service_sound_dispose(sound);
                continue;
            }
        } else if (sound->ticks_remaining > 0U) {
            sound->ticks_remaining--;
        }
        if (sound->pcm == NULL && sound->ticks_remaining == 0U) {
            ge_service_sound_dispose(sound);
        } else {
            active++;
        }
    }
    if (ge_service_audio_output != NULL && mix_frames > 0U) {
        size_t frame;
        for (frame = 0U; frame < mix_frames * 2U; frame++) {
            int32_t value = mixed[frame];
            if (value > INT16_MAX) value = INT16_MAX;
            if (value < INT16_MIN) value = INT16_MIN;
            interleaved[frame] = (int16_t)value;
        }
        (void)ge_audio_output_write(
            ge_service_audio_output, interleaved, mix_frames);
        ge_service_stats.mixed_audio_frames += mix_frames;
    }
    ge_service_stats.active_sounds = active;
}

void ge_original_gameplay_services_snapshot(
    GeOriginalGameplayServiceStats *stats)
{
    if (stats != NULL) *stats = ge_service_stats;
}

ALSoundState *sndPlaySfx(struct ALBankAlt_s *sound_bank, s16 sound_index,
                         ALSoundState *pending_state)
{
    size_t index;
    GeServiceSound *slot = NULL;
    if (sound_bank == NULL || sound_index == 0) return NULL;
    for (index = 0U; index < GE_SERVICE_SOUND_CAPACITY; index++) {
        if (!ge_service_sounds[index].allocated) {
            slot = &ge_service_sounds[index];
            break;
        }
    }
    if (slot == NULL) return NULL;
    memset(slot, 0, sizeof(*slot));
    slot->allocated = 1U;
    slot->sound_id = sound_index;
    slot->ticks_remaining = GE_SERVICE_SOUND_LIFETIME_TICKS;
    slot->state.playingState = SOUND_STATE_PLAYING;
    slot->state.state = pending_state;
    slot->state.pitch_28 = 1.0f;
    slot->state.pitch_2c = 1.0f;
    slot->state.pan = AL_PAN_CENTER;
    slot->state.vol = INT16_MAX;
    if (ge_service_sfx_bank != NULL && ge_service_audio_output != NULL) {
        GeOriginalSfxInfo info;
        size_t frames = 0U;
        if (ge_original_sfx_bank_decode(
                ge_service_sfx_bank, sound_index, NULL, 0U,
                &frames, &info) == GE_ORIGINAL_SFX_BANK_OUTPUT_TOO_SMALL
                && frames != 0U && frames <= SIZE_MAX / sizeof(*slot->pcm)) {
            slot->pcm = malloc(frames * sizeof(*slot->pcm));
            if (slot->pcm != NULL && ge_original_sfx_bank_decode(
                    ge_service_sfx_bank, sound_index, slot->pcm, frames,
                    &frames, &info) == GE_ORIGINAL_SFX_BANK_OK) {
                slot->pcm_frames = frames;
                slot->source_rate = info.source_rate;
                slot->sample_pitch_ratio = info.pitch_ratio;
                slot->sample_pan = info.pan;
                slot->sample_volume = info.volume;
                slot->state.pitch_28 = info.pitch_ratio;
                ge_service_sound_apply_params(slot);
                if (info.has_loop) {
                    slot->loop_start = info.loop_start;
                    slot->loop_end = info.loop_end;
                    slot->loop_count = info.loop_count;
                }
                ge_service_stats.decoded_sound_starts++;
            } else {
                free(slot->pcm);
                slot->pcm = NULL;
                ge_service_stats.sound_decode_failures++;
            }
        } else {
            ge_service_stats.sound_decode_failures++;
        }
    }
    if (pending_state != NULL)
        pending_state->link.next = (ALLink *)&slot->state;
    ge_service_stats.sound_play_calls++;
    ge_service_stats.active_sounds++;
    ge_service_stats.last_sound_id = sound_index;
    return &slot->state;
}

u8 sndGetPlayingState(ALSoundState *state)
{
    GeServiceSound *sound = ge_service_sound_from_state(state);
    return sound != NULL && sound->allocated
        ? sound->state.playingState : SOUND_STATE_NONE;
}

void sndDeactivate(ALSoundState *state)
{
    GeServiceSound *sound = ge_service_sound_from_state(state);
    if (sound == NULL || !sound->allocated) return;
    sound->state.playingState = SOUND_STATE_NONE;
    ge_service_sound_dispose(sound);
    if (ge_service_stats.active_sounds > 0U)
        ge_service_stats.active_sounds--;
    ge_service_stats.sound_deactivate_calls++;
}

void sndCreatePostEvent(ALSoundState *state, s16 event_type, s32 value)
{
    GeServiceSound *sound;
    /* The original posts at delta zero. This adapter has no deferred audio
     * thread, so apply the event immediately while retaining the original
     * state/sample parameter composition. */
    if (state == NULL) return;
    sound = ge_service_sound_from_state(state);
    if (sound == NULL || !sound->allocated) return;
    switch (event_type) {
    case AL_SNDP_VOL_EVT:
        state->vol = (s16)value;
        ge_service_sound_apply_params(sound);
        ge_service_stats.sound_parameter_events++;
        break;
    case AL_SNDP_PAN_EVT:
        state->pan = (ALPan)value;
        ge_service_sound_apply_params(sound);
        ge_service_stats.sound_parameter_events++;
        break;
    case AL_SNDP_PITCH_EVT: {
        float pitch;
        uint32_t bits = (uint32_t)value;
        memcpy(&pitch, &bits, sizeof(pitch));
        state->pitch_2c = pitch;
        ge_service_sound_apply_params(sound);
        ge_service_stats.sound_parameter_events++;
        break;
    }
    default:
        ge_service_stats.unsupported_object_calls++;
        break;
    }
}

void hudmsgBottomShow(char *string)
{
    if (string == NULL) return;
    if (ge_original_hud_bottom_show_exact != NULL)
        ge_original_hud_bottom_show_exact(string);
    strncpy(ge_service_hud_message, string,
            sizeof(ge_service_hud_message) - 1U);
    ge_service_hud_message[sizeof(ge_service_hud_message) - 1U] = '\0';
    ge_service_stats.hud_messages++;
}

static void ge_service_file_save_settings_exact(save_data *save)
{
    u32 temp;
    u16 bits;

    bits = 0;
    save->music_vol = get_mTrack2Vol() >> 7;
    save->sfx_vol = (call_sndGetSfxSlotFirstNaturalVolume() >> 7);

    if (get_cur_player_look_vertical_inverted())
    {
        bits = OPTION_INVERTLOOK;
    }

    if (cur_player_get_autoaim())
    {
        bits |= OPTION_AUTOAIM;
    }

    if (cur_player_get_aim_control())
    {
        bits |= OPTION_AIMCONTROL;
    }

    if (cur_player_get_sight_onscreen_control())
    {
        bits |= OPTION_SIGHTONSCREEN;
    }

    if (cur_player_get_lookahead())
    {
        bits |= OPTION_LOOKAHEAD;
    }

    if (cur_player_get_ammo_onscreen_setting())
    {
        bits |= OPTION_DISPLAYAMMO;
    }

    if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
    {
        bits |= OPTION_SCREENWIDE;
    }
    else if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
    {
        bits |= OPTION_SCREENCINEMA;
    }

    if (get_screen_ratio() != SCREEN_RATIO_NORMAL)
    {
        bits |= OPTION_SCREENRATIO;
    }

    temp = ((u16) (cur_player_get_control_type() << 8)) & OPTION_CONTROLTYPE;
    save->options = bits | temp;
}

void deleteCurrentSelectedFolder(void)
{
    save_data settings = {0};
    ge_service_file_save_settings_exact(&settings);
    ++ge_service_stats.settings_persist_calls;
    if (ge_service_persist_settings == NULL
            || !ge_service_persist_settings(ge_service_settings_context,
                settings.music_vol, settings.sfx_vol, settings.options)) {
        ++ge_service_stats.settings_persist_failures;
        ++ge_service_stats.unsupported_object_calls;
    }
}

void sub_GAME_7F0C1340(void)
{
    /* Exact mp_music.c watch-close body. sub_GAME_7F0C1310 in the retained
     * Bond input slice saved the pre-watch state before selecting state 3;
     * closing the watch restores it through the unchanged transition owner. */
    set_missionstate(dword_CODE_bss_8008C604);
}

s32 modelLoad(s32 modelid)
{
    if (ge_service_model_load != NULL)
        return ge_service_model_load(ge_service_model_load_context, modelid);
    if (modelid == ge_original_bug_model_id())
        return ge_original_bug_model_prepare() ? TRUE : FALSE;
    if (modelid == PROP_CHRGRENADE)
        return ge_original_guard_grenade_model_prepare() ? TRUE : FALSE;
    /* Other projectile PitemZ construction has not crossed the native object
     * ABI yet. Report rather than silently accepting those requests. */
    ge_service_stats.unsupported_object_calls++;
    return FALSE;
}

static u8 ge_service_interaction_object_type(
    const PropRecord *prop, const ObjectRecord *obj)
{
#if defined(GE_PORT_MS_INHERITS)
    (void)prop;
    return obj->type;
#else
    /* Legacy host-only test builds deliberately omit the anonymous inherited
     * PropDefHeaderRecord, so ObjectRecord has no definition-type storage in
     * that ABI.  Its PropRecord runtime type is the only valid discriminator;
     * the live 3DS build above continues to read the canonical obj->type. */
    (void)obj;
    return prop->type;
#endif
}

bool objTestForInteract(PropRecord *prop)
{
    f32 xdiff;
    ObjectRecord *obj;
    PropRecord *player;
    f32 vertical_limit;
    f32 ydiff;
    f32 zdiff;
    f32 distance_limit_squared;
    f32 anglediff;
    f32 playerangle;
    f32 angle_limit;
    StandTile *stan;
    f32 xzdiff;
    f32 angle;
    u8 object_type;

    ge_service_stats.interaction_tests++;
    if (prop == NULL || prop->obj == NULL) return TRUE;
    obj = prop->obj;
    object_type = ge_service_interaction_object_type(prop, obj);

    /* Exact src/game/propobj.c objTestForInteract body. Renderer publication
     * owns PROPFLAG_ONSCREEN; health, authored activation flags, yaw, and STAN
     * LOS are consumed unchanged here. */
    if (object_type == PROP_TYPE_PLAYER
            || (obj->flags & PROPFLAG_00080000)
            || (obj->runtime_bitflags
                & (RUNTIMEBITFLAG_00000001 | (UINT32_C(1) << 1U)
                   | (UINT32_C(1) << 4U)))) {
        if ((prop->flags & PROPFLAG_ONSCREEN)
                && objIsHealthy(obj)
                && !(obj->flags & PROPFLAG_CANNOT_ACTIVATE)) {
            player = getCurrentPlayerProp();
            if (player == NULL) return TRUE;
            xdiff = obj->runtime_pos.x - player->pos.x;
            ydiff = obj->runtime_pos.y - player->pos.y;
            zdiff = obj->runtime_pos.z - player->pos.z;
            stan = player->stan;

            if (object_type == 0x28
                    && (obj->flags & PROPFLAG_DOOR_OPENTOFRONT)) {
                vertical_limit = 400.0f;
                distance_limit_squared = 160000.0f;
                angle_limit = 2.0943952f;
            } else {
                vertical_limit = 200.0f;
                distance_limit_squared = 40000.0f;
                angle_limit = 0.3926991f;
            }

            xzdiff = xdiff * xdiff + zdiff * zdiff;
            if (xzdiff < distance_limit_squared
                    && ydiff < vertical_limit
                    && -vertical_limit < ydiff) {
                angle = atan2f(xdiff, zdiff);
                playerangle = (360.0f - g_CurrentPlayer->vv_theta)
                    * (M_TAU_F / 360.0f);
                anglediff = angle - playerangle;
                if (angle < playerangle) anglediff += M_TAU_F;
                if (anglediff > M_PI_F) anglediff = M_TAU_F - anglediff;
                if (anglediff <= angle_limit
                        && (!(obj->flags2 & PROPFLAG2_INTERACTCHECKLOS)
                            || walkTilesBetweenPoints_NoCallback(
                                &stan, player->pos.x, player->pos.z,
                                prop->pos.x, prop->pos.z))) {
                    g_InteractProp = prop;
                    ge_service_stats.interaction_hits++;
                }
            }
        }
    }
    return TRUE;
}

static bool ge_service_door_within_use_cone(PropRecord *prop)
{
    PropRecord *player;
    float xdiff;
    float ydiff;
    float zdiff;
    float angle;
    float player_angle;
    float angle_diff;
    if (prop == NULL || g_CurrentPlayer == NULL
            || (player = g_CurrentPlayer->prop) == NULL
            || (prop->flags & PROPFLAG_ONSCREEN) == 0) return FALSE;
    xdiff = prop->pos.x - player->pos.x;
    ydiff = prop->pos.y - player->pos.y;
    zdiff = prop->pos.z - player->pos.z;
    if (xdiff * xdiff + zdiff * zdiff >= 40000.0f
            || ydiff >= 200.0f || ydiff <= -200.0f) return FALSE;
    angle = atan2f(xdiff, zdiff);
    player_angle = (360.0f - g_CurrentPlayer->vv_theta)
        * (M_TAU_F / 360.0f);
    angle_diff = angle - player_angle;
    if (angle < player_angle) angle_diff += M_TAU_F;
    if (angle_diff > M_PI_F) angle_diff = M_TAU_F - angle_diff;
    return angle_diff <= 0.34906587f;
}

bool doorTestForInteract(PropRecord *prop)
{
    ge_service_stats.interaction_tests++;
    if (ge_service_door_within_use_cone(prop)) {
        g_InteractProp = prop;
        ge_service_stats.interaction_hits++;
        return FALSE;
    }
    return TRUE;
}

void add_ammo_to_inventory(AMMOTYPE ammo_type, int amount,
                           int play_sound, int display_text)
{
    int current;
    int maximum;
    if (amount <= 0 || ammo_type >= AMMOTYPE_MAX) return;
    current = check_cur_player_ammo_amount_in_inventory(ammo_type);
    maximum = ammo_related[ammo_type].MaxAmmo;
    if (current >= maximum) return;
    give_cur_player_ammo(ammo_type,
                         current + amount > maximum ? maximum : current + amount);
    if (play_sound)
        sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr,
                   PICKUP_AMMO_SFX, NULL);
    if (display_text)
        hudmsgBottomShow("Ammo collected");
}

/* Exact gunfire.c bodies used by campaign AI ammo/casualty opcodes. */
s32 currentPlayerGetAmmoCount(AMMOTYPE ammotype)
{
    s32 total_ammo = check_cur_player_ammo_amount_in_inventory(ammotype);

    if (get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNRIGHT)) == (s32)ammotype) {
        total_ammo += get_ammo_in_hands_magazine(GUNRIGHT);
    }

    if (get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNLEFT)) == (s32)ammotype) {
        total_ammo += get_ammo_in_hands_magazine(GUNLEFT);
    }

    return total_ammo;
}

s32 get_civilian_casualties(void)
{
    return g_playerPerm->killed_civilians;
}

void set_sound_effect_for_weapontype_collection(ITEM_IDS weapon_type)
{
    s16 sound = PICKUP_GUN_SFX;
    if (weapon_type == ITEM_KNIFE || weapon_type == ITEM_THROWKNIFE)
        sound = PICKUP_KNIFE_SFX;
    else if (weapon_type == ITEM_REMOTEMINE
            || weapon_type == ITEM_PROXIMITYMINE
            || weapon_type == ITEM_TIMEDMINE)
        sound = PICKUP_MINE_SFX;
    sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, sound, NULL);
}

void display_text_for_weapon_in_lower_left_corner(ITEM_IDS weapon_id)
{
    (void)weapon_id;
    hudmsgBottomShow("Weapon collected");
}

ObjectRecord *create_new_item_instance_of_model(PROP modelnum, s32 weaponid)
{
    ObjectRecord *object;
    object = ge_original_covert_modem_object_create(modelnum, weaponid);
    if (object != NULL) return object;
    if (modelnum == PROP_CHRBUG && weaponid == ITEM_BUG) return NULL;
    ge_service_stats.unsupported_object_calls++;
    return NULL;
}

PropRecord *something_with_generating_object(ChrRecord *self, s32 propid,
        ITEM_IDS itemid, s32 flags, WeaponObjRecord *weapon,
        ItemModelFileRecord *prop_header)
{
    if (weapon == NULL && prop_header == NULL
            && propid == PROP_CHRGRENADE && itemid == ITEM_GRENADE) {
        return ge_original_guard_grenade_object_create(
            self, propid, itemid, flags);
    }
    (void)self;
    (void)propid;
    (void)itemid;
    (void)flags;
    (void)weapon;
    (void)prop_header;
    ge_service_stats.unsupported_object_calls++;
    return NULL;
}

void chrSetWeaponFlag4(ChrRecord *chr, GUNHAND hand)
{
    /* Exact propobj.c body. The live object tick owns removal and frees the
     * concrete weapon slot after seeing this canonical flag. */
    if (chr != NULL && chr->weapons_held[hand] != NULL) {
        chr->weapons_held[hand]->obj->runtime_bitflags |= (1U << 2);
    }
}

void objFreePermanently(struct ObjectRecord *obj, bool freeprop)
{
#ifdef GE_PLATFORM_3DS
    extern void objFree(ObjectRecord *object, s32 free_prop, s32 can_regen);
    /* Exact propobj.c wrapper; full live props support owns objFree itself. */
    objFree(obj, freeprop, 0);
#else
    /* Focused host binaries intentionally omit the full props graph. */
    (void)obj;
    (void)freeprop;
    ge_service_stats.unsupported_object_calls++;
#endif
}

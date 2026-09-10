#include "ge_original_multiplayer_rules.h"
#include <ultra64.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/mp_weapon.h"
#include "game/bondview.h"
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <string.h>

extern s32 scenario;
extern struct mp_stage_setup multi_stage_setups[];
extern struct mp_stage_playercount mp_player_counts[];
extern char *ge_mp_setup_text_pointers[];
extern PadRecord *g_Startpad[16];
extern s32 startpadcount;
extern ITEM_IDS starting_weapon[2];
extern s32 get_points_for_mp_player(s32);
extern void mp_respawn_handler(void);
extern stagesetup g_CurrentSetup;
static const int32_t *ge_mp_validated_intro;
static char ge_mp_setup_keys[GE_ORIGINAL_MP_ARENAS][32];

int ge_original_multiplayer_arena(size_t index, GeOriginalMultiplayerArena *out)
{
    const struct mp_stage_setup *arena;
    const char *setup;
    if (index >= GE_ORIGINAL_MP_ARENAS || out == NULL) return 0;
    arena = &multi_stage_setups[index + 1]; /* exclude Random */
    setup = ge_mp_setup_text_pointers[arena->stage_id];
    if (setup == NULL || setup[0] != 'U') return 0;
    if (snprintf(ge_mp_setup_keys[index], sizeof(ge_mp_setup_keys[index]),
                 "Ump_%s", setup + 1) >= (int)sizeof(ge_mp_setup_keys[index]))
        return 0;
    out->level_id = arena->stage_id;
    out->max_players = arena->max_player;
    out->unlock_after = arena->unlock_after;
    out->setup_key = ge_mp_setup_keys[index];
    return 1;
}

GeOriginalMultiplayerStatus ge_original_multiplayer_validate(
    const GeOriginalMultiplayerConfig *config)
{
    size_t i;
    unsigned teams[2] = {0, 0};
    GeOriginalMultiplayerArena arena;
    if (!config || config->player_count < 2 || config->player_count > 4
            || config->scenario < 0 || config->scenario > SCENARIO_2v1
            || config->weapon_set < 0 || config->weapon_set >= 14)
        return GE_ORIGINAL_MP_INVALID_CONFIG;
    if (config->player_count < mp_player_counts[config->scenario].min
            || config->player_count > mp_player_counts[config->scenario].max)
        return GE_ORIGINAL_MP_INVALID_CONFIG;
    for (i = 0; i < GE_ORIGINAL_MP_ARENAS; ++i)
        if (ge_original_multiplayer_arena(i, &arena)
                && arena.level_id == config->level_id) break;
    if (i == GE_ORIGINAL_MP_ARENAS || config->player_count > arena.max_players)
        return GE_ORIGINAL_MP_INVALID_CONFIG;
    if (config->scenario >= SCENARIO_2v2) {
        for (i = 0; i < (size_t)config->player_count; ++i) {
            if (config->teams[i] > 1) return GE_ORIGINAL_MP_INVALID_CONFIG;
            teams[config->teams[i]]++;
        }
        if ((config->scenario == SCENARIO_2v2 && (teams[0] != 2 || teams[1] != 2))
                || (config->scenario == SCENARIO_3v1 &&
                    !((teams[0] == 3 && teams[1] == 1) || (teams[0] == 1 && teams[1] == 3)))
                || (config->scenario == SCENARIO_2v1 &&
                    !((teams[0] == 2 && teams[1] == 1) || (teams[0] == 1 && teams[1] == 2))))
            return GE_ORIGINAL_MP_INVALID_CONFIG;
    }
    return GE_ORIGINAL_MP_OK;
}

GeOriginalMultiplayerStatus ge_original_multiplayer_configure(
    const GeOriginalMultiplayerConfig *config)
{
    int i;
    if (ge_original_multiplayer_validate(config) != GE_ORIGINAL_MP_OK
            || getPlayerCount() != config->player_count)
        return GE_ORIGINAL_MP_INVALID_CONFIG;
    for (i = 0; i < config->player_count; ++i)
        if (g_playerPointers[i] == NULL) return GE_ORIGINAL_MP_INVALID_PLAYER;
    ge_mp_validated_intro = NULL;
    startpadcount = 0;
    memset(g_Startpad, 0, sizeof(PadRecord *) * 16);
    scenario = config->scenario;
    setMPWeaponSet(config->weapon_set);
    for (i = 0; i < config->player_count; ++i)
        g_playerPlayerData[i].have_token_or_goldengun =
            scenario >= SCENARIO_2v2 ? config->teams[i] : 0;
    return GE_ORIGINAL_MP_OK;
}

int ge_original_multiplayer_weapon(int32_t slot, GeOriginalMultiplayerWeapon *out)
{
    const struct s_mp_weapon_set *weapon;
    if (!out || slot < 0 || slot >= 8 || getMPWeaponSet() < 0 || getMPWeaponSet() >= 14)
        return 0;
    weapon = &getPtrMPWeaponSetData()[slot];
    out->item_id = weapon->itemID;
    out->model_id = weapon->propID;
    out->scale = weapon->size;
    out->ammo_type = weapon->ammotype;
    out->ammo_amount = weapon->ammoamount;
    out->allow_pickup = weapon->allowpickup;
    return 1;
}

int ge_original_multiplayer_ammo(void *context, int32_t *type, int32_t *amount)
{
    GeOriginalMultiplayerWeapon weapon;
    if (!context || !type || !amount
            || !ge_original_multiplayer_weapon(*(const int32_t *)context, &weapon))
        return 0;
    *type = weapon.ammo_type;
    *amount = weapon.ammo_amount;
    return 1;
}

static int ge_mp_player_valid(int32_t player)
{
    int count = getPlayerCount();
    return count >= 2 && count <= 4 && player >= 0 && player < count
        && g_playerPointers[player] != NULL;
}

GeOriginalMultiplayerStatus ge_original_multiplayer_score(int32_t player,
                                                          int32_t *score)
{
    if (!score || !ge_mp_player_valid(player)) return GE_ORIGINAL_MP_INVALID_PLAYER;
    *score = get_points_for_mp_player(player);
    return GE_ORIGINAL_MP_OK;
}

int ge_original_multiplayer_can_respawn(int32_t player, uint16_t buttons)
{
    struct player *p;
    int deaths = 0, i;
    if (!ge_mp_player_valid(player) || !(buttons & 0xB000)) return 0;
    p = g_playerPointers[player];
    if (!p->bonddead || !p->deathanimfinished || !p->redbloodfinished
            || p->colourfadetimemax60 >= 0.0f) return 0;
    for (i = 0; i < getPlayerCount(); ++i)
        deaths += g_playerPlayerData[i].kill_counts[player];
    return get_scenario() != SCENARIO_YOLT || deaths < 2;
}

GeOriginalMultiplayerStatus ge_original_multiplayer_publish_spawns(
    const int32_t *intro, size_t words, void *pads_data, size_t pads_count,
    int32_t demo_playback)
{
    PadRecord *pads = pads_data;
    PadRecord *pending[16] = {0};
    ITEM_IDS weapons[2] = {ITEM_UNARMED, ITEM_UNARMED};
    size_t cursor = 0, count = 0, stride;
    int have_starting_weapon = 0;
    if (!intro || !pads || !words) return GE_ORIGINAL_MP_INVALID_SETUP;
    while (cursor < words) {
        switch (intro[cursor]) {
        case INTROTYPE_END:
            if (!count || cursor + 1 != words) return GE_ORIGINAL_MP_INVALID_SETUP;
            ge_mp_validated_intro = intro;
            memcpy(g_Startpad, pending, sizeof(pending));
            startpadcount = (s32)count;
            if (!have_starting_weapon) weapons[GUNRIGHT] = ITEM_FIST;
            memcpy(starting_weapon, weapons, sizeof(weapons));
            return GE_ORIGINAL_MP_OK;
        case INTROTYPE_SPAWN: stride = 3; break;
        case INTROTYPE_ITEM: case INTROTYPE_AMMO: stride = 4; break;
        case INTROTYPE_SWIRL: stride = 8; break;
        case INTROTYPE_ANIM: case INTROTYPE_CUFF: stride = 2; break;
        case INTROTYPE_CAMERA: stride = 10; break;
        /* The original respawn handler advances one word for WATCH/CREDITS;
         * MP authored intros do not use these. Reject instead of misreading. */
        default: return GE_ORIGINAL_MP_INVALID_SETUP;
        }
        if (stride > words - cursor) return GE_ORIGINAL_MP_INVALID_SETUP;
        if (intro[cursor] == INTROTYPE_SPAWN
                && intro[cursor + 2] == demo_playback) {
            int32_t pad = intro[cursor + 1];
            if (count == 16 || pad < 0 || (size_t)pad >= pads_count || !pads[pad].stan
                    || !isfinite(pads[pad].pos.x) || !isfinite(pads[pad].pos.y)
                    || !isfinite(pads[pad].pos.z))
                return GE_ORIGINAL_MP_INVALID_SETUP;
            pending[count++] = &pads[pad];
        } else if (intro[cursor] == INTROTYPE_ITEM
                && intro[cursor + 3] == demo_playback && !have_starting_weapon) {
            have_starting_weapon = 1;
            weapons[GUNRIGHT] = intro[cursor + 1];
            weapons[GUNLEFT] = intro[cursor + 2] < 0 ? ITEM_UNARMED : intro[cursor + 2];
        }
        if (intro[cursor] == INTROTYPE_AMMO
                && (intro[cursor + 1] <= 0 || intro[cursor + 1] >= 30 || intro[cursor + 2] < 0))
            return GE_ORIGINAL_MP_INVALID_SETUP;
        if (intro[cursor] == INTROTYPE_ITEM
                && (intro[cursor + 1] < 0 || intro[cursor + 1] >= ITEM_IDS_MAX
                    || intro[cursor + 2] < -1 || intro[cursor + 2] >= ITEM_IDS_MAX))
            return GE_ORIGINAL_MP_INVALID_SETUP;
        cursor += stride;
    }
    return GE_ORIGINAL_MP_INVALID_SETUP;
}

GeOriginalMultiplayerStatus ge_original_multiplayer_respawn(
    int32_t player, uint16_t buttons)
{
    int previous, i;
    if (!ge_original_multiplayer_can_respawn(player, buttons))
        return GE_ORIGINAL_MP_RESPAWN_NOT_READY;
    if (startpadcount < 1 || startpadcount > 16 || !g_playerPointers[player]->prop
            || !ge_mp_validated_intro || (const void *)g_CurrentSetup.intro != ge_mp_validated_intro
            || !g_playerPointers[player]->p_itemcur || g_playerPointers[player]->equipmaxitems < 1)
        return GE_ORIGINAL_MP_INVALID_SETUP;
    for (i = 0; i < startpadcount; ++i)
        if (!g_Startpad[i] || !g_Startpad[i]->stan) return GE_ORIGINAL_MP_INVALID_SETUP;
    previous = get_cur_playernum();
    set_cur_player(player);
    mp_respawn_handler();
    set_cur_player(previous);
    return GE_ORIGINAL_MP_OK;
}


extern int ge_original_mp_end_decision_exact(s32 tick60, s32 point_limit);
GeOriginalMultiplayerEndDecision ge_original_multiplayer_end_tick(
    int32_t tick60, int32_t elapsed60, int32_t time_limit60, int32_t point_limit)
{
    int decision, i;
    if (tick60 < 0 || elapsed60 < 0 || tick60 > INT_MAX - elapsed60
            || time_limit60 < 0 || point_limit < 0 || getPlayerCount() < 2
            || getPlayerCount() > 4)
        return GE_ORIGINAL_MP_END_INVALID;
    for (i = 0; i < getPlayerCount(); ++i)
        if (g_playerPointers[i] == NULL) return GE_ORIGINAL_MP_END_INVALID;
    decision = ge_original_mp_end_decision_exact(tick60, point_limit);
    /* Exact expiration crossing from lv.c's timer branch (alarm/UI are owned
     * by its frontend). No simulation-global timer mutation here. */
    if (time_limit60 > 0 && elapsed60 < time_limit60
            && elapsed60 + tick60 >= time_limit60)
        decision |= 2;
    return (decision & 2) ? GE_ORIGINAL_MP_END_RESULTS
        : (decision & 1) ? GE_ORIGINAL_MP_END_FREEZE : GE_ORIGINAL_MP_END_RUNNING;
}

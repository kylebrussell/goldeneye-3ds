#include "ge_original_multiplayer_frontend.h"
#include "ge_stage_assets.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include "assets/obseg/text/LtitleE.h"
#include "assets/obseg/text/LmpweaponsE.h"
#include <string.h>

#include "ge_original_multiplayer_frontend_data.inc"

static uint64_t mp_frontend_request_sequence;

static void change_phase(GeOriginalMultiplayerFrontend *frontend,
    GeOriginalMultiplayerFrontendPhase phase)
{
    frontend->phase = phase;
    frontend->input_armed = 0;
    frontend->previous_buttons = 0;
}

static GeOriginalMultiplayerFrontendStatus status(
    GeOriginalMultiplayerFrontend *frontend,
    GeOriginalMultiplayerFrontendStatus value)
{
    if (frontend != NULL) frontend->status = value;
    return value;
}

int ge_original_multiplayer_frontend_character(size_t index,
    GeOriginalMultiplayerFrontendCharacter *out)
{
    if (out == NULL || index >= GE_ORIGINAL_MP_FRONTEND_CHARACTERS) return 0;
    *out = mp_frontend_characters[index];
    return 1;
}

int ge_original_multiplayer_frontend_limit(size_t index,
    GeOriginalMultiplayerFrontendLimit *out)
{
    if (out == NULL || index >= GE_ORIGINAL_MP_FRONTEND_LIMITS) return 0;
    *out = mp_frontend_limits[index];
    return 1;
}

static int scenario_players(int scenario, int players)
{
    if (scenario < SCENARIO_NORMAL || scenario > SCENARIO_2v1) return 0;
    if (scenario == SCENARIO_2v2 || scenario == SCENARIO_3v1) return players == 4;
    if (scenario == SCENARIO_2v1) return players == 3;
    return players >= 2 && players <= 4;
}

static void scenario_defaults(GeOriginalMultiplayerFrontendSelection *selection,
    int scenario)
{
    selection->config.scenario = scenario;
    if (scenario == SCENARIO_YOLT) selection->limit_index = 7;
    else if (selection->limit_index > (scenario == SCENARIO_TLD ? 3 : 6))
        selection->limit_index = 2;
    if (scenario == SCENARIO_MWTGG) selection->config.weapon_set = 13;
    if (scenario == SCENARIO_LTK) selection->config.weapon_set = 1;
    memset(selection->config.teams, 0, sizeof(selection->config.teams));
    if (scenario == SCENARIO_2v2 || scenario == SCENARIO_2v1)
        selection->config.teams[2] = 1;
    if (scenario == SCENARIO_2v2 || scenario == SCENARIO_3v1)
        selection->config.teams[3] = 1;
}

static GeOriginalMultiplayerFrontendStatus arena_status(
    const GeOriginalMultiplayerFrontend *frontend, int index, int players)
{
    GeOriginalMultiplayerArena arena;
    const GeStageAssetDescriptor *assets;
    if (index < 0 || !ge_original_multiplayer_arena((size_t)index, &arena))
        return GE_ORIGINAL_MP_FRONTEND_INVALID;
    if (players > arena.max_players) return GE_ORIGINAL_MP_FRONTEND_INVALID;
    if (arena.unlock_after >= 0 && (frontend->services.stage_unlocked == NULL
            || !frontend->services.stage_unlocked(
                frontend->services.context, arena.unlock_after)))
        return GE_ORIGINAL_MP_FRONTEND_LOCKED;
    assets = ge_stage_asset_multiplayer((size_t)index);
    if (assets == NULL || assets->level_id != arena.level_id
            || assets->setup_key == NULL || arena.setup_key == NULL
            || strcmp(assets->setup_key, arena.setup_key) != 0
            || ge_stage_asset_multiplayer_max_players((size_t)index) < (size_t)players
            || ge_stage_asset_multiplayer_spawn_count((size_t)index) == 0)
        return GE_ORIGINAL_MP_FRONTEND_UNAVAILABLE;
    return GE_ORIGINAL_MP_FRONTEND_OK;
}

static GeOriginalMultiplayerFrontendStatus validate(
    const GeOriginalMultiplayerFrontend *frontend,
    const GeOriginalMultiplayerFrontendSelection *selection)
{
    GeOriginalMultiplayerFrontendStatus checked;
    const GeStageAssetDescriptor *assets;
    int scenario = selection->config.scenario;
    int i, j;
    if (frontend->character_count != 8 && frontend->character_count != 33
            && frontend->character_count != 64)
        return GE_ORIGINAL_MP_FRONTEND_INVALID;
    if (ge_original_multiplayer_validate(&selection->config) != GE_ORIGINAL_MP_OK)
        return GE_ORIGINAL_MP_FRONTEND_INVALID;
    checked = arena_status(frontend, selection->arena_index, selection->config.player_count);
    if (checked != GE_ORIGINAL_MP_FRONTEND_OK) return checked;
    assets = ge_stage_asset_multiplayer((size_t)selection->arena_index);
    if (assets->level_id != selection->config.level_id
            || selection->limit_index < 0 || selection->limit_index > 7
            || (scenario == SCENARIO_YOLT && selection->limit_index != 7)
            || (scenario != SCENARIO_YOLT && selection->limit_index > 6)
            || (scenario == SCENARIO_TLD && selection->limit_index > 3)
            || (scenario == SCENARIO_MWTGG && selection->config.weapon_set != 13))
        return GE_ORIGINAL_MP_FRONTEND_INVALID;
    for (i = 0; i < selection->config.player_count; ++i) {
        if (selection->character_ids[i] < 0
                || selection->character_ids[i] >= frontend->character_count
                || selection->character_ids[i] >= GE_ORIGINAL_MP_FRONTEND_CHARACTERS)
            return GE_ORIGINAL_MP_FRONTEND_LOCKED;
        for (j = 0; j < i; ++j)
            if (selection->character_ids[i] == selection->character_ids[j])
                return GE_ORIGINAL_MP_FRONTEND_INVALID;
    }
    return GE_ORIGINAL_MP_FRONTEND_OK;
}

int ge_original_multiplayer_frontend_init(GeOriginalMultiplayerFrontend *frontend,
    const GeOriginalMultiplayerFrontendServices *services)
{
    const GeStageAssetDescriptor *assets = ge_stage_asset_multiplayer(0);
    if (frontend == NULL || assets == NULL) return 0;
    memset(frontend, 0, sizeof(*frontend));
    if (services != NULL) frontend->services = *services;
    frontend->character_count = frontend->services.all_characters_unlocked ? 64
        : frontend->services.cradle_completed ? 33 : 8;
    frontend->selection.config.player_count = 2;
    frontend->selection.config.level_id = assets->level_id;
    frontend->selection.config.weapon_set = 11; /* Original mp_weapon_set default. */
    frontend->selection.limit_index = 2; /* Original LEN_10MIN. */
    for (int i = 0; i < GE_ORIGINAL_MP_PLAYERS; ++i)
        frontend->selection.character_ids[i] = i;
    change_phase(frontend, GE_ORIGINAL_MP_FRONTEND_SETUP);
    frontend->status = validate(frontend, &frontend->selection);
    return frontend->status == GE_ORIGINAL_MP_FRONTEND_OK;
}

GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_set(
    GeOriginalMultiplayerFrontend *frontend,
    GeOriginalMultiplayerFrontendOption option, int32_t player, int32_t value)
{
    GeOriginalMultiplayerFrontendSelection next;
    GeOriginalMultiplayerFrontendStatus checked;
    const GeStageAssetDescriptor *assets;
    if (frontend == NULL) return GE_ORIGINAL_MP_FRONTEND_INVALID;
    if (frontend->phase != GE_ORIGINAL_MP_FRONTEND_SETUP)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_BUSY);
    if (frontend->character_count != 8 && frontend->character_count != 33
            && frontend->character_count != 64)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_INVALID);
    next = frontend->selection;
    switch (option) {
    case GE_ORIGINAL_MP_FRONTEND_PLAYERS:
        if (value < 2 || value > 4) return status(frontend, GE_ORIGINAL_MP_FRONTEND_INVALID);
        next.config.player_count = value;
        if (!scenario_players(next.config.scenario, value)) scenario_defaults(&next, SCENARIO_NORMAL);
        if (arena_status(frontend, next.arena_index, value) != GE_ORIGINAL_MP_FRONTEND_OK) {
            next.arena_index = 0;
            assets = ge_stage_asset_multiplayer(0);
            if (assets == NULL) return status(frontend, GE_ORIGINAL_MP_FRONTEND_UNAVAILABLE);
            next.config.level_id = assets->level_id;
        }
        /* A newly added seat cannot inherit a character now used elsewhere. */
        for (int i = 0; i < value; ++i) {
            int candidate = next.character_ids[i];
            for (int j = 0; j < i; ++j) {
                if (candidate == next.character_ids[j]) { candidate = (candidate + 1) % frontend->character_count; j = -1; }
            }
            next.character_ids[i] = candidate;
        }
        break;
    case GE_ORIGINAL_MP_FRONTEND_ARENA:
        checked = arena_status(frontend, value, next.config.player_count);
        if (checked != GE_ORIGINAL_MP_FRONTEND_OK) return status(frontend, checked);
        next.arena_index = value;
        next.config.level_id = ge_stage_asset_multiplayer((size_t)value)->level_id;
        break;
    case GE_ORIGINAL_MP_FRONTEND_SCENARIO:
        if (!scenario_players(value, next.config.player_count))
            return status(frontend, GE_ORIGINAL_MP_FRONTEND_INVALID);
        scenario_defaults(&next, value);
        break;
    case GE_ORIGINAL_MP_FRONTEND_WEAPONS:
        if (next.config.scenario == SCENARIO_MWTGG && value != 13)
            return status(frontend, GE_ORIGINAL_MP_FRONTEND_LOCKED);
        next.config.weapon_set = value;
        break;
    case GE_ORIGINAL_MP_FRONTEND_CHARACTER:
        if (player < 0 || player >= next.config.player_count)
            return status(frontend, GE_ORIGINAL_MP_FRONTEND_INVALID);
        next.character_ids[player] = value;
        break;
    case GE_ORIGINAL_MP_FRONTEND_LIMIT:
        if (next.config.scenario == SCENARIO_YOLT && value != 7)
            return status(frontend, GE_ORIGINAL_MP_FRONTEND_LOCKED);
        next.limit_index = value;
        break;
    default: return status(frontend, GE_ORIGINAL_MP_FRONTEND_INVALID);
    }
    checked = validate(frontend, &next);
    if (checked == GE_ORIGINAL_MP_FRONTEND_OK) frontend->selection = next;
    return status(frontend, checked);
}

int ge_original_multiplayer_frontend_snapshot(
    const GeOriginalMultiplayerFrontend *frontend,
    GeOriginalMultiplayerFrontendSnapshot *snapshot)
{
    if (frontend == NULL || snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->phase = frontend->phase;
    snapshot->status = frontend->status;
    snapshot->selection = frontend->selection;
    snapshot->result = frontend->result;
    snapshot->character_count = frontend->character_count;
    snapshot->input_armed = frontend->input_armed;
    snapshot->diagnostic_local = 1;
    if (validate(frontend, &frontend->selection) != GE_ORIGINAL_MP_FRONTEND_OK) return 1;
    snapshot->can_start = frontend->phase == GE_ORIGINAL_MP_FRONTEND_SETUP && frontend->input_armed;
    snapshot->arena_text_id = mp_frontend_arenas[frontend->selection.arena_index];
    snapshot->scenario_text_id = mp_frontend_scenarios[frontend->selection.config.scenario];
    snapshot->weapon_text_id = mp_frontend_weapons[frontend->selection.config.weapon_set];
    snapshot->limit = mp_frontend_limits[frontend->selection.limit_index];
    for (int i = 0; i < frontend->selection.config.player_count; ++i)
        snapshot->characters[i] = mp_frontend_characters[frontend->selection.character_ids[i]];
    return 1;
}

uint32_t ge_original_multiplayer_frontend_input_edges(
    GeOriginalMultiplayerFrontend *frontend, uint32_t held_buttons)
{
    uint32_t edges;
    if (frontend == NULL) return 0;
    if (!frontend->input_armed) {
        if (held_buttons == 0) frontend->input_armed = 1;
        frontend->previous_buttons = held_buttons;
        return 0;
    }
    edges = held_buttons & ~frontend->previous_buttons;
    frontend->previous_buttons = held_buttons;
    return edges;
}

GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_request_start(
    GeOriginalMultiplayerFrontend *frontend)
{
    GeOriginalMultiplayerFrontendStatus checked;
    if (frontend == NULL) return GE_ORIGINAL_MP_FRONTEND_INVALID;
    if (frontend->phase != GE_ORIGINAL_MP_FRONTEND_SETUP || !frontend->input_armed)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_BUSY);
    checked = validate(frontend, &frontend->selection);
    if (checked != GE_ORIGINAL_MP_FRONTEND_OK) return status(frontend, checked);
    if (mp_frontend_request_sequence == UINT64_MAX)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_UNAVAILABLE);
    memset(&frontend->start, 0, sizeof(frontend->start));
    memset(&frontend->result, 0, sizeof(frontend->result));
    frontend->sequence = ++mp_frontend_request_sequence;
    frontend->start.request_id = frontend->sequence;
    frontend->start.selection = frontend->selection;
    frontend->start.diagnostic_local = 1;
    frontend->start.time_limit60 = mp_frontend_limits[frontend->selection.limit_index].time_limit60;
    frontend->start.point_limit = mp_frontend_limits[frontend->selection.limit_index].point_limit;
    for (int i = 0; i < frontend->selection.config.player_count; ++i)
        frontend->start.characters[i] = mp_frontend_characters[frontend->selection.character_ids[i]];
    change_phase(frontend, GE_ORIGINAL_MP_FRONTEND_START_PENDING);
    return status(frontend, GE_ORIGINAL_MP_FRONTEND_OK);
}

int ge_original_multiplayer_frontend_take_start(
    GeOriginalMultiplayerFrontend *frontend, GeOriginalMultiplayerFrontendStart *out)
{
    GeOriginalMultiplayerFrontendStatus checked;
    if (frontend == NULL || out == NULL || frontend->phase != GE_ORIGINAL_MP_FRONTEND_START_PENDING)
        return 0;
    checked = validate(frontend, &frontend->start.selection);
    if (checked != GE_ORIGINAL_MP_FRONTEND_OK) {
        change_phase(frontend, GE_ORIGINAL_MP_FRONTEND_SETUP);
        status(frontend, checked);
        return 0;
    }
    *out = frontend->start;
    change_phase(frontend, GE_ORIGINAL_MP_FRONTEND_LOADING);
    return 1;
}

GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_started(
    GeOriginalMultiplayerFrontend *frontend, uint64_t request_id, int succeeded)
{
    if (frontend == NULL) return GE_ORIGINAL_MP_FRONTEND_INVALID;
    if (request_id == 0 || request_id != frontend->start.request_id)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_STALE);
    if (frontend->phase != GE_ORIGINAL_MP_FRONTEND_LOADING)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_BUSY);
    change_phase(frontend, succeeded ? GE_ORIGINAL_MP_FRONTEND_RUNNING : GE_ORIGINAL_MP_FRONTEND_SETUP);
    return status(frontend, succeeded ? GE_ORIGINAL_MP_FRONTEND_OK : GE_ORIGINAL_MP_FRONTEND_START_FAILED);
}

GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_finish(
    GeOriginalMultiplayerFrontend *frontend,
    const GeOriginalMultiplayerFrontendResult *result)
{
    if (frontend == NULL || result == NULL) return GE_ORIGINAL_MP_FRONTEND_INVALID;
    if (result->request_id == 0 || result->request_id != frontend->start.request_id)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_STALE);
    if (frontend->phase != GE_ORIGINAL_MP_FRONTEND_RUNNING)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_BUSY);
    if (result->player_count != frontend->start.selection.config.player_count
            || result->player_count < 2 || result->player_count > 4
            || result->winner_mask >= (1U << result->player_count) || result->aborted > 1
            || result->end_reason <= GE_ORIGINAL_MP_FRONTEND_END_NONE
            || result->end_reason > GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION
            || (result->end_reason == GE_ORIGINAL_MP_FRONTEND_END_ABORT) != (result->aborted != 0))
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_INVALID);
    for (int i = 0; i < result->player_count; ++i)
        if (result->kills[i] < 0 || result->deaths[i] < 0)
            return status(frontend, GE_ORIGINAL_MP_FRONTEND_INVALID);
    memset(&frontend->result, 0, sizeof(frontend->result));
    frontend->result.request_id = result->request_id;
    frontend->result.player_count = result->player_count;
    frontend->result.winner_mask = result->winner_mask;
    frontend->result.aborted = result->aborted;
    frontend->result.end_reason = result->end_reason;
    for (int i = 0; i < result->player_count; ++i) {
        frontend->result.scores[i] = result->scores[i];
        frontend->result.kills[i] = result->kills[i];
        frontend->result.deaths[i] = result->deaths[i];
    }
    change_phase(frontend, GE_ORIGINAL_MP_FRONTEND_RESULTS);
    return status(frontend, GE_ORIGINAL_MP_FRONTEND_OK);
}

GeOriginalMultiplayerFrontendStatus ge_original_multiplayer_frontend_return_to_setup(
    GeOriginalMultiplayerFrontend *frontend)
{
    if (frontend == NULL) return GE_ORIGINAL_MP_FRONTEND_INVALID;
    if (frontend->phase == GE_ORIGINAL_MP_FRONTEND_RUNNING
            || frontend->phase == GE_ORIGINAL_MP_FRONTEND_LOADING)
        return status(frontend, GE_ORIGINAL_MP_FRONTEND_BUSY);
    memset(&frontend->start, 0, sizeof(frontend->start));
    memset(&frontend->result, 0, sizeof(frontend->result));
    change_phase(frontend, GE_ORIGINAL_MP_FRONTEND_SETUP);
    return status(frontend, GE_ORIGINAL_MP_FRONTEND_OK);
}

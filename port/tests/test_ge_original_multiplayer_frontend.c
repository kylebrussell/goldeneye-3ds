#include "ge_original_multiplayer_frontend.h"
#include "ge_stage_assets.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>

static int unlocked;
static int unlock_calls;
static int stage_unlocked(void *context, int32_t mission)
{
    assert(context == &unlocked && mission >= 0 && mission < 20);
    ++unlock_calls;
    return unlocked;
}

static GeOriginalMultiplayerFrontendServices services(void)
{
    GeOriginalMultiplayerFrontendServices result = {&unlocked, stage_unlocked, 0, 0};
    return result;
}

static void test_options(void)
{
    GeOriginalMultiplayerFrontend frontend;
    GeOriginalMultiplayerFrontendSnapshot snapshot;
    GeOriginalMultiplayerFrontendServices provider = services();
    GeOriginalMultiplayerFrontendSelection before;
    GeOriginalMultiplayerFrontendCharacter character;
    GeOriginalMultiplayerFrontendLimit limit;
    int times[] = {0, 18000, 36000, 72000, 0, 0, 0, 0};
    int points[] = {0, 0, 0, 0, 5, 10, 20, 0};
    unlocked = 0;
    assert(ge_original_multiplayer_frontend_init(&frontend, &provider));
    assert(ge_original_multiplayer_frontend_snapshot(&frontend, &snapshot));
    assert(snapshot.phase == GE_ORIGINAL_MP_FRONTEND_SETUP);
    assert(snapshot.diagnostic_local && !snapshot.can_start);
    assert(snapshot.character_count == 8);
    assert(snapshot.selection.config.player_count == 2);
    assert(snapshot.selection.config.weapon_set == 11);
    assert(snapshot.selection.config.level_id == LEVELID_TEMPLE);
    assert(snapshot.limit.time_limit60 == 36000 && snapshot.limit.point_limit == 0);
    assert(snapshot.characters[0].body == BODY_Brosnan_Tuxedo);
    assert(snapshot.characters[1].body == BODY_Natalya_Skirt);
    assert(snapshot.characters[1].height == 0.96609998f);
    assert(snapshot.arena_text_id && snapshot.scenario_text_id && snapshot.weapon_text_id);
    for (size_t i = 0; i < GE_ORIGINAL_MP_FRONTEND_CHARACTERS; ++i) {
        assert(ge_original_multiplayer_frontend_character(i, &character));
        assert(character.text_id && character.height > 0.0f);
    }
    assert(!ge_original_multiplayer_frontend_character(64, &character));
    assert(!ge_original_multiplayer_frontend_character(0, NULL));
    for (size_t i = 0; i < GE_ORIGINAL_MP_FRONTEND_LIMITS; ++i) {
        assert(ge_original_multiplayer_frontend_limit(i, &limit));
        assert(limit.time_limit60 == times[i] && limit.point_limit == points[i]);
    }
    assert(!ge_original_multiplayer_frontend_limit(8, &limit));
    assert(!ge_original_multiplayer_frontend_limit(0, NULL));
    for (int count = 2; count <= 4; ++count) {
        assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, count) == GE_ORIGINAL_MP_FRONTEND_OK);
        for (size_t i = 0; i < GE_ORIGINAL_MP_ARENAS; ++i) {
            GeOriginalMultiplayerArena arena;
            GeOriginalMultiplayerFrontendStatus expected;
            assert(ge_original_multiplayer_arena(i, &arena));
            assert(ge_stage_asset_multiplayer(i)->level_id == arena.level_id);
            before = frontend.selection;
            expected = count > arena.max_players ? GE_ORIGINAL_MP_FRONTEND_INVALID
                : arena.unlock_after >= 0 ? GE_ORIGINAL_MP_FRONTEND_LOCKED : GE_ORIGINAL_MP_FRONTEND_OK;
            assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_ARENA, 0, (int)i) == expected);
            if (expected != GE_ORIGINAL_MP_FRONTEND_OK) assert(memcmp(&before, &frontend.selection, sizeof(before)) == 0);
        }
    }
    assert(unlock_calls > 0);
    unlocked = 1;
    for (int count = 2; count <= 4; ++count) {
        assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, count) == GE_ORIGINAL_MP_FRONTEND_OK);
        for (size_t i = 0; i < GE_ORIGINAL_MP_ARENAS; ++i) {
            GeOriginalMultiplayerArena arena;
            assert(ge_original_multiplayer_arena(i, &arena));
            assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_ARENA, 0, (int)i)
                == (count > arena.max_players ? GE_ORIGINAL_MP_FRONTEND_INVALID : GE_ORIGINAL_MP_FRONTEND_OK));
        }
    }
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, 2) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_ARENA, 0, 10) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(frontend.selection.config.level_id == LEVELID_EGYPT);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, 4) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(frontend.selection.config.level_id == LEVELID_TEMPLE); /* Canonical fallback. */
    before = frontend.selection;
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, 1) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_ARENA, 0, -1) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_WEAPONS, 0, 14) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_CHARACTER, 4, 1) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_CHARACTER, 0, 1) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_CHARACTER, 0, 8) == GE_ORIGINAL_MP_FRONTEND_LOCKED);
    assert(memcmp(&before, &frontend.selection, sizeof(before)) == 0);
    provider.cradle_completed = 1;
    assert(ge_original_multiplayer_frontend_init(&frontend, &provider));
    assert(frontend.character_count == 33);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_CHARACTER, 0, 32) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_CHARACTER, 0, 33) == GE_ORIGINAL_MP_FRONTEND_LOCKED);
    provider.all_characters_unlocked = 1;
    assert(ge_original_multiplayer_frontend_init(&frontend, &provider));
    assert(frontend.character_count == 64);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_CHARACTER, 0, 63) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_CHARACTER, 0, 64) == GE_ORIGINAL_MP_FRONTEND_LOCKED);
    assert(ge_original_multiplayer_frontend_init(&frontend, NULL));
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_ARENA, 0, 6) == GE_ORIGINAL_MP_FRONTEND_LOCKED);
}

static void test_scenarios(void)
{
    GeOriginalMultiplayerFrontend frontend;
    assert(ge_original_multiplayer_frontend_init(&frontend, NULL));
    for (int count = 2; count <= 4; ++count) {
        assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, count) == GE_ORIGINAL_MP_FRONTEND_OK);
        for (int scenario = 0; scenario < 8; ++scenario) {
            int allowed = scenario < 5 || (scenario < 7 ? count == 4 : count == 3);
            assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_SCENARIO, 0, scenario)
                == (allowed ? GE_ORIGINAL_MP_FRONTEND_OK : GE_ORIGINAL_MP_FRONTEND_INVALID));
            if (!allowed) continue;
            assert(ge_original_multiplayer_validate(&frontend.selection.config) == GE_ORIGINAL_MP_OK);
            for (int limit = 0; limit < 8; ++limit) {
                GeOriginalMultiplayerFrontendStatus expected = GE_ORIGINAL_MP_FRONTEND_OK;
                if (scenario == SCENARIO_YOLT && limit != 7) expected = GE_ORIGINAL_MP_FRONTEND_LOCKED;
                else if (scenario != SCENARIO_YOLT && limit == 7) expected = GE_ORIGINAL_MP_FRONTEND_INVALID;
                else if (scenario == SCENARIO_TLD && limit > 3) expected = GE_ORIGINAL_MP_FRONTEND_INVALID;
                assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_LIMIT, 0, limit) == expected);
            }
            for (int weapon = 0; weapon < 14; ++weapon)
                assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_WEAPONS, 0, weapon)
                    == (scenario == SCENARIO_MWTGG && weapon != 13 ? GE_ORIGINAL_MP_FRONTEND_LOCKED : GE_ORIGINAL_MP_FRONTEND_OK));
        }
    }
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_SCENARIO, 0, SCENARIO_LTK) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(frontend.selection.config.weapon_set == 1);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_SCENARIO, 0, SCENARIO_YOLT) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(frontend.selection.limit_index == 7);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_SCENARIO, 0, SCENARIO_NORMAL) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(frontend.selection.limit_index == 2);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_SCENARIO, 0, SCENARIO_2v2) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, 3) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(frontend.selection.config.scenario == SCENARIO_NORMAL);
    assert(frontend.selection.config.teams[2] == 0);
}

static void release_gate(GeOriginalMultiplayerFrontend *frontend)
{
    assert(!frontend->input_armed);
    assert(ge_original_multiplayer_frontend_input_edges(frontend, 1) == 0);
    assert(ge_original_multiplayer_frontend_input_edges(frontend, 2) == 0);
    assert(ge_original_multiplayer_frontend_input_edges(frontend, 3) == 0);
    assert(!frontend->input_armed);
    assert(ge_original_multiplayer_frontend_input_edges(frontend, 0) == 0);
    assert(frontend->input_armed);
    assert(ge_original_multiplayer_frontend_input_edges(frontend, 1) == 1);
    assert(ge_original_multiplayer_frontend_input_edges(frontend, 1) == 0);
    assert(ge_original_multiplayer_frontend_input_edges(frontend, 3) == 2);
}

static GeOriginalMultiplayerFrontendStart begin(GeOriginalMultiplayerFrontend *frontend)
{
    GeOriginalMultiplayerFrontendStart record, untouched;
    assert(ge_original_multiplayer_frontend_request_start(frontend) == GE_ORIGINAL_MP_FRONTEND_BUSY);
    release_gate(frontend);
    assert(ge_original_multiplayer_frontend_request_start(frontend) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(ge_original_multiplayer_frontend_request_start(frontend) == GE_ORIGINAL_MP_FRONTEND_BUSY);
    assert(ge_original_multiplayer_frontend_set(frontend, GE_ORIGINAL_MP_FRONTEND_WEAPONS, 0, 1) == GE_ORIGINAL_MP_FRONTEND_BUSY);
    assert(ge_original_multiplayer_frontend_take_start(frontend, &record));
    untouched = record;
    assert(!ge_original_multiplayer_frontend_take_start(frontend, &untouched));
    assert(memcmp(&record, &untouched, sizeof(record)) == 0);
    assert(record.diagnostic_local && record.request_id != 0);
    assert(record.time_limit60 == 36000 && record.point_limit == 0);
    untouched.selection.config.weapon_set = 2;
    assert(frontend->start.selection.config.weapon_set == 11);
    return record;
}

static void test_lifecycle(void)
{
    GeOriginalMultiplayerFrontend frontend;
    GeOriginalMultiplayerFrontendStart record, second;
    GeOriginalMultiplayerFrontendResult result = {0};
    GeOriginalMultiplayerFrontendSnapshot snapshot;
    assert(ge_original_multiplayer_frontend_init(&frontend, NULL));
    record = begin(&frontend);
    release_gate(&frontend);
    assert(ge_original_multiplayer_frontend_started(&frontend, record.request_id + 1, 1) == GE_ORIGINAL_MP_FRONTEND_STALE);
    assert(frontend.phase == GE_ORIGINAL_MP_FRONTEND_LOADING);
    assert(ge_original_multiplayer_frontend_return_to_setup(&frontend) == GE_ORIGINAL_MP_FRONTEND_BUSY);
    assert(ge_original_multiplayer_frontend_started(&frontend, record.request_id, 0) == GE_ORIGINAL_MP_FRONTEND_START_FAILED);
    assert(frontend.phase == GE_ORIGINAL_MP_FRONTEND_SETUP);
    second = begin(&frontend);
    assert(second.request_id > record.request_id);
    assert(ge_original_multiplayer_frontend_started(&frontend, second.request_id, 1) == GE_ORIGINAL_MP_FRONTEND_OK);
    release_gate(&frontend);
    assert(ge_original_multiplayer_frontend_started(&frontend, second.request_id, 1) == GE_ORIGINAL_MP_FRONTEND_BUSY);
    assert(ge_original_multiplayer_frontend_return_to_setup(&frontend) == GE_ORIGINAL_MP_FRONTEND_BUSY);
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, 4) == GE_ORIGINAL_MP_FRONTEND_BUSY);
    result.request_id = record.request_id;
    result.player_count = 2;
    result.scores[0] = -1; result.scores[1] = 10;
    result.kills[1] = 10; result.deaths[0] = 11;
    result.winner_mask = 2;
    result.end_reason = GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT;
    assert(ge_original_multiplayer_frontend_finish(&frontend, &result) == GE_ORIGINAL_MP_FRONTEND_STALE);
    result.request_id = second.request_id;
    result.player_count = 4;
    assert(ge_original_multiplayer_frontend_finish(&frontend, &result) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    result.player_count = 2; result.winner_mask = 4;
    assert(ge_original_multiplayer_frontend_finish(&frontend, &result) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    result.winner_mask = 2; result.kills[0] = -1;
    assert(ge_original_multiplayer_frontend_finish(&frontend, &result) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    result.kills[0] = 0; result.end_reason = GE_ORIGINAL_MP_FRONTEND_END_NONE;
    assert(ge_original_multiplayer_frontend_finish(&frontend, &result) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    result.end_reason = GE_ORIGINAL_MP_FRONTEND_END_ABORT;
    assert(ge_original_multiplayer_frontend_finish(&frontend, &result) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    result.end_reason = GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT;
    assert(ge_original_multiplayer_frontend_finish(&frontend, &result) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(ge_original_multiplayer_frontend_finish(&frontend, &result) == GE_ORIGINAL_MP_FRONTEND_BUSY);
    assert(ge_original_multiplayer_frontend_snapshot(&frontend, &snapshot));
    assert(memcmp(&snapshot.result, &result, sizeof(result)) == 0);
    assert(!snapshot.can_start && snapshot.phase == GE_ORIGINAL_MP_FRONTEND_RESULTS);
    result.scores[1] = 999;
    assert(frontend.result.scores[1] == 10);
    release_gate(&frontend);
    assert(ge_original_multiplayer_frontend_return_to_setup(&frontend) == GE_ORIGINAL_MP_FRONTEND_OK);
    release_gate(&frontend);
    assert(ge_original_multiplayer_frontend_request_start(&frontend) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(ge_original_multiplayer_frontend_return_to_setup(&frontend) == GE_ORIGINAL_MP_FRONTEND_OK);
    assert(!ge_original_multiplayer_frontend_take_start(&frontend, &record));
    /* Initialization must not reuse request IDs from a previous instance. */
    assert(ge_original_multiplayer_frontend_init(&frontend, NULL));
    record = begin(&frontend);
    assert(record.request_id > second.request_id);
    assert(ge_original_multiplayer_frontend_started(&frontend, second.request_id, 1) == GE_ORIGINAL_MP_FRONTEND_STALE);
}

static void test_pending_revalidation(void)
{
    GeOriginalMultiplayerFrontend frontend;
    GeOriginalMultiplayerFrontendServices provider = services();
    GeOriginalMultiplayerFrontendStart record;
    unlocked = 1;
    assert(ge_original_multiplayer_frontend_init(&frontend, &provider));
    assert(ge_original_multiplayer_frontend_set(&frontend, GE_ORIGINAL_MP_FRONTEND_ARENA, 0, 6) == GE_ORIGINAL_MP_FRONTEND_OK);
    release_gate(&frontend);
    assert(ge_original_multiplayer_frontend_request_start(&frontend) == GE_ORIGINAL_MP_FRONTEND_OK);
    unlocked = 0;
    memset(&record, 0xa5, sizeof(record));
    GeOriginalMultiplayerFrontendStart unchanged = record;
    assert(!ge_original_multiplayer_frontend_take_start(&frontend, &record));
    assert(memcmp(&unchanged, &record, sizeof(record)) == 0);
    assert(frontend.phase == GE_ORIGINAL_MP_FRONTEND_SETUP && frontend.status == GE_ORIGINAL_MP_FRONTEND_LOCKED);
    release_gate(&frontend);
    assert(!ge_original_multiplayer_frontend_init(NULL, NULL));
    assert(!ge_original_multiplayer_frontend_snapshot(NULL, NULL));
    assert(ge_original_multiplayer_frontend_input_edges(NULL, 1) == 0);
    assert(ge_original_multiplayer_frontend_request_start(NULL) == GE_ORIGINAL_MP_FRONTEND_INVALID);
    assert(!ge_original_multiplayer_frontend_take_start(NULL, NULL));
    assert(ge_original_multiplayer_frontend_finish(&frontend, NULL) == GE_ORIGINAL_MP_FRONTEND_INVALID);
}

static void test_end_reasons_and_four_seats(void)
{
    for (int reason = GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT;
            reason <= GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION; ++reason) {
        GeOriginalMultiplayerFrontend frontend;
        GeOriginalMultiplayerFrontendStart start;
        GeOriginalMultiplayerFrontendResult result = {0};
        assert(ge_original_multiplayer_frontend_init(&frontend, NULL));
        assert(ge_original_multiplayer_frontend_set(&frontend,
            GE_ORIGINAL_MP_FRONTEND_PLAYERS, 0, 4) == GE_ORIGINAL_MP_FRONTEND_OK);
        start = begin(&frontend);
        assert(start.selection.config.player_count == 4);
        for (int i = 0; i < 4; ++i) {
            GeOriginalMultiplayerFrontendCharacter expected;
            assert(ge_original_multiplayer_frontend_character((size_t)i, &expected));
            assert(memcmp(&start.characters[i], &expected, sizeof(expected)) == 0);
        }
        assert(ge_original_multiplayer_frontend_started(&frontend, start.request_id, 1)
            == GE_ORIGINAL_MP_FRONTEND_OK);
        result.request_id = start.request_id;
        result.player_count = 4;
        result.end_reason = GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION + 1;
        assert(ge_original_multiplayer_frontend_finish(&frontend, &result)
            == GE_ORIGINAL_MP_FRONTEND_INVALID);
        result.end_reason = (GeOriginalMultiplayerFrontendEndReason)reason;
        result.aborted = reason == GE_ORIGINAL_MP_FRONTEND_END_ABORT;
        result.winner_mask = result.aborted ? 0 : 5; /* Preserve core-supplied tied/team winners. */
        result.scores[0] = result.scores[2] = 5;
        result.scores[1] = result.scores[3] = 2;
        assert(ge_original_multiplayer_frontend_finish(&frontend, &result)
            == GE_ORIGINAL_MP_FRONTEND_OK);
        assert(memcmp(&frontend.result, &result, sizeof(result)) == 0);
        release_gate(&frontend);
        assert(ge_original_multiplayer_frontend_return_to_setup(&frontend)
            == GE_ORIGINAL_MP_FRONTEND_OK);
        release_gate(&frontend);
    }
}

int main(void)
{
    test_options();
    test_scenarios();
    test_lifecycle();
    test_pending_revalidation();
    test_end_reasons_and_four_seats();
    puts("Multiplayer frontend: authored options/restrictions, copied requests/results, stale IDs and release transitions passed");
    return 0;
}

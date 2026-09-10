#include "ge_original_multiplayer_death.h"
#include "ge_original_gun_frame_arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"

struct player *g_CurrentPlayer, *g_playerPointers[4];
struct player_data g_playerPlayerData[4], *g_playerPerm;
s32 player_num, g_ClockTimer;
f32 g_GlobalTimerDelta;
static unsigned mission_starts[4];
static _Alignas(16) unsigned char frame[0x28000];

/* Sound/music presentation is observed; real blood decode and model frame/end
 * accessors are linked below. Animation frame production belongs to MoveBond
 * and is supplied as fixture input, never by forcing completion flags. */
void set_missionstate(s32 state)
{
    assert(state == 6 && player_num >= 0 && player_num < 4);
    ++mission_starts[player_num];
}

typedef struct Result {
    unsigned blood_complete[4], animation_complete[4], fade_complete[4];
    int alpha[4];
    float fraction[4], elapsed[4];
} Result;

/* The decomp names the first word "Model *model", but this is the original
 * embedded Model storage, as used by the native gait owner and original code. */
static Model *animation(struct player *player)
{
    return (Model *)(void *)&player->model;
}

static Result run(unsigned count, unsigned display)
{
    struct player players[4] = {{0}};
    PropRecord props[4] = {{0}};
    ChrRecord chrs[4] = {{0}};
    GeOriginalMultiplayerDeathState state = {0};
    Result result = {0};
    unsigned tick, slot;
    memset(mission_starts, 0, sizeof(mission_starts));
    memset(g_playerPointers, 0, sizeof(g_playerPointers));
    for (slot = 0; slot < count; ++slot) {
        g_playerPointers[slot] = &players[slot];
        players[slot].prop = &props[slot];
        props[slot].chr = &chrs[slot];
        props[slot].type = PROP_TYPE_VIEWER;
        chrs[slot].prop = &props[slot];
        chrs[slot].fadealpha = 255;
        players[slot].colourfadetimemax60 = -1;
        players[slot].bondfadetimemax60 = -1;
        animation(&players[slot])->endframe = (float)(70+5*slot);
    }
    set_cur_player((s32)display);
    assert(ge_original_multiplayer_death_reset(&state));
    for (tick = 0; tick < 230; ++tick) {
        int paused = tick == 10 || tick == 11 || tick == 40 || tick == 100 || tick == 101;
        g_ClockTimer = !paused; g_GlobalTimerDelta = (float)!paused;
        for (slot = 0; slot < count; ++slot) {
            set_cur_player((s32)slot);
            if (tick == 3*slot) players[slot].bonddead = 1;
            assert(!ge_original_multiplayer_death_post_gun(&state, tick));
            assert(ge_original_multiplayer_death_pre_move(&state, tick));
            {
                float elapsed = players[slot].colourfadetime60;
                assert(!ge_original_multiplayer_death_pre_move(&state, tick));
                assert(!ge_original_multiplayer_death_pre_move(&state, tick+1));
                assert(players[slot].colourfadetime60 == elapsed);
            }
            if (!paused && players[slot].bonddead)
                animation(&players[slot])->animframe1 += 1.0f;
        }
        /* Display selection occurs between simulation phases and never
         * determines which players advance blood, animation completion/fade. */
        set_cur_player((s32)display);
        if (!tick) {
            unsigned char tiny[32];
            set_cur_player(0);
            assert(ge_original_gun_frame_arena_begin(tiny, sizeof(tiny)));
            assert(!ge_original_multiplayer_death_post_gun(&state, tick));
            assert(players[0].bonddead == 1 && !mission_starts[0]);
            assert(ge_original_gun_frame_arena_finalize(NULL));
        }
        assert(ge_original_gun_frame_arena_begin(frame, sizeof(frame)));
        for (slot = 0; slot < count; ++slot) {
            struct player *player = &players[slot];
            float before = player->colourfadetime60;
            set_cur_player((s32)slot);
            assert(ge_original_multiplayer_death_post_gun(&state, tick));
            if (player->redbloodfinished && !result.blood_complete[slot])
                result.blood_complete[slot] = tick;
            if (player->deathanimfinished && !result.animation_complete[slot]) {
                result.animation_complete[slot] = tick;
                assert(player->redbloodfinished);
                assert(animation(player)->animframe1 >= animation(player)->endframe);
                assert(player->colourfadetime60 == 0 && player->colourfadetimemax60 == 60);
                assert(player->bondfadetime60 == 0 && player->bondfadetimemax60 == 120);
            } else {
                assert(player->colourfadetime60 == before);
            }
            if (player->deathanimfinished && player->colourfadetimemax60 < 0
                    && !result.fade_complete[slot]) {
                result.fade_complete[slot] = tick;
                assert(player->colourfadetime60 == 60);
            }
            {
                u8 *blood_end = player->bloodImgNxt;
                float elapsed = player->colourfadetime60;
                assert(!ge_original_multiplayer_death_post_gun(&state, tick));
                assert(player->bloodImgNxt == blood_end && player->colourfadetime60 == elapsed);
            }
            if (paused && tick == 10) assert(!player->redbloodfinished);
        }
        assert(ge_original_gun_frame_arena_finalize(NULL));
    }
    for (slot = 0; slot < count; ++slot) {
        assert(mission_starts[slot] == 1);
        assert(result.blood_complete[slot] >= 44+3*slot);
        assert(result.animation_complete[slot] > result.blood_complete[slot]);
        assert(result.fade_complete[slot] == result.animation_complete[slot]+62);
        assert(players[slot].bonddead == 2); /* Respawn remains rules-owned. */
        assert(players[slot].deathanimfinished && players[slot].redbloodfinished);
        assert(players[slot].bondfadetimemax60 < 0);
        result.alpha[slot] = chrs[slot].fadealpha;
        result.fraction[slot] = players[slot].colourscreenfrac;
        result.elapsed[slot] = players[slot].colourfadetime60;
        assert(result.alpha[slot] == 0 && result.fraction[slot] == 1);
    }
    set_cur_player(0);
    assert(!ge_original_multiplayer_death_pre_move(&state, 231)); /* skipped tick */
    g_playerPointers[0] = NULL;
    assert(!ge_original_multiplayer_death_pre_move(&state, 230));
    return result;
}

int main(void)
{
    Result a = run(2, 0), b = run(2, 1);
    assert(!memcmp(&a, &b, sizeof(a)));
    a = run(4, 0); b = run(4, 3);
    assert(!memcmp(&a, &b, sizeof(a)));
    puts("canonical 2/4-player death: real blood decode, animation gate, 60/120 fades, pause/duplicate safety, display-independent progression");
    return 0;
}

#include "ge_original_multiplayer_hud.h"
#include <ultra64.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/gun.h"
#include "game/options.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct player players[4], before_draw[4];
struct player *g_playerPointers[4];
struct player *g_CurrentPlayer;
s32 g_ClockTimer = 3, g_UpperTextDisplayFlag, clock_drawn_flag = 1;
s32 g_gameOverFlag, cameraFrameCounter1, cameraFrameCounter2;
u32 watch_screen_index;
s32 mission_brief_index;
AmmoStats ammo_related[30];
static int count, current;
extern s32 upper_text_window_timer, status_bar_text_buffer_index;
extern s32 display_statusbar;
extern char stringbuffer_lowerleft[5][101];
extern void hudmsgTopShow(char *message);

s32 getPlayerCount(void) { return count; }
s32 get_cur_playernum(void) { return current; }
SCREEN_RATIO_OPTION get_screen_ratio(void) { return SCREEN_RATIO_NORMAL; }
ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand)
{ return hand == GUNRIGHT ? ITEM_WPPK : ITEM_UNARMED; }
s32 get_ammo_type_for_weapon(ITEM_IDS weapon)
{ return weapon == ITEM_UNARMED ? 0 : AMMO_9MM; }
u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask)
{ (void)item; (void)mask; return 0U; }

static void select_player(int slot)
{ current = slot; g_CurrentPlayer = &players[slot]; }

int main(void)
{
    GeOriginalMultiplayerHud state = {0};
    GeOriginalMultiplayerHudSnapshot saved[4], snapshot;
    uint64_t tick = 0U;
    unsigned frames = 0U;
    ammo_related[AMMO_9MM].IconImage = GE_ORIGINAL_AMMO_ICON_9MM_SEGMENTED_ADDRESS;
    for (count = 2; count <= 4; ++count) {
        for (int frame = 0; frame < 72; ++frame, ++tick, ++frames) {
            int expected_upper = -1;
            memset(players, 0, sizeof(players));
            ge_original_dam_mission_hud_reset();
            g_UpperTextDisplayFlag = frame % 11 == 0;
            g_gameOverFlag = frame % 7 == 0;
            cameraFrameCounter1 = frame % 9 == 0;
            cameraFrameCounter2 = frame % 13 == 0;
            watch_screen_index = WATCH_INDEX_MISSION_BRIEFING;
            mission_brief_index = BRIEF_INDEX_OBJECTIVES;
            hudmsgTopShow("Shared objective\n");
            for (int slot = 0; slot < count; ++slot) {
                char message[40];
                g_playerPointers[slot] = &players[slot];
                select_player(slot);
                snprintf(message, sizeof(message), "Player %d, frame %d\n", slot, frame);
                ge_original_hud_bottom_show_exact(message);
                players[slot].mpmenuon = (frame + slot) % 5 == 0;
                players[slot].hudmessoff = (frame + slot) % 6 == 0;
                players[slot].healthshowtime = (frame + slot) % 3 == 0 ? 30 : 0;
                players[slot].healthdisplaytime = (frame + slot) % 2 == 0 ? 2 : 10;
                if ((frame + slot) % 6 == 0) players[slot].healthdisplaytime = -1;
                players[slot].watch_animation_state = (frame + slot) % 4 == 0 ? 5 : 0;
                players[slot].cameramode = (frame + slot) % 17 == 0 ? 1 : 0;
                players[slot].apparenthealth = 0.1f * (float)(slot + 1);
                players[slot].apparentarmour = 0.15f * (float)(slot + 1);
                players[slot].crosshair_angle.f[0] = 100.0f + slot * 30.0f;
                players[slot].crosshair_angle.f[1] = 120.0f;
                players[slot].colourscreenred = 10 + slot;
                players[slot].colourscreengreen = 20 + slot;
                players[slot].colourscreenblue = 30 + slot;
                players[slot].colourscreenfrac = 0.1f * (slot + 1);
                players[slot].colourfadetime60 = 7.0f;
                players[slot].colourfadetimemax60 = 30.0f;
            }
            assert(display_statusbar == 0); /* Original MP enqueue never sets it. */
            assert(ge_original_multiplayer_hud_begin_frame(&state, tick, (uint32_t)count));
            assert(!ge_original_multiplayer_hud_begin_frame(&state, tick + 1U, (uint32_t)count));
            for (int ordinal = 0; ordinal < count; ++ordinal) {
                const int slot = (frame % count + count - ordinal) % count;
                struct player *p = &players[slot];
                const int intro = p->cameramode == 1;
                const int normal = p->healthshowtime > 0 && p->watch_animation_state == 0;
                const int alternate = !g_gameOverFlag && (p->mpmenuon | (p->healthdisplaytime > 0));
                const int health_timer = !intro && !normal && alternate
                    ? (p->healthdisplaytime > 3 ? p->healthdisplaytime - 3 : 0)
                    : p->healthdisplaytime;
                GeOriginalGameplayHudRenderSnapshot pre;
                select_player((slot + 1) % count);
                assert(!ge_original_multiplayer_hud_prepare_shooter(&state, (uint32_t)slot));
                select_player(slot);
                assert(ge_original_gameplay_hud_render_snapshot(&pre));
                assert(ge_original_multiplayer_hud_prepare_shooter(&state, (uint32_t)slot));
                assert(p->healthdisplaytime == health_timer);
                assert(!ge_original_multiplayer_hud_prepare_shooter(&state, (uint32_t)slot));
                assert(!ge_original_multiplayer_hud_get(&state, tick, (uint32_t)slot, &snapshot));
                /* A death/respawn owner may change these between draw phases. */
                p->apparenthealth = 0.99f;
                p->ammoheldarr[AMMO_9MM] = 100 + slot;
                p->hands[GUNRIGHT].weapon_ammo_in_magazine = 8 + slot;
                assert(ge_original_multiplayer_hud_capture_shooter(&state, (uint32_t)slot));
                assert(!ge_original_multiplayer_hud_capture_shooter(&state, (uint32_t)slot));
                assert(p->bondmesscnt == (p->mpmenuon || p->hudmessoff ? 120 : 117));
                if (!g_UpperTextDisplayFlag && !p->mpmenuon)
                    expected_upper = expected_upper < 0 ? 240 : expected_upper - 3;
                assert(upper_text_window_timer == expected_upper);
                assert(ge_original_multiplayer_hud_get(&state, tick, (uint32_t)slot, &saved[slot]));
                assert(saved[slot].upper.timer == expected_upper);
                assert(saved[slot].bottom.visible == (!p->mpmenuon && !p->hudmessoff));
                if (saved[slot].bottom.visible) {
                    assert(status_bar_text_buffer_index == slot);
                    assert(strcmp(saved[slot].bottom.message, stringbuffer_lowerleft[slot]) == 0);
                    assert(saved[slot].bottom.x == (count >= 3 && (slot & 1) ? 50 : 70));
                    assert(saved[slot].bottom.y == (count >= 3 ? 16 : 228 - (slot == 1 ? 8 : 0)));
                }
                assert(saved[slot].gameplay.gauges_visible == (!intro && (normal || alternate)));
                if (!intro) {
                    assert(memcmp(saved[slot].gameplay.health, pre.health, sizeof(pre.health)) == 0);
                    if (!p->mpmenuon) {
                        assert(saved[slot].gameplay.magazine_ammo == 8 + slot);
                        assert(saved[slot].gameplay.reserve_ammo == 100 + slot);
                    }
                }
                assert(saved[slot].watch_open == (p->watch_animation_state != 0));
                assert(saved[slot].sight_visible == (!intro && !p->mpmenuon));
                if (saved[slot].sight_visible)
                    assert(saved[slot].sight_draw.vertices[0].x == 84.0f + slot * 30.0f);
                assert(saved[slot].fade.fraction == ((cameraFrameCounter1 || cameraFrameCounter2)
                    ? 1.0f : p->colourscreenfrac));
                assert(saved[slot].fade.red == ((cameraFrameCounter1 || cameraFrameCounter2) ? 0 : 10 + slot));
                /* Subsequent shooter simulation can overwrite the live source. */
                memset(stringbuffer_lowerleft[slot], 'Z', 100);
                p->colourscreenfrac = 0.99f;
            }
            memcpy(before_draw, players, sizeof(players));
            for (int eye = 0; eye < 2; ++eye) {
                for (int local = 0; local < count; ++local) {
                    assert(ge_original_multiplayer_hud_get(&state, tick, (uint32_t)local, &snapshot));
                    assert(memcmp(&snapshot, &saved[local], sizeof(snapshot)) == 0);
                    assert(!ge_original_multiplayer_hud_get(&state, tick + 1U, (uint32_t)local, &snapshot));
                }
            }
            assert(memcmp(before_draw, players, sizeof(players)) == 0);
            assert(upper_text_window_timer == expected_upper);
        }
    }
    /* Failure after ticking poisons capture so retry cannot advance timers. */
    count = 2;
    assert(ge_original_multiplayer_hud_begin_frame(&state, tick, 2U));
    select_player(0);
    players[0].cameramode = players[0].mpmenuon = players[0].gunsightmode = 0;
    players[0].crosshair_angle.f[0] = NAN;
    assert(ge_original_multiplayer_hud_prepare_shooter(&state, 0U));
    assert(!ge_original_multiplayer_hud_capture_shooter(&state, 0U));
    const int lower = players[0].bondmesscnt, upper = upper_text_window_timer;
    assert(!ge_original_multiplayer_hud_capture_shooter(&state, 0U));
    assert(players[0].bondmesscnt == lower && upper_text_window_timer == upper);
    printf("multiplayer HUD: %u shuffled 2/3/4-player frames, exact timer guards, "
        "pre-death gauges, post-death ammo/fade/sight, deep copies and read-only eyes passed\n", frames);
    return 0;
}

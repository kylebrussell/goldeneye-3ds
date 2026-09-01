#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int32_t s32;
typedef uint64_t Gfx;
typedef struct Mtx { uint8_t bytes[64]; } Mtx;

extern Gfx *ge_original_draw_watch_current_page_exact(
    Gfx *gdl, Mtx *matrix, s32 transitioning);

s32 watch_screen_index;
static int rectangle_updates;
static int outside_watch;
static int transition_mode;
static int beep_count;
static uint32_t pressed;
static int page_calls[6];

void set_page_rectangle_colors(s32 index, void *vertices)
{
    assert(index == watch_screen_index && vertices != NULL);
    ++rectangle_updates;
}

void set_BONDdata_outside_watch_menu_flag(s32 value) { outside_watch = value; }
void sub_GAME_7F0BD8FC(s32 value) { transition_mode = value; }
uint32_t joyGetButtonsPressedThisFrame(s32 player, uint32_t mask)
{
    assert(player == 0 && mask == UINT32_C(0xa000));
    return pressed & mask;
}
void watch_play_beep_sound(void) { ++beep_count; }

#define PAGE_STUB(name, slot) \
    Gfx *name(Gfx *gdl, Mtx *matrix) { \
        assert(matrix != NULL); ++page_calls[slot]; return gdl + slot + 1; \
    }
PAGE_STUB(draw_watch_mission_status_page, 0)
PAGE_STUB(draw_watch_inventory_page, 1)
PAGE_STUB(draw_watch_control_options_page, 2)
PAGE_STUB(draw_watch_game_options_page, 3)
PAGE_STUB(draw_watch_mission_briefing_page, 4)
PAGE_STUB(draw_background_health_and_armor_transitioning, 5)

int main(void)
{
    Gfx commands[64];
    Mtx matrix;
    int page;

    memset(&matrix, 0, sizeof(matrix));
    for (page = 0; page < 5; ++page) {
        Gfx *result;
        memset(page_calls, 0, sizeof(page_calls));
        watch_screen_index = page;
        pressed = UINT32_C(0xa000);
        result = ge_original_draw_watch_current_page_exact(
            commands, &matrix, 1);
        assert(result == commands + page + 1);
        assert(page_calls[page] == 1);
        assert(outside_watch == 0 && transition_mode == 0);
        assert(beep_count == page + (page < 1 ? 1 : 0));
    }

    memset(page_calls, 0, sizeof(page_calls));
    watch_screen_index = 0;
    assert(ge_original_draw_watch_current_page_exact(
        commands, &matrix, 0) == commands + 6);
    assert(page_calls[5] == 1);
    assert(outside_watch == 1 && transition_mode == 1);
    assert(rectangle_updates == 6);
    puts("Exact watch page dispatcher preserves all five pages and transition path");
    return 0;
}

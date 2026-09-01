#include "ge_original_frontend_cursor.h"

#include <assert.h>
#include <math.h>
#include <string.h>

int main(void)
{
    GeOriginalFrontendCursor cursor;
    uint8_t unlocked[20];
    memset(unlocked, 0, sizeof(unlocked));
    unlocked[0] = unlocked[1] = unlocked[5] = 1U;
    ge_original_frontend_cursor_reset(&cursor);
    assert(cursor.x == 220.0f && cursor.y == 165.0f);
    ge_original_frontend_cursor_tick(&cursor, 5, -5, 1.0f);
    assert(cursor.x == 220.0f && cursor.y == 165.0f);
    ge_original_frontend_cursor_tick(&cursor, 70, 70, 1.0f);
    assert(fabsf(cursor.x - 225.375f) < 0.0001f);
    assert(fabsf(cursor.y - 159.625f) < 0.0001f);
    ge_original_frontend_cursor_set_mission(&cursor, 6);
    assert(cursor.x == 142.0f && cursor.y == 131.0f);
    assert(ge_original_frontend_cursor_mission(&cursor, unlocked) == 5);
    ge_original_frontend_cursor_set_difficulty(&cursor, 2);
    assert(cursor.x == 106.0f && cursor.y == 246.0f);
    assert(ge_original_frontend_cursor_difficulty(&cursor, 3) == 2);
    ge_original_frontend_cursor_set_next_tab(&cursor);
    assert(ge_original_frontend_cursor_on_next_tab(&cursor));
    assert(!ge_original_frontend_cursor_on_previous_tab(&cursor));
    cursor.y = 250.0f;
    assert(ge_original_frontend_cursor_on_previous_tab(&cursor));
    cursor.x = 1000.0f; cursor.y = -1000.0f;
    ge_original_frontend_cursor_tick(&cursor, 0, 0, 1.0f);
    assert(cursor.x == 420.0f && cursor.y == 20.0f);
    return 0;
}

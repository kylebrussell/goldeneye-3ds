#include "ge_original_frontend_cursor.h"

#include <stddef.h>

static const float ge_mission_x[5] = {73.0f, 142.0f, 212.0f, 282.0f, 352.0f};
static const float ge_mission_y[4] = {62.0f, 131.0f, 201.0f, 270.0f};

void ge_original_frontend_cursor_reset(GeOriginalFrontendCursor *cursor)
{
    if (cursor == NULL) return;
    cursor->x = 220.0f;
    cursor->y = 165.0f;
}

void ge_original_frontend_cursor_set_next_tab(
    GeOriginalFrontendCursor *cursor)
{
    if (cursor == NULL) return;
    cursor->x = 399.0f;
    cursor->y = 144.0f;
}

void ge_original_frontend_cursor_set_mission(
    GeOriginalFrontendCursor *cursor, int32_t mission)
{
    if (cursor == NULL || mission < 0 || mission >= 20) return;
    cursor->x = ge_mission_x[mission % 5];
    cursor->y = ge_mission_y[mission / 5];
}

void ge_original_frontend_cursor_set_difficulty(
    GeOriginalFrontendCursor *cursor, int32_t difficulty)
{
    if (cursor == NULL) return;
    cursor->x = 106.0f;
    cursor->y = (float)(difficulty * 0x1e + 0xba);
}

void ge_original_frontend_cursor_tick(GeOriginalFrontendCursor *cursor,
    int8_t stick_x, int8_t stick_y, float timer_delta)
{
    int x = stick_x;
    int y = -(int)stick_y;
    if (cursor == NULL) return;
    if (x < -5) x += 5;
    else if (x >= 6) x -= 5;
    else x = 0;
    if (x > 70) x = 70;
    else if (x < -70) x = -70;
    if (y < -5) y += 5;
    else if (y >= 6) y -= 5;
    else y = 0;
    if (y > 70) y = 70;
    else if (y < -70) y = -70;
    if (x > 0) cursor->x += ((float)x * 0.075f + 0.5f) * timer_delta;
    else if (x < 0) cursor->x += ((float)x * 0.075f - 0.5f) * timer_delta;
    if (y > 0) cursor->y += ((float)y * 0.075f + 0.5f) * timer_delta;
    else if (y < 0) cursor->y += ((float)y * 0.075f - 0.5f) * timer_delta;
    if (cursor->x > 420.0f) cursor->x = 420.0f;
    else if (cursor->x < 20.0f) cursor->x = 20.0f;
    if (cursor->y > 310.0f) cursor->y = 310.0f;
    else if (cursor->y < 20.0f) cursor->y = 20.0f;
}

int ge_original_frontend_cursor_on_previous_tab(
    const GeOriginalFrontendCursor *cursor)
{
    return cursor != NULL && cursor->x > 390.0f && cursor->y > 223.0f;
}

int ge_original_frontend_cursor_on_next_tab(
    const GeOriginalFrontendCursor *cursor)
{
    return cursor != NULL && cursor->x > 390.0f
        && cursor->y > 130.5f && cursor->y <= 223.0f;
}

int32_t ge_original_frontend_cursor_mission(
    const GeOriginalFrontendCursor *cursor,
    const uint8_t unlocked[20])
{
    int32_t column = 0;
    int32_t row = 0;
    int32_t scan;
    if (cursor == NULL || unlocked == NULL
            || ge_original_frontend_cursor_on_previous_tab(cursor)) return -1;
    while (column < 4 && cursor->x >=
            (ge_mission_x[column] + ge_mission_x[column + 1]) * 0.5f)
        ++column;
    while (row < 3 && cursor->y >=
            (ge_mission_y[row] + ge_mission_y[row + 1]) * 0.5f)
        ++row;
    while (row > 0) {
        for (scan = 0; scan < 5; ++scan)
            if (unlocked[row * 5 + scan]) break;
        if (scan < 5) break;
        --row;
    }
    for (scan = column; scan >= 0; --scan)
        if (unlocked[row * 5 + scan]) return row * 5 + scan;
    for (scan = 0; scan < 5; ++scan)
        if (unlocked[row * 5 + scan]) return row * 5 + scan;
    return -1;
}

int32_t ge_original_frontend_cursor_difficulty(
    const GeOriginalFrontendCursor *cursor, int32_t highest_unlocked)
{
    if (cursor == NULL || ge_original_frontend_cursor_on_previous_tab(cursor))
        return -1;
    if (highest_unlocked >= 3 && cursor->y >= 275.0f) return 3;
    if (highest_unlocked >= 2 && cursor->y >= 243.0f) return 2;
    if (highest_unlocked > 0 && cursor->y >= 211.0f) return 1;
    return 0;
}

int ge_original_frontend_wallet_bounds_from_top_screen(
    float left,float top,float right,float bottom,
    GeOriginalFrontendWalletBounds *bounds)
{
    if(bounds==NULL||!(left<=right)||!(top<=bottom))return 0;
    bounds->left=(left-40.0f)*(440.0f/320.0f);
    bounds->right=(right-40.0f)*(440.0f/320.0f);
    bounds->top=top*(330.0f/240.0f);
    bounds->bottom=bottom*(330.0f/240.0f);
    return 1;
}

int32_t ge_original_frontend_cursor_wallet(
    const GeOriginalFrontendCursor *cursor,
    const GeOriginalFrontendWalletBounds bounds[4])
{
    int32_t folder;
    if(cursor==NULL||bounds==NULL)return -1;
    for(folder=0;folder<4;++folder)
        if(bounds[folder].left<=cursor->x
                &&cursor->x<=bounds[folder].right
                &&bounds[folder].top<=cursor->y
                &&cursor->y<=bounds[folder].bottom)return folder;
    return -1;
}

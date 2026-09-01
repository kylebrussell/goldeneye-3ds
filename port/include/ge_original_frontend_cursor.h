#ifndef GE_ORIGINAL_FRONTEND_CURSOR_H
#define GE_ORIGINAL_FRONTEND_CURSOR_H

#include <stdint.h>

typedef struct GeOriginalFrontendCursor {
    float x;
    float y;
} GeOriginalFrontendCursor;

typedef struct GeOriginalFrontendWalletBounds {
    float left;
    float top;
    float right;
    float bottom;
} GeOriginalFrontendWalletBounds;

void ge_original_frontend_cursor_reset(GeOriginalFrontendCursor *cursor);
void ge_original_frontend_cursor_set_next_tab(
    GeOriginalFrontendCursor *cursor);
void ge_original_frontend_cursor_set_mission(
    GeOriginalFrontendCursor *cursor, int32_t mission);
void ge_original_frontend_cursor_set_difficulty(
    GeOriginalFrontendCursor *cursor, int32_t difficulty);
void ge_original_frontend_cursor_tick(GeOriginalFrontendCursor *cursor,
    int8_t stick_x, int8_t stick_y, float timer_delta);
int ge_original_frontend_cursor_on_previous_tab(
    const GeOriginalFrontendCursor *cursor);
int ge_original_frontend_cursor_on_next_tab(
    const GeOriginalFrontendCursor *cursor);
int32_t ge_original_frontend_cursor_mission(
    const GeOriginalFrontendCursor *cursor,
    const uint8_t unlocked[20]);
int32_t ge_original_frontend_cursor_difficulty(
    const GeOriginalFrontendCursor *cursor, int32_t highest_unlocked);
/* Converts the centered 320x240 3DS realization back into front.c's authored
 * 440x330 cursor space, then applies the exact inclusive rectangle test used
 * by interface_menu05_fileselect. */
int ge_original_frontend_wallet_bounds_from_top_screen(
    float left,float top,float right,float bottom,
    GeOriginalFrontendWalletBounds *bounds);
int32_t ge_original_frontend_cursor_wallet(
    const GeOriginalFrontendCursor *cursor,
    const GeOriginalFrontendWalletBounds bounds[4]);

#endif

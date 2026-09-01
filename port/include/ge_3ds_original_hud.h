#ifndef GE_3DS_ORIGINAL_HUD_H
#define GE_3DS_ORIGINAL_HUD_H

#include <stddef.h>
#include <stdint.h>

#include "ge_original_dam_mission_hud.h"
#include "ge_original_frontend_cursor.h"

#define GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH 128U
#define GE_3DS_ORIGINAL_HUD_ATLAS_HEIGHT 128U
#define GE_3DS_ORIGINAL_HUD_ATLAS_BYTES_PER_PIXEL 2U
#define GE_3DS_ORIGINAL_HUD_GLYPH_COUNT 94U
#define GE_3DS_ORIGINAL_HUD_VERTEX_CAPACITY 4096U
#define GE_3DS_ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_CAPACITY 256U
#define GE_3DS_ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_CAPACITY 512U

typedef struct Ge3dsOriginalHudGlyph {
    uint8_t baseline;
    uint8_t height;
    uint8_t width;
    uint8_t kerning_index;
    uint8_t atlas_x;
    uint8_t atlas_y;
} Ge3dsOriginalHudGlyph;

typedef struct Ge3dsOriginalHudAtlas {
    /* Exact ROM I8 coverage quantized into every nibble of native GPU_RGBA4
     * in PICA200 8x8 Morton-tiled layout.  Identical RGBA nibbles are endian
     * neutral and preserve four-bit antialiasing on the proven colour-texture
     * sampler path. */
    uint8_t pixels[GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH
                   * GE_3DS_ORIGINAL_HUD_ATLAS_HEIGHT
                   * GE_3DS_ORIGINAL_HUD_ATLAS_BYTES_PER_PIXEL];
    int8_t kerning[13U * 13U];
    Ge3dsOriginalHudGlyph glyphs[GE_3DS_ORIGINAL_HUD_GLYPH_COUNT];
    uint32_t nonzero_pixels;
    uint8_t ready;
} Ge3dsOriginalHudAtlas;

typedef struct Ge3dsOriginalHudVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
} Ge3dsOriginalHudVertex;

typedef struct Ge3dsOriginalHudDrawList {
    Ge3dsOriginalHudVertex vertices[GE_3DS_ORIGINAL_HUD_VERTEX_CAPACITY];
    /* Frontend-only subset superseded by the exact PwalletbondZ model pass.
     * Other HUD builders leave this zero. */
    size_t background_vertex_count;
    size_t box_vertex_count;
    size_t glyph_vertex_count;
    size_t tab_glyph_vertex_offset;
    size_t tab_glyph_vertex_count;
    size_t glyph_count;
    uint8_t visible;
    /* Frontend draw lists only: 0 is Zurich Bold, 1 is Bank Gothic. */
    uint8_t frontend_glyph_font;
} Ge3dsOriginalHudDrawList;

typedef struct Ge3dsOriginalWatchObjectiveLine {
    const char *text;
    const char *status_text;
    uint8_t menu;
    uint8_t status;
} Ge3dsOriginalWatchObjectiveLine;

typedef struct Ge3dsOriginalCreditsLine {
    const char *text;
    int16_t position;
    int16_t y;
    int16_t alignment;
} Ge3dsOriginalCreditsLine;

enum {
    GE_3DS_ORIGINAL_CREDITS_ALIGN_RIGHT = 0,
    GE_3DS_ORIGINAL_CREDITS_ALIGN_LEFT = 1,
    GE_3DS_ORIGINAL_CREDITS_ALIGN_CENTER = 2
};

typedef struct Ge3dsOriginalFrontendLine {
    const char *text;
    const char *value_text;
    uint8_t selected;
    uint8_t objective;
    uint8_t locked;
    int16_t x;
    int16_t y;
    uint8_t horizontal_align;
    uint8_t vertical_align;
    uint8_t has_authored_position;
    float value;
    uint8_t tab;
    int16_t value_x;
    uint8_t has_authored_color;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Ge3dsOriginalFrontendLine;

typedef enum Ge3dsOriginalFrontendTab {
    GE_3DS_ORIGINAL_FRONTEND_TAB_NONE = 0,
    GE_3DS_ORIGINAL_FRONTEND_TAB_START,
    GE_3DS_ORIGINAL_FRONTEND_TAB_NEXT,
    GE_3DS_ORIGINAL_FRONTEND_TAB_PREVIOUS
} Ge3dsOriginalFrontendTab;

typedef enum Ge3dsOriginalFrontendPage {
    GE_3DS_ORIGINAL_FRONTEND_PAGE_TITLE = 0,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_LEGAL,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_MODE_SELECT,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_MISSION_SELECT,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_DIFFICULTY,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_007_OPTIONS,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_BRIEFING,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_REPORT,
    GE_3DS_ORIGINAL_FRONTEND_PAGE_STATISTICS
} Ge3dsOriginalFrontendPage;

/* ROM image-table entries used by constructor_menu05_fileselect and
 * constructor_menu08_difficulty.  The platform renderer can use this list to
 * import the authored sprites without duplicating front.c's positions or
 * dimensions. */
typedef enum Ge3dsOriginalFrontendSpriteImage {
    GE_3DS_ORIGINAL_FRONTEND_SPRITE_COPY = 0,
    GE_3DS_ORIGINAL_FRONTEND_SPRITE_DELETE,
    GE_3DS_ORIGINAL_FRONTEND_SPRITE_SELECT_FILE,
    GE_3DS_ORIGINAL_FRONTEND_SPRITE_CROSS,
    GE_3DS_ORIGINAL_FRONTEND_SPRITE_CHECK,
    GE_3DS_ORIGINAL_FRONTEND_SPRITE_DOT
} Ge3dsOriginalFrontendSpriteImage;

typedef struct Ge3dsOriginalFrontendSprite {
    uint8_t image;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    int16_t center_x;
    int16_t center_y;
    uint16_t width;
    uint16_t height;
} Ge3dsOriginalFrontendSprite;

#define GE_3DS_ORIGINAL_FRONTEND_MAX_SPRITES 6U

typedef struct Ge3dsOriginalFrontendSpriteList {
    Ge3dsOriginalFrontendSprite sprites
        [GE_3DS_ORIGINAL_FRONTEND_MAX_SPRITES];
    size_t count;
    int16_t folder_background_x_offset;
} Ge3dsOriginalFrontendSpriteList;

/* Converts the exact ROM-derived Zurich Bold metadata and I8 glyph pixels to
 * the native PICA texture layout. No system/replacement font is involved. */
int ge_3ds_original_hud_build_atlas(Ge3dsOriginalHudAtlas *atlas);
int ge_3ds_original_hud_build_bank_gothic_atlas(
    Ge3dsOriginalHudAtlas *atlas);

/* Builds the platform triangles for the exact active upper-HUD message. The
 * N64 320-wide viewport is centered in the 400-wide 3DS top screen. */
int ge_3ds_original_hud_build_draw_list(
    const Ge3dsOriginalHudAtlas *atlas,
    const GeOriginalDamMissionHudRenderSnapshot *snapshot,
    Ge3dsOriginalHudDrawList *draw_list);

/* Test/read-only inverse of the PICA swizzle. */
uint8_t ge_3ds_original_hud_atlas_alpha(
    const Ge3dsOriginalHudAtlas *atlas, size_t x, size_t y);

/* Canonical VI top-down Y to the active tilted PICA projection. */
float ge_3ds_original_hud_screen_y(float canonical_y);

typedef struct Ge3dsOriginalGameplayHudDrawList {
    Ge3dsOriginalHudVertex solid_vertices
        [GE_3DS_ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_CAPACITY];
    Ge3dsOriginalHudVertex font_vertices
        [GE_3DS_ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_CAPACITY];
    size_t solid_vertex_count;
    size_t font_vertex_count;
    size_t gauge_segment_count;
    size_t ammo_glyph_count;
} Ge3dsOriginalGameplayHudDrawList;

int ge_3ds_original_gameplay_hud_build_draw_list(
    const Ge3dsOriginalHudAtlas *bank_gothic,
    const GeOriginalGameplayHudRenderSnapshot *snapshot,
    Ge3dsOriginalGameplayHudDrawList *draw_list);

int ge_3ds_original_bottom_hud_build_draw_list(
    const Ge3dsOriginalHudAtlas *bank_gothic,
    const GeOriginalBottomHudRenderSnapshot *snapshot,
    Ge3dsOriginalHudDrawList *draw_list);

/* PICA realization of the canonical watch Objectives subpage. Navigation,
 * filtering, text IDs and statuses remain owned by the original runtime. */
int ge_3ds_original_watch_objectives_build_draw_list(
    const Ge3dsOriginalHudAtlas *bank_gothic,
    const Ge3dsOriginalWatchObjectiveLine *lines, size_t line_count,
    Ge3dsOriginalHudDrawList *draw_list);

/* PICA realization of bondviewRenderCredits' Zurich Bold text pass. The
 * canonical body still owns frame selection, authored anchors/alignment and
 * completion state; this function only emits its visible glyphs. */
int ge_3ds_original_credits_build_draw_list(
    const Ge3dsOriginalHudAtlas *zurich,
    const Ge3dsOriginalCreditsLine *lines, size_t line_count,
    Ge3dsOriginalHudDrawList *draw_list);

/* PICA text/paper realization for a canonical frontend snapshot. Authored
 * text IDs, selection, navigation and stage requests remain frontend-owned. */
int ge_3ds_original_frontend_build_draw_list(
    const Ge3dsOriginalHudAtlas *zurich,
    Ge3dsOriginalFrontendPage page,
    const Ge3dsOriginalFrontendLine *lines, size_t line_count,
    Ge3dsOriginalHudDrawList *draw_list);

/* Exact-font variant. front.c uses Zurich Bold for dossier prose and Bank
 * Gothic for the 5x4 mission grid.  The compatibility entry point above is
 * retained for existing callers, while this function prevents the platform
 * from flattening both authored font passes into one atlas. */
int ge_3ds_original_frontend_build_draw_list_exact(
    const Ge3dsOriginalHudAtlas *zurich,
    const Ge3dsOriginalHudAtlas *bank_gothic,
    Ge3dsOriginalFrontendPage page,
    const Ge3dsOriginalFrontendLine *lines, size_t line_count,
    Ge3dsOriginalHudDrawList *draw_list);

/* Publishes the fixed image passes from front.c. Cursor motion and folder
 * model projection remain original-runtime state and are deliberately not
 * guessed here. completed_difficulties is a three-bit Agent/Secret/00 mask. */
int ge_3ds_original_frontend_build_sprite_list(
    Ge3dsOriginalFrontendPage page, uint8_t completed_difficulties,
    Ge3dsOriginalFrontendSpriteList *sprite_list);
int ge_3ds_original_frontend_append_cursor_sprite(
    Ge3dsOriginalFrontendSpriteList *sprite_list,
    float cursor_x,float cursor_y,uint8_t file_action);
int ge_3ds_original_frontend_file_action_bounds(
    const Ge3dsOriginalHudAtlas *zurich,
    const char *copy_text,const char *erase_text,
    GeOriginalFrontendWalletBounds bounds[2]);
const char *ge_3ds_original_frontend_sprite_resource(uint8_t image);

#endif

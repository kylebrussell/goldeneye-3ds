#include "ge_3ds_original_hud.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* These are the decomp's exact ROM-derived Zurich Bold tables. The original
 * file blob is kerning + 94 fontchar records + padding + I8 glyph pixels. */
extern uint32_t fontZurichBold_kerning[13U * 13U];
extern uint32_t fontZurichBold_fontchartable[];
extern uint32_t fontZurichBold_fontbytes[];
extern uint32_t fontBankGothic_kerning[13U * 13U];
extern uint32_t fontBankGothic_fontchartable[];
extern uint32_t fontBankGothic_fontbytes[];

enum {
    GE_FRONTEND_OBJECTIVE_COMPLETE = 1,
    GE_FONT_CHAR_WORDS = 6,
    /* Packed N64 font layout: 169 kerning words followed by 94 24-byte
     * fontchar records.  The first glyph's recorded offset is 0xb80, twelve
     * bytes into this pixel array; subtracting 0xb80 shears every glyph. */
    GE_FONT_PIXEL_ARRAY_FILE_OFFSET =
        13 * 13 * 4 + 94 * GE_FONT_CHAR_WORDS * 4,
    GE_HUD_VIEWPORT_LEFT = 40,
    GE_HUD_MESSAGE_X = GE_HUD_VIEWPORT_LEFT + 0x1e,
    GE_HUD_MESSAGE_Y = 0x0d,
    GE_HUD_VIEWPORT_RIGHT = GE_HUD_VIEWPORT_LEFT + 320,
    GE_HUD_SPACE_WIDTH = 5,
    GE_HUD_SCREEN_HEIGHT = 240
};

#define GE_HUD_SOLID_U \
    (0.5f / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH)
#define GE_HUD_SOLID_V \
    (1.0f - 0.5f / (float)GE_3DS_ORIGINAL_HUD_ATLAS_HEIGHT)

static void ge_hud_draw_list_reset(Ge3dsOriginalHudDrawList *draw_list)
{
    const size_t metadata = offsetof(Ge3dsOriginalHudDrawList, background_vertex_count);
    if (draw_list != NULL)
        memset((unsigned char *)draw_list + metadata, 0, sizeof(*draw_list) - metadata);
}

void ge_3ds_original_gameplay_hud_draw_list_reset(
    Ge3dsOriginalGameplayHudDrawList *draw_list)
{
    const size_t metadata = offsetof(Ge3dsOriginalGameplayHudDrawList, solid_vertex_count);
    if (draw_list != NULL)
        memset((unsigned char *)draw_list + metadata, 0, sizeof(*draw_list) - metadata);
}

/* GoldenEye's VI/HUD coordinates are top-down, while the current tilted
 * orthographic PICA projection consumes bottom-up Y coordinates. */
float ge_3ds_original_hud_screen_y(float canonical_y)
{
    return (float)GE_HUD_SCREEN_HEIGHT - canonical_y;
}

static size_t ge_morton8(size_t x, size_t y)
{
    return (x & 1U) | ((y & 1U) << 1U)
        | ((x & 2U) << 1U) | ((y & 2U) << 2U)
        | ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

static size_t ge_swizzled_offset(size_t x, size_t y)
{
    return (y & ~(size_t)7U) * GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH
        + (x & ~(size_t)7U) * 8U + ge_morton8(x & 7U, y & 7U);
}

static float ge_atlas_v(size_t y)
{
    /* The ROM glyph rows and atlas metadata are top-down. Tex3DS/PICA texture
     * coordinates use the lower edge as v=0, matching the full-texture
     * subtexture contract (top=1, bottom=0). */
    return 1.0f
        - (float)y / (float)GE_3DS_ORIGINAL_HUD_ATLAS_HEIGHT;
}

static uint8_t ge_font_byte(const uint32_t *font_bytes, size_t file_offset)
{
    size_t offset;
    uint32_t word;
    if (file_offset < GE_FONT_PIXEL_ARRAY_FILE_OFFSET) return 0U;
    offset = file_offset - GE_FONT_PIXEL_ARRAY_FILE_OFFSET;
    word = font_bytes[offset / 4U];
    return (uint8_t)(word >> (24U - 8U * (offset & 3U)));
}

static void ge_atlas_set(Ge3dsOriginalHudAtlas *atlas,
                         size_t x, size_t y, uint8_t alpha)
{
    const size_t offset = ge_swizzled_offset(x, y)
        * GE_3DS_ORIGINAL_HUD_ATLAS_BYTES_PER_PIXEL;
    const uint8_t replicated_nibble = (uint8_t)((alpha >> 4U) * 0x11U);
    atlas->pixels[offset] = replicated_nibble;
    atlas->pixels[offset + 1U] = replicated_nibble;
    if (replicated_nibble != 0U) atlas->nonzero_pixels++;
}

uint8_t ge_3ds_original_hud_atlas_alpha(
    const Ge3dsOriginalHudAtlas *atlas, size_t x, size_t y)
{
    if (atlas == NULL || x >= GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH
            || y >= GE_3DS_ORIGINAL_HUD_ATLAS_HEIGHT) return 0U;
    return atlas->pixels[ge_swizzled_offset(x, y)
        * GE_3DS_ORIGINAL_HUD_ATLAS_BYTES_PER_PIXEL];
}

static int ge_build_atlas(Ge3dsOriginalHudAtlas *atlas,
                          const uint32_t *font_kerning,
                          const uint32_t *font_chartable,
                          const uint32_t *font_bytes)
{
    size_t glyph_index;
    size_t x = 1U;
    size_t y = 1U;
    size_t row_height = 0U;
    if (atlas == NULL) return 0;
    memset(atlas, 0, sizeof(*atlas));
    for (glyph_index = 0U; glyph_index < 13U * 13U; ++glyph_index)
        atlas->kerning[glyph_index] =
            (int8_t)(int32_t)font_kerning[glyph_index];
    for (glyph_index = 0U;
            glyph_index < GE_3DS_ORIGINAL_HUD_GLYPH_COUNT;
            ++glyph_index) {
        const size_t word = glyph_index * GE_FONT_CHAR_WORDS;
        const size_t height = font_chartable[word + 2U];
        const size_t width = font_chartable[word + 3U];
        const size_t source_offset =
            font_chartable[word + 5U];
        const size_t source_stride = (width + 7U) & ~(size_t)7U;
        Ge3dsOriginalHudGlyph *glyph = &atlas->glyphs[glyph_index];
        size_t source_y;

        if (width == 0U || height == 0U || width > 15U || height > 15U)
            return 0;
        if (x + width + 1U > GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH) {
            x = 1U;
            y += row_height + 1U;
            row_height = 0U;
        }
        if (y + height + 1U > GE_3DS_ORIGINAL_HUD_ATLAS_HEIGHT)
            return 0;
        glyph->baseline =
            (uint8_t)font_chartable[word + 1U];
        glyph->height = (uint8_t)height;
        glyph->width = (uint8_t)width;
        glyph->kerning_index =
            (uint8_t)font_chartable[word + 4U];
        glyph->atlas_x = (uint8_t)x;
        glyph->atlas_y = (uint8_t)y;
        for (source_y = 0U; source_y < height; ++source_y) {
            size_t source_x;
            /* Keep the CPU atlas in the source font's top-down row order.
             * Glyph UV generation converts that to PICA's lower-left origin. */
            const size_t atlas_y = y + source_y;
            for (source_x = 0U; source_x < width; ++source_x)
                ge_atlas_set(atlas, x + source_x, atlas_y,
                    ge_font_byte(font_bytes, source_offset
                        + source_y * source_stride + source_x));
        }
        x += width + 1U;
        if (height > row_height) row_height = height;
    }
    /* Reserve the otherwise-empty corner texel as the renderer's opaque
     * modulation source. This lets solid HUD paper and ROM glyphs remain in
     * one PICA texture state instead of switching texture use between draws. */
    ge_atlas_set(atlas, 0U, 0U, UINT8_MAX);
    atlas->ready = atlas->nonzero_pixels != 0U;
    return atlas->ready != 0U;
}

int ge_3ds_original_hud_build_atlas(Ge3dsOriginalHudAtlas *atlas)
{
    return ge_build_atlas(atlas, fontZurichBold_kerning,
                          fontZurichBold_fontchartable,
                          fontZurichBold_fontbytes);
}

int ge_3ds_original_hud_build_bank_gothic_atlas(
    Ge3dsOriginalHudAtlas *atlas)
{
    return ge_build_atlas(atlas, fontBankGothic_kerning,
                          fontBankGothic_fontchartable,
                          fontBankGothic_fontbytes);
}

static int ge_emit_quad(Ge3dsOriginalHudDrawList *draw_list,
                        size_t *vertex_count,
                        float left, float top, float right, float bottom,
                        float u0, float v0, float u1, float v1,
                        float red, float green, float blue, float alpha)
{
    Ge3dsOriginalHudVertex *vertices;
    if (*vertex_count + 6U > GE_3DS_ORIGINAL_HUD_VERTEX_CAPACITY) return 0;
    vertices = draw_list->vertices + *vertex_count;
    top = ge_3ds_original_hud_screen_y(top);
    bottom = ge_3ds_original_hud_screen_y(bottom);
    vertices[0] = (Ge3dsOriginalHudVertex){left, top, 0.5f, u0, v0,
                                           red, green, blue, alpha};
    vertices[1] = (Ge3dsOriginalHudVertex){right, top, 0.5f, u1, v0,
                                           red, green, blue, alpha};
    vertices[2] = (Ge3dsOriginalHudVertex){right, bottom, 0.5f, u1, v1,
                                           red, green, blue, alpha};
    vertices[3] = vertices[0];
    vertices[4] = vertices[2];
    vertices[5] = (Ge3dsOriginalHudVertex){left, bottom, 0.5f, u0, v1,
                                           red, green, blue, alpha};
    *vertex_count += 6U;
    return 1;
}

int ge_3ds_original_hud_build_draw_list(
    const Ge3dsOriginalHudAtlas *atlas,
    const GeOriginalDamMissionHudRenderSnapshot *snapshot,
    Ge3dsOriginalHudDrawList *draw_list)
{
    const unsigned char *text;
    size_t vertex_count = 0U;
    int x = GE_HUD_MESSAGE_X;
    int y = GE_HUD_MESSAGE_Y;
    int saved_x = x;
    unsigned char previous = 'H';
    int line_height;
    if (draw_list == NULL) return 0;
    ge_hud_draw_list_reset(draw_list);
    if (atlas == NULL || atlas->ready == 0U || snapshot == NULL
            || snapshot->visible == 0U || snapshot->count == 0U
            || snapshot->messages[0][0] == '\0') return 1;

    line_height = atlas->glyphs['[' - 0x21].baseline
        + atlas->glyphs['[' - 0x21].height;
    /* Exact US upper-window background: viewport x=0..320, y-2..bottom,
     * colour 0x00000064. Give a one-line message its glyph line height; this
     * is the platform realization of the original fill before textRender. */
    if (!ge_emit_quad(draw_list, &vertex_count,
            (float)GE_HUD_VIEWPORT_LEFT, (float)(GE_HUD_MESSAGE_Y - 2),
            (float)GE_HUD_VIEWPORT_RIGHT,
            (float)(GE_HUD_MESSAGE_Y + line_height),
            GE_HUD_SOLID_U, GE_HUD_SOLID_V,
            GE_HUD_SOLID_U, GE_HUD_SOLID_V,
            0.0f, 0.0f, 0.0f, 100.0f / 255.0f)) return 0;
    draw_list->box_vertex_count = vertex_count;

    text = (const unsigned char *)snapshot->messages[0];
    while (*text != '\0') {
        const Ge3dsOriginalHudGlyph *glyph;
        const Ge3dsOriginalHudGlyph *previous_glyph;
        int kerning;
        float u0;
        float v0;
        float u1;
        float v1;
        if (*text == ' ') {
            x += GE_HUD_SPACE_WIDTH;
            previous = 'H';
            ++text;
            continue;
        }
        if (*text == '\n') {
            y += line_height;
            x = saved_x;
            previous = 'H';
            ++text;
            continue;
        }
        if (*text < 0x21U || *text > 0x7eU) {
            ++text;
            continue;
        }
        glyph = &atlas->glyphs[*text - 0x21U];
        previous_glyph = &atlas->glyphs[previous - 0x21U];
        kerning = atlas->kerning[previous_glyph->kerning_index * 13U
                                 + glyph->kerning_index];
        x -= kerning - 1;
        u0 = (float)glyph->atlas_x
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v0 = ge_atlas_v(glyph->atlas_y);
        u1 = (float)(glyph->atlas_x + glyph->width)
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v1 = ge_atlas_v(glyph->atlas_y + glyph->height);
        if (!ge_emit_quad(draw_list, &vertex_count,
                (float)x, (float)(y + glyph->baseline),
                (float)(x + glyph->width),
                (float)(y + glyph->baseline + glyph->height),
                u0, v0, u1, v1, 1.0f, 1.0f, 1.0f, 1.0f)) return 0;
        ++draw_list->glyph_count;
        x += glyph->width;
        previous = *text++;
    }
    draw_list->glyph_vertex_count = vertex_count
        - draw_list->box_vertex_count;
    draw_list->visible = draw_list->glyph_vertex_count != 0U;
    return 1;
}

static int ge_emit_gameplay_quad(Ge3dsOriginalHudVertex *vertices,
                                 size_t capacity, size_t *count,
                                 float left, float top,
                                 float right, float bottom,
                                 float u0, float v0, float u1, float v1,
                                 float red, float green,
                                 float blue, float alpha)
{
    Ge3dsOriginalHudVertex *quad;
    if (*count + 6U > capacity) return 0;
    quad = vertices + *count;
    top = ge_3ds_original_hud_screen_y(top);
    bottom = ge_3ds_original_hud_screen_y(bottom);
    quad[0] = (Ge3dsOriginalHudVertex){left, top, 0.5f, u0, v0,
                                       red, green, blue, alpha};
    quad[1] = (Ge3dsOriginalHudVertex){right, top, 0.5f, u1, v0,
                                       red, green, blue, alpha};
    quad[2] = (Ge3dsOriginalHudVertex){right, bottom, 0.5f, u1, v1,
                                       red, green, blue, alpha};
    quad[3] = quad[0];
    quad[4] = quad[2];
    quad[5] = (Ge3dsOriginalHudVertex){left, bottom, 0.5f, u0, v1,
                                       red, green, blue, alpha};
    *count += 6U;
    return 1;
}

static int ge_emit_gauge(const GeOriginalGameplayHudGaugeVertex *source,
                         Ge3dsOriginalGameplayHudDrawList *draw_list)
{
    size_t segment;
    for (segment = 0U; segment < 22U; ++segment) {
        const int drawn = segment < 9U ? (segment & 1U) == 0U
            : segment >= 10U && ((segment + 3U) % 4U) != 0U;
        const size_t indices[6] = {
            segment * 2U, segment * 2U + 1U, segment * 2U + 2U,
            segment * 2U + 1U, segment * 2U + 2U, segment * 2U + 3U,
        };
        size_t vertex;
        if (!drawn) continue;
        if (draw_list->solid_vertex_count + 6U
                > GE_3DS_ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_CAPACITY)
            return 0;
        for (vertex = 0U; vertex < 6U; ++vertex) {
            const GeOriginalGameplayHudGaugeVertex *input =
                &source[indices[vertex]];
            draw_list->solid_vertices[draw_list->solid_vertex_count++] =
                (Ge3dsOriginalHudVertex){
                    200.0f + (float)input->x * 0.2f,
                    ge_3ds_original_hud_screen_y(
                        120.0f + (float)input->z * 0.2f),
                    0.5f, GE_HUD_SOLID_U, GE_HUD_SOLID_V,
                    (float)input->red / 255.0f,
                    (float)input->green / 255.0f,
                    (float)input->blue / 255.0f,
                    (float)input->alpha / 255.0f,
                };
        }
        ++draw_list->gauge_segment_count;
    }
    return 1;
}

static int ge_text_width(const Ge3dsOriginalHudAtlas *font,
                         const char *text)
{
    unsigned char previous = 'H';
    int width = 0;
    while (*text != '\0') {
        const unsigned char value = (unsigned char)*text++;
        const Ge3dsOriginalHudGlyph *glyph;
        const Ge3dsOriginalHudGlyph *prior;
        if (value == ' ') {
            width += GE_HUD_SPACE_WIDTH;
            previous = 'H';
        } else if (value >= 0x21U && value <= 0x7eU) {
            glyph = &font->glyphs[value - 0x21U];
            prior = &font->glyphs[previous - 0x21U];
            width += glyph->width
                - (font->kerning[prior->kerning_index * 13U
                                  + glyph->kerning_index] - 1);
            previous = value;
        }
    }
    return width;
}

int ge_3ds_original_credits_build_draw_list(
    const Ge3dsOriginalHudAtlas *zurich,
    const Ge3dsOriginalCreditsLine *lines, size_t line_count,
    Ge3dsOriginalHudDrawList *draw_list)
{
    size_t vertex_count = 0U;
    size_t line_index;
    if (draw_list == NULL) return 0;
    ge_hud_draw_list_reset(draw_list);
    if (zurich == NULL || !zurich->ready || lines == NULL
            || line_count == 0U) return 1;

    for (line_index = 0U; line_index < line_count; ++line_index) {
        const Ge3dsOriginalCreditsLine *line = &lines[line_index];
        const unsigned char *cursor;
        unsigned char previous = 'H';
        int x;
        const int width = line->text != NULL
            ? ge_text_width(zurich, line->text) : 0;
        if (line->text == NULL) continue;
        /* Exact bondviewRenderCredits alignment branches. The authored
         * 320-wide VI viewport is centred on the 400-wide top screen. */
        if (line->alignment == GE_3DS_ORIGINAL_CREDITS_ALIGN_LEFT)
            x = GE_HUD_VIEWPORT_LEFT + line->position - width;
        else if (line->alignment == GE_3DS_ORIGINAL_CREDITS_ALIGN_CENTER)
            x = GE_HUD_VIEWPORT_LEFT + line->position - (width >> 1);
        else
            x = GE_HUD_VIEWPORT_LEFT + line->position;

        cursor = (const unsigned char *)line->text;
        while (*cursor != '\0' && *cursor != '\n') {
            const unsigned char value = *cursor++;
            const Ge3dsOriginalHudGlyph *glyph;
            const Ge3dsOriginalHudGlyph *prior;
            int kerning;
            float u0;
            float v0;
            float u1;
            float v1;
            if (value == ' ') {
                x += GE_HUD_SPACE_WIDTH;
                previous = 'H';
                continue;
            }
            if (value < 0x21U || value > 0x7eU) continue;
            glyph = &zurich->glyphs[value - 0x21U];
            prior = &zurich->glyphs[previous - 0x21U];
            kerning = zurich->kerning[prior->kerning_index * 13U
                                      + glyph->kerning_index];
            x -= kerning - 1;
            u0 = (float)glyph->atlas_x
                / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
            v0 = ge_atlas_v(glyph->atlas_y);
            u1 = (float)(glyph->atlas_x + glyph->width)
                / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
            v1 = ge_atlas_v(glyph->atlas_y + glyph->height);
            if (!ge_emit_quad(draw_list, &vertex_count,
                    (float)x, (float)(line->y + glyph->baseline),
                    (float)(x + glyph->width),
                    (float)(line->y + glyph->baseline + glyph->height),
                    u0, v0, u1, v1,
                    1.0f, 1.0f, 1.0f, 1.0f)) return 0;
            x += glyph->width;
            previous = value;
            ++draw_list->glyph_count;
        }
    }
    draw_list->glyph_vertex_count = vertex_count;
    draw_list->visible = vertex_count != 0U;
    return 1;
}

static int ge_emit_ammo_text_pass(
    const Ge3dsOriginalHudAtlas *font, const char *text,
    int start_x, int start_y, int offset_x, int offset_y,
    float red, float green, float blue,
    Ge3dsOriginalGameplayHudDrawList *draw_list)
{
    unsigned char previous = 'H';
    const unsigned char *cursor = (const unsigned char *)text;
    int x = start_x + offset_x;
    while (*cursor != '\0') {
        const unsigned char value = *cursor++;
        const Ge3dsOriginalHudGlyph *glyph;
        const Ge3dsOriginalHudGlyph *prior;
        int kerning;
        float u0;
        float v0;
        float u1;
        float v1;
        if (value == ' ') {
            x += GE_HUD_SPACE_WIDTH;
            previous = 'H';
            continue;
        }
        if (value < 0x21U || value > 0x7eU) continue;
        glyph = &font->glyphs[value - 0x21U];
        prior = &font->glyphs[previous - 0x21U];
        kerning = font->kerning[prior->kerning_index * 13U
                                + glyph->kerning_index];
        x -= kerning - 1;
        u0 = (float)glyph->atlas_x
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v0 = ge_atlas_v(glyph->atlas_y);
        u1 = (float)(glyph->atlas_x + glyph->width)
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v1 = ge_atlas_v(glyph->atlas_y + glyph->height);
        if (!ge_emit_gameplay_quad(draw_list->font_vertices,
                GE_3DS_ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_CAPACITY,
                &draw_list->font_vertex_count,
                (float)x, (float)(start_y + offset_y + glyph->baseline),
                (float)(x + glyph->width),
                (float)(start_y + offset_y + glyph->baseline
                        + glyph->height),
                u0, v0, u1, v1, red, green, blue, 1.0f)) return 0;
        x += glyph->width;
        previous = value;
        ++draw_list->ammo_glyph_count;
    }
    return 1;
}

static int ge_emit_ammo_text(const Ge3dsOriginalHudAtlas *font,
                             int value, int anchor_x, int y,
                             int right_aligned,
                             Ge3dsOriginalGameplayHudDrawList *draw_list)
{
    static const int offsets[5][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 0},
    };
    char text[12];
    int x;
    size_t pass;
    (void)snprintf(text, sizeof(text), "%d", value);
    x = right_aligned ? anchor_x - ge_text_width(font, text) : anchor_x;
    for (pass = 0U; pass < 5U; ++pass) {
        const float shade = pass == 4U ? 1.0f : 100.0f / 255.0f;
        if (!ge_emit_ammo_text_pass(font, text, x, y,
                offsets[pass][0], offsets[pass][1],
                shade, shade, shade, draw_list)) return 0;
    }
    return 1;
}

int ge_3ds_original_gameplay_hud_build_draw_list(
    const Ge3dsOriginalHudAtlas *bank_gothic,
    const GeOriginalGameplayHudRenderSnapshot *snapshot,
    Ge3dsOriginalGameplayHudDrawList *draw_list)
{
    if (draw_list == NULL) return 0;
    ge_3ds_original_gameplay_hud_draw_list_reset(draw_list);
    if (bank_gothic == NULL || !bank_gothic->ready || snapshot == NULL)
        return 0;
    if (snapshot->gauges_visible
            && (!ge_emit_gauge(snapshot->armour, draw_list)
                || !ge_emit_gauge(snapshot->health, draw_list))) return 0;
    if (snapshot->ammo_visible) {
        if (!ge_emit_ammo_text(bank_gothic, snapshot->magazine_ammo,
                snapshot->magazine_x, snapshot->ammo_y, 1, draw_list))
            return 0;
        if (snapshot->reserve_visible
                && !ge_emit_ammo_text(bank_gothic, snapshot->reserve_ammo,
                    snapshot->reserve_x, snapshot->ammo_y, 0, draw_list))
            return 0;
    }
    if (snapshot->left_ammo_visible) {
        if (!ge_emit_ammo_text(bank_gothic, snapshot->left_magazine_ammo,
                snapshot->left_magazine_x, snapshot->ammo_y, 0, draw_list))
            return 0;
        if (snapshot->left_reserve_visible
                && !ge_emit_ammo_text(bank_gothic,
                    snapshot->left_reserve_ammo,
                    snapshot->left_reserve_x, snapshot->ammo_y, 1,
                    draw_list))
            return 0;
    }
    return 1;
}

static int ge_emit_bottom_text_pass(
    const Ge3dsOriginalHudAtlas *font, const char *text,
    int start_x, int start_y, int offset_x, int offset_y,
    float shade, Ge3dsOriginalHudDrawList *draw_list,
    size_t *vertex_count)
{
    const unsigned char *cursor = (const unsigned char *)text;
    unsigned char previous = 'H';
    int x = start_x + offset_x;
    int y = start_y + offset_y;
    const int line_height = font->glyphs['[' - 0x21].baseline
        + font->glyphs['[' - 0x21].height;
    while (*cursor != '\0') {
        const unsigned char value = *cursor++;
        const Ge3dsOriginalHudGlyph *glyph;
        const Ge3dsOriginalHudGlyph *prior;
        int kerning;
        float u0;
        float v0;
        float u1;
        float v1;
        if (value == ' ') {
            x += GE_HUD_SPACE_WIDTH;
            previous = 'H';
            continue;
        }
        if (value == '\n') {
            x = start_x + offset_x;
            y += line_height;
            previous = 'H';
            continue;
        }
        if (value < 0x21U || value > 0x7eU) continue;
        glyph = &font->glyphs[value - 0x21U];
        prior = &font->glyphs[previous - 0x21U];
        kerning = font->kerning[prior->kerning_index * 13U
                                + glyph->kerning_index];
        x -= kerning - 1;
        u0 = (float)glyph->atlas_x
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v0 = ge_atlas_v(glyph->atlas_y);
        u1 = (float)(glyph->atlas_x + glyph->width)
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v1 = ge_atlas_v(glyph->atlas_y + glyph->height);
        if (!ge_emit_quad(draw_list, vertex_count,
                (float)x, (float)(y + glyph->baseline),
                (float)(x + glyph->width),
                (float)(y + glyph->baseline + glyph->height),
                u0, v0, u1, v1, shade, shade, shade, 1.0f)) return 0;
        x += glyph->width;
        previous = value;
        ++draw_list->glyph_count;
    }
    return 1;
}

int ge_3ds_original_bottom_hud_build_draw_list(
    const Ge3dsOriginalHudAtlas *bank_gothic,
    const GeOriginalBottomHudRenderSnapshot *snapshot,
    Ge3dsOriginalHudDrawList *draw_list)
{
    static const int offsets[5][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 0},
    };
    size_t vertex_count = 0U;
    int width;
    int line_height;
    int lines = 0;
    const char *cursor;
    size_t pass;
    if (draw_list == NULL) return 0;
    ge_hud_draw_list_reset(draw_list);
    if (bank_gothic == NULL || !bank_gothic->ready || snapshot == NULL
            || !snapshot->visible || snapshot->message[0] == '\0') return 1;
    width = ge_text_width(bank_gothic, snapshot->message);
    line_height = bank_gothic->glyphs['[' - 0x21].baseline
        + bank_gothic->glyphs['[' - 0x21].height;
    for (cursor = snapshot->message; *cursor != '\0'; ++cursor)
        if (*cursor == '\n') ++lines;
    if (!ge_emit_quad(draw_list, &vertex_count,
            (float)snapshot->x,
            (float)(snapshot->y - lines * line_height),
            (float)(snapshot->x + width), (float)snapshot->y,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f)) return 0;
    draw_list->box_vertex_count = vertex_count;
    for (pass = 0U; pass < 5U; ++pass) {
        const float shade = pass == 4U ? 1.0f : 100.0f / 255.0f;
        if (!ge_emit_bottom_text_pass(bank_gothic, snapshot->message,
                snapshot->x, snapshot->y - lines * line_height,
                offsets[pass][0], offsets[pass][1], shade,
                draw_list, &vertex_count)) return 0;
    }
    draw_list->glyph_vertex_count = vertex_count
        - draw_list->box_vertex_count;
    draw_list->visible = draw_list->glyph_vertex_count != 0U;
    return 1;
}

static int ge_emit_watch_text(
    const Ge3dsOriginalHudAtlas *font, const char *text,
    int start_x, int start_y, float red, float green, float blue,
    Ge3dsOriginalHudDrawList *draw_list, size_t *vertex_count)
{
    const unsigned char *cursor = (const unsigned char *)text;
    unsigned char previous = 'H';
    int x = start_x;
    while (cursor != NULL && *cursor != '\0' && *cursor != '\n') {
        const unsigned char value = *cursor++;
        const Ge3dsOriginalHudGlyph *glyph;
        const Ge3dsOriginalHudGlyph *prior;
        int kerning;
        float u0, v0, u1, v1;
        if (value == ' ') {
            x += GE_HUD_SPACE_WIDTH;
            previous = 'H';
            continue;
        }
        if (value < 0x21U || value > 0x7eU) continue;
        glyph = &font->glyphs[value - 0x21U];
        prior = &font->glyphs[previous - 0x21U];
        kerning = font->kerning[prior->kerning_index * 13U
                                + glyph->kerning_index];
        x -= kerning - 1;
        u0 = (float)glyph->atlas_x
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v0 = ge_atlas_v(glyph->atlas_y);
        u1 = (float)(glyph->atlas_x + glyph->width)
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v1 = ge_atlas_v(glyph->atlas_y + glyph->height);
        if (!ge_emit_quad(draw_list, vertex_count,
                (float)x, (float)(start_y + glyph->baseline),
                (float)(x + glyph->width),
                (float)(start_y + glyph->baseline + glyph->height),
                u0, v0, u1, v1, red, green, blue, 1.0f)) return 0;
        x += glyph->width;
        previous = value;
        ++draw_list->glyph_count;
    }
    return 1;
}

int ge_3ds_original_watch_objectives_build_draw_list(
    const Ge3dsOriginalHudAtlas *bank_gothic,
    const Ge3dsOriginalWatchObjectiveLine *lines, size_t line_count,
    Ge3dsOriginalHudDrawList *draw_list)
{
    size_t vertex_count = 0U;
    size_t index;
    if (draw_list == NULL) return 0;
    ge_hud_draw_list_reset(draw_list);
    if (bank_gothic == NULL || !bank_gothic->ready
            || lines == NULL || line_count == 0U) return 1;
    /* The N64 page uses the centered 320-wide VI region. Keep those authored
     * coordinates and reserve the 40-pixel 3DS side gutters. */
    if (!ge_emit_quad(draw_list, &vertex_count,
            48.0f, 18.0f, 352.0f, 222.0f,
            GE_HUD_SOLID_U, GE_HUD_SOLID_V,
            GE_HUD_SOLID_U, GE_HUD_SOLID_V,
            0.0f, 0.10f, 0.0f, 220.0f / 255.0f)) return 0;
    draw_list->box_vertex_count = vertex_count;
    if (!ge_emit_watch_text(bank_gothic, "MISSION OBJECTIVES",
            105, 30, 160.0f / 255.0f, 1.0f, 160.0f / 255.0f,
            draw_list, &vertex_count)) return 0;
    for (index = 0U; index < line_count; ++index) {
        char label[4] = {(char)('a' + lines[index].menu), '.', '\0', '\0'};
        const int y = 64 + (int)index * 34;
        float status_red = 1.0f;
        float status_green = 160.0f / 255.0f;
        float status_blue = 0.0f;
        if (lines[index].status == 1U) {
            status_red = 160.0f / 255.0f;
            status_green = 1.0f;
            status_blue = 160.0f / 255.0f;
        } else if (lines[index].status == 2U) {
            status_red = 1.0f;
            status_green = 0.15f;
            status_blue = 0.10f;
        }
        if (!ge_emit_watch_text(bank_gothic, label, 64, y,
                0.0f, 1.0f, 0.0f, draw_list, &vertex_count)
                || !ge_emit_watch_text(bank_gothic, lines[index].text,
                    82, y, 0.0f, 1.0f, 0.0f,
                    draw_list, &vertex_count)
                || !ge_emit_watch_text(bank_gothic,
                    lines[index].status_text, 230, y,
                    status_red, status_green, status_blue,
                    draw_list, &vertex_count)) return 0;
    }
    draw_list->glyph_vertex_count = vertex_count
        - draw_list->box_vertex_count;
    draw_list->visible = draw_list->glyph_vertex_count != 0U;
    return 1;
}

static int ge_frontend_word_width(const Ge3dsOriginalHudAtlas *font,
                                  const unsigned char *text)
{
    char word[96];
    size_t length = 0U;
    while (text[length] != '\0' && text[length] != ' '
            && text[length] != '\n' && length + 1U < sizeof(word)) {
        word[length] = (char)text[length];
        ++length;
    }
    word[length] = '\0';
    return ge_text_width(font, word);
}

static int ge_frontend_text_height(const Ge3dsOriginalHudAtlas *font,
                                   const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int height = 0;
    while (cursor != NULL && *cursor != '\0' && *cursor != '\n') {
        const unsigned char value = *cursor++;
        if (value >= 0x21U && value <= 0x7eU) {
            const Ge3dsOriginalHudGlyph *glyph =
                &font->glyphs[value - 0x21U];
            const int bottom = glyph->baseline + glyph->height;
            if (bottom > height) height = bottom;
        }
    }
    return height;
}

static int ge_emit_frontend_wrapped_text(
    const Ge3dsOriginalHudAtlas *font, const char *text,
    int start_x, int *start_y, int right_x,
    float red, float green, float blue,
    Ge3dsOriginalHudDrawList *draw_list, size_t *vertex_count)
{
    const unsigned char *cursor = (const unsigned char *)text;
    unsigned char previous = 'H';
    const int line_height = font->glyphs['[' - 0x21].baseline
        + font->glyphs['[' - 0x21].height + 2;
    int x = start_x;
    int y = *start_y;
    while (cursor != NULL && *cursor != '\0' && y < 222) {
        const unsigned char value = *cursor;
        const Ge3dsOriginalHudGlyph *glyph;
        const Ge3dsOriginalHudGlyph *prior;
        int kerning;
        float u0, v0, u1, v1;
        if (value == '\n') {
            x = start_x;
            y += line_height;
            previous = 'H';
            ++cursor;
            continue;
        }
        if (value == ' ') {
            if (x + GE_HUD_SPACE_WIDTH
                    + ge_frontend_word_width(font, cursor + 1U) > right_x) {
                x = start_x;
                y += line_height;
            } else {
                x += GE_HUD_SPACE_WIDTH;
            }
            previous = 'H';
            ++cursor;
            continue;
        }
        ++cursor;
        if (value < 0x21U || value > 0x7eU) continue;
        glyph = &font->glyphs[value - 0x21U];
        prior = &font->glyphs[previous - 0x21U];
        kerning = font->kerning[prior->kerning_index * 13U
                                + glyph->kerning_index];
        if (x - (kerning - 1) + glyph->width > right_x) {
            x = start_x;
            y += line_height;
            previous = 'H';
            prior = &font->glyphs[previous - 0x21U];
            kerning = font->kerning[prior->kerning_index * 13U
                                    + glyph->kerning_index];
        }
        x -= kerning - 1;
        u0 = (float)glyph->atlas_x
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v0 = ge_atlas_v(glyph->atlas_y);
        u1 = (float)(glyph->atlas_x + glyph->width)
            / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v1 = ge_atlas_v(glyph->atlas_y + glyph->height);
        if (!ge_emit_quad(draw_list, vertex_count,
                (float)x, (float)(y + glyph->baseline),
                (float)(x + glyph->width),
                (float)(y + glyph->baseline + glyph->height),
                u0, v0, u1, v1, red, green, blue, 1.0f)) return 0;
        x += glyph->width;
        previous = value;
        ++draw_list->glyph_count;
    }
    *start_y = y + line_height;
    return 1;
}

/* front.c authors its folder renderer in a 440x330 menu space. The 3DS top
 * screen preserves the complete composition by scaling that space into the
 * centred 320x240 region rather than reflowing it as a platform-native list. */
static float ge_frontend_x(float original_x)
{
    return 40.0f + original_x * (320.0f / 440.0f);
}

static float ge_frontend_y(float original_y)
{
    return original_y * (240.0f / 330.0f);
}

static int ge_emit_frontend_box(
    Ge3dsOriginalHudDrawList *draw_list, size_t *vertex_count,
    float left, float top, float right, float bottom,
    float red, float green, float blue, float alpha)
{
    return ge_emit_quad(draw_list, vertex_count,
        ge_frontend_x(left), ge_frontend_y(top),
        ge_frontend_x(right), ge_frontend_y(bottom),
        GE_HUD_SOLID_U, GE_HUD_SOLID_V,
        GE_HUD_SOLID_U, GE_HUD_SOLID_V,
        red, green, blue, alpha);
}

static int ge_emit_frontend_tab_text(
    const Ge3dsOriginalHudAtlas *font,
    const Ge3dsOriginalFrontendLine *line,
    Ge3dsOriginalHudDrawList *draw_list,size_t *vertex_count)
{
    const float scale=320.0f/440.0f;
    const float centers[4]={0.0f,84.0f,177.0f,269.0f};
    const unsigned char *cursor=(const unsigned char *)line->text;
    unsigned char previous='H';
    const int width=ge_text_width(font,line->text);
    const int height=ge_frontend_text_height(font,line->text);
    const float authored_right=411.0f-(float)height*0.5f;
    float authored_y=centers[line->tab]-(float)width*0.5f;
    while(cursor!=NULL&&*cursor!='\0'&&*cursor!='\n'){
        const unsigned char value=*cursor++;
        const Ge3dsOriginalHudGlyph *glyph;
        const Ge3dsOriginalHudGlyph *prior;
        Ge3dsOriginalHudVertex *vertices;
        float left,top,right,bottom,u0,v0,u1,v1;
        int kerning;
        if(value==' '){authored_y+=4.0f;previous='H';continue;}
        if(value<0x21U||value>0x7eU)continue;
        glyph=&font->glyphs[value-0x21U];
        prior=&font->glyphs[previous-0x21U];
        kerning=font->kerning[prior->kerning_index*13U
            +glyph->kerning_index];
        authored_y-=(float)(kerning-1);
        left=ge_frontend_x(authored_right
            -(float)(glyph->baseline+glyph->height));
        right=ge_frontend_x(authored_right-(float)glyph->baseline);
        top=ge_frontend_y(authored_y);
        bottom=ge_frontend_y(authored_y+(float)glyph->width);
        u0=(float)glyph->atlas_x/(float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v0=ge_atlas_v(glyph->atlas_y);
        u1=(float)(glyph->atlas_x+glyph->width)
            /(float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH;
        v1=ge_atlas_v(glyph->atlas_y+glyph->height);
        if(*vertex_count+6U>GE_3DS_ORIGINAL_HUD_VERTEX_CAPACITY)return 0;
        vertices=draw_list->vertices+*vertex_count;
        top=ge_3ds_original_hud_screen_y(top);
        bottom=ge_3ds_original_hud_screen_y(bottom);
        /* ROT_90CW: destination TL/TR/BR/BL sample source BL/TL/TR/BR. */
        vertices[0]=(Ge3dsOriginalHudVertex){left,top,0.5f,u0,v1,
            18.0f/255.0f,18.0f/255.0f,18.0f/255.0f,1.0f};
        vertices[1]=(Ge3dsOriginalHudVertex){right,top,0.5f,u0,v0,
            18.0f/255.0f,18.0f/255.0f,18.0f/255.0f,1.0f};
        vertices[2]=(Ge3dsOriginalHudVertex){right,bottom,0.5f,u1,v0,
            18.0f/255.0f,18.0f/255.0f,18.0f/255.0f,1.0f};
        vertices[3]=vertices[0];vertices[4]=vertices[2];
        vertices[5]=(Ge3dsOriginalHudVertex){left,bottom,0.5f,u1,v1,
            18.0f/255.0f,18.0f/255.0f,18.0f/255.0f,1.0f};
        *vertex_count+=6U;
        authored_y+=(float)glyph->width;
        previous=value;++draw_list->glyph_count;
    }
    (void)scale;
    return 1;
}

static int ge_emit_frontend_text_at(
    const Ge3dsOriginalHudAtlas *font, const char *text,
    float original_x, float original_y, float original_right,
    float red, float green, float blue,
    Ge3dsOriginalHudDrawList *draw_list, size_t *vertex_count)
{
    int y = (int)ge_frontend_y(original_y);
    return ge_emit_frontend_wrapped_text(font, text,
        (int)ge_frontend_x(original_x), &y,
        (int)ge_frontend_x(original_right),
        red, green, blue, draw_list, vertex_count);
}

static int ge_emit_frontend_text_centered(
    const Ge3dsOriginalHudAtlas *font, const char *text,
    float original_center_x, float original_y,
    float red, float green, float blue,
    Ge3dsOriginalHudDrawList *draw_list, size_t *vertex_count)
{
    const float scale = 320.0f / 440.0f;
    const int width = ge_text_width(font, text);
    int y = (int)ge_frontend_y(original_y);
    const int x = (int)(ge_frontend_x(original_center_x)
        - (float)width * 0.5f);
    return ge_emit_frontend_wrapped_text(font, text, x, &y,
        (int)(x + (float)width + 16.0f * scale),
        red, green, blue, draw_list, vertex_count);
}

static int ge_emit_frontend_legal_text(
    const Ge3dsOriginalHudAtlas *font, const Ge3dsOriginalFrontendLine *line,
    Ge3dsOriginalHudDrawList *draw_list, size_t *vertex_count)
{
    const float scale = 240.0f / 330.0f;
    const size_t first_vertex = *vertex_count;
    const float center_or_left = ge_frontend_x((float)line->x);
    const float authored_y = ge_frontend_y((float)line->y);
    const float top = line->vertical_align != 0U
        ? authored_y
            - (float)ge_frontend_text_height(font, line->text)
                * scale * 0.5f
        : authored_y;
    const float bottom_up_anchor = ge_3ds_original_hud_screen_y(top);
    const float width = (float)ge_text_width(font, line->text) * scale;
    const float left = line->horizontal_align != 0U
        ? center_or_left - width * 0.5f : center_or_left;
    int y = (int)top;
    size_t vertex;
    if (!ge_emit_frontend_wrapped_text(font, line->text,
            (int)left, &y, 10000, 1.0f, 1.0f, 1.0f,
            draw_list, vertex_count)) return 0;
    /* The menu's 440x330 VI composition is reduced to the centred 320x240
     * top-screen viewport. Other dossier pages intentionally retain their
     * larger readable type, but the legal constructor authored twelve fixed
     * unwrapped baselines and therefore requires the same uniform VI scale
     * for glyph geometry as for its positions. */
    for (vertex = first_vertex; vertex < *vertex_count; ++vertex) {
        draw_list->vertices[vertex].x = left
            + (draw_list->vertices[vertex].x - left) * scale;
        /* ge_emit_quad has already converted top-down HUD coordinates into
         * PICA's bottom-up target space.  Scale around the correspondingly
         * converted anchor; mixing the authored top-down anchor with those
         * vertices collapsed the legal lines toward the middle of screen. */
        draw_list->vertices[vertex].y = bottom_up_anchor
            + (draw_list->vertices[vertex].y - bottom_up_anchor) * scale;
    }
    return 1;
}

static int ge_emit_frontend_objective_rows(
    const Ge3dsOriginalHudAtlas *font,
    const Ge3dsOriginalFrontendLine *lines, size_t first, size_t line_count,
    float first_y, int report,
    Ge3dsOriginalHudDrawList *draw_list, size_t *vertex_count)
{
    const float ink = 18.0f / 255.0f;
    const float scale = 240.0f / 330.0f;
    float authored_y = first_y;
    size_t index;
    unsigned objective = 0U;
    for (index = first; index < line_count; ++index) {
        char prefix[3] = {(char)('a' + objective), '.', '\0'};
        int scaled_y;
        int next_y;
        if (lines[index].tab != GE_3DS_ORIGINAL_FRONTEND_TAB_NONE
                || lines[index].has_authored_position
                || !lines[index].objective) continue;
        if (!ge_emit_frontend_text_at(font, prefix,
                55.0f, authored_y, 74.0f, ink, ink, ink,
                draw_list, vertex_count)) return 0;
        scaled_y = (int)ge_frontend_y(authored_y);
        next_y = scaled_y;
        if (!ge_emit_frontend_wrapped_text(font, lines[index].text,
                (int)ge_frontend_x(75.0f), &next_y,
                (int)ge_frontend_x(report ? 295.0f : 395.0f),
                ink, ink, ink, draw_list, vertex_count)) return 0;
        if (report && lines[index].value_text != NULL) {
            const int complete = lines[index].locked
                == GE_FRONTEND_OBJECTIVE_COMPLETE;
            if (!ge_emit_frontend_text_at(font, lines[index].value_text,
                    310.0f, authored_y, 395.0f,
                    complete ? ink : 120.0f / 255.0f,
                    complete ? ink : 0.0f,
                    complete ? ink : 0.0f,
                    draw_list, vertex_count)) return 0;
        }
        authored_y += (float)(next_y - scaled_y) / scale;
        ++objective;
    }
    return 1;
}

static int ge_emit_frontend_folder_background(
    Ge3dsOriginalHudDrawList *draw_list, size_t *vertex_count,
    int paper)
{
    /* These layers are the PICA realization of clear_framebuffer_black plus
     * frontSetupMenuBackground. The exact wallet mesh is still a separate
     * model pass, but the authored black border, dossier, paper and tabs no
     * longer collapse into the old generic green rectangle. */
    if (!ge_emit_frontend_box(draw_list, vertex_count,
            0.0f, 0.0f, 440.0f, 330.0f,
            20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f)
            || !ge_emit_frontend_box(draw_list, vertex_count,
            17.0f, 18.0f, 427.0f, 322.0f,
            73.0f / 255.0f, 65.0f / 255.0f, 43.0f / 255.0f, 1.0f)
            || !ge_emit_frontend_box(draw_list, vertex_count,
            27.0f, 28.0f, 417.0f, 312.0f,
            112.0f / 255.0f, 98.0f / 255.0f, 63.0f / 255.0f, 1.0f))
        return 0;
    if (paper && !ge_emit_frontend_box(draw_list, vertex_count,
            47.0f, 47.0f, 393.0f, 302.0f,
            224.0f / 255.0f, 213.0f / 255.0f, 174.0f / 255.0f, 1.0f))
        return 0;
    return ge_emit_frontend_box(draw_list, vertex_count,
        22.0f, 270.0f, 78.0f, 318.0f,
        91.0f / 255.0f, 80.0f / 255.0f, 51.0f / 255.0f, 1.0f);
}

int ge_3ds_original_frontend_build_draw_list_exact(
    const Ge3dsOriginalHudAtlas *zurich,
    const Ge3dsOriginalHudAtlas *bank_gothic,
    Ge3dsOriginalFrontendPage page,
    const Ge3dsOriginalFrontendLine *lines, size_t line_count,
    Ge3dsOriginalHudDrawList *draw_list)
{
    size_t vertex_count = 0U;
    size_t index;
    int authored_header_present = 0;
    const float ink = 18.0f / 255.0f;
    if (draw_list == NULL) return 0;
    ge_hud_draw_list_reset(draw_list);
    if (zurich == NULL || !zurich->ready || bank_gothic == NULL
            || !bank_gothic->ready || lines == NULL
            || line_count == 0U) return 1;
    draw_list->frontend_glyph_font =
        (uint8_t)(page == GE_3DS_ORIGINAL_FRONTEND_PAGE_MISSION_SELECT);
    for (index = 0U; index < line_count; ++index)
        if (lines[index].has_authored_position) {
            authored_header_present = 1;
            break;
        }

    if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_TITLE
            || page == GE_3DS_ORIGINAL_FRONTEND_PAGE_LEGAL) {
        if (!ge_emit_frontend_box(draw_list, &vertex_count,
                0.0f, 0.0f, 440.0f, 330.0f,
                0.0f, 0.0f, 0.0f, 1.0f)) return 0;
        draw_list->background_vertex_count = vertex_count;
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT) {
        if (!ge_emit_frontend_folder_background(
                draw_list, &vertex_count, 0)) return 0;
        /* Four authored wallet positions, projected into the same quadrants
         * as folderpositions[] in front.c. The snapshot carries the selected
         * folder in its sole file-page line. */
        for (index = 0U; index < 4U; ++index) {
            const float left = (index & 1U) ? 234.0f : 36.0f;
            const float top = index < 2U ? 47.0f : 161.0f;
            const int selected = index == (size_t)lines[0].locked;
            if (!ge_emit_frontend_box(draw_list, &vertex_count,
                    left - (selected ? 4.0f : 0.0f),
                    top - (selected ? 4.0f : 0.0f),
                    left + 168.0f + (selected ? 4.0f : 0.0f),
                    top + 91.0f + (selected ? 4.0f : 0.0f),
                    selected ? 235.0f / 255.0f : 98.0f / 255.0f,
                    selected ? 216.0f / 255.0f : 85.0f / 255.0f,
                    selected ? 121.0f / 255.0f : 54.0f / 255.0f,
                    1.0f)) return 0;
            if (!ge_emit_frontend_box(draw_list, &vertex_count,
                    left + 10.0f, top + 12.0f,
                    left + 158.0f, top + 82.0f,
                    38.0f / 255.0f, 36.0f / 255.0f,
                    29.0f / 255.0f, 1.0f)) return 0;
        }
        draw_list->background_vertex_count = vertex_count;
    } else {
        const int paper = page != GE_3DS_ORIGINAL_FRONTEND_PAGE_MISSION_SELECT;
        if (!ge_emit_frontend_folder_background(
                draw_list, &vertex_count, paper)) return 0;
        draw_list->background_vertex_count = vertex_count;
        for (index = 0U; index < line_count; ++index) {
            if (!lines[index].selected) continue;
            if (lines[index].tab != GE_3DS_ORIGINAL_FRONTEND_TAB_NONE) {
                static const float tops[4]={0.0f,51.0f,144.0f,236.0f};
                static const float bottoms[4]={0.0f,117.0f,210.0f,302.0f};
                const int height=ge_frontend_text_height(
                    bank_gothic,lines[index].text);
                const float right=411.0f-(float)height*0.5f;
                if (!ge_emit_frontend_box(draw_list,&vertex_count,
                        right-(float)height+1.0f,tops[lines[index].tab],
                        right,bottoms[lines[index].tab],
                        0.0f,0.0f,0.0f,50.0f/255.0f)) return 0;
                continue;
            }
            if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_MODE_SELECT) {
                const float top = index == 0U ? 218.0f : 250.0f;
                if (!ge_emit_frontend_box(draw_list, &vertex_count,
                        148.0f, top, 353.0f, top + 16.0f,
                        0.0f, 0.0f, 0.0f, 50.0f / 255.0f)) return 0;
            } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_DIFFICULTY) {
                const float top = 178.0f + (float)(index - 1U) * 30.0f;
                if (!ge_emit_frontend_box(draw_list, &vertex_count,
                        126.0f, top, 240.0f, top + 17.0f,
                        0.0f, 0.0f, 0.0f, 50.0f / 255.0f)) return 0;
            } else if (page
                    == GE_3DS_ORIGINAL_FRONTEND_PAGE_MISSION_SELECT
                    && index > 0U) {
                const size_t mission_index = index - 1U;
                static const float xs[5] = {73, 142, 212, 282, 352};
                static const float ys[4] = {62, 131, 201, 270};
                const float cx = xs[mission_index % 5U];
                const float cy = ys[mission_index / 5U];
                if (!ge_emit_frontend_box(draw_list, &vertex_count,
                        cx - 34.0f, cy - 13.0f,
                        cx + 34.0f, cy + 17.0f,
                        1.0f, 1.0f, 1.0f, 42.0f / 255.0f)) return 0;
            }
        }
        if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_007_OPTIONS) {
            for (index = 2U; index < line_count && index < 6U; ++index) {
                const float bar_y = 181.0f
                    + (float)(index - 2U) * 33.0f;
                float fraction = index == 5U ? lines[index].value
                    : sqrtf(lines[index].value / 10.0f);
                if (fraction < 0.0f) fraction = 0.0f;
                if (fraction > 1.0f) fraction = 1.0f;
                if (!ge_emit_frontend_box(draw_list, &vertex_count,
                        55.0f, bar_y, 355.0f, bar_y + 11.0f,
                        0.0f, 0.0f, 0.0f, 50.0f / 255.0f)
                        || !ge_emit_frontend_box(draw_list, &vertex_count,
                        55.0f, bar_y, 55.0f + fraction * 300.0f,
                        bar_y + 11.0f, 0.0f, 0.0f, 0.0f,
                        100.0f / 255.0f)) return 0;
            }
        }
    }
    if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT
            && line_count >= 3U && lines[0].has_authored_position) {
        /* constructor_menu05_fileselect: wallet-relative erase panel is
         * center-49,+25 through center+50,+67. line zero is authored at
         * center-47,+30, so its coordinates retain that exact relation. */
        if (!ge_emit_frontend_box(draw_list, &vertex_count,
                (float)lines[0].x - 2.0f, (float)lines[0].y - 5.0f,
                (float)lines[0].x + 97.0f, (float)lines[0].y + 37.0f,
                0.0f, 0.0f, 0.0f, 50.0f / 255.0f)) return 0;
        for (index = 1U; index < 3U; ++index) {
            const float width=(float)ge_text_width(zurich,lines[index].text);
            const float height=(float)ge_frontend_text_height(
                zurich,lines[index].text);
            if (lines[index].selected
                    && !ge_emit_frontend_box(draw_list, &vertex_count,
                        (float)lines[index].x - 1.0f,
                        (float)lines[index].y - 1.0f,
                        (float)lines[index].x + width + 3.0f,
                        (float)lines[index].y + height,
                        0.0f,0.0f,0.0f,50.0f/255.0f)) return 0;
        }
    }
    draw_list->box_vertex_count = vertex_count;

    if (page != GE_3DS_ORIGINAL_FRONTEND_PAGE_TITLE
            && page != GE_3DS_ORIGINAL_FRONTEND_PAGE_LEGAL) {
        for (index = 0U; index < line_count; ++index) {
            const float red = lines[index].has_authored_color
                ? (float)lines[index].red / 255.0f : ink;
            const float green = lines[index].has_authored_color
                ? (float)lines[index].green / 255.0f : ink;
            const float blue = lines[index].has_authored_color
                ? (float)lines[index].blue / 255.0f : ink;
            if (!lines[index].has_authored_position) continue;
            if (lines[index].horizontal_align != 0U) {
                if (!ge_emit_frontend_text_centered(zurich,
                        lines[index].text, (float)lines[index].x,
                        (float)lines[index].y, red, green, blue,
                        draw_list, &vertex_count)) return 0;
            } else if (!ge_emit_frontend_text_at(zurich, lines[index].text,
                        (float)lines[index].x, (float)lines[index].y,
                        410.0f, red, green, blue,
                        draw_list, &vertex_count)) return 0;
            if (lines[index].value_text != NULL
                    && !ge_emit_frontend_text_at(zurich,
                    lines[index].value_text, (float)lines[index].value_x,
                    (float)lines[index].y, 410.0f, red, green, blue,
                    draw_list, &vertex_count)) return 0;
        }
    }

    if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_TITLE) {
        /* constructor_menu04_goldeneyelogo renders only the model on black;
         * its interface accepts any button but does not draw a replacement
         * PRESS START label.  constructor_menu18_displaycast shares this
         * black page but supplies its three exact authored text baselines. */
        for (index = 0U; index < line_count; ++index) {
            if (!lines[index].has_authored_position) continue;
            if (!ge_emit_frontend_legal_text(
                    zurich, &lines[index], draw_list, &vertex_count)) return 0;
        }
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_LEGAL) {
        for (index = 0U; index < line_count; ++index) {
            if (!lines[index].has_authored_position) continue;
            if (!ge_emit_frontend_legal_text(
                    zurich, &lines[index], draw_list, &vertex_count)) return 0;
        }
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT) {
        static const float action_x[3] = {110.0f, 247.0f, 357.0f};
        for (index = 0U; index < line_count && index < 3U; ++index) {
            if (lines[index].has_authored_position) continue;
            const float shade = lines[index].selected
                ? 1.0f : 235.0f / 255.0f;
            const float green = lines[index].selected
                ? 1.0f : 216.0f / 255.0f;
            const float blue = lines[index].selected
                ? 1.0f : 121.0f / 255.0f;
            /* constructor_menu05_fileselect places Select/Copy/Erase on the
             * same authored baseline.  During erase confirmation the bridge
             * supplies Erase file?/Cancel/Confirm in these three channels,
             * so the complete confirmation UI remains visible as well. */
            if (!ge_emit_frontend_text_centered(zurich, lines[index].text,
                    action_x[index], 285.0f, shade, green, blue,
                    draw_list, &vertex_count)) return 0;
        }
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_MODE_SELECT) {
        for (index = 0U; index < line_count; ++index) {
            if (lines[index].tab != GE_3DS_ORIGINAL_FRONTEND_TAB_NONE)
                continue;
            char number[4] = {(char)('1' + index), '.', '\0', '\0'};
            const float y = 220.0f + (float)index * 32.0f;
            if (!ge_emit_frontend_text_at(zurich, number,
                    150.0f, y, 170.0f, ink, ink, ink,
                    draw_list, &vertex_count)
                    || !ge_emit_frontend_text_at(zurich, lines[index].text,
                    170.0f, y, 365.0f, ink, ink, ink,
                    draw_list, &vertex_count)) return 0;
        }
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_MISSION_SELECT) {
        static const float xs[5] = {73, 142, 212, 282, 352};
        static const float ys[4] = {62, 131, 201, 270};
        /* constructor_menu07_missionsel does not print the bridge's semantic
         * SELECT MISSION heading.  It draws only the twenty authored Bank
         * Gothic labels over the wallet and the Previous tab. */
        for (index = 1U; index < line_count && index <= 20U; ++index) {
            const size_t mission_index = index - 1U;
            const float shade = lines[index].selected
                ? 1.0f : 110.0f / 255.0f;
            const float original_x = xs[mission_index % 5U] - 31.0f;
            const float original_y = ys[mission_index / 5U]
                - (float)ge_frontend_text_height(
                    bank_gothic, lines[index].text) + 29.0f;
            /* constructor_menu07_missionsel emits no text at all for a
             * locked mission; the underlying photo quad alone is darkened
             * to 0x0f by interface_menu07_missionsel. */
            if (lines[index].locked) continue;
            if (!ge_emit_frontend_text_at(bank_gothic,
                    lines[index].text, original_x, original_y,
                    original_x + 64.0f, shade, shade, shade,
                    draw_list, &vertex_count)) return 0;
        }
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_DIFFICULTY) {
        if (!ge_emit_frontend_text_at(zurich, lines[0].text,
                55.0f, 143.0f, 360.0f, ink, ink, ink,
                draw_list, &vertex_count)) return 0;
        for (index = 1U; index < line_count; ++index) {
            if (lines[index].tab != GE_3DS_ORIGINAL_FRONTEND_TAB_NONE)
                continue;
            if (lines[index].has_authored_position) continue;
            char number[4] = {(char)('0' + index), '.', '\0', '\0'};
            const float y = 180.0f + (float)(index - 1U) * 30.0f;
            if (!ge_emit_frontend_text_at(zurich, number,
                    130.0f, y, 150.0f, ink, ink, ink,
                    draw_list, &vertex_count)
                    || !ge_emit_frontend_text_at(zurich, lines[index].text,
                    150.0f, y, 330.0f, ink, ink, ink,
                    draw_list, &vertex_count)) return 0;
        }
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_007_OPTIONS) {
        if (!ge_emit_frontend_text_at(zurich, lines[0].text,
                55.0f, 87.0f, 385.0f, ink, ink, ink,
                draw_list, &vertex_count)
                || !ge_emit_frontend_text_at(zurich, lines[1].text,
                55.0f, 143.0f, 385.0f, ink, ink, ink,
                draw_list, &vertex_count)) return 0;
        for (index = 2U; index < line_count && index < 6U; ++index) {
            const float text_y = 164.0f + (float)(index - 2U) * 33.0f;
            if (!ge_emit_frontend_text_at(zurich, lines[index].text,
                    57.0f, text_y, 280.0f, ink, ink, ink,
                    draw_list, &vertex_count)) return 0;
            if (lines[index].value_text != NULL
                    && !ge_emit_frontend_text_at(zurich,
                    lines[index].value_text, 285.0f, text_y, 355.0f,
                    ink, ink, ink, draw_list, &vertex_count)) return 0;
        }
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_BRIEFING) {
        if ((!authored_header_present
                && !ge_emit_frontend_text_at(zurich, lines[0].text,
                    55.0f, 87.0f, 385.0f, ink, ink, ink,
                    draw_list, &vertex_count))
                || !ge_emit_frontend_text_at(zurich, lines[1].text,
                55.0f, 143.0f, 385.0f, ink, ink, ink,
                draw_list, &vertex_count)) return 0;
        if (line_count > 2U && lines[2].objective) {
            if (!ge_emit_frontend_objective_rows(zurich, lines, 2U,
                    line_count, 167.0f, 0, draw_list, &vertex_count))
                return 0;
        } else if (line_count > 2U
                && !ge_emit_frontend_text_at(zurich, lines[2].text,
                    55.0f, 167.0f, 395.0f, ink, ink, ink,
                    draw_list, &vertex_count)) return 0;
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_REPORT) {
        const int status_complete = lines[3].locked
            == GE_FRONTEND_OBJECTIVE_COMPLETE;
        if (!ge_emit_frontend_text_at(zurich, lines[1].text,
                55.0f, 143.0f, 385.0f, ink, ink, ink,
                draw_list, &vertex_count)
                || !ge_emit_frontend_text_at(zurich, lines[2].text,
                55.0f, 167.0f, 385.0f, ink, ink, ink,
                draw_list, &vertex_count)
                || !ge_emit_frontend_text_at(zurich, lines[3].text,
                55.0f + (float)ge_text_width(zurich, lines[2].text),
                167.0f, 395.0f,
                status_complete ? ink : 120.0f / 255.0f,
                status_complete ? ink : 0.0f,
                status_complete ? ink : 0.0f,
                draw_list, &vertex_count)
                || !ge_emit_frontend_objective_rows(zurich, lines, 4U,
                line_count, 191.0f, 1, draw_list, &vertex_count)) return 0;
    } else if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_STATISTICS) {
        /* constructor_menu0D_missioncomplete: stage/title and heading, then
         * Time, Accuracy, Weapon, Shot/Kill in the left column and the four
         * hit registers in the right column.  Each bridge line already owns
         * its live value; retain the source row/column anchors. */
        static const float label_xs[11] = {
            55.0f, 55.0f, 55.0f, 55.0f, 55.0f, 55.0f,
            180.0f, 180.0f, 180.0f, 180.0f, 55.0f
        };
        static const float value_xs[11] = {
            55.0f, 55.0f, 130.0f, 130.0f, 190.0f, 130.0f,
            300.0f, 300.0f, 300.0f, 300.0f, 130.0f
        };
        static const float ys[11] = {
            87.0f, 143.0f, 167.0f, 204.0f, 220.0f, 244.0f,
            244.0f, 260.0f, 276.0f, 292.0f, 260.0f
        };
        for (index = 0U; index < line_count && index < 11U; ++index) {
            if (!ge_emit_frontend_text_at(zurich, lines[index].text,
                    label_xs[index], ys[index], 410.0f, ink, ink, ink,
                    draw_list, &vertex_count)) return 0;
            if (lines[index].value_text != NULL
                    && !ge_emit_frontend_text_at(zurich,
                    lines[index].value_text, value_xs[index], ys[index],
                    410.0f, ink, ink, ink,
                    draw_list, &vertex_count)) return 0;
        }
    } else {
        if (!ge_emit_frontend_text_at(zurich, lines[0].text,
                55.0f, 87.0f, 385.0f, ink, ink, ink,
                draw_list, &vertex_count)) return 0;
        if (line_count > 1U && !ge_emit_frontend_text_at(
                zurich, lines[1].text, 55.0f, 143.0f, 385.0f,
                ink, ink, ink, draw_list, &vertex_count)) return 0;
        for (index = 2U; index < line_count; ++index) {
            if (lines[index].tab != GE_3DS_ORIGINAL_FRONTEND_TAB_NONE)
                continue;
            const float y = 167.0f + (float)(index - 2U) * 22.0f;
            const float x = lines[index].objective ? 72.0f : 55.0f;
            if (!ge_emit_frontend_text_at(zurich, lines[index].text,
                    x, y, 382.0f, ink, ink, ink,
                    draw_list, &vertex_count)) return 0;
        }
    }
    for (index = 0U; index < line_count; ++index) {
        if (lines[index].tab == GE_3DS_ORIGINAL_FRONTEND_TAB_NONE) continue;
        if (draw_list->tab_glyph_vertex_count == 0U)
            draw_list->tab_glyph_vertex_offset=vertex_count;
        {
            const size_t before=vertex_count;
            if (!ge_emit_frontend_tab_text(bank_gothic,&lines[index],
                    draw_list,&vertex_count)) return 0;
            draw_list->tab_glyph_vertex_count+=vertex_count-before;
        }
    }
    draw_list->glyph_vertex_count = vertex_count
        - draw_list->box_vertex_count;
    draw_list->visible = vertex_count != 0U;
    return 1;
}

int ge_3ds_original_frontend_build_draw_list(
    const Ge3dsOriginalHudAtlas *zurich,
    Ge3dsOriginalFrontendPage page,
    const Ge3dsOriginalFrontendLine *lines, size_t line_count,
    Ge3dsOriginalHudDrawList *draw_list)
{
    return ge_3ds_original_frontend_build_draw_list_exact(
        zurich, zurich, page, lines, line_count, draw_list);
}

static int ge_frontend_append_sprite(
    Ge3dsOriginalFrontendSpriteList *list, uint8_t image,
    int16_t center_x, int16_t center_y, uint16_t width, uint16_t height,
    uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    if (list->count >= GE_3DS_ORIGINAL_FRONTEND_MAX_SPRITES) return 0;
    list->sprites[list->count++] = (Ge3dsOriginalFrontendSprite){
        image, red, green, blue, alpha,
        center_x, center_y, width, height,
    };
    return 1;
}

int ge_3ds_original_frontend_build_sprite_list(
    Ge3dsOriginalFrontendPage page, uint8_t completed_difficulties,
    Ge3dsOriginalFrontendSpriteList *sprite_list)
{
    unsigned difficulty;
    if (sprite_list == NULL) return 0;
    memset(sprite_list, 0, sizeof(*sprite_list));
    if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT) {
        /* floor(440 * -80 / 1280), followed by the three exact
         * s_mainfolderimages entries and coordinates in front.c. */
        sprite_list->folder_background_x_offset = -28;
        return ge_frontend_append_sprite(sprite_list,
                    GE_3DS_ORIGINAL_FRONTEND_SPRITE_COPY,
                    225, 285, 0x20, 0x1c, 255, 255, 255, 255)
            && ge_frontend_append_sprite(sprite_list,
                    GE_3DS_ORIGINAL_FRONTEND_SPRITE_DELETE,
                    335, 285, 0x20, 0x1c, 255, 255, 255, 255)
            && ge_frontend_append_sprite(sprite_list,
                    GE_3DS_ORIGINAL_FRONTEND_SPRITE_SELECT_FILE,
                    110, 285, 0x7a, 0x12, 255, 255, 255, 255);
    }
    if (page != GE_3DS_ORIGINAL_FRONTEND_PAGE_DIFFICULTY) return 1;
    for (difficulty = 0U; difficulty < 3U; ++difficulty) {
        if ((completed_difficulties & (1U << difficulty)) == 0U) continue;
        if (!ge_frontend_append_sprite(sprite_list,
                GE_3DS_ORIGINAL_FRONTEND_SPRITE_CHECK,
                280, (int16_t)(0xbaU + difficulty * 0x1eU),
                0x14, 0x14, 0xb4, 0, 0, 255)) return 0;
    }
    return 1;
}

int ge_3ds_original_frontend_file_action_bounds(
    const Ge3dsOriginalHudAtlas *zurich,
    const char *copy_text,const char *erase_text,
    GeOriginalFrontendWalletBounds bounds[2])
{
    if(zurich==NULL||!zurich->ready||copy_text==NULL||erase_text==NULL
            ||bounds==NULL)return 0;
    bounds[0]=(GeOriginalFrontendWalletBounds){
        225.0f-16.0f,285.0f-14.0f,
        247.0f+(float)ge_text_width(zurich,copy_text),285.0f+14.0f};
    bounds[1]=(GeOriginalFrontendWalletBounds){
        335.0f-16.0f,285.0f-14.0f,
        357.0f+(float)ge_text_width(zurich,erase_text),285.0f+14.0f};
    return 1;
}

int ge_3ds_original_frontend_append_cursor_sprite(
    Ge3dsOriginalFrontendSpriteList *sprite_list,
    float cursor_x,float cursor_y,uint8_t file_action)
{
    uint8_t image=GE_3DS_ORIGINAL_FRONTEND_SPRITE_CROSS;
    uint16_t width=0x20U;
    uint16_t height=0x20U;
    if(sprite_list==NULL)return 0;
    if(file_action==1U){
        image=GE_3DS_ORIGINAL_FRONTEND_SPRITE_COPY;height=0x1cU;
    }else if(file_action==2U){
        image=GE_3DS_ORIGINAL_FRONTEND_SPRITE_DELETE;height=0x1cU;
    }
    return ge_frontend_append_sprite(sprite_list,image,
        (int16_t)floorf(cursor_x+0.5f),(int16_t)floorf(cursor_y+0.5f),
        width,height,255,255,255,220);
}

const char *ge_3ds_original_frontend_sprite_resource(uint8_t image)
{
    static const char *const resources[] = {
        "COPYICON.bin", "DELICON.bin", "SELECTFILE.bin",
        "CROSSHAIR1.bin", "CHECK.bin", "DOT.bin",
    };
    if ((size_t)image >= sizeof(resources) / sizeof(resources[0]))
        return NULL;
    return resources[image];
}

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ge_3ds_original_hud.h"

static void check_unused_bytes(const void *bytes, size_t size)
{
    const unsigned char *data = bytes;
    for (size_t i = 0; i < size; ++i) assert(data[i] == 0xa5U);
}

static void compare_hud(const Ge3dsOriginalHudDrawList *actual,
    const Ge3dsOriginalHudDrawList *zeroed)
{
    const size_t metadata = offsetof(Ge3dsOriginalHudDrawList, background_vertex_count);
    const size_t count = actual->box_vertex_count + actual->glyph_vertex_count;
    assert(count <= GE_3DS_ORIGINAL_HUD_VERTEX_CAPACITY);
    assert(memcmp((const unsigned char *)actual + metadata,
        (const unsigned char *)zeroed + metadata, sizeof(*actual) - metadata) == 0);
    assert(memcmp(actual->vertices, zeroed->vertices,
        count * sizeof(actual->vertices[0])) == 0);
    check_unused_bytes(actual->vertices + count,
        sizeof(actual->vertices) - count * sizeof(actual->vertices[0]));
}

static void compare_gameplay(const Ge3dsOriginalGameplayHudDrawList *actual,
    const Ge3dsOriginalGameplayHudDrawList *zeroed)
{
    const size_t metadata = offsetof(Ge3dsOriginalGameplayHudDrawList, solid_vertex_count);
    assert(actual->solid_vertex_count <= GE_3DS_ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_CAPACITY);
    assert(actual->font_vertex_count <= GE_3DS_ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_CAPACITY);
    assert(memcmp((const unsigned char *)actual + metadata,
        (const unsigned char *)zeroed + metadata, sizeof(*actual) - metadata) == 0);
    assert(memcmp(actual->solid_vertices, zeroed->solid_vertices,
        actual->solid_vertex_count * sizeof(actual->solid_vertices[0])) == 0);
    assert(memcmp(actual->font_vertices, zeroed->font_vertices,
        actual->font_vertex_count * sizeof(actual->font_vertices[0])) == 0);
    check_unused_bytes(actual->solid_vertices + actual->solid_vertex_count,
        sizeof(actual->solid_vertices)
            - actual->solid_vertex_count * sizeof(actual->solid_vertices[0]));
    check_unused_bytes(actual->font_vertices + actual->font_vertex_count,
        sizeof(actual->font_vertices)
            - actual->font_vertex_count * sizeof(actual->font_vertices[0]));
}

/* Run every existing authored-font/gauge/layout case on poisoned and zeroed
 * storage. Every published byte must match and unused capacity must remain
 * untouched. These wrappers affect this test only, not the builders. */
#define CHECKED_BUILDER(name, type, compare, parameters, arguments) \
    static int name##_checked parameters { \
        static type reference; \
        type *output = draw_list; \
        memset(output, 0xa5, sizeof(*output)); \
        const int actual = name arguments; \
        memset(&reference, 0, sizeof(reference)); \
        draw_list = &reference; \
        const int expected = name arguments; \
        assert(actual == expected); \
        if (actual) compare(output, &reference); \
        return actual; \
    }
CHECKED_BUILDER(ge_3ds_original_hud_build_draw_list, Ge3dsOriginalHudDrawList, compare_hud,
    (const Ge3dsOriginalHudAtlas *atlas, const GeOriginalDamMissionHudRenderSnapshot *snapshot,
     Ge3dsOriginalHudDrawList *draw_list), (atlas, snapshot, draw_list))
CHECKED_BUILDER(ge_3ds_original_bottom_hud_build_draw_list, Ge3dsOriginalHudDrawList, compare_hud,
    (const Ge3dsOriginalHudAtlas *atlas, const GeOriginalBottomHudRenderSnapshot *snapshot,
     Ge3dsOriginalHudDrawList *draw_list), (atlas, snapshot, draw_list))
CHECKED_BUILDER(ge_3ds_original_gameplay_hud_build_draw_list, Ge3dsOriginalGameplayHudDrawList, compare_gameplay,
    (const Ge3dsOriginalHudAtlas *atlas, const GeOriginalGameplayHudRenderSnapshot *snapshot,
     Ge3dsOriginalGameplayHudDrawList *draw_list), (atlas, snapshot, draw_list))
CHECKED_BUILDER(ge_3ds_original_watch_objectives_build_draw_list, Ge3dsOriginalHudDrawList, compare_hud,
    (const Ge3dsOriginalHudAtlas *atlas, const Ge3dsOriginalWatchObjectiveLine *lines,
     size_t count, Ge3dsOriginalHudDrawList *draw_list), (atlas, lines, count, draw_list))
CHECKED_BUILDER(ge_3ds_original_credits_build_draw_list, Ge3dsOriginalHudDrawList, compare_hud,
    (const Ge3dsOriginalHudAtlas *atlas, const Ge3dsOriginalCreditsLine *lines,
     size_t count, Ge3dsOriginalHudDrawList *draw_list), (atlas, lines, count, draw_list))
CHECKED_BUILDER(ge_3ds_original_frontend_build_draw_list, Ge3dsOriginalHudDrawList, compare_hud,
    (const Ge3dsOriginalHudAtlas *atlas, Ge3dsOriginalFrontendPage page,
     const Ge3dsOriginalFrontendLine *lines, size_t count, Ge3dsOriginalHudDrawList *draw_list),
    (atlas, page, lines, count, draw_list))
CHECKED_BUILDER(ge_3ds_original_frontend_build_draw_list_exact, Ge3dsOriginalHudDrawList, compare_hud,
    (const Ge3dsOriginalHudAtlas *atlas, const Ge3dsOriginalHudAtlas *bank,
     Ge3dsOriginalFrontendPage page, const Ge3dsOriginalFrontendLine *lines,
     size_t count, Ge3dsOriginalHudDrawList *draw_list), (atlas, bank, page, lines, count, draw_list))
#undef CHECKED_BUILDER
#define ge_3ds_original_hud_build_draw_list ge_3ds_original_hud_build_draw_list_checked
#define ge_3ds_original_bottom_hud_build_draw_list ge_3ds_original_bottom_hud_build_draw_list_checked
#define ge_3ds_original_gameplay_hud_build_draw_list ge_3ds_original_gameplay_hud_build_draw_list_checked
#define ge_3ds_original_watch_objectives_build_draw_list ge_3ds_original_watch_objectives_build_draw_list_checked
#define ge_3ds_original_credits_build_draw_list ge_3ds_original_credits_build_draw_list_checked
#define ge_3ds_original_frontend_build_draw_list ge_3ds_original_frontend_build_draw_list_checked
#define ge_3ds_original_frontend_build_draw_list_exact ge_3ds_original_frontend_build_draw_list_exact_checked

int main(void)
{
    Ge3dsOriginalHudAtlas atlas;
    Ge3dsOriginalHudAtlas bank_gothic;
    Ge3dsOriginalHudDrawList draw;
    Ge3dsOriginalFrontendSpriteList sprites;
    Ge3dsOriginalGameplayHudDrawList gameplay_draw;
    GeOriginalDamMissionHudRenderSnapshot snapshot;
    GeOriginalGameplayHudRenderSnapshot gameplay;
    GeOriginalBottomHudRenderSnapshot bottom;
    Ge3dsOriginalWatchObjectiveLine objective_lines[4];
    Ge3dsOriginalCreditsLine credits_lines[2];
    const Ge3dsOriginalHudGlyph *glyph_a;
    const Ge3dsOriginalHudGlyph *glyph_h;
    const Ge3dsOriginalHudGlyph *glyph_t;
    int first_x;
    size_t x;
    size_t y;
    size_t nonzero = 0U;
    uint8_t maximum_alpha = 0U;
    size_t digit_coverage = 0U;
    size_t digit_transparency = 0U;

    memset(&gameplay_draw, 0xa5, sizeof(gameplay_draw));
    ge_3ds_original_gameplay_hud_draw_list_reset(&gameplay_draw);
    assert(gameplay_draw.solid_vertex_count == 0U && gameplay_draw.font_vertex_count == 0U
        && gameplay_draw.gauge_segment_count == 0U && gameplay_draw.ammo_glyph_count == 0U);
    check_unused_bytes(gameplay_draw.solid_vertices, sizeof(gameplay_draw.solid_vertices));
    check_unused_bytes(gameplay_draw.font_vertices, sizeof(gameplay_draw.font_vertices));
    ge_3ds_original_gameplay_hud_draw_list_reset(NULL);

    assert(!ge_3ds_original_hud_build_atlas(NULL));
    assert(ge_3ds_original_hud_build_atlas(&atlas));
    assert(ge_3ds_original_hud_build_bank_gothic_atlas(&bank_gothic));
    {
        GeOriginalFrontendWalletBounds action_bounds[2];
        assert(ge_3ds_original_frontend_file_action_bounds(
            &atlas,"Copy\n","Erase\n",action_bounds));
        assert(action_bounds[0].left==209.0f
            &&action_bounds[0].top==271.0f
            &&action_bounds[0].bottom==299.0f
            &&action_bounds[0].right>247.0f
            &&action_bounds[1].left==319.0f
            &&action_bounds[1].right>357.0f);
    }
    assert(ge_3ds_original_hud_screen_y(0.0f) == 240.0f);
    assert(ge_3ds_original_hud_screen_y(120.0f) == 120.0f);
    assert(ge_3ds_original_hud_screen_y(220.0f) == 20.0f);
    assert(atlas.ready);
    assert(atlas.nonzero_pixels > 1000U);
    for (y = 0U; y < GE_3DS_ORIGINAL_HUD_ATLAS_HEIGHT; ++y) {
        for (x = 0U; x < GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH; ++x) {
            if (ge_3ds_original_hud_atlas_alpha(&atlas, x, y) != 0U)
                ++nonzero;
            if (ge_3ds_original_hud_atlas_alpha(&atlas, x, y)
                    > maximum_alpha)
                maximum_alpha =
                    ge_3ds_original_hud_atlas_alpha(&atlas, x, y);
        }
    }
    assert(nonzero == atlas.nonzero_pixels);
    assert(maximum_alpha >= 0x80U);
    assert(ge_3ds_original_hud_atlas_alpha(&atlas, 0U, 0U)
        == UINT8_MAX);
    assert(sizeof(atlas.pixels)
        == GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH
            * GE_3DS_ORIGINAL_HUD_ATLAS_HEIGHT
            * GE_3DS_ORIGINAL_HUD_ATLAS_BYTES_PER_PIXEL);
    glyph_a = &bank_gothic.glyphs['7' - 0x21];
    for (y = 0U; y < glyph_a->height; ++y) {
        for (x = 0U; x < glyph_a->width; ++x) {
            if (ge_3ds_original_hud_atlas_alpha(
                    &bank_gothic, glyph_a->atlas_x + x,
                    glyph_a->atlas_y + y) != 0U)
                ++digit_coverage;
            else
                ++digit_transparency;
        }
    }
    assert(digit_coverage > 8U);
    /* A decoded glyph must carry its authored I8 silhouette, not an opaque
     * rectangle. This catches both a bad file-offset basis and a bad stride. */
    assert(digit_transparency > 4U);
    glyph_t = &atlas.glyphs['T' - 0x21];
    /* Zurich Bold T: the authored cap row spans the glyph, while its stem is
     * opaque in the middle and transparent at the edge.  This catches the
     * packed-font 12-byte pixel-base bias, which mere nonzero counts miss. */
    assert(ge_3ds_original_hud_atlas_alpha(
        &atlas, glyph_t->atlas_x, glyph_t->atlas_y + 1U) != 0U);
    assert(ge_3ds_original_hud_atlas_alpha(
        &atlas, glyph_t->atlas_x + glyph_t->width - 1U,
        glyph_t->atlas_y + 1U) != 0U);
    assert(ge_3ds_original_hud_atlas_alpha(
        &atlas, glyph_t->atlas_x, glyph_t->atlas_y + 4U) == 0U);
    assert(ge_3ds_original_hud_atlas_alpha(
        &atlas, glyph_t->atlas_x + 4U, glyph_t->atlas_y + 4U) != 0U);

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.count = 1U;
    snapshot.visible = 1U;
    strcpy(snapshot.messages[0], "AB");
    assert(ge_3ds_original_hud_build_draw_list(&atlas, &snapshot, &draw));
    assert(draw.visible);
    assert(draw.box_vertex_count == 6U);
    assert(draw.glyph_vertex_count == 12U);
    assert(draw.glyph_count == 2U);
    assert(draw.vertices[0].u > 0.0f && draw.vertices[0].v > 0.99f);
    glyph_a = &atlas.glyphs['A' - 0x21];
    glyph_h = &atlas.glyphs['H' - 0x21];
    first_x = 70 - (atlas.kerning[glyph_h->kerning_index * 13U
                                  + glyph_a->kerning_index] - 1);
    assert(draw.vertices[6].x == (float)first_x);
    assert(draw.vertices[6].y
        == (float)(240 - (13 + glyph_a->baseline)));
    assert(draw.vertices[8].x == (float)(first_x + glyph_a->width));
    assert(draw.vertices[6].v > draw.vertices[8].v);

    snapshot.visible = 0U;
    assert(ge_3ds_original_hud_build_draw_list(&atlas, &snapshot, &draw));
    assert(!draw.visible && draw.glyph_vertex_count == 0U);

    memset(&gameplay, 0, sizeof(gameplay));
    gameplay.gauges_visible = 1U;
    gameplay.ammo_visible = 1U;
    gameplay.reserve_visible = 1U;
    gameplay.magazine_ammo = 7;
    gameplay.reserve_ammo = 93;
    gameplay.magazine_x = 295;
    gameplay.reserve_x = 307;
    gameplay.left_ammo_visible = 1U;
    gameplay.left_reserve_visible = 1U;
    gameplay.left_magazine_ammo = 6;
    gameplay.left_reserve_ammo = 93;
    gameplay.left_magazine_x = 104;
    gameplay.left_reserve_x = 92;
    gameplay.ammo_y = 222;
    for (x = 0U; x < GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT; ++x) {
        gameplay.health[x].x = (int16_t)(-500 + (int)x * 20);
        gameplay.health[x].z = (int16_t)(400 - (int)x * 10);
        gameplay.health[x].red = UINT8_MAX;
        gameplay.health[x].alpha = UINT8_MAX;
        gameplay.armour[x] = gameplay.health[x];
        gameplay.armour[x].x = (int16_t)-gameplay.health[x].x;
        gameplay.armour[x].blue = UINT8_MAX;
    }
    assert(ge_3ds_original_gameplay_hud_build_draw_list(
        &bank_gothic, &gameplay, &gameplay_draw));
    assert(gameplay_draw.gauge_segment_count == 28U);
    assert(gameplay_draw.solid_vertex_count == 28U * 6U);
    assert(gameplay_draw.font_vertex_count == 6U * 5U * 6U);
    /* Canonical bottom-up PICA conversion keeps the N64 HUD physically at
     * the bottom of the tilted 3DS target. */
    assert(gameplay_draw.solid_vertices[0].x == 300.0f);
    assert(gameplay_draw.solid_vertices[0].y == 40.0f);
    assert(gameplay_draw.solid_vertices[0].u > 0.0f
        && gameplay_draw.solid_vertices[0].v > 0.99f);
    assert(gameplay_draw.font_vertices[0].y
        == (float)(240 - (222
            + bank_gothic.glyphs['7' - 0x21].baseline)));
    /* generate_ammo_total_microcode uses the unintuitive canonical enum
     * values HUDHALIGN_RIGHT=0 and HUDHALIGN_LEFT=1.  Keep each counter on
     * the outside of its cartridge icon: the right-hand magazine ends at
     * its left anchor and reserve starts at its right anchor; dual-wielded
     * left-hand counters mirror that layout. */
    assert(gameplay_draw.font_vertices[0].x
        < (float)gameplay.magazine_x - 1.0f);
    assert(gameplay_draw.font_vertices[30].x
        >= (float)gameplay.reserve_x - 1.0f);
    assert(gameplay_draw.font_vertices[90].x
        >= (float)gameplay.left_magazine_x - 1.0f);
    assert(gameplay_draw.font_vertices[120].x
        < (float)gameplay.left_reserve_x - 1.0f);
    {
        const float magazine_seven_u = gameplay_draw.font_vertices[24].u;
        gameplay.magazine_ammo = 6;
        assert(ge_3ds_original_gameplay_hud_build_draw_list(
            &bank_gothic, &gameplay, &gameplay_draw));
        assert(gameplay_draw.font_vertices[24].u != magazine_seven_u);
        assert(gameplay_draw.font_vertices[0].x
            < (float)gameplay.magazine_x - 1.0f);
    }

    memset(&bottom, 0, sizeof(bottom));
    bottom.visible = 1U;
    bottom.x = 70;
    bottom.y = 218;
    strcpy(bottom.message, "Ammo collected");
    assert(ge_3ds_original_bottom_hud_build_draw_list(
        &bank_gothic, &bottom, &draw));
    assert(draw.visible && draw.box_vertex_count == 6U);
    assert(draw.glyph_count == 13U * 5U);
    assert(draw.glyph_vertex_count == 13U * 5U * 6U);
    assert(draw.vertices[6].y
        == (float)(240 - (218
            + bank_gothic.glyphs['A' - 0x21].baseline)));

    memset(objective_lines, 0, sizeof(objective_lines));
    objective_lines[0] = (Ge3dsOriginalWatchObjectiveLine){
        "neutralize all alarms", "incomplete", 0U, 0U};
    objective_lines[1] = (Ge3dsOriginalWatchObjectiveLine){
        "install covert modem", "completed", 1U, 1U};
    objective_lines[2] = (Ge3dsOriginalWatchObjectiveLine){
        "intercept data backup", "failed", 2U, 2U};
    objective_lines[3] = (Ge3dsOriginalWatchObjectiveLine){
        "bungee jump from platform", "incomplete", 3U, 0U};
    assert(ge_3ds_original_watch_objectives_build_draw_list(
        &bank_gothic, objective_lines, 4U, &draw));
    assert(draw.visible && draw.box_vertex_count == 6U);
    assert(draw.vertices[0].u > 0.0f && draw.vertices[0].v > 0.99f);
    assert(draw.glyph_count > 80U);
    assert(draw.glyph_vertex_count == draw.glyph_count * 6U);
    assert(draw.vertices[6].r > 0.6f && draw.vertices[6].g == 1.0f);

    credits_lines[0] = (Ge3dsOriginalCreditsLine){
        "A\n", 220, 16, GE_3DS_ORIGINAL_CREDITS_ALIGN_LEFT};
    credits_lines[1] = (Ge3dsOriginalCreditsLine){
        "B\n", 220, 32, GE_3DS_ORIGINAL_CREDITS_ALIGN_RIGHT};
    assert(ge_3ds_original_credits_build_draw_list(
        &atlas, credits_lines, 2U, &draw));
    assert(draw.visible && draw.box_vertex_count == 0U
        && draw.glyph_count == 2U && draw.glyph_vertex_count == 12U);
    /* Canonical credits LEFT means text ending at the authored anchor;
     * the 320-wide VI composition is centred at x=40 on the top screen. */
    assert(draw.vertices[2].x == 260.0f);
    assert(draw.vertices[0].y
        == (float)(240 - (16 + atlas.glyphs['A' - 0x21].baseline)));
    assert(draw.vertices[6].x >= 260.0f);

    {
        const Ge3dsOriginalFrontendLine legal_lines[] = {
            {.text="TWYCROSS BOARD OF GAME CLASSIFICATION", .x=220, .y=30,
                .horizontal_align=1U, .vertical_align=1U,
                .has_authored_position=1U},
            {.text="This is to certify", .x=34, .y=83,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="(c) 1997 Nintendo/Rare", .x=226, .y=84,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="(c) 1962, 1995 Danjaq, LLC. &", .x=226, .y=97,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="U.A.C. All Rights Reserved", .x=226, .y=110,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="(c) 1997 Eon Productions", .x=226, .y=122,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="Ltd. & Mac B. Inc.", .x=227, .y=134,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="Suitable only for 1-4 persons", .x=219, .y=211,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="PRESIDENT", .x=60, .y=169,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="VICE", .x=60, .y=201,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="James Bond theme by Monty Norman.", .x=99, .y=266,
                .vertical_align=1U, .has_authored_position=1U},
            {.text="Used by permission of EMI Unart Catalog Inc.",
                .x=80, .y=280, .vertical_align=1U,
                .has_authored_position=1U},
        };
        size_t vertex;
        assert(ge_3ds_original_frontend_build_draw_list_exact(
            &atlas, &bank_gothic,
            GE_3DS_ORIGINAL_FRONTEND_PAGE_LEGAL,
            legal_lines,
            sizeof(legal_lines) / sizeof(legal_lines[0]), &draw));
        assert(draw.visible);
        assert(draw.background_vertex_count == 6U);
        assert(draw.box_vertex_count == 6U);
        assert(draw.glyph_count > 200U);
        assert(draw.glyph_vertex_count == draw.glyph_count * 6U);
        /* The legal constructor's 440x330 authored composition is uniformly
         * reduced into the centred 320x240 top-screen viewport.  Keep every
         * emitted glyph visible; an invalid language pointer or a regression
         * to unscaled VI coordinates makes this fail before reaching PICA. */
        for (vertex = draw.box_vertex_count;
                vertex < draw.box_vertex_count + draw.glyph_vertex_count;
                ++vertex) {
            assert(draw.vertices[vertex].x >= 40.0f);
            assert(draw.vertices[vertex].x <= 360.0f);
            assert(draw.vertices[vertex].y >= 0.0f);
            assert(draw.vertices[vertex].y <= 240.0f);
            assert(draw.vertices[vertex].r == 1.0f);
            assert(draw.vertices[vertex].g == 1.0f);
            assert(draw.vertices[vertex].b == 1.0f);
        }
    }

    {
        const Ge3dsOriginalFrontendLine cast_lines[] = {
            {.text="JAMES BOND", .x=315, .y=108,
                .horizontal_align=1U, .has_authored_position=1U},
            {.text="AS", .x=315, .y=152,
                .horizontal_align=1U, .has_authored_position=1U},
            {.text="007", .x=315, .y=174,
                .horizontal_align=1U, .has_authored_position=1U},
        };
        assert(ge_3ds_original_frontend_build_draw_list_exact(
            &atlas, &bank_gothic, GE_3DS_ORIGINAL_FRONTEND_PAGE_TITLE,
            cast_lines, sizeof(cast_lines) / sizeof(cast_lines[0]), &draw));
        assert(draw.background_vertex_count==6U);
        assert(draw.box_vertex_count==6U);
        assert(draw.glyph_count==14U);
        assert(draw.glyph_vertex_count==14U*6U);
        assert(draw.vertices[draw.box_vertex_count].x>200.0f);
    }

    {
        const Ge3dsOriginalFrontendLine file_lines[] = {
            {.text="Confirm", .selected=0U, .objective=0U, .locked=3U},
            {.text="Copy", .selected=1U, .objective=0U, .locked=0U},
            {.text="Erase", .selected=0U, .objective=0U, .locked=0U},
        };
        assert(ge_3ds_original_frontend_build_draw_list(
            &atlas, GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT, file_lines,
            sizeof(file_lines) / sizeof(file_lines[0]), &draw));
        assert(draw.visible && draw.box_vertex_count >= 48U);
        assert(draw.glyph_count == 16U);
        assert(draw.glyph_vertex_count == 16U * 6U);
        assert(ge_3ds_original_frontend_build_sprite_list(
            GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT, 0U, &sprites));
        assert(sprites.count == 3U);
        assert(sprites.folder_background_x_offset == -28);
        assert(sprites.sprites[0].image
            == GE_3DS_ORIGINAL_FRONTEND_SPRITE_COPY);
        assert(sprites.sprites[0].center_x == 225
            && sprites.sprites[0].center_y == 285
            && sprites.sprites[0].width == 0x20U
            && sprites.sprites[0].height == 0x1cU);
        assert(sprites.sprites[1].center_x == 335
            && sprites.sprites[1].center_y == 285);
        assert(sprites.sprites[2].image
            == GE_3DS_ORIGINAL_FRONTEND_SPRITE_SELECT_FILE);
        assert(sprites.sprites[2].center_x == 110
            && sprites.sprites[2].width == 0x7aU
            && sprites.sprites[2].height == 0x12U);
        assert(strcmp(ge_3ds_original_frontend_sprite_resource(
            sprites.sprites[0].image), "COPYICON.bin") == 0);
        assert(strcmp(ge_3ds_original_frontend_sprite_resource(
            sprites.sprites[1].image), "DELICON.bin") == 0);
        assert(strcmp(ge_3ds_original_frontend_sprite_resource(
            sprites.sprites[2].image), "SELECTFILE.bin") == 0);
        assert(ge_3ds_original_frontend_append_cursor_sprite(
            &sprites,220.4f,165.6f,0U));
        assert(sprites.count==4U
            &&sprites.sprites[3].image
                ==GE_3DS_ORIGINAL_FRONTEND_SPRITE_CROSS
            &&sprites.sprites[3].center_x==220
            &&sprites.sprites[3].center_y==166
            &&sprites.sprites[3].width==0x20U
            &&sprites.sprites[3].height==0x20U
            &&sprites.sprites[3].alpha==220U);
        assert(strcmp(ge_3ds_original_frontend_sprite_resource(
            sprites.sprites[3].image),"CROSSHAIR1.bin")==0);
        assert(ge_3ds_original_frontend_sprite_resource(0xffU) == NULL);
    }

    {
        const Ge3dsOriginalFrontendLine frontend_lines[] = {
            {.text="SELECT MISSION"},
            {.text="Dam", .selected=1U},
            {.text="MI6 has confirmed the existence of a secret chemical "
             "warfare facility at the Byelomorye dam, USSR.",
             .objective=1U},
        };
        size_t vertex;
        int selected_green_seen = 0;
        assert(ge_3ds_original_frontend_build_draw_list(
            &atlas, GE_3DS_ORIGINAL_FRONTEND_PAGE_BRIEFING, frontend_lines,
            sizeof(frontend_lines) / sizeof(frontend_lines[0]), &draw));
        assert(draw.visible && draw.box_vertex_count > 6U);
        assert(draw.glyph_count > 90U);
        assert(draw.glyph_vertex_count == draw.glyph_count * 6U);
        for (vertex = draw.box_vertex_count;
                vertex < draw.box_vertex_count + draw.glyph_vertex_count;
                ++vertex) {
            if (draw.vertices[vertex].r < 0.2f
                    && draw.vertices[vertex].g < 0.2f)
                selected_green_seen = 1;
            assert(draw.vertices[vertex].x >= 40.0f);
            assert(draw.vertices[vertex].x <= 360.0f);
        }
        assert(selected_green_seen);
    }

    {
        const Ge3dsOriginalFrontendLine report_lines[] = {
            {.text=""},
            {.text="REPORT:\n"},
            {.text="Mission status:\n"},
            {.text=" FAILED\n", .locked=2U},
            {.text="Bungee jump from platform", .value_text="FAILED\n",
             .objective=1U, .locked=2U},
            {.text="Secret Agent: James Bond\n", .x=55, .y=87,
             .has_authored_position=1U},
            {.text="Mission 1: Arkangelsk\n", .x=55, .y=103,
             .has_authored_position=1U},
            {.text="Part i: Dam\n", .x=55, .y=119,
             .has_authored_position=1U},
        };
        size_t vertex;
        int red_status_seen = 0;
        int right_status_seen = 0;
        assert(ge_3ds_original_frontend_build_draw_list_exact(
            &atlas, &bank_gothic, GE_3DS_ORIGINAL_FRONTEND_PAGE_REPORT,
            report_lines,
            sizeof(report_lines) / sizeof(report_lines[0]), &draw));
        assert(draw.visible && draw.glyph_count > 75U);
        for (vertex = draw.box_vertex_count;
                vertex < draw.box_vertex_count + draw.glyph_vertex_count;
                ++vertex) {
            if (fabsf(draw.vertices[vertex].r - 120.0f / 255.0f)
                        < 0.0001f
                    && draw.vertices[vertex].g == 0.0f
                    && draw.vertices[vertex].b == 0.0f) {
                red_status_seen = 1;
                if (draw.vertices[vertex].x
                        >= 40.0f + 300.0f * (320.0f / 440.0f))
                    right_status_seen = 1;
            }
        }
        assert(red_status_seen && right_status_seen);
    }

    {
        const Ge3dsOriginalFrontendLine statistic_lines[] = {
            {.text=""}, {.text="STATISTICS:\n"},
            {.text="Time:\n", .value_text="01:00"},
            {.text="Accuracy:\n", .value_text="50.0%"},
            {.text="Weapon of choice:\n", .value_text="PP7"},
            {.text="Shot total:\n", .value_text="12"},
            {.text="Head hits:\n", .value_text="3 (50%)"},
            {.text="Body hits:\n", .value_text="2 (33%)"},
            {.text="Limb hits:\n", .value_text="1 (17%)"},
            {.text="Others:\n", .value_text="0 (0%)"},
            {.text="Kill total:\n", .value_text="4"},
            {.text="Agent: James Bond\n", .x=55, .y=87,
             .has_authored_position=1U},
            {.text="Mission 1: Arkangelsk\n", .x=55, .y=103,
             .has_authored_position=1U},
            {.text="Part i: Dam\n", .x=55, .y=119,
             .has_authored_position=1U},
        };
        size_t vertex;
        int right_value_seen = 0;
        assert(ge_3ds_original_frontend_build_draw_list_exact(
            &atlas, &bank_gothic,
            GE_3DS_ORIGINAL_FRONTEND_PAGE_STATISTICS,
            statistic_lines,
            sizeof(statistic_lines) / sizeof(statistic_lines[0]), &draw));
        for (vertex = draw.box_vertex_count;
                vertex < draw.box_vertex_count + draw.glyph_vertex_count;
                ++vertex)
            if (draw.vertices[vertex].x
                    >= 40.0f + 300.0f * (320.0f / 440.0f))
                right_value_seen = 1;
        assert(right_value_seen);
    }

    {
        const Ge3dsOriginalFrontendLine mission_lines[] = {
            {.text="SELECT MISSION"},
            {.text="DAM", .selected=1U},
            {.text="FACILITY"},
            {.text="RUNWAY"},
            {.text="SURFACE"},
            {.text="BUNKER", .locked=1U},
        };
        assert(ge_3ds_original_frontend_build_draw_list_exact(
            &atlas, &bank_gothic,
            GE_3DS_ORIGINAL_FRONTEND_PAGE_MISSION_SELECT,
            mission_lines,
            sizeof(mission_lines) / sizeof(mission_lines[0]), &draw));
        assert(draw.visible && draw.box_vertex_count >= 30U);
        assert(draw.frontend_glyph_font == 1U);
        assert(draw.glyph_count > 15U);
        /* Canonical five-column menu coordinates span the dossier instead of
         * reverting to a platform vertical list.  The semantic heading is
         * not rendered and the first visible glyph uses Bank Gothic. */
        assert(draw.vertices[draw.box_vertex_count].x < 80.0f);
        assert(draw.vertices[draw.box_vertex_count].u
            == (float)bank_gothic.glyphs['D' - 0x21].atlas_x
                / (float)GE_3DS_ORIGINAL_HUD_ATLAS_WIDTH);
        assert(ge_3ds_original_frontend_build_sprite_list(
            GE_3DS_ORIGINAL_FRONTEND_PAGE_DIFFICULTY, 0x5U, &sprites));
        assert(sprites.count == 2U);
        assert(sprites.sprites[0].image
            == GE_3DS_ORIGINAL_FRONTEND_SPRITE_CHECK);
        assert(sprites.sprites[0].center_x == 280
            && sprites.sprites[0].center_y == 0xba);
        assert(sprites.sprites[0].red == 0xb4U
            && sprites.sprites[0].green == 0U
            && sprites.sprites[0].blue == 0U);
        assert(sprites.sprites[1].center_y == 0xf6);
    }

    {
        const Ge3dsOriginalFrontendLine option_lines[] = {
            {.text="MISSION 1: ARKANGELSK"},
            {.text="SPECIAL OPTIONS:"},
            {.text="Enemy health 100%", .value=1.0f},
            {.text="Enemy damage 250%", .value=2.5f},
            {.text="Enemy accuracy 10%", .value=1.0f},
            {.text="Enemy reaction speed 50%", .value=0.5f},
            {.text="START", .selected=0U,
                .tab=GE_3DS_ORIGINAL_FRONTEND_TAB_START},
            {.text="NEXT", .selected=1U,
                .tab=GE_3DS_ORIGINAL_FRONTEND_TAB_NEXT},
            {.text="PREVIOUS", .selected=0U,
                .tab=GE_3DS_ORIGINAL_FRONTEND_TAB_PREVIOUS},
        };
        assert(ge_3ds_original_frontend_build_draw_list_exact(
            &atlas, &bank_gothic,
            GE_3DS_ORIGINAL_FRONTEND_PAGE_007_OPTIONS,
            option_lines,
            sizeof(option_lines) / sizeof(option_lines[0]), &draw));
        /* Folder paper plus exact background/filled bar pair for each of the
         * four 007 modifiers. */
        assert(draw.background_vertex_count>=6U);
        assert(draw.box_vertex_count>=draw.background_vertex_count+48U);
        assert(draw.glyph_vertex_count>0U);
        assert(draw.tab_glyph_vertex_count==17U*6U);
        assert(draw.tab_glyph_vertex_offset>=draw.box_vertex_count);
    }

    puts("3DS original HUD: ROM fonts build gameplay/watch/frontend text and canonical gauges");
    return 0;
}

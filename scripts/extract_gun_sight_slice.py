#!/usr/bin/env python3
"""Retain the canonical sight renderer and rectangle commands, not a HUD replica."""
from __future__ import annotations

import argparse
import re
from pathlib import Path
from extract_dam_mission_hud_slice import extract_function


def canonical_bodies(repo: Path) -> str:
    gun = (repo / "src/game/gunfire.c").read_text()
    rectangles = (repo / "src/game/bondwalk2.c").read_text()
    texture_modes = (repo / "src/game/othermodemicrocode.c").read_text()
    sight = extract_function(gun, "gunDrawSight")
    # Correct the decompiler's 32-bit display-list pointer locals/parameters
    # and pointer-to-array spelling; leave all decisions/data/math unchanged.
    sight = sight.replace("s32 *gdl", "Gfx **gdl").replace("s32 sp54;", "Gfx *sp54;")
    sight = sight.replace("&xypos, &halfedxy", "xypos, halfedxy")
    return "\n\n".join((
        extract_function(texture_modes, "texSetRenderMode"),
        extract_function(rectangles, "draw_textured_rectangle"),
        extract_function(rectangles, "display_image_at_position"),
        sight,
    ))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    textures = (args.repo / "assets/oddtextures.c").read_text()
    record = re.search(r"(?ms)^sImageTableEntry s_crosshairimage\[\] = \{(.*?)^\};", textures)
    if record is None:
        raise ValueError("missing original sight image record")
    image_name = re.search(r"IMAGE_(\w+)", record[1]).group(1)
    names = re.findall(r"(?m)^\s*IMAGE\(([^,]+),", (args.repo / "assets/images.def").read_text())
    image_id = [name.strip() for name in names].index(image_name)
    image_record = record[1].replace("IMAGE_" + image_name, str(image_id))
    output = r'''/* Generated from gunfire.c, bondwalk2.c, othermodemicrocode.c and oddtextures.c. */
#include <ultra64.h>
#include <math.h>
#include <string.h>
#include "bondtypes.h"
#include "game/bondview.h"
#include "game/options.h"
#include "ge_original_gun_sight.h"

extern struct player *g_CurrentPlayer;
static sImageTableEntry ge_sight_image[] = { IMAGE_RECORD };
static GeOriginalGunSightSnapshot *ge_sight_capture;
void ge_original_sight_render_mode_exact(Gfx **gdl, s32 alpha, s32 cycles, s32 z);

static void ge_sight_tex_select(Gfx **gdl, sImageTableEntry *image,
    u32 alpha_mode, s32 texture_mode, u32 offset)
{
    /* Exact level-zero texSelect render-state branch; image allocation and
     * upload are the native platform binding rather than N64 TMEM commands. */
    ge_original_sight_render_mode_exact(gdl, (s32)alpha_mode, 1, texture_mode);
    ge_sight_capture->image_id = (uint16_t)image->index;
    ge_sight_capture->width = image->width;
    ge_sight_capture->height = image->height;
    ge_sight_capture->level = image->level;
    ge_sight_capture->format = image->format;
    ge_sight_capture->depth = image->depth;
    ge_sight_capture->flags_s = image->flagsS;
    ge_sight_capture->flags_t = image->flagsT;
    ge_sight_capture->alpha_mode = (uint8_t)alpha_mode;
    ge_sight_capture->texture_mode = (uint8_t)texture_mode;
    ge_sight_capture->texture_offset = (int32_t)offset;
    ge_sight_capture->selected = 1U;
}

#define G_CC_MODULATEIA_ENV COMBINED, 0, ENVIRONMENT, 0, COMBINED, 0, ENVIRONMENT, 0
#define texSelect ge_sight_tex_select
#define texSetRenderMode ge_original_sight_render_mode_exact
#define crosshairimage ge_sight_image
#define gunDrawSight ge_original_gun_draw_sight_exact
#define display_image_at_position ge_original_sight_display_image_exact
#define draw_textured_rectangle ge_original_sight_draw_rectangle_exact
CANONICAL_BODIES
#undef draw_textured_rectangle
#undef display_image_at_position
#undef gunDrawSight
#undef crosshairimage
#undef texSelect
#undef texSetRenderMode

const char *ge_original_gun_sight_texture_source(void) { return "IMAGE_SOURCE.bin"; }

int ge_original_gun_sight_snapshot(GeOriginalGunSightSnapshot *snapshot)
{
    Gfx commands[GE_ORIGINAL_GUN_SIGHT_COMMAND_CAPACITY] = {0};
    Gfx *end = commands;
    size_t index;
    if (snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    if (g_CurrentPlayer == NULL) return 0;
    snapshot->suppression_reasons = (uint32_t)g_CurrentPlayer->gunsightmode;
    if (!isfinite(g_CurrentPlayer->crosshair_angle.f[0])
            || !isfinite(g_CurrentPlayer->crosshair_angle.f[1])) return 0;
    /* The unchanged rectangle body converts (position +/- half-size) * 4
     * to s32. Reject corrupt native state outside that conversion domain. */
    for (index = 0U; index < 2U; ++index)
        if ((double)((fabsf(g_CurrentPlayer->crosshair_angle.f[index]) + 16.0f)
                * 4.0f) > (double)INT32_MAX) return 0;
    /* This isolated capture inherits the regular gameplay filter used by
     * the original world/HUD passes. The original ammo renderer restores
     * G_TF_BILERP after its own point-filtered rectangles (gunfire.c). */
    gDPSetTextureFilter(end++, G_TF_BILERP);
    ge_sight_capture = snapshot;
    ge_original_gun_draw_sight_exact(&end);
    ge_sight_capture = NULL;
    if (end < commands || end >= commands + GE_ORIGINAL_GUN_SIGHT_COMMAND_CAPACITY)
        return 0;
    gSPEndDisplayList(end++);
    snapshot->command_count = (size_t)(end - commands);
    for (index = 0U; index < snapshot->command_count; ++index) {
        snapshot->commands[index][0] = commands[index].words.w0;
        snapshot->commands[index][1] = commands[index].words.w1;
    }
    return 1;
}
'''
    output = output.replace("IMAGE_RECORD", image_record)
    output = output.replace("CANONICAL_BODIES", canonical_bodies(args.repo))
    output = output.replace("IMAGE_SOURCE", image_name)
    args.output.write_text(output)
    print("canonical sight: 4 retained bodies and authored image record")


if __name__ == "__main__":
    main()

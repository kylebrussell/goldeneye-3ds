#!/usr/bin/env python3
"""Extract GoldenEye's exact upper-HUD enqueue body for Dam mission AI."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


SOLO_LANGUAGE_BANKS = (
    ("LdamE", "LDAM"), ("LarkE", "LARK"), ("LrunE", "LRUN"),
    ("LsevxE", "LSEVX"), ("LsevE", "LSEV"), ("LsiloE", "LSILO"),
    ("LdestE", "LDEST"), ("LsevxbE", "LSEVXB"),
    ("LsevbE", "LSEVB"), ("LstatE", "LSTAT"),
    ("LarchE", "LARCH"), ("LpeteE", "LPETE"),
    ("LdepoE", "LDEPO"), ("LtraE", "LTRA"),
    ("LjunE", "LJUN"), ("LarecE", "LAREC"),
    ("LcaveE", "LCAVE"), ("LcradE", "LCRAD"),
    ("LaztE", "LAZT"), ("LcrypE", "LCRYP"),
)


def count_language_entries(source: str, symbol: str) -> int:
    """Count top-level entries in an authored native language pointer table."""
    match = re.search(rf"char\s*\*\s*{symbol}\s*\[\s*\]\s*=\s*\{{", source)
    if match is None:
        raise ValueError(f"missing canonical language table {symbol}")
    index = match.end()
    commas = 0
    in_string = False
    in_line_comment = False
    in_block_comment = False
    escaped = False
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if in_line_comment:
            if char == "\n":
                in_line_comment = False
        elif in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
                index += 1
        elif in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif char == '"':
            in_string = True
        elif char == "/" and next_char == "/":
            in_line_comment = True
            index += 1
        elif char == "/" and next_char == "*":
            in_block_comment = True
            index += 1
        elif char == "}":
            break
        elif char == ",":
            commas += 1
        index += 1
    if index >= len(source) or commas == 0:
        raise ValueError(f"unterminated or empty language table {symbol}")
    # The generated text assets omit the optional trailing comma on their
    # final entry, so N entries contain N-1 top-level separators.
    return commas + 1


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{name}\s*\([^;\n]*\)[^;{{}}]*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing {name}")
    brace = source.index("{", match.start())
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():pos + 1]
    raise ValueError(f"unterminated {name}")


def extract_ammo_icon_assets(
    gun: str, oddtextures: str, images_def: str
) -> list[tuple[int, int, int, int, int, float, str]]:
    """Join the original ammo table to its global-bank image records.

    ammo_related stores segmented addresses. The matching image-table symbols
    are consecutive 12-byte N64 records beginning at 0x02000c84; preserving
    that source/link order gives the exact image ID and dimensions without a
    port-authored ammo/icon switch.
    """
    ammo_match = re.search(
        r"(?ms)^AmmoStats ammo_related\[AMMO_RELATED_MAX\]\s*=\s*\{(.*?)^\};",
        gun,
    )
    if ammo_match is None:
        raise ValueError("missing canonical ammo_related table")
    ammo_rows = re.findall(
        r"\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*"
        r"(0x[0-9A-Fa-f]+|\d+)\s*,\s*"
        r"(-?(?:\d+(?:\.\d*)?|\.\d+))f?\s*,?\s*\}",
        ammo_match.group(1),
    )
    if len(ammo_rows) != 30:
        raise ValueError(f"expected 30 canonical ammo rows, found {len(ammo_rows)}")

    icon_records = []
    icon_pattern = re.compile(
        r"(?ms)^sImageTableEntry\s+s_([A-Za-z0-9_]+)\[\]\s*=\s*\{\s*"
        r"\{\s*IMAGE_([A-Za-z0-9_]+)\s*,\s*"
        r"(0x[0-9A-Fa-f]+|\d+)\s*,\s*"
        r"(0x[0-9A-Fa-f]+|\d+)\s*,"
    )
    for match in icon_pattern.finditer(oddtextures):
        icon_records.append((
            match.group(1), match.group(2),
            int(match.group(3), 0), int(match.group(4), 0),
        ))
    first = next((i for i, row in enumerate(icon_records)
                  if row[0] == "ammo9mmimage"), None)
    last = next((i for i, row in enumerate(icon_records)
                 if row[0] == "tankammoimage"), None)
    if first is None or last is None or last - first + 1 != 14:
        raise ValueError("missing contiguous canonical ammo image records")
    icon_records = icon_records[first:last + 1]

    image_ids: dict[str, int] = {}
    for line in images_def.splitlines():
        match = re.match(r"\s*IMAGE\(([^,]+),", line)
        if match is not None:
            image_ids[match.group(1).strip()] = len(image_ids)
    if not image_ids:
        raise ValueError("missing canonical images.def IDs")

    assets = []
    base_address = 0x02000C84
    for ammo_type, (_maximum, icon_text, y_text) in enumerate(ammo_rows):
        address = int(icon_text, 0)
        if address == 0:
            continue
        delta = address - base_address
        if address >> 24 != 2 or delta < 0 or delta % 12 != 0:
            raise ValueError(
                f"ammo {ammo_type} has unexpected icon address {address:#010x}"
            )
        icon_index = delta // 12
        if icon_index >= len(icon_records):
            raise ValueError(
                f"ammo {ammo_type} icon address is outside canonical records"
            )
        _symbol, image_name, width, height = icon_records[icon_index]
        if image_name not in image_ids:
            raise ValueError(f"missing image ID for {image_name}")
        assets.append((
            ammo_type, address, image_ids[image_name], width, height,
            float(y_text), f"{image_name}.bin",
        ))
    if len(assets) != 13:
        raise ValueError(f"expected 13 canonical ammo icons, found {len(assets)}")
    return assets


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    bondview = (args.repo / "src/game/bondview2.c").read_text()
    glass = (args.repo / "src/game/glass2.c").read_text()
    gun = (args.repo / "src/game/gun.c").read_text()
    oddtextures = (args.repo / "assets/oddtextures.c").read_text()
    images_def = (args.repo / "assets/images.def").read_text()
    ammo_icon_assets = extract_ammo_icon_assets(gun, oddtextures, images_def)
    ammo_icon_table = "\n".join(
        "    { %d, UINT32_C(0x%08X), UINT16_C(%d), UINT8_C(%d), "
        "UINT8_C(%d), %.1ff, \"%s\" },"
        % asset
        for asset in ammo_icon_assets
    )
    body = extract_function(bondview, "hudmsgTopShow")
    timer_body = extract_function(
        bondview, "bondviewUpperTextWindowTimerTick"
    ).replace(
        "bondviewUpperTextWindowTimerTick",
        "ge_original_dam_mission_hud_tick",
        1,
    )
    gauge_body = extract_function(
        glass[glass.index("#if !defined(LEFTOVERDEBUG)"):],
        "hudMakeDamageSegments",
    ).replace(
        "hudMakeDamageSegments",
        "ge_original_hud_make_damage_segments_exact",
        1,
    )
    lower_region = bondview[bondview.index(
        "#else\n#ifdef DEBUG\nvoid hudmsgBottomShow"
    ):]
    bottom_show_raw = extract_function(lower_region, "hudmsgBottomShow")
    bottom_show_body = (
        "void ge_original_hud_bottom_show_exact(char *mess)\n"
        + bottom_show_raw[bottom_show_raw.index("{"):]
    )
    bottom_tick_body = extract_function(
        bondview, "bondviewIntroCameraTextTick"
    ).replace(
        "bondviewIntroCameraTextTick", "ge_original_bottom_hud_tick", 1
    )
    solo_language_rows = []
    solo_language_externs = []
    for symbol, bank in SOLO_LANGUAGE_BANKS:
        language_source = (args.repo / f"assets/obseg/text/{symbol}.c").read_text()
        count = count_language_entries(language_source, symbol)
        solo_language_externs.append(f"extern char *{symbol}[];")
        solo_language_rows.append(
            f"    {{ {bank}, {symbol}, UINT32_C({count}) }},"
        )
    solo_language_externs_text = "\n".join(solo_language_externs)
    solo_language_rows_text = "\n".join(solo_language_rows)
    output = f"""/* Generated exact upper-HUD enqueue plus native language ABI adapter. */
#include <ultra64.h>
#include <stddef.h>
#include <string.h>
#ifndef AIPARSE
#define AIPARSE
#endif
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/bondview.h"
#include "game/gun.h"
#include "game/lv.h"
#include "ge_original_dam_mission_hud.h"

#define BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH \
    GE_ORIGINAL_DAM_MISSION_HUD_MESSAGE_CAPACITY
#define GE_GUN_LANGUAGE_STRING_COUNT 224
#define GE_TITLE_LANGUAGE_STRING_COUNT 302
#define GE_PROPOBJ_LANGUAGE_STRING_COUNT 68
#define GE_OPTIONS_LANGUAGE_STRING_COUNT 64
#define GE_MISC_LANGUAGE_STRING_COUNT 69
#define MAXMESSAGELEN 100
#define BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH 0x65
#if defined(VERSION_EU)
#define BONDVIEW_UPPER_TEXT_TIMER_A 0x33
#define BONDVIEW_UPPER_TEXT_TIMER_B 0x32
#define BONDVIEW_UPPER_TEXT_TIMER_C 0xc8
#define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_A 0x1a
#define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_B 0x19
#define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_C 0x64
#else
#define BONDVIEW_UPPER_TEXT_TIMER_A 0x3d
#define BONDVIEW_UPPER_TEXT_TIMER_B 0x3c
#define BONDVIEW_UPPER_TEXT_TIMER_C 0xf0
#define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_A 0x1f
#define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_B 0x1e
#define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_C 0x78
#endif

{solo_language_externs_text}
extern char *LgunE[];
extern char *LtitleE[];
extern char *LpropobjE[];
extern char *LoptionsE[];
extern char *LmiscE[];
s32 upper_text_buffer_index;
s32 display_upper_text_window;
s32 upper_text_window_timer = -1;
extern s32 g_UpperTextDisplayFlag;
extern struct player *g_CurrentPlayer;
extern AmmoStats ammo_related[30];
extern s32 get_ammo_type_for_weapon(ITEM_IDS weapon);
extern s32 getPlayerCount(void);
extern s32 get_cur_playernum(void);
char dword_CODE_bss_80079DC8[2][BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH];
s32 status_bar_text_buffer_index;
s32 display_statusbar;
char stringbuffer_lowerleft[5][BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH];
extern s32 clock_drawn_flag;

static const GeOriginalAmmoIconAsset ge_original_ammo_icon_assets[] = {{
{ammo_icon_table}
}};

size_t ge_original_ammo_icon_asset_count(void)
{{
    return sizeof(ge_original_ammo_icon_assets)
        / sizeof(ge_original_ammo_icon_assets[0]);
}}

const GeOriginalAmmoIconAsset *ge_original_ammo_icon_asset_at(size_t index)
{{
    if (index >= ge_original_ammo_icon_asset_count()) return NULL;
    return &ge_original_ammo_icon_assets[index];
}}

const GeOriginalAmmoIconAsset *ge_original_ammo_icon_asset_for_ammo_type(
    int32_t ammo_type)
{{
    size_t index;
    for (index = 0U; index < ge_original_ammo_icon_asset_count(); ++index) {{
        if (ge_original_ammo_icon_assets[index].ammo_type == ammo_type)
            return &ge_original_ammo_icon_assets[index];
    }}
    return NULL;
}}

const GeOriginalAmmoIconAsset *
ge_original_ammo_icon_asset_for_segmented_address(uint32_t address)
{{
    size_t index;
    for (index = 0U; index < ge_original_ammo_icon_asset_count(); ++index) {{
        if (ge_original_ammo_icon_assets[index].segmented_address == address)
            return &ge_original_ammo_icon_assets[index];
    }}
    return NULL;
}}

/* The decompiled asset is a native pointer table rather than the ROM's packed
 * u32-offset bank. This is the language-bank ABI boundary used by langGet. */
u8 *langGet(s32 slotID)
{{
    u32 bank = (u32)slotID >> 10;
    u32 index = (u32)slotID & 0x03ffU;
    static const struct {{
        u32 bank;
        char **strings;
        u32 count;
    }} solo_banks[] = {{
{solo_language_rows_text}
    }};
    size_t solo_index;
    for (solo_index = 0U;
            solo_index < sizeof(solo_banks) / sizeof(solo_banks[0]);
            ++solo_index) {{
        if (bank == solo_banks[solo_index].bank)
            return index < solo_banks[solo_index].count
                ? (u8 *)solo_banks[solo_index].strings[index] : NULL;
    }}
    if (bank == LGUN && index < GE_GUN_LANGUAGE_STRING_COUNT)
        return (u8 *)LgunE[index];
    if (bank == LTITLE && index < GE_TITLE_LANGUAGE_STRING_COUNT)
        return (u8 *)LtitleE[index];
    if (bank == LPROPOBJ && index < GE_PROPOBJ_LANGUAGE_STRING_COUNT)
        return (u8 *)LpropobjE[index];
    if (bank == LOPTIONS && index < GE_OPTIONS_LANGUAGE_STRING_COUNT)
        return (u8 *)LoptionsE[index];
    if (bank == LMISC && index < GE_MISC_LANGUAGE_STRING_COUNT)
        return (u8 *)LmiscE[index];
    return NULL;
}}

{body}

{timer_body}

{gauge_body}

{bottom_show_body}

{bottom_tick_body}

void ge_original_dam_mission_hud_reset(void)
{{
    upper_text_buffer_index = 0;
    display_upper_text_window = 0;
    upper_text_window_timer = -1;
    memset(dword_CODE_bss_80079DC8, 0, sizeof(dword_CODE_bss_80079DC8));
}}

void ge_original_bottom_hud_reset(void)
{{
    status_bar_text_buffer_index = 0;
    display_statusbar = 0;
    memset(stringbuffer_lowerleft, 0, sizeof(stringbuffer_lowerleft));
    if (g_CurrentPlayer != NULL) g_CurrentPlayer->bondmesscnt = -1;
}}

int ge_original_bottom_hud_render_snapshot(
    GeOriginalBottomHudRenderSnapshot *snapshot)
{{
    s32 weapon_left;
    s32 ammo_type;
    if (snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->count = (size_t)display_statusbar;
    if (g_CurrentPlayer == NULL) return 1;

    snapshot->timer = g_CurrentPlayer->bondmesscnt;
    snapshot->visible = (uint8_t)(g_CurrentPlayer->hudmessoff == FALSE
        && g_CurrentPlayer->bondmesscnt >= 0
        && g_CurrentPlayer->mpmenuon == FALSE
        && display_statusbar > 0
        && stringbuffer_lowerleft[status_bar_text_buffer_index][0] != '\\0');
    if (!snapshot->visible) return 1;
    snapshot->x = 40 + 0x1e;
    weapon_left = getCurrentPlayerWeaponId(GUNLEFT);
    ammo_type = get_ammo_type_for_weapon((ITEM_IDS)weapon_left);
    snapshot->y = (int16_t)(240
        - ((ammo_type == 0 && clock_drawn_flag != 0) ? 0x16 : 0x32));
    memcpy(snapshot->message,
        stringbuffer_lowerleft[status_bar_text_buffer_index],
        GE_ORIGINAL_BOTTOM_HUD_MESSAGE_CAPACITY);
    snapshot->message[GE_ORIGINAL_BOTTOM_HUD_MESSAGE_CAPACITY - 1U] = '\\0';
    return 1;
}}

size_t ge_original_dam_mission_hud_count(void)
{{
    return (size_t)display_upper_text_window;
}}

const char *ge_original_dam_mission_hud_message(size_t index)
{{
    if (index >= (size_t)display_upper_text_window
            || index >= GE_ORIGINAL_DAM_MISSION_HUD_CAPACITY) return NULL;
    return dword_CODE_bss_80079DC8[(upper_text_buffer_index + (s32)index)
        % GE_ORIGINAL_DAM_MISSION_HUD_CAPACITY];
}}

int ge_original_dam_mission_hud_render_snapshot(
    GeOriginalDamMissionHudRenderSnapshot *snapshot)
{{
    size_t index;
    if (snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->count = ge_original_dam_mission_hud_count();
    snapshot->timer = upper_text_window_timer;
    snapshot->visible = (uint8_t)(snapshot->count != 0U
        && upper_text_window_timer >= 0 && g_UpperTextDisplayFlag == 0);
    if (snapshot->count > GE_ORIGINAL_DAM_MISSION_HUD_CAPACITY)
        snapshot->count = GE_ORIGINAL_DAM_MISSION_HUD_CAPACITY;
    for (index = 0U; index < snapshot->count; index++) {{
        const char *message = ge_original_dam_mission_hud_message(index);
        if (message != NULL) {{
            memcpy(snapshot->messages[index], message,
                   GE_ORIGINAL_DAM_MISSION_HUD_MESSAGE_CAPACITY);
            snapshot->messages[index]
                [GE_ORIGINAL_DAM_MISSION_HUD_MESSAGE_CAPACITY - 1U] = '\\0';
        }}
    }}
    return 1;
}}

int ge_original_gameplay_hud_render_snapshot(
    GeOriginalGameplayHudRenderSnapshot *snapshot)
{{
    struct damage_display_val armour[GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT];
    struct damage_display_val health[GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT];
    ITEM_IDS weapon_left;
    ITEM_IDS weapon_right;
    const GeOriginalAmmoIconAsset *icon_asset;
    s32 ammo_type;
    s32 text_width;
    size_t index;
    if (snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    if (g_CurrentPlayer == NULL) return 1;

    snapshot->ammo_suppression_reasons =
        (uint32_t)g_CurrentPlayer->gunammooff;

    snapshot->gauges_visible = (uint8_t)(
        g_CurrentPlayer->healthshowtime > 0
        && g_CurrentPlayer->watch_animation_state == 0);
    if (snapshot->gauges_visible) {{
        ge_original_hud_make_damage_segments_exact(
            armour, GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT, 1,
            g_CurrentPlayer->apparentarmour);
        ge_original_hud_make_damage_segments_exact(
            health, GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT, -1,
            g_CurrentPlayer->apparenthealth);
        for (index = 0U;
                index < GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT;
                ++index) {{
            snapshot->armour[index] = (GeOriginalGameplayHudGaugeVertex){{
                armour[index].pos.x, armour[index].pos.z,
                armour[index].colour.r, armour[index].colour.g,
                armour[index].colour.b, armour[index].colour.a,
            }};
            snapshot->health[index] = (GeOriginalGameplayHudGaugeVertex){{
                health[index].pos.x, health[index].pos.z,
                health[index].colour.r, health[index].colour.g,
                health[index].colour.b, health[index].colour.a,
            }};
        }}
    }}

    if (g_CurrentPlayer->gunammooff != 0 || g_CurrentPlayer->mpmenuon != 0)
        return 1;
    weapon_left = getCurrentPlayerWeaponId(GUNLEFT);
    weapon_right = getCurrentPlayerWeaponId(GUNRIGHT);
    snapshot->ammo_y = 222;     /* 240 - 18, HUDVALIGN_BOTTOM */
    if (weapon_right != ITEM_UNARMED) {{
        ammo_type = get_ammo_type_for_weapon(weapon_right);
        snapshot->ammo_type = ammo_type;
        if (ammo_type != 0
                && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state
                    != GUN_ANIM_STATE_SWITCH_SWAP
                && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state
                    != GUN_ANIM_STATE_SWITCH_HOLD
                && !bondwalkItemCheckBitflags(
                    weapon_right, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY)) {{
            snapshot->ammo_visible = 1U;
            snapshot->icon_image = ammo_related[ammo_type].IconImage;
            icon_asset = ge_original_ammo_icon_asset_for_segmented_address(
                snapshot->icon_image);
            text_width = icon_asset != NULL ? icon_asset->width : 5;
            snapshot->icon_x = 301; /* 40 + 320 - rightx */
            snapshot->icon_y = (int16_t)(220
                + ammo_related[ammo_type].IconYOffset);
            snapshot->magazine_x = (int16_t)(snapshot->icon_x
                - text_width / 2 - 4);
            snapshot->reserve_x = (int16_t)(snapshot->icon_x
                + (text_width + 1) / 2 + 3);
            if (bondwalkItemCheckBitflags(
                    weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS)) {{
                snapshot->magazine_ammo = 0;
                snapshot->reserve_ammo =
                    g_CurrentPlayer->ammoheldarr[ammo_type]
                    + g_CurrentPlayer->hands[GUNRIGHT]
                        .weapon_ammo_in_magazine;
                if (weapon_left == weapon_right)
                    snapshot->reserve_ammo +=
                        g_CurrentPlayer->hands[GUNLEFT]
                            .weapon_ammo_in_magazine;
                snapshot->reserve_visible = 1U;
            }} else {{
                snapshot->magazine_ammo =
                    g_CurrentPlayer->hands[GUNRIGHT]
                        .weapon_ammo_in_magazine;
                snapshot->reserve_ammo =
                    g_CurrentPlayer->ammoheldarr[ammo_type];
                snapshot->reserve_visible = snapshot->reserve_ammo > 0;
            }}
        }}
    }}
    if (weapon_left != ITEM_UNARMED) {{
        ammo_type = get_ammo_type_for_weapon(weapon_left);
        snapshot->left_ammo_type = ammo_type;
        if (ammo_type != 0
                && g_CurrentPlayer->hands[GUNLEFT].weapon_action_state
                    != GUN_ANIM_STATE_SWITCH_SWAP
                && g_CurrentPlayer->hands[GUNLEFT].weapon_action_state
                    != GUN_ANIM_STATE_SWITCH_HOLD
                && !bondwalkItemCheckBitflags(
                    weapon_left, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY)) {{
            snapshot->left_ammo_visible = 1U;
            snapshot->left_icon_image = ammo_related[ammo_type].IconImage;
            icon_asset = ge_original_ammo_icon_asset_for_segmented_address(
                snapshot->left_icon_image);
            text_width = icon_asset != NULL ? icon_asset->width : 5;
            snapshot->left_icon_x = 99; /* 40 + leftx */
            snapshot->left_icon_y = (int16_t)(220
                + ammo_related[ammo_type].IconYOffset);
            snapshot->left_magazine_x = (int16_t)(snapshot->left_icon_x
                + text_width / 2 + 3);
            snapshot->left_reserve_x = (int16_t)(snapshot->left_icon_x
                - (text_width + 1) / 2 - 4);
            if (bondwalkItemCheckBitflags(
                    weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS)) {{
                snapshot->left_magazine_ammo = 0;
                snapshot->left_reserve_ammo =
                    g_CurrentPlayer->ammoheldarr[ammo_type]
                    + g_CurrentPlayer->hands[GUNLEFT]
                        .weapon_ammo_in_magazine;
                if (weapon_left == weapon_right)
                    snapshot->left_reserve_ammo +=
                        g_CurrentPlayer->hands[GUNRIGHT]
                            .weapon_ammo_in_magazine;
                snapshot->left_reserve_visible = 1U;
            }} else {{
                snapshot->left_magazine_ammo =
                    g_CurrentPlayer->hands[GUNLEFT]
                        .weapon_ammo_in_magazine;
                snapshot->left_reserve_ammo =
                    g_CurrentPlayer->ammoheldarr[ammo_type];
                snapshot->left_reserve_visible =
                    snapshot->left_reserve_ammo > 0;
            }}
        }}
    }}
    return 1;
}}
"""
    args.output.write_text(output)
    print(f"generated exact Dam upper-HUD enqueue -> {args.output}")


if __name__ == "__main__":
    main()

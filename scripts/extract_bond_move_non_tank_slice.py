#!/usr/bin/env python3
"""Extract exact non-tank player tick and weapon-sway MoveBond dependencies."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


FUNCTIONS = {
    "src/game/gun.c": (
        "get_ptr_itemheader_in_hand",
        "draw_item_in_hand",
        "sub_GAME_7F05DAE4",
        "gunSetBondWeaponSway",
        "gunSetOffsetRelated",
    ),
    "src/game/gunfire.c": ("gunSetGunAmmoVisible",),
    "src/game/propobj.c": (
        "countdownTimerSetVisible",
        "deactivate_alarm_sound_effect",
        "check_deactivate_gas_sound",
    ),
    "src/game/mp_music.c": (
        "musicPlaySlot",
        "musicStopSlot",
        "sub_GAME_7F0C0BF0",
        "sub_GAME_7F0C0C10",
        "sub_GAME_7F0C1310",
        "set_missionstate",
    ),
    "src/game/lv.c": ("lvlSetControlsLockedFlag",),
    "src/game/options.c": (
        "call_sndGetSfxSlotFirstNaturalVolume",
        "get_mTrack2Vol",
        "watch_play_beep_sound",
        "reset_watch_item_is_actively_selected",
        "is_holding_greater_than_2E_left_on_stick",
        "is_holding_greater_than_2E_right_on_stick",
        "get_controlstick_lr_enabled",
        "set_controlstick_lr_disabled",
        "sub_GAME_7F0A4FB0",
        "sub_GAME_7F0A4FEC",
        "is_holding_greater_than_2E_up_on_stick",
        "is_holding_greater_than_2E_down_on_stick",
        "get_watch_stick_y_nav_ready",
        "disable_watch_stick_y_nav_ready",
        "sub_GAME_7F0A5088",
        "sub_GAME_7F0A50C4",
        "is_holding_less_than_10_up_on_stick",
        "is_holding_less_than_10_down_on_stick",
        "watch_stick_y_was_active",
        "reset_watch_stick_y_latch",
        "watch_stick_y_pressed_up",
        "watch_stick_y_pressed_down",
        "sub_GAME_7F0A5210",
        "watch_screen0_navigation",
        "watch_screen1_navigation",
        "watch_screen2_navigation",
        "watch_screen3_navigation",
        "watch_screen4_navigation",
        "unused_watch_screen_navigation",
        "controller_options_controlstyle_navigation",
        "controller_options_inputs_navigation",
        "game_options_music_volume_navigation",
        "game_options_fx_volume_navigation",
        "game_options_inventory_navigation",
        "sub_GAME_7F0A611C",
        "mission_brief_background_navigation",
        "mission_brief_m_briefing_navigation",
        "mission_brief_q_branch_navigation",
        "mission_brief_moneypenny_navigation",
        "mission_brief_objectives_navigation",
        "build_watch_static_scanline_vertices",
        "sub_GAME_7F0A69A8",
        "sub_GAME_7F0A51D8",
        "sub_GAME_7F0A5998",
        "watchWrapAroundPI",
        "sub_GAME_7F0A6A80",
        "reset_controller_options_index",
        "reset_game_options_index",
        "zero_D_800409A4",
    ),
    "src/game/debugmenu_handler.c": ("get_debug_gunwatchpos_flag",),
    "src/game/bondview2.c": (
        "bondviewGetBondBreathing",
        "bondviewSetupPauseTransition",
        "bondviewStartPauseTransition",
        "bondviewStartUnpauseTransition",
        "bondViewIsPauseTransitioning",
        "sub_GAME_7F07E7CC",
        "bondviewSetPauseWatchRelated",
        "bondviewSetPauseWatchRelatedAlt",
        "hudmsgsSetOn",
        "hudmsgsSetOff",
        "bondviewClearUpperTextDisplayFlag",
        "bondviewSetUpperTextDisplayFlag",
        "bondviewPlayerTickDamageAndHealth",
        "bondviewPlayerTickExplode",
        "bondviewPlayerStopAudioForPause",
        "bondviewUpdatePauseTransition",
        "bondviewStepWatchAnimation",
        "bondviewWatchAnimationTick",
        "set_open_close_solo_watch_menu_to1",
    ),
    "src/game/music_0D2720.c": (
        "getmusictrack_or_randomtrack",
        "musicGetBgTrackForStage",
        "musicGetXTrackForStage",
    ),
    "src/snd.c": (
        "sndGetSfxSlotFirstNaturalVolume",
        "sndGetSfxSlotNaturalVolume",
    ),
}

VARIABLES = {
    "src/snd.c": ("g_sndBootswitchSound",),
    "src/game/bondview.c": (
        "g_DamageTypes",
        "g_HealthDisplayDurations",
        "g_UpperTextDisplayFlag",
        "g_SurroundBondWithExplosionsFlag",
        "watch_transition_time",
    ),
    "src/game/bondview2.c": (
        "g_SurroundBondWithExplosionsTicks",
        "g_PlayerTickExplodeCreatePosition",
    ),
    "src/game/propobj.c": (
        "clock_drawn_flag",
        "ptr_alarm_sfx",
        "ptr_gas_sound",
    ),
    "src/game/mp_music.c": (
        "music_slot_active_0",
        "music_slot_minutes_0",
        "music_slot_seconds_0",
        "stageMusicID",
        "dword_CODE_bss_8008C604",
        "mission_state",
    ),
    "src/game/options.c": (
        "mTrack2Vol",
        "watch_screen_index",
        "controller_options_index",
        "game_options_index",
        "mission_brief_index",
        "D_800409A4",
        "watch_item_is_actively_selected",
        "watch_inventory_text_y",
        "watch_inventory_text_target_y",
        "g_curWatchItemIndex",
        "watch_inventory_cursor_pos",
        "watch_inventory_text_is_settled",
        "D_800409C8",
        "D_800409CC",
        "D_800409D8",
        "controlstick_lr_enabled",
        "watch_stick_y_nav_ready",
        "watch_stick_y_prev_active",
        "D_80040AF4",
        "D_80040AF8",
        "D_80040AFC",
        "D_80040B00",
        "D_80040B0C",
        "D_80040B10",
        "D_80040B14",
        "D_80040B1C",
        "D_80040B44",
        "g_WatchBackgroundGreen",
        "g_WatchStaticScanlineAlpha",
        "g_WatchStaticScanlineY",
    ),
    "src/game/front.c": ("mission_failed_or_aborted",),
    "src/game/music_0D2720.c": (
        "music_setup_entries",
        "random_tracks",
    ),
}


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{re.escape(name)}\s*\([^;\n]*\)[^;{{}}]*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing {name}")
    brace = source.index("{", match.start())
    # The US and non-US conditional branches close different lexical blocks,
    # so raw brace counting cannot delimit this body before preprocessing.
    if name == "bondviewPlayerTickDamageAndHealth":
        next_function = source.find("\nvoid bondviewPlayerTickExplode", match.end())
        if next_function < 0:
            raise ValueError(f"missing successor for {name}")
        end = source.rfind("}", brace, next_function)
        if end < brace:
            raise ValueError(f"unterminated {name}")
        return source[match.start():end + 1]
    depth = 0
    in_string = in_char = in_line_comment = in_block_comment = escaped = False
    pos = brace
    while pos < len(source):
        char = source[pos]
        next_char = source[pos + 1] if pos + 1 < len(source) else ""
        if in_line_comment:
            if char == "\n": in_line_comment = False
        elif in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
                pos += 1
        elif in_string:
            if escaped: escaped = False
            elif char == "\\": escaped = True
            elif char == '"': in_string = False
        elif in_char:
            if escaped: escaped = False
            elif char == "\\": escaped = True
            elif char == "'": in_char = False
        elif char == "/" and next_char == "/":
            in_line_comment = True
            pos += 1
        elif char == "/" and next_char == "*":
            in_block_comment = True
            pos += 1
        elif char == '"': in_string = True
        elif char == "'": in_char = True
        elif char == "{": depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0: return source[match.start():pos + 1]
        pos += 1
    raise ValueError(f"unterminated {name}")


def extract_variable(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^(?:/\*[^\n]*\*/\s*)?[A-Za-z_][^;\n]*\b{re.escape(name)}(?:\s*\[[^\n]*?\])?\s*(?:=|;)",
        source,
    )
    if match is None:
        raise ValueError(f"missing {name}")
    start = match.start()
    comment_end = source.find("*/", start, match.end())
    if comment_end >= 0:
        start = comment_end + 2
        while start < len(source) and source[start].isspace() and source[start] != "\n":
            start += 1
    braces = brackets = parens = 0
    for pos in range(match.end() - 1, len(source)):
        char = source[pos]
        if char == "{": braces += 1
        elif char == "}": braces -= 1
        elif char == "[": brackets += 1
        elif char == "]": brackets -= 1
        elif char == "(": parens += 1
        elif char == ")": parens -= 1
        elif char == ";" and braces == brackets == parens == 0:
            return source[start:pos + 1]
    raise ValueError(f"unterminated {name}")


def digest(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def generate(repo: Path) -> str:
    paths = set(FUNCTIONS) | set(VARIABLES)
    sources = {path: (repo / path).read_text() for path in paths}
    sections = [
        "/* Generated mechanically from canonical GoldenEye non-tank bodies/data. */",
        "#include <math.h>",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        '#include "game/bondview.h"',
        '#include "assets/animationtable_data.h"',
        '#include "game/chrobjdata.h"',
        '#include "game/explosion.h"',
        '#include "game/gun.h"',
        '#include "game/chrai.h"',
        '#include "game/player.h"',
        '#include "game/propobj.h"',
        '#include "game/model.h"',
        '#include "game/initanitable.h"',
        '#include "game/lv.h"',
        '#include "game/mp_music.h"',
        '#include "game/music_0D2720.h"',
        '#include "game/options.h"',
        '#include "game/bondinv.h"',
        '#include "game/language.h"',
        '#include "game/debugmenu_handler.h"',
        '#include "game/file.h"',
        '#include "game/front.h"',
        '#include "joy.h"',
        '#include "boss.h"',
        '#include "bondgame.h"',
        '#include "snd.h"',
        '#include "music.h"',
        '#include "random.h"',
        '#include "ge_original_bond_input_internal.h"',
        "#ifdef VERSION_EU",
        "#define MP_MUSIC_FRAMERATE 50",
        "#else",
        "#define MP_MUSIC_FRAMERATE 60",
        "#endif",
        "#ifdef GE_PORT_BOND_MOVE_ANIMATION_OFFSETS",
        "/* Preserve the original serialized animation-offset ABI without an",
        " * absolute ELF symbol, which the 3DSX relocation format rejects. */",
        "#undef ANIM_DATA_bond_watch",
        "#define ANIM_DATA_bond_watch "
        "(*(s32 *)(uintptr_t)PTR_ANIM_bond_watch)",
        "#endif",
        "struct music_setup { s16 stage_id; s16 main_music; s16 bg_sound; s16 xtrack; };",
        "extern void bondviewZoomToWatchOnOpen(void);",
        "extern void bondviewZoomFromWatchOnExit(void);",
        "extern void bondviewUpdatePauseTransition(void);",
        "extern void bondviewStepWatchAnimation(void);",
        "extern void bondviewUpdateWatchZoomIn(void);",
        "extern void sub_GAME_7F0A6A80(void);",
        "extern void sub_GAME_7F0A51D8(void);",
        "extern void bondinvDetermineEquippedItem(void);",
        "extern void sub_GAME_7F0C1310(void);",
        "extern s32 g_ControlsLockedFlag;",
        "extern u16 sndGetSfxSlotNaturalVolume(u8 sfxIndex);",
        "extern ModelFileHeader *get_ptr_itemheader_in_hand(GUNHAND hand);",
        "extern void place_item_in_hand_swap_and_make_visible(GUNHAND hand, s32 weapon);",
        "/* PLAYERFLAG_NOTIMER is the third canonical PLAYERFLAG BITFLAG entry. */",
        "#define PLAYERFLAG_NOTIMER (1 << 2)",
        "#define PLAYERFLAG_LOCKCONTROLS (1 << 0)",
        "/* Exact non-EU regional value from bondview2.c. */",
        "#define PLAYER_TICKEXPLODE_FACTOR 15",
        "#ifndef M_MINUS_PI_F",
        "#define M_MINUS_PI_F -3.1415927f",
        "#endif",
        "",
    ]
    for relative, names in VARIABLES.items():
        for name in names:
            declaration = extract_variable(sources[relative], name)
            sections.extend((f"/* {name} sha256={digest(declaration)} */", declaration, ""))
    for relative, names in FUNCTIONS.items():
        for name in names:
            body = extract_function(sources[relative], name)
            sections.extend((f"/* {name} sha256={digest(body)} */", body, ""))
    return "\n".join(sections)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.repo)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    count = sum(map(len, FUNCTIONS.values())) + sum(map(len, VARIABLES.values()))
    print(f"generated {count} exact non-tank MoveBond bodies/data -> {args.output}")


if __name__ == "__main__":
    main()

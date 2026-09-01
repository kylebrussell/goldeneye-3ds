#!/usr/bin/env python3
"""Generate the portable cast scheduler data from unchanged front.c."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

from extract_gun_update_and_fire_slice import extract_function


FUNCTIONS = (
    "do_extended_cast_display",
    "init_menu18_displaycast",
    "interface_menu18_displaycast",
    "constructor_menu18_displaycast",
)

REQUIRED = (
    "intro_character_index = 1",
    "randomly_selected_intro_animation = randomGetNext() %",
    "modelSetAnimTranslationScale(cast_model, 0.1f)",
    "modelSetAnimPlaySpeed(cast_model, 0.5f, 0)",
    "g_MenuTimer >= INTERFACE_MENU18_TIMER",
    "viSetFovY(46.0f)",
    "viSetZRange(10.0f, 2000.0f)",
    "set_cur_player_screen_size(440, 330)",
    "cast_camera_dist = (cast_camera_dist_end - cast_camera_dist_start)",
    "modelTickAnim(cast_model, g_ClockTimer, 1)",
    "cast_rootvel_accumulator.x = vec.f[0] + CAST_DAMP",
)


def array_body(source: str, declaration: str) -> str:
    match = re.search(re.escape(declaration) + r"\s*=\s*\{(.*?)\n\};",
                      source, re.DOTALL)
    if match is None:
        raise ValueError(f"cannot find canonical array {declaration}")
    return match.group(1)


def generate(repo: Path) -> str:
    front = (repo / "src/game/front.c").read_text()
    bodies = [extract_function(front, name) for name in FUNCTIONS]
    contract = "\n\n".join(bodies)
    for needle in REQUIRED:
        if needle not in contract:
            raise ValueError(f"canonical cast contract lost: {needle}")

    chars_source = array_body(front, "struct intro_char intro_char_table[]")
    char_pattern = re.compile(
        r"\{\s*(BODY_[A-Za-z0-9_]+),\s*(HEAD_[A-Za-z0-9_]+),\s*"
        r"getStringID\(LTITLE,\s*(TITLE_STR_[A-Za-z0-9_]+)\),\s*"
        r"getStringID\(LTITLE,\s*(TITLE_STR_[A-Za-z0-9_]+)\),\s*"
        r"getStringID\(LTITLE,\s*(TITLE_STR_[A-Za-z0-9_]+)\),\s*"
        r"(-?\d+),\s*(-?\d+)\s*\}")
    chars = char_pattern.findall(chars_source)
    if len(chars) != 34:
        raise ValueError(f"expected 34 authored cast characters, got {len(chars)}")

    animation_source = array_body(
        front, "struct intro_animation intro_animation_table[]")
    animations = re.findall(
        r"\{\s*(ANIM_[A-Za-z0-9_]+),\s*([-+0-9.eEfF]+),\s*"
        r"([-+0-9.eEfF]+),\s*(-?\d+)\s*\}", animation_source)
    if len(animations) != 22:
        raise ValueError(
            f"expected 22 authored cast animations, got {len(animations)}")
    offsets_source = (repo / "assets/animationtable_data.h").read_text()
    offsets = dict(re.findall(
        r"^#define\s+PTR_(ANIM_[A-Za-z0-9_]+)\s+(0x[0-9A-Fa-f]+|\d+)",
        offsets_source, re.MULTILINE))
    if any(animation[0] not in offsets for animation in animations):
        raise ValueError("cast animation missing canonical record offset")

    rifles = re.findall(r"PROP_[A-Za-z0-9_]+", array_body(
        front, "struct intro_random_rifles random_rifles_in_intro"))
    pistols = re.findall(r"PROP_[A-Za-z0-9_]+", array_body(
        front, "struct intro_random_pistols random_pistols_in_intro"))
    if len(rifles) != 6 or len(pistols) != 10:
        raise ValueError("canonical cast weapon pools changed")

    digest = hashlib.sha256((contract + chars_source + animation_source
        + "\n".join(rifles + pistols)).encode()).hexdigest()
    char_rows = "\n".join(
        f"    {{{body},{head},{{getStringID(LTITLE,{text1}),"
        f"getStringID(LTITLE,{text2}),getStringID(LTITLE,{text3})}},"
        f"{reserved},{flag}}},"
        for body, head, text1, text2, text3, reserved, flag in chars)
    animation_rows = "\n".join(
        f"    {{{anim},{start},{speed},{preset},{offsets[anim]}}},"
        for anim, start, speed, preset in animations)
    return f'''/* Generated mechanically from unchanged NTSC-U front.c.
 * Canonical frontend cast contract SHA-256: {digest} */
#define GE_FRONTEND_CAST_CONTRACT_SHA256 "{digest}"
static const GeOriginalFrontendCastCharacter ge_cast_characters[]={{
{char_rows}
}};
static const GeOriginalFrontendCastAnimation ge_cast_animations[]={{
{animation_rows}
}};
static const int32_t ge_cast_rifles[]={{{','.join(rifles)}}};
static const int32_t ge_cast_pistols[]={{{','.join(pistols)}}};
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output = generate(args.repo)
    if args.check:
        if not args.output.exists() or args.output.read_text() != output:
            raise SystemExit(f"stale canonical cast contract: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output)


if __name__ == "__main__":
    main()

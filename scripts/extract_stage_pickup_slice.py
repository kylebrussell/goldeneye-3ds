#!/usr/bin/env python3
"""Extract GoldenEye's unchanged single-player pickup call chain."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def function_text(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;={{}}]*\b{name}\s*\([^;]*\)\s*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing function {name}")
    brace = source.index("{", match.start())
    depth = 0
    state = "code"
    pos = brace
    while pos < len(source):
        char = source[pos]
        nxt = source[pos + 1] if pos + 1 < len(source) else ""
        if state == "code":
            if char == "/" and nxt == "*":
                state = "block"
                pos += 2
                continue
            if char == "/" and nxt == "/":
                state = "line"
                pos += 2
                continue
            if char == '"':
                state = "string"
            elif char == "'":
                state = "char"
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    return source[match.start():pos + 1]
        elif state == "block" and char == "*" and nxt == "/":
            state = "code"
            pos += 2
            continue
        elif state == "line" and char == "\n":
            state = "code"
        elif state in ("string", "char"):
            if char == "\\":
                pos += 2
                continue
            if ((state == "string" and char == '"')
                    or (state == "char" and char == "'")):
                state = "code"
        pos += 1
    raise ValueError(f"unterminated function {name}")


def render(repo: Path) -> str:
    propobj = (repo / "src/game/propobj.c").read_text()
    chrprop = (repo / "src/game/chrprop.c").read_text()
    bondinv = (repo / "src/game/bondinv.c").read_text()
    bondview = (repo / "src/game/bondview2.c").read_text()
    gunfire = (repo / "src/game/gunfire.c").read_text()
    includes = """/* Generated unchanged canonical single-player pickup slice. */
#include <ultra64.h>
#include <math.h>
#include <PR/libaudio.h>
#include <assets/oddtextures.h>
#include <bondgame.h>
#include <boss.h>
#include <limits.h>
#include <string.h>
#include <music.h>
#include <memp.h>
#include <snd.h>
#include <gbi_extension.h>
#include "propobj.h"
#include "assets/obseg/text/LpropobjE.h"
#include "bg.h"
#include "bondaicommands.h"
#include "bondinv.h"
#include "bondview.h"
#include "chr.h"
#include "chrai.h"
#include "chraction.h"
#include "gun.h"
#include "language.h"
#include "lv.h"
#include "player.h"
#include "stan.h"
#include "ge_original_stage_pickup.h"

extern bool objCanPickupFromSafe(ObjectRecord *obj);
extern s32 bondinvHasInvItem(ITEM_IDS weapon);
extern s32 get_ammo_type_for_weapon(ITEM_IDS weapon);
extern s32 get_max_ammo_for_weapon(ITEM_IDS weapon);
extern s32 get_ammo_count_for_weapon(ITEM_IDS weapon);
extern bool bondinvHasDualWeapon(ITEM_IDS right, ITEM_IDS left);
extern s32 check_cur_player_ammo_amount_in_inventory(AMMOTYPE type);
extern u8 *bondinvGetActivatedTextObject(ObjectRecord *obj);
extern u8 *bondinvGetActivatedTextWeapon(ITEM_IDS weaponnum);
extern s32 get_ammo_in_magazine(AmmoCrateRecord *crate);
extern void add_ammo_to_inventory(AMMOTYPE type, s32 amount,
                                  s32 play_sound, s32 display_text);
extern void set_sound_effect_for_weapontype_collection(ITEM_IDS weapon);
extern s32 bondinvAddWeaponByProp(PropRecord *prop);
extern s32 bondinvAddPropToInv(PropRecord *prop);
extern void display_text_for_weapon_in_lower_left_corner(ITEM_IDS weapon);
extern void display_text_when_ammo_collected(s32 type, s32 quantity);
extern s32 ammo_collected_from_weapon(WeaponObjRecord *weapon);
extern void objFree(ObjectRecord *obj, s32 free_prop, s32 can_regenerate);
extern MPSCENARIOS get_scenario(void);
extern AmmoStats ammo_related[30];
extern InvItem *bondinvGetNextAvailItem(void);
extern void bondinvInsertItem(InvItem *item);
#ifndef RUNTIMEBITFLAG_REMOVE
#define RUNTIMEBITFLAG_REMOVE (1u << 2)
#define RUNTIMEBITFLAG_TAGGED (1u << 4)
#define RUNTIMEBITFLAG_DESTROYED (1u << 10)
#endif
#ifndef RUNTIMEBITFLAG_ACTIVATED
#define RUNTIMEBITFLAG_ACTIVATED (1u << 14)
#endif
"""
    pickup = function_text(propobj, "propPickupByPlayer")
    interact = function_text(propobj, "propobjInteract")
    obj_tick = function_text(propobj, "objTickPlayer")
    props_tick = function_text(chrprop, "propsTickPlayer")
    support = [
        "f32 g_SoloAmmoMultiplier = 1.0f;",
        "s32 j_text_trigger = 0;",
        function_text(bondview, "isBondInTank"),
        function_text(bondview, "bondviewGetPlayerPitchRadians"),
        function_text(bondview, "bondviewAddCurrentPlayerArmor"),
        function_text(bondinv, "bondinvAddPropToInv"),
        function_text(bondinv, "bondinvGetTextbyWeaponID"),
        function_text(bondinv, "bondinvGetActivatedTextObject"),
        function_text(bondinv, "bondinvGetActivatedTextWeapon"),
        function_text(bondinv, "bondinvAddWeaponByProp"),
        function_text(propobj, "append_text_picked_up"),
        "#define append_text_picked_up(buffer, type, quantity) \\\n    append_text_picked_up((buffer), (u8 *)(uintptr_t)(type), \\\n                          (u8 *)(uintptr_t)(quantity))",
        function_text(propobj, "append_text_ammo_amount_word"),
        function_text(propobj, "apped_text_ammotype"),
        function_text(propobj, "prepare_ammo_type_collection_text"),
        "#undef append_text_picked_up",
        function_text(propobj, "display_text_when_ammo_collected"),
        function_text(propobj, "get_ammo_in_magazine"),
        function_text(propobj, "ammo_collected_from_weapon"),
        function_text(gunfire, "get_max_ammo_for_type"),
        function_text(gunfire, "get_max_ammo_for_weapon"),
    ]
    weapon_tick = function_text(propobj, "weaponTickPlayer")
    return "\n\n".join((
        includes,
        *support,
        "#define propPickupByPlayer ge_original_stage_prop_pickup_exact",
        pickup,
        "#undef propPickupByPlayer",
        "#if defined(__APPLE__)",
        "TICKOP propPickupByPlayer(PropRecord *prop, bool showstring)\n"
        "{\n"
        "    return ge_original_stage_prop_pickup_exact(prop, showstring);\n"
        "}",
        "#else",
        "TICKOP propPickupByPlayer(PropRecord *prop, bool showstring) "
        "__attribute__((alias(\"ge_original_stage_prop_pickup_exact\")));",
        "#endif",
        interact,
        "#define propPickupByPlayer ge_original_stage_prop_pickup_exact\n"
        "#define objTickPlayer ge_original_stage_obj_tick_player_exact",
        obj_tick,
        "#undef objTickPlayer\n#undef propPickupByPlayer",
        "#define objTickPlayer ge_original_stage_obj_tick_player_exact",
        weapon_tick,
        "#undef objTickPlayer",
        "#define objTickPlayer ge_original_stage_obj_tick_player_exact\n"
        "#define propsTickPlayer ge_original_stage_props_tick_player_exact",
        props_tick,
        "#undef propsTickPlayer\n#undef objTickPlayer",
        "",
    ))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(args.repo))
    print("generated unchanged canonical single-player pickup slice")


if __name__ == "__main__":
    main()

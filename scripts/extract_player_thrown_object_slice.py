#!/usr/bin/env python3
"""Extract the canonical player thrown-object chain used by ITEM_BUG."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{re.escape(name)}\s*\([^;\n]*\)[^;{{}}]*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing {name}")
    brace = source.index("{", match.start())
    depth = 0
    state = "code"
    escaped = False
    pos = brace
    while pos < len(source):
        char = source[pos]
        nxt = source[pos + 1] if pos + 1 < len(source) else ""
        if state == "line":
            if char == "\n":
                state = "code"
        elif state == "block":
            if char == "*" and nxt == "/":
                state = "code"
                pos += 1
        elif state in ("string", "char"):
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif (state == "string" and char == '"') or (
                state == "char" and char == "'"
            ):
                state = "code"
        elif char == "/" and nxt == "/":
            state = "line"
            pos += 1
        elif char == "/" and nxt == "*":
            state = "block"
            pos += 1
        elif char == '"':
            state = "string"
        elif char == "'":
            state = "char"
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[match.start() : pos + 1]
        pos += 1
    raise ValueError(f"unterminated {name}")


def generate(repo: Path) -> str:
    sources = {
        "gun": (repo / "src/game/gun.c").read_text(),
        "gunfire": (repo / "src/game/gunfire.c").read_text(),
        "objective": (repo / "src/game/objective_status2.c").read_text(),
        "bondview": (repo / "src/game/bondview2.c").read_text(),
        "bondview_base": (repo / "src/game/bondview.c").read_text(),
        "bondinv": (repo / "src/game/bondinv.c").read_text(),
        "chr": (repo / "src/game/chr.c").read_text(),
        "chraction": (repo / "src/game/chraction.c").read_text(),
        "chrprop": (repo / "src/game/chrprop.c").read_text(),
        "propobj": (repo / "src/game/propobj.c").read_text(),
    }
    requested = (
        ("objective", "mtxLoadRandomRotation"),
        ("objective", "sub_GAME_7F057C14"),
        ("bondview_base", "transformAndNormalizeByLength2Dto3D"),
        ("bondview_base", "getPlayer_c_screenwidth"),
        ("bondview_base", "getPlayer_c_screenheight"),
        ("bondview_base", "getPlayer_c_perspaspect"),
        ("bondview_base", "currentPlayerGetViewToWorldMtxf"),
        ("bondview", "getCurrentPlayerPrevPos"),
        ("bondview", "getCurrentPlayerProp"),
        ("gun", "get_ptr_item_statistics"),
        ("gun", "currentPlayerEquipWeaponWrapper"),
        ("gun", "getCurrentPlayerWeaponId"),
        ("gun", "get_item_in_hand_or_watch_menu"),
        ("gun", "bondwalkItemCheckBitflags"),
        ("gun", "sub_GAME_7F05D690"),
        ("bondinv", "bondinvRemoveItem"),
        ("bondinv", "bondinvRemoveItemByID"),
        ("bondinv", "bondinvRemovePropWeaponByID"),
        ("chrprop", "chrpropDetach"),
        ("chr", "chrGetEquippedWeaponProp"),
        ("propobj", "weaponSetGunfireVisible"),
        ("chraction", "chrSetFiring"),
        ("propobj", "objDetach"),
        ("propobj", "sub_GAME_7F0537B8"),
        ("propobj", "sub_GAME_7F053894"),
        ("propobj", "chrobjSndCreatePostEvent"),
        ("propobj", "chrobjSndCreatePostEventDefault"),
        ("gunfire", "bullet_path_from_screen_center"),
        ("gun", "generate_player_thrown_object"),
    )
    bodies = [extract_function(sources[file], name) for file, name in requested]
    digest = hashlib.sha256("\n\n".join(bodies).encode()).hexdigest()
    # The decompiler typed this three-word scratch coordinate as one s32 even
    # though bullet_path_from_screen_center writes x/y/z through its address.
    # Preserve the canonical source digest above, but repair the native ABI
    # type so sanitizers and non-N64 stack layouts do not overflow it.
    old_decl = "s32 sp94; // sp148"
    new_decl = "coord3d sp94; // native type repair for 3-word output"
    if bodies[-1].count(old_decl) != 1:
        raise ValueError("canonical thrown-object scratch declaration drifted")
    bodies[-1] = bodies[-1].replace(old_decl, new_decl)
    preamble = f'''/* Generated mechanically from decompiled original sources; do not edit. */
/* Canonical bodies SHA-256: {digest} */
#include <ultra64.h>
#include "include/limits.h"
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include <music.h>
#include <snd.h>
#include "game/bondinv.h"
#include "game/bondview.h"
#include "game/chr.h"
#include "game/gun.h"
#include "game/matrixmath.h"
#include "game/model.h"
#include "game/objective_status.h"
#include "game/propobj.h"
#include "random.h"
#include "ge_original_bond_input_internal.h"
#include "ge_original_covert_modem_projectile.h"

extern void chrpropDetach(PropRecord *prop);
extern void chrSetFiring(ChrRecord *chr, GUNHAND hand, s32 firing);
extern void bondinvRemoveItem(InvItem *item);
extern GunModelFileRecord gitem_structs[];
extern WeaponStats default_weaponstats;

#ifndef SCREEN_HEIGHT_240
#define SCREEN_HEIGHT_240 240
#endif
#ifndef SCREEN_HEIGHT_272
#define SCREEN_HEIGHT_272 272
#endif

#define THROWN_ITEM_REFRESH_RATE 60
#define THROWN_ITEM_TIMER_SOLO 300
#define THROWN_ITEM_TIMER_MULTI 180
#define THROWN_ITEM_TIMER_DEFAULT 240
#ifndef RUNTIMEBITFLAG_HASPROJECTILE
#define RUNTIMEBITFLAG_HASPROJECTILE RUNTIMEBITFLAG_00000080
#endif

const f32 ge_original_gun_screen_aspect_ratio = 4.0f / 3.0f;
#define g_GunScreenAspectRatio ge_original_gun_screen_aspect_ratio
ModelJoint ge_original_prop_weapon_joints[2] = {{
    {{0x0015, 0x0000, 0x0000}}, {{0x0015, 0x0001, 0x0001}}
}};
ModelSkeleton ge_original_skeleton_prop_weapon = {{
    2, 0, ge_original_prop_weapon_joints, 0, 0
}};
#define skeleton_prop_weapon ge_original_skeleton_prop_weapon
#define ZeroCoord ge_original_zero_coord
coord3d ge_original_zero_coord = {{0.0f, 0.0f, 0.0f}};
#define mtxLoadRandomRotation ge_original_mtx_load_random_rotation_exact
#define sub_GAME_7F057C14 ge_original_random_throw_transform_exact
#define transformAndNormalizeByLength2Dto3D ge_original_transform_screen_to_direction_exact
#define getPlayer_c_screenwidth ge_original_get_player_screen_width_exact
#define getPlayer_c_screenheight ge_original_get_player_screen_height_exact
#define getPlayer_c_perspaspect ge_original_get_player_perspective_aspect_exact
#define currentPlayerGetViewToWorldMtxf ge_original_get_player_view_to_world_exact
#define getCurrentPlayerPrevPos ge_original_get_current_player_prev_pos_exact
#define getCurrentPlayerProp ge_original_get_current_player_prop_exact
#define get_ptr_item_statistics ge_original_get_item_statistics_exact
#define currentPlayerEquipWeaponWrapper ge_original_equip_weapon_wrapper_exact
#define getCurrentPlayerWeaponId ge_original_get_current_player_weapon_exact
#define get_item_in_hand_or_watch_menu ge_original_get_item_in_hand_exact
#define bondwalkItemCheckBitflags ge_original_item_check_bitflags_exact
#define sub_GAME_7F05D690 ge_original_restore_previous_hand_weapons_exact
#define bondinvRemoveItem ge_original_bondinv_remove_item_exact
#define bondinvRemoveItemByID ge_original_bondinv_remove_item_by_id_exact
#define bondinvRemovePropWeaponByID ge_original_bondinv_remove_prop_weapon_exact
#define chrpropDetach ge_original_chrprop_detach_exact
#define chrGetEquippedWeaponProp ge_original_chr_get_equipped_weapon_exact
#define weaponSetGunfireVisible ge_original_weapon_set_gunfire_visible_exact
#define chrSetFiring ge_original_chr_set_firing_exact
#define objDetach ge_original_obj_detach_exact
#define sub_GAME_7F0537B8 ge_original_sound_volume_exact
#define sub_GAME_7F053894 ge_original_sound_position_volume_exact
#define chrobjSndCreatePostEvent ge_original_sound_position_event_exact
#define chrobjSndCreatePostEventDefault ge_original_sound_position_event_default_exact
#define bullet_path_from_screen_center ge_original_bullet_path_from_screen_center_exact
#define gunInitProjectileFromPlayer ge_original_gun_init_projectile_from_player_exact
#define generate_player_thrown_object ge_original_generate_player_thrown_object_exact
'''
    return preamble + "\n\n" + "\n\n".join(bodies) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(args.repo))
    print(f"generated canonical player thrown-object slice -> {args.output}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generate exact mutable-state helpers used by bondviewProcessInput.

The emitted translation unit contains verbatim function definitions from the
decompiled source files.  Only g_CurrentPlayer is rebound through the typed
input provider; no gameplay behavior is authored here.
"""

from __future__ import annotations

import argparse
from pathlib import Path


FUNCTIONS = {
    "src/game/gun.c": (
        "bgunCalculateBlend",
        "get_ptr_item_statistics",
        "Gun_hand_without_item",
        "getCurrentPlayerWeaponId",
        "get_item_in_hand_or_watch_menu",
        "get_item_in_hand_zoom",
        "camera_sniper_zoom_out",
        "camera_sniper_zoom_in",
        "get_hands_firing_status",
        "sub_GAME_7F05DCB8",
        "bondwalkItemHasAmmo",
        "bondwalkItemCheckBitflags",
        "get_next_weapon_in_cycle_for_hand",
        "gunRequestHandWeaponChange",
        "advance_through_inventory",
        "backstep_through_inventory",
        "autoadvance_on_deplete_all_ammo",
        "get_ptr_item_text_call_line",
        "get_ptr_weapon_model_header_line",
        "getPlayerWeaponBufferForHand",
        "getSizeBufferWeaponInHand",
        "used_to_load_1st_person_model_on_demand",
        "place_item_in_hand_swap_and_make_visible",
        "sub_GAME_7F05D690",
        "gunSample1PTransform",
        "currentPlayerEquipWeaponWrapper",
        "sub_GAME_7F05DA8C",
        "currentPlayerUnEquipWeaponWrapper",
        "gunSetHorizontalOffset",
        "bondwalkItemGetAutomaticFiringRate",
        "bondwalkItemGetSoundTriggerRate",
        "bondwalkItemGetSound",
        "sub_GAME_7F05E808",
        "sub_GAME_7F05FB00",
        "currentPlayerCreateRocket",
    ),
    "src/game/gunfire.c": (
        "gunTickGameplay",
        "gunTickHandState",
        "analyzeGEKey",
        "give_weapon_case_items",
        "sub_GAME_7F0649AC",
        "sub_GAME_7F0649D8",
        "get_ammo_in_hands_weapon",
        "gunTickNoise",
        "gunCanUseWeapon",
        "getCurrentPlayerNoise",
        "get_ammo_in_hands_magazine",
        "get_ammo_type_for_weapon",
        "get_ammo_count_for_weapon",
        "give_cur_player_ammo",
        "add_ammo_to_weapon",
        "gunSetAimType",
        "gunSetSightVisible",
        "sub_GAME_7F067AB4",
        "caclulate_gun_crosshair_position_rotation",
        "sub_GAME_7F067F58",
        "sub_GAME_7F067FBC",
        "check_cur_player_ammo_amount_in_inventory",
    ),
    "src/game/bondview.c": (
        "transformAndNormalizeByLength2Dto3D",
        "getPlayer_c_screenwidth",
        "getPlayer_c_screenheight",
        "getPlayer_c_screenleft",
        "getPlayer_c_screentop",
        "currentPlayerSetSwayTarget",
        "currentPlayerAdjustCrouchPos",
    ),
    "src/game/bondview2.c": (
        "bondviewInitPauseTransition",
        "trigger_solo_watch_menu",
        "bondviewTankModelRotationRelated",
        "bondviewGetIfCurrentPlayerDamageShowTime",
        "bondviewSetVisibleToGuardsFlag",
        "bondviewGetVisibleToGuardsFlag",
        "bondviewYPositionRelated",
        "bondviewGetPlayerDuckingHeightRelated",
        "bondviewGetCollisionRadius",
        "getCurrentPlayerProp",
        "currentPlayerGetHealth",
        "currentPlayerGetArmor",
        "currentPlayerSetFadeColour",
        "currentPlayerAdjustFade",
    ),
    "src/game/player.c": (
        "getPropForHeldItem",
        "getPlayerPointerIndex",
        "sub_GAME_7F09B368",
        "sub_GAME_7F09B398",
    ),
    "src/game/propobj.c": (
        "trigger_remote_mine_detonation",
        "set_color_shading_from_tile",
        "update_color_shading",
    ),
    "src/game/chraction.c": (
        "chrlvStanPointPointIntersection",
        "chrGetDistanceToBond",
        "chrlvAlertGuardToPlayerPosition",
    ),
    "src/game/chrprop.c": (
        "propFindForInteract",
    ),
    "src/game/chr.c": (
        "chrCheckGuardsHeardSound",
    ),
    "src/game/front.c": (
        "get_scenario",
    ),
    "src/game/image.c": (
        "texInitPool",
    ),
    "src/game/bondinv.c": (
        "bondinvReinitInv",
        "bondinvGetDualWeapon",
        "bondinvHasDualWeapon",
        "bondinvHasGEKey",
        "bondinvSortInv",
        "bondinvInsertItem",
        "bondinvRemoveItem",
        "bondinvGetNextAvailItem",
        "bondinvGetInvItem",
        "bondinvHasInvItem",
        "bondinvItemAvailable",
        "bondinvAddInvItem",
        "bondinvAddDoublesInvItem",
        "bondinvRemoveItemByID",
        "bondinvIncrementHeldTime",
        "bondinvIsAliveWithFlag",
        "bondinvCycleForward",
        "bondinvCycleBackward",
        "bondinvItemAvailableForHand",
        "bondinvCountTotalItemsInInv",
        "bondinvGetItemByIndex",
        "bondinvGetTextbyObj",
        "bondinvGetTextbyInvIndex",
        "bondinvDetermineEquippedItem",
    ),
    "src/game/matrixmath_misc.c": (
        "coord3dCubicSplineInterp",
    ),
    "src/game/mpmenu.c": (
        "checkGamePaused",
    ),
    "src/game/stan.c": (
        "copy_tile_RGB_as_24bit",
    ),
    "src/game/glass2.c": (
        "buildGaugeBarDL",
        "setup_watch_rectangles",
        "sub_GAME_7F0A3B40",
    ),
}

BONDVIEW_GLOBALS = (
    "f32 g_TankTurnSpeed = 0;",
    "f32 g_TankOrientationAngle = 0;",
    "f32 g_TankTurretVerticalAngle = 0;",
    "f32 g_TankTurretVerticalAngleRelated = 0;",
    "f32 g_TankTurretOrientationAngleRad = 0;",
    "f32 g_TankTurretOrientationAngleDeg = 0;",
    "f32 tank_turret_turn_speed = 0;",
    "f32 g_TankTurretAngle = 0;",
    "f32 g_TankTurretTurn = 0;",
    "s32 g_TankDamagePenaltyTicks = 0;",
)

BONDVIEW2_HOST_GLOBALS = (
    "struct coord3d g_TankModelPositionOffset;",
    "s32 g_EnterTankAudioState;",
    "f32 g_TankEnteringSitHeight;",
    "f32 g_TankEnteringSitHeightRemain;",
    "f32 g_TankEnterBondHorizAngleDeg;",
    "f32 g_TankEnterBondVertAngleDeg;",
    "struct coord3d g_EnterTankCoord;",
)

SQR_MACRO = "#define SQR(x)    ((x) * (x))                     /* square of x */"


def find_function(source: str, name: str) -> str:
    needle = f"{name}("
    search_from = 0
    while True:
        pos = source.find(needle, search_from)
        if pos < 0:
            raise ValueError(f"function not found: {name}")
        line_start = source.rfind("\n", 0, pos) + 1
        prefix = source[line_start:pos]
        paren = pos + len(name)
        depth = 0
        end_paren = paren
        while end_paren < len(source):
            if source[end_paren] == "(":
                depth += 1
            elif source[end_paren] == ")":
                depth -= 1
                if depth == 0:
                    break
            end_paren += 1
        after = end_paren + 1
        while after < len(source):
            if source[after].isspace():
                after += 1
            elif source.startswith("//", after):
                newline = source.find("\n", after + 2)
                after = len(source) if newline < 0 else newline + 1
            elif source.startswith("/*", after):
                close = source.find("*/", after + 2)
                after = len(source) if close < 0 else close + 2
            else:
                break
        if (prefix.strip() and ";" not in prefix and "=" not in prefix
                and after < len(source) and source[after] == "{"):
            brace = after
            break
        search_from = pos + len(needle)

    depth = 0
    index = brace
    state = "code"
    while index < len(source):
        char = source[index]
        nxt = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if char == "/" and nxt == "*":
                state = "block"
                index += 2
                continue
            if char == "/" and nxt == "/":
                state = "line"
                index += 2
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
                    return source[line_start:index + 1]
        elif state == "block":
            if char == "*" and nxt == "/":
                state = "code"
                index += 2
                continue
        elif state == "line":
            if char == "\n":
                state = "code"
        elif state in ("string", "char"):
            if char == "\\":
                index += 2
                continue
            if ((state == "string" and char == '"')
                    or (state == "char" and char == "'")):
                state = "code"
        index += 1
    raise ValueError(f"unterminated function: {name}")


def find_initializer(source: str, declaration: str) -> str:
    start = source.find(declaration)
    if start < 0:
        raise ValueError(f"initializer not found: {declaration}")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                semicolon = source.find(";", index)
                return source[start:semicolon + 1]
    raise ValueError(f"unterminated initializer: {declaration}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    if SQR_MACRO not in (args.repo / "include/math.h").read_text():
        raise ValueError("canonical SQR macro changed")

    sections = [
        "/* Generated from canonical decompiled sources; do not edit. */",
        "#include <bondconstants.h>",
        "typedef int PLAYERFLAG;",
        '#include "game/player.h"',
        '#include "game/stan.h"',
        '#include "game/gun.h"',
        '#include "game/bondinv.h"',
        '#include "game/chr.h"',
        '#include "game/chraction.h"',
        '#include "game/file.h"',
        '#include "game/glass.h"',
        '#include "game/mp_music.h"',
        '#include "game/propobj.h"',
        '#include "music.h"',
        '#include "snd.h"',
        '#include "os_extension.h"',
        '#include "game/matrixmath.h"',
        '#include "game/loadobjectmodel.h"',
        '#include "game/lv.h"',
        '#include "game/model.h"',
        '#include "game/mpmenu.h"',
        '#include "joy.h"',
        '#include "random.h"',
        '#include "game/quaternion.h"',
        '#include "game/textrelated.h"',
        '#include "game/language.h"',
        '#include "assets/obseg/text/LgunE.h"',
        '#include "ge_original_bond_input_internal.h"',
        "#ifndef SQR",
        SQR_MACRO,
        "#endif",
        "#ifndef M_PI_2F",
        "#define M_PI_2F     1.5707964f",
        "#endif",
        "#ifndef M_LN2F",
        "#define M_LN2F      0.69813174f",
        "#endif",
        "#define GAUGE_BAR_VERTEX_PAIR_STRIDE (2 * sizeof(struct WatchVertex))",
        "#if defined(GE_PORT_BOND_INPUT_HOST_STATE)",
        "f32 watch_transition_time = 0.90909088f;",
        "#endif",
        "extern GunModelFileRecord gitem_structs[];",
        "extern WeaponStats default_weaponstats;",
        "extern AmmoStats ammo_related[30];",
        "extern WeaponStats sniperrifle_stats;",
        "extern WeaponStats camera_stats;",
        "s32 get_ammo_type_for_weapon(ITEM_IDS weapon);",
        "void bondviewInitPauseTransition(void);",
        "void bondviewTriggerWatchZoomDefault(void);",
        "s32 gunCanUseWeapon(enum GUNHAND hand);",
        "void gunTickHandState(enum GUNHAND hand, s32 trigger_on);",
        "void bgunCalculateBlend(enum GUNHAND hand);",
        "void used_to_load_1st_person_model_on_demand(GUNHAND hand);",
        "void gunTickNoise(void);",
        "void bondinvIncrementHeldTime(s32 weapon1, s32 weapon2);",
        "s32 get_ammo_in_hands_weapon(enum GUNHAND hand);",
        "s32 gunSample1PTransform(Weapon1PTransformKeyframe *keyframes, f32 time, Mtxf *matrix, GUNHAND hand);",
        "void analyzeGEKey(void);",
        "void give_weapon_case_items(void);",
        "f32 gunSetHorizontalOffset(GUNHAND hand);",
        "void sub_GAME_7F05DA8C(GUNHAND hand, ITEM_IDS weaponnum_watchmenu);",
        "void sub_GAME_7F05E808(GUNHAND hand);",
        "void sub_GAME_7F0649D8(enum GUNHAND hand);",
        "void place_item_in_hand_swap_and_make_visible(GUNHAND hand, ITEM_IDS item);",
        "void bondinvDetermineEquippedItem(void);",
        "bool bondinvHasGEKey(void);",
        "bool bondinvHasDualWeapon(ITEM_IDS right, ITEM_IDS left);",
        "void sub_GAME_7F05D690(void);",
        "void add_ammo_to_inventory(AMMOTYPE ammotype, int amount, int doplaysound, int dodisplaytext);",
        "void set_sound_effect_for_weapontype_collection(ITEM_IDS weapontype);",
        "void display_text_for_weapon_in_lower_left_corner(ITEM_IDS weaponid);",
        "s32 check_cur_player_ammo_amount_in_inventory(AMMOTYPE ammotype);",
        "void currentPlayerCreateRocket(GUNHAND hand);",
        "void load_object_fill_header(ModelFileHeader *objheader, u8 *name, u8 *dst, s32 size, struct texpool *buffer);",
        "PROP getPropForHeldItem(ITEM_IDS item);",
        "bool objTestForInteract(PropRecord *prop);",
        "bool doorTestForInteract(PropRecord *prop);",
        "",
    ]
    bondview_source = (args.repo / "src/game/bondview.c").read_text()
    global_line = "s32 g_VisibleToGuardsFlag = TRUE;"
    if global_line not in bondview_source:
        raise ValueError("canonical visible-to-guards global not found")
    sections.extend((global_line, ""))
    for line in BONDVIEW_GLOBALS:
        if line not in bondview_source:
            raise ValueError(f"canonical tank global not found: {line}")
        sections.append(line)
    sections.append("")
    bondview2_source = (args.repo / "src/game/bondview2.c").read_text()
    for line in BONDVIEW2_HOST_GLOBALS:
        if line not in bondview2_source:
            raise ValueError(f"canonical input host global not found: {line}")
        sections.append(line)
    sections.append("")

    front_source = (args.repo / "src/game/front.c").read_text()
    scenario_line = "s32 scenario = SCENARIO_NORMAL;"
    if scenario_line not in front_source:
        raise ValueError("canonical scenario global not found")
    sections.extend((scenario_line, ""))

    mpmenu_source = (args.repo / "src/game/mpmenu.c").read_text()
    paused_line = "s32 g_pausedFlag;"
    if paused_line not in mpmenu_source:
        raise ValueError("canonical pause global not found")
    sections.extend((paused_line, ""))

    gunfire_source = (args.repo / "src/game/gunfire.c").read_text()
    for line in (
        "#define DUAL_WIELD_TRIGGER_SWAP_TICKS 20",
        "#define DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS 30",
        "#define WATCH_SOUND_DURATION_TICKS 300",
    ):
        if line not in gunfire_source:
            raise ValueError(f"canonical NTSC gun timing changed: {line}")
        sections.append(line)
    sections.extend(("extern struct gun_trigger_state g_ZeroTriggerState;", ""))

    for line in (
        "#define WEAPON_1P_ANIM_TIME(x) ((f32)(x))",
        "#define WHEN_1_CASE_GRENADELAUNCH_FLD890 6",
        "#define WHEN_1_CASE_GRENADE_FLD890 0xf0",
        "#define WHEN_D_FLD890 0x14",
        "#define WHEN_5_SP188_INIT 0x10",
        "#define WHEN_5_SP188_MULTI 0xc",
        "#define WHEN_5_FLD8B0_SP 0x11",
        "#define WHEN_5_FLD8B0_MULTI 0xd",
        "#define WHEN_8_SP178_INIT 0x17",
        "#define WHEN_8_SP178_MULTI 0xc",
        "#define WHEN_A_FLD890 0x10",
        "#define WHEN_A_FLD8B0 0x11",
        "#define WHEN_C_FLD890 0x17",
        "#define WHEN_E_FLD890 0x10",
        "#define WHEN_10_FLD890 0x17",
        "#define WHEN_11_FLD890_1 0x10",
        "#define WHEN_11_FLD890_2 0x18",
        "#define WHEN_1E_FLD890 0x1e",
    ):
        if line not in gunfire_source:
            raise ValueError(f"canonical US gun hand timing changed: {line}")
        sections.append(line)
    sections.append("")

    gun_source = (args.repo / "src/game/gun.c").read_text()
    for line in (
        "u32 D_80032458 = 0;",
        "u32 size_item_buffer[] = {0x14820, 0x14820};",
        "u32 D_80032464[] ={0x7530, 0x7530};",
    ):
        if line not in gun_source:
            raise ValueError(f"canonical gun model state changed: {line}")
        sections.append(line)
    sections.extend((
        "extern Weapon1PTransformKeyframe throwKnifeDrawBackKeyframes[];",
        "extern Weapon1PTransformKeyframe throwKnifeReleaseKeyframes[];",
        "extern Weapon1PTransformKeyframe grenadeThrowKeyframes[];",
        "extern Weapon1PTransformKeyframe timedMineThrowKeyframes[];",
        "extern Weapon1PTransformKeyframe proxMineThrowKeyframes[];",
        "extern Weapon1PTransformKeyframe remoteMineThrowKeyframes[];",
        "extern Weapon1PTransformKeyframe fistMeleeKeyframes1[];",
        "extern Weapon1PTransformKeyframe fistMeleeKeyframes2[];",
        "extern Weapon1PTransformKeyframe sniperMeleeKeyframes1[];",
        "extern Weapon1PTransformKeyframe sniperMeleeKeyframes2[];",
        "extern Weapon1PTransformKeyframe taserFireKeyFrames[];",
        "extern Weapon1PTransformKeyframe taserRaiseKeyframes[];",
        "extern struct sfx2 watchlaser_fire_sounds;",
        "extern struct sfx3 knife_throw_sounds;",
        "extern u32 D_80034CA4[];",
        "extern u32 D_80034E0C[];",
        "",
    ))

    init_source = (args.repo / "src/game/initBondDATA.c").read_text()
    sections.extend((
        "#define VALUENAME 19.999996",
        find_initializer(init_source, "struct hand hand_data_dummy ="),
        "#undef VALUENAME",
        "",
    ))
    hand_init = find_function(init_source, "init_player_BONDdata_stats")
    hand_init = hand_init.replace(
        "void init_player_BONDdata_stats(void)",
        "void ge_original_bond_input_initialize_player_hands("
        "void *right_buffer, void *left_buffer)",
        1,
    )
    allocation = """    g_CurrentPlayer->ptr_hand_weapon_buffer[GUNRIGHT] = mempAllocBytesInBank(size_item_buffer[0], MEMPOOL_STAGE);

    if (getPlayerCount() == 1)
    {
        g_CurrentPlayer->ptr_hand_weapon_buffer[GUNLEFT] = mempAllocBytesInBank(size_item_buffer[1], MEMPOOL_STAGE);
    }"""
    replacement = """    g_CurrentPlayer->ptr_hand_weapon_buffer[GUNRIGHT] = right_buffer;
    g_CurrentPlayer->ptr_hand_weapon_buffer[GUNLEFT] = left_buffer;"""
    if allocation not in hand_init:
        raise ValueError("canonical hand buffer allocation changed")
    sections.extend((hand_init.replace(allocation, replacement, 1), ""))

    propobj_source = (args.repo / "src/game/propobj.c").read_text()
    remote_mine_line = "s32 g_RemoteMineOwnerTriggerFlag = 0;"
    if remote_mine_line not in propobj_source:
        raise ValueError("canonical remote-mine trigger global not found")
    sections.extend((remote_mine_line, ""))

    chr_source = (args.repo / "src/game/chr.c").read_text()
    for line in ("ChrRecord *g_ChrSlots = 0;", "s32 g_NumChrSlots = 0;"):
        if line not in chr_source:
            raise ValueError(f"canonical guard-slot global not found: {line}")
        sections.append(line)
    sections.append("")

    count = 0
    for relative, names in FUNCTIONS.items():
        source = (args.repo / relative).read_text()
        sections.append(f"/* Exact bodies from {relative}. */")
        for name in names:
            sections.extend((find_function(source, name), ""))
            count += 1

    glass_source = (args.repo / "src/game/glass2.c").read_text()
    release_watch = glass_source.index("#if !defined(LEFTOVERDEBUG)")
    sections.extend((find_function(glass_source[release_watch:],
                                   "hudMakeDamageSegments"), ""))
    count += 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(sections))
    print(f"generated {count} exact Bond input state helpers -> {args.output}")


if __name__ == "__main__":
    main()

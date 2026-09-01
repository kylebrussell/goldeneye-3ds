#!/usr/bin/env python3
"""Prove the retained normal-input state helpers match decompiled sources."""

from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "scripts/extract_bond_input_live_state_slice.py"

SOURCES = {
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


def function_body(text: str, name: str) -> str:
    match = re.search(
        r"^\s*[A-Za-z_][A-Za-z0-9_\s*]*\b" + re.escape(name)
        + r"\s*\([^;{}]*?\)(?:\s|//[^\n]*\n|/\*.*?\*/)*\{",
        text,
        flags=re.DOTALL | re.MULTILINE,
    )
    assert match, name
    start = text.index("{", match.start())
    depth = 0
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"unterminated function {name}")


def canonical_tokens(body: str) -> list[str]:
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//.*", "", body)
    return re.findall(r"[A-Za-z_]\w*|0[xX][0-9a-fA-F]+|(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuUlL]*|==|!=|<=|>=|&&|\|\||<<|>>|->|\+\+|--|[{}()\[\];,.~!+*/%<>=&|^-]", body)


def main() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        output = Path(temp_dir) / "ge_original_bond_input_live_state_slice.c"
        subprocess.run(
            ["python3", str(GENERATOR), str(ROOT), str(output)],
            check=True,
            capture_output=True,
            text=True,
        )
        generated_text = output.read_text()
        assert "s32 g_VisibleToGuardsFlag = TRUE;" in generated_text
        assert "s32 scenario = SCENARIO_NORMAL;" in generated_text
        assert "f32 g_TankTurnSpeed = 0;" in generated_text
        assert "s32 g_RemoteMineOwnerTriggerFlag = 0;" in generated_text
        assert "ChrRecord *g_ChrSlots = 0;" in generated_text
        assert "s32 g_NumChrSlots = 0;" in generated_text
        assert "struct coord3d g_EnterTankCoord;" in generated_text
        checked = 0
        for relative, names in SOURCES.items():
            source_text = (ROOT / relative).read_text()
            for name in names:
                expected = canonical_tokens(function_body(source_text, name))
                actual = canonical_tokens(function_body(generated_text, name))
                assert actual == expected, f"{name} differs from {relative}"
                checked += 1
        glass_source = (ROOT / "src/game/glass2.c").read_text()
        release_watch = glass_source.index("#if !defined(LEFTOVERDEBUG)")
        expected_hud = canonical_tokens(function_body(
            glass_source[release_watch:], "hudMakeDamageSegments"))
        actual_hud = canonical_tokens(function_body(
            generated_text, "hudMakeDamageSegments"))
        assert actual_hud == expected_hud
        checked += 1
        # Watch open is retained in the generated Bond input slice and saves
        # mission_state through sub_GAME_7F0C1310.  Its platform service must
        # retain the exact paired mp_music.c close body rather than silently
        # leaving the mission in watch/pause music state 3.
        expected_music_restore = canonical_tokens(function_body(
            (ROOT / "src/game/mp_music.c").read_text(),
            "sub_GAME_7F0C1340"))
        actual_music_restore = canonical_tokens(function_body(
            (ROOT / "port/src/ge_original_gameplay_services.c").read_text(),
            "sub_GAME_7F0C1340"))
        assert actual_music_restore == expected_music_restore
        checked += 1
        expected_save_settings = canonical_tokens(function_body(
            (ROOT / "src/game/file2.c").read_text(),
            "fileSaveSettingsForFolder"))
        actual_save_settings = canonical_tokens(function_body(
            (ROOT / "port/src/ge_original_gameplay_services.c").read_text(),
            "ge_service_file_save_settings_exact"))
        assert actual_save_settings == expected_save_settings
        checked += 1
        assert checked == 125
    print(f"bond input live-state exactness: {checked} canonical bodies")


if __name__ == "__main__":
    main()

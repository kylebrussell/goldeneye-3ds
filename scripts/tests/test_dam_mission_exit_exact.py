#!/usr/bin/env python3
"""Audit that the live ai_24 tranche is copied from canonical chrai.c."""

from __future__ import annotations

import importlib.util
import re
import tempfile
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
EXTRACTOR = REPO / "scripts/extract_dam_guard_ai_tick_slice.py"


def load_extractor():
    spec = importlib.util.spec_from_file_location("dam_guard_ai_extract", EXTRACTOR)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def balanced_block(source: str, marker: str) -> str:
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 0
    for position in range(brace, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[start:position + 1]
    raise AssertionError(f"unterminated block: {marker}")


def main() -> None:
    extractor = load_extractor()
    chrai = (REPO / "src/game/chrai.c").read_text()
    setup = (REPO / "assets/obseg/setup/UsetupdamZ.c").read_text()
    script = re.search(r"(?ms)^u8 ai_24\[\] = \{(.*?)^\};", setup)
    assert script is not None

    authored = set(re.findall(r"(?m)^\s{4}([a-z][a-z0-9_]*)", script.group(1)))
    required_authored = {
        "if_bond_in_room_with_pad",
        "objective_bitfield_set_on",
        "bond_disable_damage_and_pickups",
        "hud_hide_and_lock_controls_and_pause_mission_time",
        "bond_equip_item",
        "bond_set_locked_velocity",
        "local_timer_reset_start",
        "if_bond_y_pos_less_than",
        "screen_fade_to_black",
        "if_screen_fade_completed",
        "if_objective_all_completed",
        "chr_hide_all",
        "trigger_fade_and_exit_level_on_button_press",
        "bond_hide_weapons",
        "camera_switch",
        "jump_to_ai_list",
    }
    assert required_authored <= authored

    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "exit.c"
        import subprocess
        subprocess.run(
            ["python3", str(EXTRACTOR), str(REPO), str(output)], check=True)
        generated = output.read_text()

    for name in extractor.EXIT_CASES:
        canonical = extractor.case_text(chrai, name)
        assert canonical in generated, f"canonical ai_24 case missing: {name}"

    assert "chr->ailist != ailistFindById(0x1004)" in generated
    assert "ge_original_global_ai_find(0x000f)" in generated
    assert "ge_original_global_ai_find(0x0001)" in generated
    assert "ge_original_dam_mission_set_camera_posend_exact" in generated
    assert "ge_original_dam_mission_return_title_exact" in generated

    services = (
        REPO / "port/src/ge_original_dam_mission_exit_services.c"
    ).read_text()
    viewport = (REPO / "src/game/bondview2.c").read_text()
    viewport_tail = viewport[viewport.index(
        "void bondviewMovePlayerUpdateViewport"):]
    canonical_stop = balanced_block(viewport_tail, "if (stop_time_flag != 0)")
    assert canonical_stop in services
    assert "g_CurrentPlayer->buttons_pressed = buttons;" in services
    return_title = balanced_block(
        services, "void ge_original_dam_mission_return_title_exact")
    assert return_title.index("++ge_exit_snapshot.briefing_frontiers") \
        < return_title.index("bossRunTitleStage()")
    assert "return;" not in return_title

    file_source = (REPO / "src/game/file.c").read_text()
    result_source = (
        REPO / "port/src/ge_original_mission_result.c"
    ).read_text()
    canonical_result = extractor.function_text(
        file_source, "end_of_mission_briefing")
    assert canonical_result in result_source
    front_source = (REPO / "src/game/front.c").read_text()
    target_table = front_source[front_source.index(
        "s16 solo_target_time_array[20][3]"):]
    target_table = target_table[:target_table.index("};") + 2]
    for row in re.findall(r"\{\s*\d+\s*,\s*\d+\s*,\s*\d+\s*\}",
                          target_table):
        assert re.sub(r"\s+", "", row) in re.sub(r"\s+", "", result_source)
    mission_folder = front_source[front_source.index(
        "struct mission_folder_setup mission_folder_setup_entries"):
        front_source.index("struct FolderSelectColour")]
    assert [int(value) for value in re.findall(
        r"MISSION_PART,\s+(\d+),", mission_folder)] == list(range(20))
    constants = (REPO / "src/bondconstants.h").read_text()
    solo_enum = constants[constants.index(
        "typedef enum LEVEL_SOLO_SEQUENCE"):]
    solo_enum = solo_enum[:solo_enum.index("} LEVEL_SOLO_SEQUENCE")]
    missions = re.findall(r"\bSP_LEVEL_(?!MAX\b)[A-Z0-9_]+", solo_enum)
    assert len(missions) == 20
    for mission in missions:
        assert "{ " + mission + " }" in result_source

    platform_main = (REPO / "platform/3ds/source/main.c").read_text()
    stage_start = platform_main[platform_main.index(
        "ge_original_gameplay_services_reset();"):]
    assert stage_start.index("ge_original_dam_mission_exit_services_reset();") \
        < stage_start.index("ge_original_mission_result_bind(NULL);") \
        < stage_start.index("ge_original_mission_result_set_current_mission(")

    # Only a canonical boss stage request may enter the report/retry/next-stage
    # frontend. Closing the application or ending a diagnostic route must not
    # manufacture a mission outcome. The same live loop also avoids walking
    # every guard twice when camera publication has already refreshed the
    # authored room visibility set, while retaining a fresh pass after the
    # unchanged propsTick because guards can cross rooms independently.
    assert "gameplay_stage_ended = true;" in platform_main
    report_gate = platform_main[platform_main.index(
        "if (gameplay_stage_ended"):]
    assert "&& !input_probe.enabled && !visual_probe_tour.enabled" \
        in report_gate[:report_gate.index("cleanup_runtime:")]
    assert report_gate.index("run_original_mission_complete_report(") \
        < report_gate.index("cleanup_runtime:")
    report_body = balanced_block(
        platform_main, "static bool run_original_mission_complete_report(")
    assert report_body.index("ge_original_mission_outcome_evaluate_exact(") \
        < report_body.index("ge_original_frontend_start_stage_ended(") \
        < report_body.index("run_original_frontend(")
    assert "result.bond_kia = (uint8_t)(outcome.status" in report_body
    assert "== GE_ORIGINAL_MISSION_OUTCOME_KIA);" in report_body
    assert "result.mission_failed_or_aborted = (uint8_t)(outcome.status" \
        in report_body
    assert "== GE_ORIGINAL_MISSION_OUTCOME_ABORTED);" in report_body
    reload_gate = platform_main[platform_main.index(
        "if (next_stage_requested)"):]
    assert reload_gate.index("ge_stage_asset_descriptor_by_level_id(") \
        < reload_gate.index("goto start_stage_runtime;")
    visibility_calls = re.findall(
        r"publish_stage_ordinary_visibility\(\s*"
        r"&stage_ordinary_objects,\s*(true|false)\);",
        platform_main,
    )
    assert visibility_calls == ["true", "true", "true", "false"]
    visibility_publish = balanced_block(
        platform_main, "static void publish_stage_ordinary_visibility(")
    assert "if (!guard_visibility_is_current)" in visibility_publish
    assert visibility_publish.count("update_stage_guard_visibility(") == 1

    file2_source = (REPO / "src/game/file2.c").read_text()
    crc_source = (REPO / "src/game/crc.c").read_text()
    save_provider = (
        REPO / "port/src/ge_3ds_save_provider.c"
    ).read_text()
    for helper in (
        "fileGetSaveFolder",
        "fileSetSaveFoldernum",
        "fileGetSaveStageDifficultyTime",
        "fileSetDifficultyStageTime",
        "fileCheckSaveStageDifficultyTime",
        "fileGetIsCheatUnlocked",
        "fileSetSaveCheatUnlocked",
        "fileUnlockStageInFolderAtDifficulty",
        "fileSaveFolderUnlockCheat",
    ):
        assert extractor.function_text(file2_source, helper) in save_provider
    assert extractor.function_text(
        crc_source, "fileGenerateCRC") in save_provider

    hud_extractor = REPO / "scripts/extract_dam_mission_hud_slice.py"
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "hud.c"
        import subprocess
        subprocess.run(
            ["python3", str(hud_extractor), str(REPO), str(output)],
            check=True,
        )
        generated_hud = output.read_text()
    canonical_hud_timer = extractor.function_text(
        viewport, "bondviewUpperTextWindowTimerTick"
    ).replace(
        "bondviewUpperTextWindowTimerTick",
        "ge_original_dam_mission_hud_tick",
        1,
    )
    assert canonical_hud_timer in generated_hud
    canonical_bottom_tick = extractor.function_text(
        viewport, "bondviewIntroCameraTextTick"
    ).replace(
        "bondviewIntroCameraTextTick",
        "ge_original_bottom_hud_tick",
        1,
    )
    assert canonical_bottom_tick in generated_hud
    lower_region = viewport[viewport.index(
        "#else\n#ifdef DEBUG\nvoid hudmsgBottomShow"
    ):]
    # The release signature is separated from its body by the DEBUG
    # preprocessor alternative, so the generic C-function matcher cannot
    # identify it. Balance from the exact release declaration instead.
    canonical_bottom_show = balanced_block(
        lower_region, "void hudmsgBottomShow(char *mess)\n#endif\n"
    )
    canonical_bottom_body = canonical_bottom_show[
        canonical_bottom_show.index("{"):
    ]
    assert canonical_bottom_body in generated_hud
    glass = (REPO / "src/game/glass2.c").read_text()
    canonical_gauge = extractor.function_text(
        glass[glass.index("#if !defined(LEFTOVERDEBUG)"):],
        "hudMakeDamageSegments",
    ).replace(
        "hudMakeDamageSegments",
        "ge_original_hud_make_damage_segments_exact",
        1,
    )
    assert canonical_gauge in generated_hud
    hud_adapter = (REPO / "port/src/ge_3ds_original_hud.c").read_text()
    assert "fontZurichBold_kerning" in hud_adapter
    assert "fontZurichBold_fontchartable" in hud_adapter
    assert "fontZurichBold_fontbytes" in hud_adapter
    assert "fontBankGothic_fontbytes" in hud_adapter

    print(
        "Dam ai_24 exactness: 16 authored exit command bodies and canonical "
        "post-MoveBond fade/title, save/CRC and upper-HUD timer bodies retained"
    )


if __name__ == "__main__":
    main()

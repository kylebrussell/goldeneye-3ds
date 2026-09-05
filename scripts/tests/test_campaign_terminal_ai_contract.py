#!/usr/bin/env python3
"""Audit authored solo terminal AI through the shared AI_EndLevel service."""

from __future__ import annotations

import re
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
SETUP = REPO / "assets/obseg/setup"

# Canonical solo mission order followed by the authored Cuba credits stage.
SOLO_SETUPS = (
    ("Dam", "UsetupdamZ.c"),
    ("Facility", "UsetuparkZ.c"),
    ("Runway", "UsetuprunZ.c"),
    ("Surface 1", "UsetupsevxZ.c"),
    ("Bunker 1", "UsetupsevbunkerZ.c"),
    ("Silo", "u/UsetupsiloZ.c"),
    ("Frigate", "u/UsetupdestZ.c"),
    ("Surface 2", "UsetupsevxbZ.c"),
    ("Bunker 2", "UsetupsevbZ.c"),
    ("Statue", "u/UsetupstatueZ.c"),
    ("Archives", "UsetuparchZ.c"),
    ("Streets", "UsetuppeteZ.c"),
    ("Depot", "UsetupdepoZ.c"),
    ("Train", "u/UsetuptraZ.c"),
    ("Jungle", "u/UsetupjunZ.c"),
    ("Control", "UsetupcontrolZ.c"),
    ("Caverns", "UsetupcaveZ.c"),
    ("Cradle", "u/UsetupcradZ.c"),
    ("Aztec", "UsetupaztZ.c"),
    ("Egyptian", "UsetupcrypZ.c"),
    ("Cuba", "u/UsetuplenZ.c"),
)


def uncomment(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def arrays(source: str) -> list[tuple[str, str]]:
    result: list[tuple[str, str]] = []
    for match in re.finditer(r"\bu8\s+(\w+)\s*\[\s*\]\s*=\s*\{", source):
        end = source.find("};", match.end())
        assert end >= 0, f"unterminated AI list {match.group(1)}"
        result.append((match.group(1), source[match.end():end]))
    return result


def raw_bytes(body: str) -> list[int]:
    return [int(value, 16) for value in re.findall(r"\b0x([0-9a-fA-F]{1,2})\b", body)]


def has_sequence(values: list[int], sequence: list[int]) -> bool:
    width = len(sequence)
    return any(values[index:index + width] == sequence
               for index in range(len(values) - width + 1))


def main() -> None:
    direct_stage_count = 0
    global_jump_stage_count = 0
    fade_stage_count = 0

    for stage, relative in SOLO_SETUPS:
        source = uncomment((SETUP / relative).read_text())
        setup_lists = arrays(source)
        terminal_lists: set[str] = set()
        fade_lists: set[str] = set()

        for name, body in setup_lists:
            values = raw_bytes(body)
            raw_only = re.search(r"\b[A-Za-z_]\w*\s*\(", body) is None
            if re.search(r"\bexit_level\b", body):
                terminal_lists.add(name)
            # Archives is emitted as exact command bytes: SetChrAiList self,
            # GAILIST_END_LEVEL. Macro-authored stages use exit_level directly.
            if ("GAILIST_END_LEVEL" in body
                    or (raw_only
                        and has_sequence(values, [0x05, 0xFD, 0x00, 0x0F]))):
                terminal_lists.add(name)
                global_jump_stage_count += 1
            if (re.search(
                    r"\btrigger_fade_and_exit_level_on_button_press\b", body)
                    or (raw_only and 0xEA in values)):
                fade_lists.add(name)

        assert terminal_lists, f"{stage}: no authored terminal AI list"
        registrations = source[source.index("AIListRecord ailists[]"):]
        for name in terminal_lists:
            assert re.search(rf"\{{\s*&{re.escape(name)}\s*,", registrations), (
                f"{stage}: terminal list {name} is not registered")
        if stage != "Cuba":
            assert fade_lists, f"{stage}: no authored fade/input terminal path"
            fade_stage_count += 1
        else:
            assert "credits_roll" in source
        if re.search(r"\bexit_level\b", source):
            direct_stage_count += 1

    # The one setup which jumps to a global list must retain the unchanged
    # global EndLevel -> dead-AI ordering and registration.
    chraidata = (REPO / "src/game/chraidata.c").read_text()
    end_level = re.search(
        r"u8\s+m_EndLevel\[\]\s*=\s*\{(.*?)\};", chraidata, re.DOTALL)
    assert end_level is not None
    body = end_level.group(1)
    assert body.index("EndLevel()") < body.index("JumpTo( GAILIST_DEAD_AI)")
    assert "{m_EndLevel                , GAILIST_END_LEVEL}" in chraidata

    # The 3DS build dispatches the full command graph, and AI_EndLevel delegates
    # only its unchanged service body. Offset advancement stays in chrai.
    makefile = (REPO / "platform/3ds/Makefile").read_text()
    assert "chrai.o: CFLAGS +=" in makefile
    assert "-DGE_PORT_FULL_CAMPAIGN_INTERPRETER" in makefile
    chrai = (REPO / "src/game/chrai.c").read_text()
    command = chrai[chrai.index("case AI_EndLevel: // canonical name"):]
    command = command[:command.index("case AI_CameraReturnToBond:")]
    assert command.index("ge_original_campaign_end_level_dispatch_exact();") \
        < command.index("Offset += sizeof(AiEndLevelRecord);")

    service = (
        REPO / "port/src/ge_original_dam_mission_exit_services.c"
    ).read_text()
    dispatch = service[service.index(
        "void ge_original_campaign_end_level_dispatch_exact(void)"):]
    dispatch = dispatch[:dispatch.index("#define currentPlayerIsFadeComplete")]
    assert dispatch.index("if (cameraBufferToggle)") \
        < dispatch.index("if (cameraFrameCounter2 == FALSE)") \
        < dispatch.index("cameraFrameCounter2 = TRUE;") \
        < dispatch.index("bossReturnTitleStage();")
    posend = service[service.index(
        "int ge_original_campaign_posend_camera_tick_exact("):]
    posend = posend[:posend.index("u32 bondviewGetCameraMode(void)")]
    assert posend.index("if (g_CameraLookAtBondPad != NULL)") \
        < posend.index("if (gBondViewCutscene != NULL)") \
        < posend.index("if (isNotBoundPad(dword_CODE_bss_80079A14))")
    assert "cosf(gBondViewCutscene->verta)" in posend
    assert "sinf(gBondViewCutscene->theta)" in posend

    # The shared objective runtime owns status messages in every solo stage.
    # Keep the live frontend from accidentally retaining a Dam-only gate.
    main_source = (REPO / "platform/3ds/source/main.c").read_text()
    assert re.search(
        r"if\s*\(dam_objects\.objective_runtime\.bound\s*"
        r"\|\|\s*stage_ordinary_objects\.objective_runtime\.bound\)\s*"
        r"display_objective_status_text_on_status_change\(\);",
        main_source,
    ), "Campaign objective HUD must dispatch for every bound stage runtime"

    assert direct_stage_count == 20
    assert global_jump_stage_count == 1
    assert fade_stage_count == 20
    print(
        "Campaign terminal AI: 21 authored stages, 20 direct exits, "
        "Archives global end-list jump, exact shared dispatch retained"
    )


if __name__ == "__main__":
    main()

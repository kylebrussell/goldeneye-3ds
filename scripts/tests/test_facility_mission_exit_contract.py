#!/usr/bin/env python3
"""Pin Facility's authored completion/exit chain to the decompiled sources."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
SETUP = (ROOT / "assets/obseg/setup/UsetuparkZ.c").read_text()
CHRAI = (ROOT / "src/game/chrai.c").read_text()
MAKEFILE = (ROOT / "platform/3ds/Makefile").read_text()


# These are precisely the canonical interpreter bodies reached by Facility's
# ai_47 exit owner and its two ai_37/ai_38 cutscene lists that the current Dam
# compile slice removes.  Keep this list narrow: it is the dependency frontier
# for activating those authored lists, not a request to enable all chrai cases.
FACILITY_EXIT_CASES_EXCLUDED_BY_DAM_SLICE = {
    "AI_SetChrAiList",                  # jump_to_ai_list
    "AI_PlayAnimation",                 # guard_play_animation
    "AI_IFImOnPatrolOrStopped",         # if_guard_has_stopped_moving
    "AI_IFBondInRoomWithPad",
    "AI_IFBondHasItemEquipped",
    "AI_SetMyFlags2",                   # guard_bitfield_set_on
    "AI_UnsetMyFlags2",                 # guard_bitfield_set_off
    "AI_IFMyFlags2Has",                 # if_guard_bitfield_is_set_on
    "AI_MyTimerStart",                  # local_timer_reset_start
    "AI_IFMyTimerGreaterThanTicks",     # if_local_timer_greater_than
    "AI_EndLevel",                      # exit_level
    "AI_CameraSwitch",
    "AI_BondDisableControl",            # HUD/control/timer lock command
    "AI_TRYTeleportingChrToPad",
    "AI_ScreenFadeToBlack",
    "AI_ScreenFadeFromBlack",
    "AI_IFScreenFadeCompleted",
    "AI_HideAllChrs",
    "AI_DoorOpenInstant",
    "AI_ChrRemoveItemInHand",
    "AI_BondEquipItemCinema",
    "AI_TriggerFadeAndExitLevelOnButtonPress",
    "AI_IFBondIsDead",
    "AI_BondDisableDamageAndPickups",
    "AI_BondHideWeapons",
    "AI_IFObjectiveAllCompleted",
}


def cases_disabled_by_dam_slice(source: str) -> set[str]:
    """Return switch cases nested in the slice's plain #ifndef regions."""
    disabled_depths: list[int] = []
    conditional_stack: list[bool] = []
    found: set[str] = set()

    for line in source.splitlines():
        stripped = line.strip()
        if stripped.startswith("#if"):
            is_dam_disable = (
                stripped == "#ifndef GE_PORT_DAM_MISSION_FLOW_SLICE"
            )
            conditional_stack.append(is_dam_disable)
            if is_dam_disable:
                disabled_depths.append(len(conditional_stack))
        elif stripped.startswith("#endif"):
            if conditional_stack:
                if (
                    disabled_depths
                    and disabled_depths[-1] == len(conditional_stack)
                ):
                    disabled_depths.pop()
                conditional_stack.pop()

        if disabled_depths:
            match = re.search(r"case (AI_[A-Za-z0-9_]+):", line)
            if match:
                found.add(match.group(1))

    return found


def block(symbol: str) -> str:
    match = re.search(
        rf"u8 {re.escape(symbol)}\[\] = \{{(?P<body>.*?)\n\}};",
        SETUP,
        re.DOTALL,
    )
    assert match is not None, symbol
    return match.group("body")


def ordered(text: str, needles: list[str]) -> None:
    cursor = 0
    for needle in needles:
        found = text.find(needle, cursor)
        assert found >= 0, needle
        cursor = found + len(needle)


exit_owner = block("ai_47")
ordered(
    exit_owner,
    [
        "if_bond_is_dead(0x46)",
        "if_bond_in_room_with_pad(0x3501, 0x29)",
        "if_bond_in_room_with_pad(0x8700, 0x45)",
        "objective_bitfield_set_on(0x00800000)",
        "bond_disable_damage_and_pickups",
        "hud_hide_and_lock_controls_and_pause_mission_time(0x00)",
        "screen_fade_to_black",
        "if_screen_fade_completed(0x29)",
        "if_objective_all_completed(0x29)",
        "jump_to_ai_list(0xfd, 0x0f00)",
        "chr_hide_all",
        "trigger_fade_and_exit_level_on_button_press",
        "camera_switch(0x25, 0x0200, 0x0000)",
        "camera_switch(0x24, 0x0200, 0x0000)",
    ],
)

for cutscene in ("ai_37", "ai_38"):
    ordered(
        block(cutscene),
        [
            "screen_fade_to_black",
            "if_screen_fade_completed(0x29)",
            "exit_level",
            "jump_to_ai_list(0xfd, 0x0100)",
        ],
    )

assert "{ &ai_37, 0x00000426 }" in SETUP
assert SETUP.count("{ &ai_38, 0x00000427 }") == 2
assert "{ &ai_47, 0x00001006 }" in SETUP
assert re.search(
    r"case AI_EndLevel:.*?bossReturnTitleStage\(\);",
    CHRAI,
    re.DOTALL,
)
assert re.search(
    r"case AI_TriggerFadeAndExitLevelOnButtonPress:.*?"
    r"stop_time_flag = TRUE;",
    CHRAI,
    re.DOTALL,
)

# The Facility expansion opens all 26 canonical bodies while the base Dam
# slice remains narrow.  Keep this source audit alongside the sustained host
# execution test so a future guard edit cannot silently remove a reached case.
assert FACILITY_EXIT_CASES_EXCLUDED_BY_DAM_SLICE.isdisjoint(
    cases_disabled_by_dam_slice(CHRAI)
)
assert "defined(GE_PORT_FACILITY_MISSION_FLOW_SLICE)" in CHRAI
assert "return ge_original_global_ai_find(ID);" in CHRAI

# SetObjectiveBitfield plus the interpreter's label/yield/goto primitives are
# already retained, so they are intentionally absent from the expansion list.
for retained_case in (
    "AI_GotoFirst",
    "AI_Label",
    "AI_Yield",
    "AI_SetObjectiveBitfield",
):
    assert retained_case not in cases_disabled_by_dam_slice(CHRAI)

# The ARM runtime now links the complete unchanged campaign interpreter.  Keep
# the Facility source audit above, but pin the production build to that wider
# contract so this test cannot accidentally force the old Dam/Facility slice
# back into service.
assert re.search(
    r"chrai\.o: CFLAGS \+=.*?"
    r"-DGE_PORT_FULL_CAMPAIGN_INTERPRETER",
    MAKEFILE,
    re.DOTALL,
)
chrai_flags = re.search(r"chrai\.o: CFLAGS \+=(.*?)(?:\n[^ \t]|\Z)", MAKEFILE, re.DOTALL)
assert chrai_flags is not None
assert "GE_PORT_DAM_MISSION_FLOW_SLICE" not in chrai_flags.group(1)
assert "GE_PORT_FACILITY_MISSION_FLOW_SLICE" not in chrai_flags.group(1)

print(
    "Facility mission exit: exact ai_47/ai_37/ai_38 chain pinned; "
    "full campaign chrai build and exact global lookup pinned"
)

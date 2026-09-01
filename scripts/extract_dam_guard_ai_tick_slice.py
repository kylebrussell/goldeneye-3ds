#!/usr/bin/env python3
"""Extract the first retained canonical Dam guard AI/action tick tranche.

The setup bytecode and switch cases are copied verbatim from the decomp.  The
small outer dispatcher is the native-platform boundary: it only admits command
IDs whose unchanged case bodies have been audited into this slice.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


LISTS = {
    12: "ge_original_dam_guard_ai_040d",
    18: "ge_original_dam_guard_ai_0413",
    19: "ge_original_dam_guard_ai_0414",
}

CASES = (
    "GotoNext",
    "GotoFirst",
    "Label",
    "Yield",
    "EndList",
    "Return",
    "SetChrAiList",
    "SetReturnAiList",
    "PlayAnimation",
    "IFChrDyingOrDead",
    "IFImOnPatrolOrStopped",
    "IFISeeBond",
    "IFIHeardBond",
    "IFIHeardBondRecently",
    "IFBondMissedMe",
    "IFISeeSomeoneShot",
    "IFISeeSomeoneDie",
    "IFICouldSeeBond",
    "IFPlayingAnimation",
    "PointAtBond",
    "LookSurprised",
    "SetNewRandom",
    "IFRandomGreaterThan",
    "TRYFireOrAimAtTarget",
    "TRYFireOrAimAtTargetKneel",
    "TRYSidestepping",
    "TRYSideHopping",
    "TRYSideRunning",
    "TRYFiringWalk",
    "TRYFiringRun",
    "TRYFiringRoll",
    "TRYRunToBond",
    "TRYFacingTarget",
    "TRYThrowingGrenade",
    "TRYTriggeringAlarmAtPad",
    "RunToPad",
    "WalkToPad",
    "StartPatrol",
    "IFAlarmIsOn",
    "IFIWasShotRecently",
    "TvChangeScreenBank",
    "DoorOpen",
    "DoorClose",
    "IFDoorStateEqual",
    "SetMyHealthTotal",
    "SetMyArmour",
    "SetMyAlertness",
    "SetChrchrflags",
    "SetMyFlags2",
    "IFMyFlags2Has",
    "IFMyNumCloseArghsGreaterThan",
    "IFMyNumArghsGreaterThan",
    "IFChrDistanceToPadGreaterThanDecimeter",
    "MyTimerStart",
    "IFMyTimerGreaterThanTicks",
)

# Complete Dam's authored ai_24 bungee/mission-exit actor.  These are the
# unchanged chrai.c cases reached after its initial label/yield; retaining the
# whole command set avoids advancing the actor through a port-authored state
# machine.  Existing offset-only host tests omit this service-heavy tranche;
# the dedicated exit-flow test enables it explicitly.
EXIT_CASES = (
    "IFBondInRoomWithPad",
    "SetObjectiveBitfield",
    "BondDisableControl",
    "BondEquipItem",
    "BondSetLockedVelocity",
    "IFBondYPosLessThan",
    "ScreenFadeToBlack",
    "IFScreenFadeCompleted",
    "IFObjectiveAllCompleted",
    "HideAllChrs",
    "TriggerFadeAndExitLevelOnButtonPress",
    "IFBondIsDead",
    "BondDisableDamageAndPickups",
    "BondHideWeapons",
    "CameraSwitch",
    "EndLevel",
)


def function_text(source: str, name: str) -> str:
    match = re.search(rf"(?m)^.*\b{name}\s*\([^;]*\)\s*\{{", source)
    if match is None:
        raise ValueError(f"missing function {name}")
    brace = source.index("{", match.start())
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():pos + 1]
    raise ValueError(f"unterminated function {name}")


def data_text(source: str, number: int) -> str:
    match = re.search(rf"(?ms)^u8 ai_{number}\[\] = \{{.*?^\}};", source)
    if match is None:
        raise ValueError(f"missing Dam ai_{number}")
    return match.group(0)


def case_text(source: str, name: str) -> str:
    marker = f"                case AI_{name}:"
    start = source.find(marker, source.find("void                   ai("))
    if start < 0:
        raise ValueError(f"missing AI case {name}")
    brace = source.find("{", start + len(marker))
    if brace < 0:
        raise ValueError(f"missing AI case block {name}")
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise ValueError(f"unterminated AI case {name}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    setup = (args.repo / "assets/obseg/setup/UsetupdamZ.c").read_text()
    chrai = (args.repo / "src/game/chrai.c").read_text()
    chraction = (args.repo / "src/game/chraction.c").read_text()

    pieces = [
        "/* Generated from canonical decompiled sources; do not hand-edit. */",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#include <bondaicommands.h>",
        '#include "game/chraction.h"',
        '#include "game/chrai.h"',
        '#include "game/chr.h"',
        '#include "game/bondview.h"',
        '#include "game/gun.h"',
        '#include "game/loadobjectmodel.h"',
        '#include "game/lv.h"',
        '#include "game/initanitable.h"',
        '#include "game/model.h"',
        '#include "game/objective_status.h"',
        '#include "game/player.h"',
        '#include "game/propobj.h"',
        '#include "boss.h"',
        '#include "random.h"',
        '#include "ge_original_dam_guard_ai_tick.h"',
        '#include "ge_original_dam_mission_exit_services.h"',
        '#include "ge_original_guard_ai_trace.h"',
        '#include "ge_original_global_ai.h"',
        '#include "ge_original_stage_monitor.h"',
        "#ifndef PLAYERFLAG_LOCKCONTROLS",
        "#define PLAYERFLAG_LOCKCONTROLS (1 << 0)",
        "#define PLAYERFLAG_NOCONTROL (1 << 1)",
        "#define PLAYERFLAG_NOTIMER (1 << 2)",
        "#endif",
        "extern void ai(PropDefHeaderRecord *Entityp, PROP_TYPE EntityType);",
        "extern PadRecord *g_CameraLookAtBondPad;",
        "extern CutsceneRecord *gBondViewCutscene;",
        "extern enum CAMERAMODE dword_CODE_bss_80079A18;",
        "extern s32 dword_CODE_bss_80079A1C;",
        "extern s32 chraiGoToLabel(AIRecord *list, s32 offset, u8 label);",
        "extern PathRecord *pathFindById(s32 ID);",
        "extern void chrlvTickStand(ChrRecord *self);",
        "extern void chrlvTickKneel(ChrRecord *self);",
        "extern void chrlvTickAnim(ChrRecord *self);",
        "extern void chrlvTickDie(ChrRecord *self);",
        "extern void chrlvTickArgh(ChrRecord *self);",
        "extern void chrlvTickPreArgh(ChrRecord *self);",
        "extern void chrlvTickSidestep(ChrRecord *self);",
        "extern void chrlvTickJumpout(ChrRecord *self);",
        "extern void chrlvTickDead(ChrRecord *self);",
        "extern void chrlvTickAttack(ChrRecord *self);",
        "extern void chrlvTickAttackWalk(ChrRecord *self);",
        "extern void chrlvTickAttackRoll(ChrRecord *self);",
        "extern void chrlvTickRunPos(ChrRecord *self);",
        "extern void chrlvTickPatrol(ChrRecord *self);",
        "extern void chrlvTickGoPos(ChrRecord *self);",
        "extern void chrlvTickSurrender(ChrRecord *self);",
        "extern void chrlvTickTest(ChrRecord *self);",
        "extern void chrlvTickSurprised(ChrRecord *self);",
        "extern void chrlvTickStartAlarm(ChrRecord *self);",
        "extern void chrlvTickThrowGrenade(ChrRecord *self);",
        "extern void chrlvTickBondIntro(ChrRecord *self);",
        "extern void chrlvTickBondDieRemoved(ChrRecord *self);",
        "",
    ]
    for number, renamed in LISTS.items():
        pieces.extend((
            f"#define ai_{number} {renamed}",
            data_text(setup, number),
            f"#undef ai_{number}",
            "",
        ))

    pieces.extend((
        "#define monitorSetImageByNum "
        "ge_original_stage_monitor_set_image_exact",
        "#define bondviewSetCameraMode "
        "ge_original_dam_mission_set_camera_posend_exact",
        "#define bossReturnTitleStage "
        "ge_original_dam_mission_return_title_exact",
        "#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
        "static AIRecord *ge_original_guard_ai_find_list_native(s32 id)",
        "{",
        "    if (isGlobalAIListID(id)) return ge_original_global_ai_find(id);",
        "    return ailistFindById(id);",
        "}",
        "#define ailistFindById ge_original_guard_ai_find_list_native",
        "#endif",
        "void ge_original_dam_guard_ai_interpret_exact(",
        "    PropDefHeaderRecord *Entityp, PROP_TYPE EntityType)",
        "{",
        "    ChrRecord *ChrEntityp = EntityType == PROP_TYPE_CHR",
        "        ? (ChrRecord *)Entityp : NULL;",
        "    struct { AIRecord *ailist; s32 aioffset; s32 aireturnlist; }",
        "        *VehichleEntityp = NULL;",
        "    struct { AIRecord *ailist; s32 aioffset; s32 aireturnlist;",
        "        Model *model; } *AircraftEntityp = NULL;",
        "    AIRecord *AiListp;",
        "    s32 Offset;",
        "    if (ChrEntityp == NULL || ChrEntityp->ailist == NULL) return;",
        "    Offset = ChrEntityp->aioffset;",
        "    AiListp = ChrEntityp->ailist;",
        "    for (;;) {",
        "        switch ((AiListp + Offset)->cmd) {",
    ))
    pieces.extend(case_text(chrai, name) for name in CASES)
    pieces.append(
        "#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS) "
        "|| defined(GE_PORT_DAM_MISSION_EXIT_TEST)")
    pieces.extend(case_text(chrai, name) for name in EXIT_CASES)
    pieces.append("#endif")
    pieces.extend((
        "                default:",
        "#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
        "                    ge_original_guard_ai_trace_unknown_opcode(",
        "                        ChrEntityp, AiListp, Offset,",
        "                        (AiListp + Offset)->cmd);",
        "#endif",
        "                    return;",
        "        }",
        "    }",
        "}",
        "#undef bossReturnTitleStage",
        "#undef bondviewSetCameraMode",
        "#undef monitorSetImageByNum",
        "#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
        "#undef ailistFindById",
        "#endif",
        "",
        "void ge_original_dam_guard_ai_dispatch_exact(",
        "    PropDefHeaderRecord *Entityp, PROP_TYPE EntityType)",
        "{",
        "#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
        "    ChrRecord *chr = EntityType == PROP_TYPE_CHR",
        "        ? (ChrRecord *)Entityp : NULL;",
        "    if (chr != NULL && (u8)chr->chrnum == 0xfeU",
        "            && chr->ailist != ailistFindById(0x1004)",
        "            && chr->ailist != ge_original_global_ai_find(0x000f)",
        "            && chr->ailist != ge_original_global_ai_find(0x0001)) {",
        "        ai(Entityp, EntityType);",
        "        return;",
        "    }",
        "#endif",
        "    ge_original_dam_guard_ai_interpret_exact(Entityp, EntityType);",
        "}",
        "",
        "#define ai ge_original_dam_guard_ai_dispatch_exact",
        "#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
        "#define chrlvTickStand ge_original_dam_guard_tick_stand_exact",
        "#define chrlvTickKneel ge_original_dam_guard_tick_kneel_exact",
        "#define chrlvTickAnim ge_original_dam_guard_tick_anim_exact",
        "#define chrlvTickDie ge_original_dam_guard_tick_die_exact",
        "#define chrlvTickArgh ge_original_dam_guard_tick_argh_exact",
        "#define chrlvTickPreArgh ge_original_dam_guard_tick_preargh_exact",
        "#define chrlvTickSidestep ge_original_dam_guard_tick_sidestep_exact",
        "#define chrlvTickJumpout ge_original_dam_guard_tick_jumpout_exact",
        "#define chrlvTickDead ge_original_dam_guard_tick_dead_exact",
        "#define chrlvTickAttack ge_original_dam_guard_tick_attack_exact",
        "#define chrlvTickAttackWalk ge_original_dam_guard_tick_attack_walk_exact",
        "#define chrlvTickAttackRoll ge_original_dam_guard_tick_attack_roll_exact",
        "#define chrlvTickRunPos ge_original_dam_guard_tick_run_pos_exact",
        "#define chrlvTickPatrol ge_original_dam_guard_tick_patrol_exact",
        "#define chrlvTickSurrender ge_original_dam_guard_tick_surrender_exact",
        "#define chrlvTickTest ge_original_dam_guard_tick_test_exact",
        "#define chrlvTickSurprised ge_original_dam_guard_tick_surprised_exact",
        "#define chrlvTickStartAlarm ge_original_dam_guard_tick_start_alarm_exact",
        "#define chrlvTickThrowGrenade ge_original_dam_guard_tick_throw_grenade_exact",
        "#define chrlvTickBondIntro ge_original_dam_guard_tick_bond_intro_exact",
        "#define chrlvTickBondDieRemoved ge_original_dam_guard_tick_bond_die_removed_exact",
        "#endif",
        "#if defined(GE_PORT_DAM_GUARD_AI_DEATH_DISPATCH_TEST)",
        "#define chrlvTickDie ge_original_dam_guard_tick_die_exact",
        "#define chrlvTickDead ge_original_dam_guard_tick_dead_exact",
        "#endif",
        "#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)",
        "#define chrlvTickAttack ge_original_dam_guard_tick_attack_exact",
        "#endif",
        "#define chrlvActionTick ge_original_dam_guard_action_tick_body_exact",
        function_text(chraction, "chrlvActionTick"),
        "#undef chrlvActionTick",
        "void ge_original_dam_guard_action_tick_exact(ChrRecord *self)",
        "{",
        "#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
        "    ge_original_guard_ai_trace_action_tick(self);",
        "#endif",
        "    ge_original_dam_guard_action_tick_body_exact(self);",
        "}",
        "#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
        "#undef chrlvTickBondDieRemoved",
        "#undef chrlvTickBondIntro",
        "#undef chrlvTickThrowGrenade",
        "#undef chrlvTickStartAlarm",
        "#undef chrlvTickSurprised",
        "#undef chrlvTickTest",
        "#undef chrlvTickSurrender",
        "#undef chrlvTickPatrol",
        "#undef chrlvTickRunPos",
        "#undef chrlvTickAttackRoll",
        "#undef chrlvTickAttackWalk",
        "#undef chrlvTickAttack",
        "#undef chrlvTickDead",
        "#undef chrlvTickJumpout",
        "#undef chrlvTickSidestep",
        "#undef chrlvTickPreArgh",
        "#undef chrlvTickArgh",
        "#undef chrlvTickDie",
        "#undef chrlvTickAnim",
        "#undef chrlvTickKneel",
        "#undef chrlvTickStand",
        "#endif",
        "#if defined(GE_PORT_DAM_GUARD_AI_DEATH_DISPATCH_TEST)",
        "#undef chrlvTickDead",
        "#undef chrlvTickDie",
        "#endif",
        "#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)",
        "#undef chrlvTickAttack",
        "#endif",
        "#undef ai",
        "",
    ))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(pieces), encoding="utf-8")


if __name__ == "__main__":
    main()

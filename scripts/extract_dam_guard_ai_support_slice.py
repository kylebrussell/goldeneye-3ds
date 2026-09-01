#!/usr/bin/env python3
"""Extract direct canonical services used by the first Dam guard AI tranche."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


FUNCTIONS = (
    ("file", "get_007_health_mod"),
    ("chr", "chrSetMaxDamage"),
    ("chr", "chrAddHealth"),
    ("chr", "chrFindByLiteralId"),
    ("chraction", "chrResolveId"),
    ("chraction", "chrFindById"),
    ("chraction", "chrGetAngleFromBond"),
    ("chraction", "chrGetDistanceToChr"),
    ("chraction", "chrGetDistanceFromBondToPad"),
    ("chraction", "check_if_room_for_preset_loaded"),
    ("chraction", "chrSetFlags2ById"),
    ("chraction", "chrUnsetFlags2ById"),
    ("chraction", "chrHasFlags2ById"),
    ("chraction", "chrIfInPadRoom"),
    ("chraction", "chrTrySurrender"),
    ("chraction", "chrlvActorThrowWeaponSurrender"),
    ("chraction", "chrFadeOut"),
    ("chraction", "chraiStopAnimation"),
    ("chraction", "chrTrySurprisedSurrender"),
    ("chraction", "chrlvSurrenderAnimationRelated"),
    ("chraction", "check_if_able_to_then_kneel"),
    ("chraction", "chrKneelChooseAnimation"),
    ("chraction", "chrCanHearAlarm"),
    ("chraction", "chrIsTargetNearlyInSight"),
    ("chraction", "chrSetChrPreset"),
    ("chraction", "chrSetChrPreset2"),
    ("chraction", "chrSetPadPreset"),
    ("chraction", "chrSetPadPresetByChrnum"),
    ("chraction", "chrGoToChr"),
    ("chraction", "chrDropItem"),
    ("chraction", "chrSpawnAtPad"),
    ("chraction", "chrSpawnAtChr"),
    ("chraction", "check_2328_preset_set_with_method"),
    ("chraction", "sub_GAME_7F0333F8"),
    ("chraction", "sub_GAME_7F033AAC"),
    ("chraction", "sub_GAME_7F033B38"),
    ("chraction", "chrlvCurrentPlayerCall7F0B0E24"),
    ("chraction", "sub_GAME_7F033780"),
    ("chraction", "chrlvFindPathNeighborRelated"),
    ("bondview2", "bondviewGetPlayerYawRadians"),
    ("chraction", "removed_animation_routine_27"),
    ("chraction", "removed_animation_routine_2B"),
    ("chraction", "chrRestartTimer"),
    ("chraction", "chrGetTimer"),
    ("chraction", "chrIfNearMiss"),
    ("chraction", "chrSawInjury"),
    ("chraction", "chrSawDeath"),
    ("chraction", "chrIsNotDeadOrShot"),
    ("chraction", "chrIsDead"),
    ("chraction", "weaponIsOneHanded"),
    ("chraction", "chrlvIdleAnimationRelated"),
    ("chraction", "chrlvMergeKneelToStand"),
    ("chraction", "chrlvGetGuard007SpeedRating"),
    ("chraction", "chrlvKneelingAnimationRelated"),
    ("chraction", "chrlvPerformAnimationForActor"),
    ("chraction", "check_if_able_to_then_perform_animation"),
    ("chraction", "chrHasStoppedOrPatroling"),
    ("chraction", "chrHasFlags2"),
    ("chraction", "chrIsHearingBond"),
    ("chraction", "chrHeardTargetRecently"),
    ("chraction", "chrSetFlags2"),
    ("chraction", "chrUnsetFlags2"),
    ("chraction", "chrIsPosOffScreen"),
    ("chraction", "chrAdjustPosForSpawn"),
    ("chraction", "check_set_actor_standing_still"),
    ("model", "modelSetAnimTranslationScale"),
)

# These canonical bodies are owned by other retained production slices.  Keep
# them in the focused host harness so it remains self-contained, but never emit
# a second ARM definition.
HOST_ONLY_FUNCTIONS = (
    ("file", "get_007_reaction_speed"),
    ("chraction", "chrlvResetAimend"),
    ("chraction", "chrSetFiring"),
    ("chraction", "chrStopFiring"),
)

# This is the unchanged navigation entry point reached by AI_GotoPad*.  The
# existing interpreter harness instruments it so the authored bytecode can be
# tested independently; a second sanitizer harness enables these bodies and
# verifies their exact branch/service behavior.
PRODUCTION_NAVIGATION_FUNCTIONS = (
    ("chraction", "chrResolvePadId"),
    ("chraction", "chrGoToPad"),
)

PRODUCTION_ROUTE_FUNCTIONS = (
    ("chraction", "get_ptrpreset_in_table_matching_tile"),
    ("chraction", "check_if_any_path_preset_lies_on_tile"),
    ("chraction", "chrlvPadPresetRelated"),
    ("chraction", "chrlvStanPathRelated"),
    ("chraction", "chrlvStanRoomRelated"),
    ("chraction", "chrlvSetGoposSegDistTotal"),
    ("chraction", "chrlvActGoposRelated"),
    ("chraction", "sub_GAME_7F0281F4"),
    ("chraction", "chrlvActGoposSetTargetPosRelated"),
    ("chraction", "get_sound_at_range"),
    ("chraction", "play_hit_soundeffect_and_proper_volume"),
    ("chraction", "plot_course_for_actor"),
    ("padhall", "waygroupFindByDist"),
    ("padhall", "waygroupsSetUnvisitedDist"),
    ("padhall", "waygroupsPropagateDist"),
    ("padhall", "waygroupsFloodDist"),
    ("padhall", "waygroupsMarkRoute"),
    ("padhall", "findPadWithDistAndSet"),
    ("padhall", "waypointsSetUnvisitedDist"),
    ("padhall", "waypointsPropagateDist"),
    ("padhall", "do_BFS_withinPathSet"),
    ("padhall", "waypointMarkRoute"),
    ("padhall", "waypointFindRouteInGroup"),
    ("padhall", "sub_GAME_7F08F438"),
    ("padhall", "waypointFindRoute"),
    ("padhall", "resetWaypointDistances"),
    ("padhall", "waypointFindRandomByDist"),
    ("padhall", "waygroupFindRandomByDist"),
    ("padhall", "waypointFindNextStepToward"),
    ("bg", "getROOMID_isRendered"),
    ("model", "setsubroty"),
    ("stan", "sub_GAME_7F0B0D0C"),
    ("stan", "stanFillSearch"),
)

PRODUCTION_ACTION_FUNCTIONS = (
    ("chraction", "chrStartAlarmChooseAnimation"),
    ("chraction", "chrlvActorLookFlustered"),
    ("chraction", "set_actor_on_path"),
    ("chraction", "if_actor_able_set_on_path"),
    ("chraction", "chrlvModelScaleAnimationRelated"),
    ("chraction", "chrlvMovementTargetRelated"),
    ("chraction", "chrlvPlotCourseRelated"),
    ("chraction", "chrlvActGoposIncCurIndex"),
    ("chraction", "chrlvGeometryRelated7F02FC34"),
    ("chraction", "chrlvIsArrivingLaterallyAtPos"),
    ("chraction", "sub_GAME_7F030128"),
    ("chraction", "sub_GAME_7F0301FC"),
    ("chraction", "chrlvPatrolCalculateStep"),
    ("chraction", "chrlvGetPatrolStepPad"),
    ("chraction", "chrlvGetNextPatrolStepPad"),
    ("chraction", "chrlvSetNextActPatrolStepPadPos"),
    ("chraction", "chrlvAdvancePatrolStep"),
    ("chraction", "chrlvIsPosClearOfObjectBounds"),
    ("chr", "chrDetectRooms"),
    ("chr", "sub_GAME_7F01F614"),
    ("chr", "sub_GAME_7F01FC10"),
    ("objective", "sub_GAME_7F057D44"),
    ("chraction", "chrlvWalkingAnimationRelated"),
    ("chraction", "chrlvApplySpeed"),
    ("chraction", "sub_GAME_7F03081C"),
    ("chraction", "sub_GAME_7F0304AC"),
    ("chraction", "chrlvSwapIfDiffArg2Determinate"),
    ("chraction", "sub_GAME_7F030D70"),
    ("chraction", "sub_GAME_7F03130C"),
    ("propobj", "chrobjCallsApplySpeed"),
    ("stan", "sub_GAME_7F0B1410"),
    ("propobj", "doorStartOpen"),
    ("propobj", "doorStartClose"),
    ("propobj", "doorSetOpenState"),
    ("propobj", "doorActivate"),
    ("propobj", "posIsInFrontOfDoor"),
    ("propobj", "doorsChooseSwingDirection"),
    ("propobj", "doorActivatePortal"),
    ("propobj", "sub_GAME_7F053A3C"),
    ("propobj", "door7F053B10"),
    ("propobj", "doorPlayOpenSound0"),
    ("propobj", "doorPlayOpenSound1"),
    ("chrprop", "propDoorGetCdTypes"),
    ("chrprop", "propIsOfCdType"),
    ("chraction", "chrlvTravelTickMagic"),
    ("chraction", "chrlvTravelTick"),
    ("chraction", "chrlvTickGoPos"),
    ("chraction", "chrGetNumCloseArghs"),
    ("chraction", "chrGetNumArghs"),
    ("chraction", "chrGetDistanceToPad"),
    ("chraction", "chrTrySurprisedLookAround"),
    ("chraction", "chrTryStartAlarm"),
    ("chrprop", "scan_position_data_table_for_normal_object_at_preset"),
)

# The authored Dam lists 0x040d/0x0413/0x0414 leave their setup interpreter in
# ACT_STAND or ACT_ANIM.  Retain these two unchanged action handlers under
# port-specific names so their linker frontier can be closed without making
# the still-incomplete all-actions chrlvActionTick reachable in production.
PRODUCTION_STAND_ANIM_FUNCTIONS = (
    ("chraction", "chrlvTickStand",
     "ge_original_dam_guard_tick_stand_exact"),
    ("chraction", "chrlvTickAnim",
     "ge_original_dam_guard_tick_anim_exact"),
)

PRODUCTION_REMAINING_ACTION_HANDLERS = (
    ("chraction", "chrlvTickKneel",
     "ge_original_dam_guard_tick_kneel_exact"),
    ("chraction", "chrlvTickDie",
     "ge_original_dam_guard_tick_die_exact"),
    ("chraction", "chrlvTickArgh",
     "ge_original_dam_guard_tick_argh_exact"),
    ("chraction", "chrlvTickPreArgh",
     "ge_original_dam_guard_tick_preargh_exact"),
    ("chraction", "chrlvTickSidestep",
     "ge_original_dam_guard_tick_sidestep_exact"),
    ("chraction", "chrlvTickJumpout",
     "ge_original_dam_guard_tick_jumpout_exact"),
    ("chraction", "chrlvTickDead",
     "ge_original_dam_guard_tick_dead_exact"),
    ("chraction", "chrlvTickAttack",
     "ge_original_dam_guard_tick_attack_exact"),
    ("chraction", "chrlvTickAttackWalk",
     "ge_original_dam_guard_tick_attack_walk_exact"),
    ("chraction", "chrlvTickAttackRoll",
     "ge_original_dam_guard_tick_attack_roll_exact"),
    ("chraction", "chrlvTickRunPos",
     "ge_original_dam_guard_tick_run_pos_exact"),
    ("chraction", "chrlvTickPatrol",
     "ge_original_dam_guard_tick_patrol_exact"),
    ("chraction", "chrlvTickSurrender",
     "ge_original_dam_guard_tick_surrender_exact"),
    ("chraction", "chrlvTickTest",
     "ge_original_dam_guard_tick_test_exact"),
    ("chraction", "chrlvTickSurprised",
     "ge_original_dam_guard_tick_surprised_exact"),
    ("chraction", "chrlvTickStartAlarm",
     "ge_original_dam_guard_tick_start_alarm_exact"),
    ("chraction", "chrlvTickThrowGrenade",
     "ge_original_dam_guard_tick_throw_grenade_exact"),
    ("chraction", "chrlvTickBondIntro",
     "ge_original_dam_guard_tick_bond_intro_exact"),
    ("chraction", "chrlvTickBondDieRemoved",
     "ge_original_dam_guard_tick_bond_die_removed_exact"),
)

PRODUCTION_ACTION_GRAPH_DEPENDENCIES = (
    ("chraction", "chrlvNormDistanceToPlayer"),
    ("chraction", "sub_GAME_7F02A0EC"),
    ("chraction", "chrlvModelRotyRelated"),
    ("chraction", "sub_GAME_7F02A1E8"),
    ("chraction", "chrlvSideStepAnimationRelated"),
    ("chraction", "chrlvFireJumpToSideAnimationRelated"),
    ("chraction", "sub_GAME_7F024CF8"),
    ("chraction", "chrlvInitActAttackWalk"),
    ("chraction", "chrlvInitActAttackRoll"),
    ("chraction", "actor_steps_sideways"),
    ("chraction", "actor_hops_sideways"),
    ("chraction", "actor_jogs_sideways"),
    ("chraction", "actor_walks_and_fires"),
    ("chraction", "actor_runs_and_fires"),
    ("chraction", "actor_rolls_fires_crouched"),
    ("chraction", "chrGoToBond"),
    ("chraction", "get_distance_actor_to_position"),
    ("chraction", "chrGetAngleToBond"),
    ("chraction", "chrlvSpotBondAnimationRelated"),
    ("chraction", "chrlvActorShuffleFeet"),
    ("chraction", "chrTrySurprisedOneHand"),
    ("chraction", "chrlvKneelingAnimationRelated7F023E48"),
    ("chraction", "chrlvActorFadeAway"),
    ("chraction", "chrlvIterateGuardSeeShotDie"),
    ("chraction", "chrlvIdleAnimationRelated7F023E14"),
    ("chraction", "chrlvTickAttackCommon"),
    ("chraction", "sub_GAME_7F025560"),
    ("chraction", "actor_aim_at_actor"),
    ("chraction", "actor_kneel_aim_at_actor"),
    ("chraction", "actor_fire_or_aim_at_target_update"),
    ("chraction", "chrlvUpdateAimendsideback"),
    ("chraction", "sub_GAME_7F0256F0"),
    ("chraction", "chrlvToggleHiddenRelated"),
    ("chraction", "chrlvStanRoomRelatedPad"),
    ("chraction", "chrlvCall7F02982C"),
    ("propobj", "alarmActivate"),
    ("chr", "get_numguards"),
    ("chraction", "chrlvMaybeSameRoom"),
    ("chraction", "chrlvInitActAttack"),
    ("chr", "chrGetEquippedWeaponPropWithCheck"),
    ("chraction", "chrlvUpdateAimendbackShoulders"),
    ("chraction", "chrlvGetSubrotySideback"),
    ("bondview", "currentPlayerGetMatrix10EC"),
    ("chraction", "chrlvGetAimLimitAngle"),
    ("chraction", "chrlvAttackrollAnimationRelated7F02E2E0"),
    ("chraction", "chrlvAttackrollAnimationRelated7F02E3B8"),
    ("chraction", "chrlvCall7F0B0E24WithChrWidthHeight"),
    ("chraction", "check_if_position_in_same_room"),
    ("chraction", "chrlvAttackActionRelated"),
)

# Exact proactive perception frontier reached by the authored 0x040d list.
# Keeping it independently selectable lets the sanitizer exercise only the
# original facing/range/fog/player/STAN closure.
PRODUCTION_SIGHT_FUNCTIONS = (
    ("chraction", "chrlvGetGuard007SpeedRatingInt"),
    ("chraction", "setSeenBondTimeToNow"),
    ("chraction", "chrCanSeeBond"),
    ("chraction", "chrlvSetTargetToPlayer"),
    ("chraction", "chrSawTargetRecently"),
    ("chraction", "chrCheckTargetInSight"),
)

PRODUCTION_STAND_ANIM_DEPENDENCIES = (
    ("chraction", "chrlvGetChrOrPresetLocation"),
    ("chraction", "chrlvDistanceToChrRelated"),
    ("chraction", "chrlvSetSubroty"),
)

# Exact grenade decision/animation boundary reached by the authored 0x040d
# list.  Fresh object construction deliberately remains owned by the shared
# gameplay service: on production it either returns a real native weapon prop
# or explicitly reports the unsupported model instead of substituting one.
PRODUCTION_GRENADE_FUNCTIONS = (
    ("propobj", "chrGiveWeapon"),
    ("chraction", "chrlvThrowGrenade"),
    ("chraction", "actor_draws_throws_grenade_at_player_if_possible"),
)


def function_text(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;={{}}]*\b{name}\s*\([^;]*\)"
        rf"\s*(?://[^\n]*)?\s*\{{",
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
            if (state == "string" and char == '"') or \
                    (state == "char" and char == "'"):
                state = "code"
        pos += 1
    raise ValueError(f"unterminated function {name}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    sources = {
        "chr": (args.repo / "src/game/chr.c").read_text(),
        "chrprop": (args.repo / "src/game/chrprop.c").read_text(),
        "bg": (args.repo / "src/game/bg.c").read_text(),
        "bgfog": (args.repo / "src/game/bgfog.c").read_text(),
        "bondview": (args.repo / "src/game/bondview.c").read_text(),
        "bondview2": (args.repo / "src/game/bondview2.c").read_text(),
        "chraction": (args.repo / "src/game/chraction.c").read_text(),
        "file": (args.repo / "src/game/file.c").read_text(),
        "front": (args.repo / "src/game/front.c").read_text(),
        "initanitable": (args.repo / "src/game/initanitable.c").read_text(),
        "model": (args.repo / "src/game/model.c").read_text(),
        "objective": (args.repo / "src/game/objective_status2.c").read_text(),
        "padhall": (args.repo / "src/game/padhalllv.c").read_text(),
        "propobj": (args.repo / "src/game/propobj.c").read_text(),
        "stan": (args.repo / "src/game/stan.c").read_text(),
    }
    forward_declarations = re.search(
        r"(?ms)^// forward declarations\n\n(.*?)\n\n// end forward declarations",
        sources["chraction"],
    )
    if forward_declarations is None:
        raise ValueError("missing canonical chraction forward declarations")
    pieces = [
        "/* Generated from canonical decompiled sources; do not hand-edit. */",
        "#include <limits.h>",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#include <bondaicommands.h>",
        '#include "game/bondview.h"',
        '#include "game/bg.h"',
        '#include "game/bgfog.h"',
        '#include "game/chraction.h"',
        '#include "game/chr.h"',
        '#include "game/file.h"',
        '#include "game/front.h"',
        '#include "game/gun.h"',
        '#include "game/initanitable.h"',
        '#include "game/lv.h"',
        '#include "game/model.h"',
        '#include "music.h"',
        '#include "game/player.h"',
        '#include "game/propobj.h"',
        '#include "game/stan.h"',
        '#include "game/stanintersection.h"',
        '#include "ge_stan_native.h"',
        '#include "ge_original_guard_ai_trace.h"',
        '#include "ge_original_stage_setup.h"',
        '#include "random.h"',
        "#ifndef SQR",
        "#define SQR(x) ((x) * (x))",
        "#endif",
        forward_declarations.group(1),
        "extern s32 g_ActiveChrsCount;",
        "extern ChrRecord *g_ActiveChrs;",
        "extern f32 g_ScaledFarFogIntensity;",
        "static GeOriginalGuardAiLosStats ge_original_guard_ai_los_stats;",
        "extern void chrlvResetAimend(ChrRecord *self);",
        "extern void chrStopFiring(ChrRecord *self);",
        "extern s32 chrlvSetSubroty(ChrRecord *self, s32 turning, "
        "f32 target, f32 scale, f32 acceleration);",
        "extern f32 chrlvDistanceToChrRelated(ChrRecord *self, "
        "s32 entitytype, s32 entityid);",
        "extern f32 chrlvPathingCollisionRelated(PropRecord *prop, "
        "f32 angle, f32 distance, s32 cdtypes, f32 height, f32 radius);",
        "extern f32 get_distance_actor_to_position(ChrRecord *self, "
        "coord3d *position);",
        "extern coord3d *chrlvGetChrOrPresetLocation(ChrRecord *self, "
        "s32 flags, s32 lookup_id, StandTile **stan);",
        "extern s32 plot_course_for_actor(ChrRecord *self, coord3d *position, "
        "StandTile *stan, SPEED speed);",
        "extern waypoint *chrlvStanPathRelated(coord3d *position, "
        "StandTile *stan);",
        "extern s32 waypointFindRoute(waypoint *from, waypoint *to, "
        "waypoint **route, s32 route_length);",
        "extern waypoint *waypointFindNextStepToward(waypoint *from, "
        "waypoint *to);",
        "extern void resetWaypointDistances(void);",
        "extern waypoint *waypointFindRandomByDist(s32 *pointnums, s32 dist);",
        "extern waygroup *waygroupFindRandomByDist(s32 *groupnums, s32 dist);",
        "extern void chrlvActGoposSetTargetPosRelated(ChrRecord *self);",
        "extern void play_hit_soundeffect_and_proper_volume(ChrRecord *self);",
        "extern void chrlvActGoposRelated(ChrRecord *self, coord3d *position, "
        "StandTile **stan);",
        "extern s32 chrlvStanRoomRelated(ChrRecord *self, coord3d *position, "
        "StandTile *stan);",
        "extern void chrlvSetGoposSegDistTotal(ChrRecord *self, "
        "struct waydata *waydata, coord3d *position);",
        "extern void chrlvPlotCourseRelated(ChrRecord *self);",
        "extern void chrlvKneelingAnimationRelated7F023E48(ChrRecord *self);",
        "extern void chrlvTravelTickMagic(ChrRecord *self, "
        "struct waydata *waydata, f32 scale, coord3d *position, "
        "StandTile *stan);",
        "extern f32 chrlvModelScaleAnimationRelated(ChrRecord *self);",
        "extern void chrlvActGoposIncCurIndex(ChrRecord *self);",
        "extern s32 sub_GAME_7F030128(ChrRecord *self, coord3d *point, "
        "StandTile *stan, coord3d *destination, StandTile *destination_stan, "
        "s32 cdtypes);",
        "extern s32 sub_GAME_7F0301FC(ChrRecord *self, coord3d *point, "
        "StandTile *stan, coord3d *destination, f32 radius, s32 cdtypes);",
        "extern void chrlvTravelTick(ChrRecord *self, coord3d *position, "
        "StandTile *stan, struct waydata *waydata);",
        "extern bool chrlvIsPosClearOfObjectBounds(coord3d *position, "
        "StandTile *stan);",
        "extern void chrlvAdvancePatrolStep(ChrRecord *self);",
        "extern PadRecord *chrlvGetNextPatrolStepPad(ChrRecord *self);",
        "extern void chrlvSetNextActPatrolStepPadPos(ChrRecord *self);",
        "extern StandTile *sub_GAME_7F01F614(ChrRecord *self, StandTile *stan, "
        "coord3d *source, coord3d *destination, s32 update_last_move);",
        "extern void sub_GAME_7F057D44(f32 *position, f32 *speed, f32 delta);",
        "extern s32 sub_GAME_7F03081C(ChrRecord *self, coord3d *source, "
        "StandTile *stan, coord3d *destination, coord3d *left, "
        "coord3d *right, f32 inner_radius, f32 outer_radius, s32 cdtypes);",
        "extern s32 sub_GAME_7F030D70(ChrRecord *self, coord3d *source, "
        "StandTile *stan, coord3d *destination, coord3d *left, "
        "coord3d *right, f32 inner_radius, f32 outer_radius, s32 cdtypes);",
        "extern s32 sub_GAME_7F03130C(ChrRecord *self, coord3d *candidate, "
        "s32 side, coord3d *result, f32 radius, s32 use_destination, "
        "coord3d *destination, struct waydata *waydata, f32 clearance, "
        "s32 cdtypes, s32 set_copy);",
        "extern void chrlvWalkingAnimationRelated(ChrRecord *self);",
        "extern s32 chrlvApplySpeed(ChrRecord *self, coord3d *position, "
        "s32 speed_mode, f32 *speed);",
        "extern s32 sub_GAME_7F0304AC(ChrRecord *self, coord3d *source, "
        "StandTile *stan, coord3d *corner, coord3d *destination, "
        "StandTile *destination_stan, s32 cdtypes);",
        "extern void chrlvSwapIfDiffArg2Determinate(coord3d *first, "
        "coord3d *second, coord3d *direction);",
        "extern void doorPlayOpenSound0(DoorRecord *door);",
        "extern void doorPlayOpenSound1(DoorRecord *door);",
        "#ifndef RUNTIMEBITFLAG_BEENOPENED",
        "#define RUNTIMEBITFLAG_BEENOPENED 0x00000200U",
        "#endif",
        "#ifndef M_U16_MAX_VALUE_F",
        "#define M_U16_MAX_VALUE_F 65536.0f",
        "#endif",
        "#define g_BgRoomInfo ge_bg_visibility_room_info",
        "extern s_room_info ge_bg_visibility_room_info[MAXROOMCOUNT];",
        "",
    ]
    health_data = re.search(
        r"(?m)^f32 slider_007_mode_health\s*=\s*1\.0f;",
        sources["front"],
    )
    reaction_data = re.search(
        r"(?m)^f32 slider_007_mode_reaction\s*=\s*0\.0f;",
        sources["front"],
    )
    animation_data = re.search(
        r"(?ms)^s32 animation_table_ptrs1\[\]\s*=\s*\{.*?^\};",
        sources["initanitable"],
    )
    if health_data is None or reaction_data is None or animation_data is None:
        raise ValueError("missing canonical health/animation data")
    animation_names = list(dict.fromkeys(re.findall(
        r"\bPTR_ANIM_([A-Za-z0-9_]+)\b", animation_data.group(0))))
    if not animation_names:
        raise ValueError("missing canonical animation offsets")
    rate_data = re.search(
        r"(?ms)^#ifdef REFRESH_PAL\n#define RATE 1\.2f\n#else\n"
        r"#define RATE 1\.0f\n#endif",
        sources["chraction"],
    )
    if rate_data is None:
        raise ValueError("missing canonical guard animation RATE")
    pieces.extend((health_data.group(0), animation_data.group(0),
                   rate_data.group(0)))
    pieces.append("#if defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)")
    pieces.append(reaction_data.group(0))
    pieces.append("#endif")
    pieces.append("#if defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS) || "
                  "defined(GE_PORT_DAM_GUARD_AI_ANIMATION_OFFSETS)")
    for anim in animation_names:
        pieces.append(
            f"#define ANIM_DATA_{anim} "
            f"(*(s32 *)(uintptr_t)PTR_ANIM_{anim})")
    pieces.append("#endif")
    pieces.append("#if defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)")
    pieces.extend(function_text(sources[source], name)
                  for source, name in HOST_ONLY_FUNCTIONS)
    pieces.append("#endif")
    for source, name in FUNCTIONS:
        body = function_text(sources[source], name)
        if name == "chrHasStoppedOrPatroling":
            pieces.extend((
                "#define chrHasStoppedOrPatroling "
                "ge_original_guard_ai_has_stopped_exact",
                body,
                "#undef chrHasStoppedOrPatroling",
                "bool chrHasStoppedOrPatroling(ChrRecord *self)\n"
                "{\n"
                "    bool result = "
                "ge_original_guard_ai_has_stopped_exact(self);\n"
                "    ge_original_guard_ai_los_stats.stopped_check_calls++;\n"
                "    if (result) "
                "ge_original_guard_ai_los_stats.stopped_check_passes++;\n"
                "    if (self != NULL && self->chrnum == 7) {\n"
                "        ge_original_guard_ai_los_stats."
                "chr7_stopped_check_calls++;\n"
                "        if (result) ge_original_guard_ai_los_stats."
                "chr7_stopped_check_passes++;\n"
                "    }\n"
                "    return result;\n"
                "}",
            ))
        else:
            pieces.append(body)
    pieces.append("#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS) || "
                  "defined(GE_PORT_DAM_GUARD_AI_NAVIGATION_TEST)")
    pieces.extend(function_text(sources[source], name)
                  for source, name in PRODUCTION_NAVIGATION_FUNCTIONS)
    pieces.append("#endif")
    pieces.append("#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)")
    ground_macros = re.search(
        r"(?ms)^#ifdef VERSION_EU\n#define GROUND_SMOOTH_FACTOR.*?^#endif",
        sources["chr"],
    )
    speed_macros = re.search(
        r"(?ms)^#if defined\(REFRESH_NTSC\)\n/\* NTSC \*/\n"
        r"#define MAX_SPEED_A.*?^#endif\n\n"
        r"#if defined\(REFRESH_PAL\).*?^#endif",
        sources["chraction"],
    )
    bfs_locals = re.search(
        r"(?ms)^typedef struct BfsSearchLocals \{.*?^\} BfsSearchLocals;",
        sources["stan"],
    )
    bfs_storage = re.search(
        r"(?m)^StandTile \*bfsTileStack\[352\];",
        sources["stan"],
    )
    seen_bond_storage = re.search(
        r"(?m)^s32 g_SeenBondRecentlyGuardCount\s*=\s*0;",
        sources["chr"],
    )
    if (ground_macros is None or speed_macros is None
            or bfs_locals is None or bfs_storage is None
            or seen_bond_storage is None):
        raise ValueError("missing canonical STAN BFS data")
    pieces.extend((ground_macros.group(0), speed_macros.group(0),
                   bfs_locals.group(0), bfs_storage.group(0),
                   seen_bond_storage.group(0)))
    for source, name in PRODUCTION_ROUTE_FUNCTIONS:
        body = function_text(sources[source], name)
        if name == "get_ptrpreset_in_table_matching_tile":
            pieces.extend((
                "#define get_ptrpreset_in_table_matching_tile "
                "ge_original_get_ptrpreset_matching_tile_exact",
                body,
                "#undef get_ptrpreset_in_table_matching_tile",
                "waypoint *get_ptrpreset_in_table_matching_tile("
                "StandTile *tile)\n"
                "{\n"
                "    if (!ge_original_stage_setup_active_path_valid()) "
                "return NULL;\n"
                "    return ge_original_get_ptrpreset_matching_tile_exact("
                "tile);\n"
                "}",
            ))
        elif name == "stanFillSearch":
            pieces.extend((
                "#define stanFillSearch ge_original_stan_fill_search_exact",
                body,
                "#undef stanFillSearch",
                "StandTile *stanFillSearch(StandTile *starttile, "
                "tilePredicate_t predicate)\n"
                "{\n"
                "    StandTile *result;\n"
                "    if (!ge_stan_native_route_search_start(starttile)) "
                "return NULL;\n"
                "    result = ge_original_stan_fill_search_exact(starttile, "
                "predicate);\n"
                "    if (!ge_stan_native_route_search_result(result)) "
                "return NULL;\n"
                "    return result;\n"
                "}",
            ))
        else:
            pieces.append(body)
    pieces.extend(function_text(sources[source], name)
                  for source, name in PRODUCTION_ACTION_FUNCTIONS)
    pieces.append("#endif")
    pieces.append("#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS) || "
                  "defined(GE_PORT_DAM_GUARD_AI_STAND_ANIM_TEST)")
    pieces.extend(function_text(sources[source], name)
                  for source, name in PRODUCTION_STAND_ANIM_DEPENDENCIES)
    for source, name, renamed in PRODUCTION_STAND_ANIM_FUNCTIONS:
        pieces.extend((f"#define {name} {renamed}",
                       function_text(sources[source], name),
                       f"#undef {name}"))
    pieces.append("#endif")
    pieces.append("#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS) || "
                  "defined(GE_PORT_DAM_GUARD_AI_GRENADE_TEST)")
    pieces.extend(function_text(sources[source], name)
                  for source, name in PRODUCTION_GRENADE_FUNCTIONS)
    pieces.append("#endif")
    pieces.append("#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS) || "
                  "defined(GE_PORT_DAM_GUARD_AI_SIGHT_TEST)")
    pieces.append(function_text(
        sources["bgfog"], "fogGetScaledFarFogIntensitySquared"))
    pieces.extend((
        "void ge_original_guard_ai_los_trace_snapshot("
        "GeOriginalGuardAiLosStats *stats)\n"
        "{\n"
        "    if (stats != NULL) *stats = ge_original_guard_ai_los_stats;\n"
        "}",
        "void ge_original_guard_ai_trace_action_tick(ChrRecord *self)\n"
        "{\n"
        "    ge_original_guard_ai_los_stats.action_tick_calls++;\n"
        "    if (self != NULL && self->chrnum == 7) "
        "ge_original_guard_ai_los_stats.chr7_action_tick_calls++;\n"
        "}",
        "void ge_original_guard_ai_trace_unknown_opcode(ChrRecord *self, "
        "void *list, int32_t offset, uint8_t opcode)\n"
        "{\n"
        "    ge_original_guard_ai_los_stats.unknown_opcode_calls++;\n"
        "    ge_original_guard_ai_los_stats."
        "unknown_opcode_histogram[opcode]++;\n"
        "    ge_original_guard_ai_los_stats.last_unknown_chr = "
        "self != NULL ? self->chrnum : -1;\n"
        "    ge_original_guard_ai_los_stats.last_unknown_offset = offset;\n"
        "    ge_original_guard_ai_los_stats.last_unknown_opcode = opcode;\n"
        "    ge_original_guard_ai_los_stats.last_unknown_list = list;\n"
        "    if (self != NULL && self->chrnum == 7) "
        "ge_original_guard_ai_los_stats.chr7_unknown_opcode_calls++;\n"
        "}",
        "static s32 ge_original_guard_ai_stan_test_line_traced("
        "StandTile **tile, f32 start_x, f32 start_z, f32 destination_x, "
        "f32 destination_z, s32 cdtypes, f32 height, f32 height_end, "
        "f32 slope_start, f32 slope_end)\n"
        "{\n"
        "    GeOriginalGuardAiLosStats *stats = "
        "&ge_original_guard_ai_los_stats;\n"
        "    PropRecord *player;\n"
        "    f32 dx;\n"
        "    f32 dz;\n"
        "    f32 distance_squared;\n"
        "    s32 result;\n"
        "    stats->calls++;\n"
        "    stats->last_start_tile = tile != NULL ? *tile : NULL;\n"
        "    stats->last_start_x = start_x;\n"
        "    stats->last_start_z = start_z;\n"
        "    stats->last_destination_x = destination_x;\n"
        "    stats->last_destination_z = destination_z;\n"
        "    stats->last_cdtypes = cdtypes;\n"
        "    result = stanTestLineUnobstructed(tile, start_x, start_z, "
        "destination_x, destination_z, cdtypes, height, height_end, "
        "slope_start, slope_end);\n"
        "    stats->last_result_tile = tile != NULL ? *tile : NULL;\n"
        "    stats->last_collision_prop = stanSavedColl_posData;\n"
        "    stats->last_collision_prop_type = stanSavedColl_posData != NULL "
        "? (u8)stanSavedColl_posData->type : UINT8_MAX;\n"
        "    stats->last_fraction = stanSavedColl_someMin;\n"
        "    if (result != 0) stats->clear_results++;\n"
        "    else stats->blocked_results++;\n"
        "    player = getCurrentPlayerProp();\n"
        "    if (result != 0 && tile != NULL && player != NULL "
        "&& *tile == player->stan) stats->destination_tile_matches++;\n"
        "    dx = destination_x - start_x;\n"
        "    dz = destination_z - start_z;\n"
        "    distance_squared = dx * dx + dz * dz;\n"
        "    if (stats->shortest_calls == 0 "
        "|| distance_squared < stats->shortest_distance_squared) {\n"
        "        stats->shortest_calls++;\n"
        "        stats->shortest_distance_squared = distance_squared;\n"
        "        stats->shortest_result = result;\n"
        "        stats->shortest_start_tile = stats->last_start_tile;\n"
        "        stats->shortest_result_tile = stats->last_result_tile;\n"
        "        stats->shortest_player_tile = player != NULL "
        "? player->stan : NULL;\n"
        "        stats->shortest_collision_prop = stats->last_collision_prop;\n"
        "        stats->shortest_collision_prop_type = "
        "stats->last_collision_prop_type;\n"
        "        stats->shortest_start_x = start_x;\n"
        "        stats->shortest_start_z = start_z;\n"
        "        stats->shortest_destination_x = destination_x;\n"
        "        stats->shortest_destination_z = destination_z;\n"
        "        stats->shortest_fraction = stats->last_fraction;\n"
        "    }\n"
        "    return result;\n"
        "}",
    ))
    for source, name in PRODUCTION_SIGHT_FUNCTIONS:
        body = function_text(sources[source], name)
        if name == "chrCanSeeBond":
            pieces.extend((
                "#define stanTestLineUnobstructed "
                "ge_original_guard_ai_stan_test_line_traced",
                body,
                "#undef stanTestLineUnobstructed",
            ))
        elif name == "chrCheckTargetInSight":
            pieces.extend((
                "#define chrCheckTargetInSight "
                "ge_original_guard_ai_check_target_in_sight_exact",
                body,
                "#undef chrCheckTargetInSight",
                "bool chrCheckTargetInSight(ChrRecord *self)\n"
                "{\n"
                "    bool result = "
                "ge_original_guard_ai_check_target_in_sight_exact(self);\n"
                "    ge_original_guard_ai_los_stats.sight_check_calls++;\n"
                "    if (result) "
                "ge_original_guard_ai_los_stats.sight_check_passes++;\n"
                "    ge_original_guard_ai_los_stats.last_sight_check_chr = "
                "self != NULL ? self->chrnum : -1;\n"
                "    if (self != NULL && self->chrnum == 7) {\n"
                "        ge_original_guard_ai_los_stats."
                "chr7_sight_check_calls++;\n"
                "        if (result) ge_original_guard_ai_los_stats."
                "chr7_sight_check_passes++;\n"
                "    }\n"
                "    return result;\n"
                "}",
            ))
        else:
            pieces.append(body)
    pieces.append("#endif")
    pieces.append("#if !defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS) || "
                  "defined(GE_PORT_DAM_GUARD_AI_ACTION_GRAPH_TEST)")
    action_graph_data = re.search(
        r"(?m)^coord3d D_80030A44\s*=\s*\{0, 0, 0\};",
        sources["chraction"],
    )
    if action_graph_data is None:
        raise ValueError("missing canonical action graph data")
    pieces.append(action_graph_data.group(0))
    for pattern, source, label in (
        (r"(?m)^point2d D_800309B8\s*=\s*\{0, 0\};", "chr", "D_800309B8"),
        (r"(?m)^point2d D_800309C0\s*=\s*\{0, 0\};", "chr", "D_800309C0"),
        (r"(?m)^point2d D_800309A8\s*=\s*\{0, 0\};", "chr", "D_800309A8"),
        (r"(?m)^point2d D_800309B0\s*=\s*\{0, 0\};", "chr", "D_800309B0"),
        (r"(?m)^point2d D_800309C8\s*=\s*\{0, 0\};", "chr", "D_800309C8"),
        (r"(?m)^point2d D_800309D0\s*=\s*\{0, 0\};", "chr", "D_800309D0"),
        (r"(?m)^point2d D_800309D8\s*=\s*\{0, 0\};", "chr", "D_800309D8"),
        (r"(?m)^point2d D_800309E0\s*=\s*\{0, 0\};", "chr", "D_800309E0"),
        (r"(?m)^point2d D_800309E8\s*=\s*\{0, 0\};", "chr", "D_800309E8"),
        (r"(?m)^point2d D_800309F0\s*=\s*\{0, 0\};", "chraction", "D_800309F0"),
        (r"(?m)^/\* 0x80030AC8 \*/ s32 alarm_timer\s*=\s*0;", "propobj", "alarm_timer"),
    ):
        match = re.search(pattern, sources[source])
        if match is None:
            raise ValueError(f"missing canonical action graph data {label}")
        pieces.append(match.group(0))
    for source, name in PRODUCTION_ACTION_GRAPH_DEPENDENCIES:
        body = function_text(sources[source], name)
        if name == "get_distance_actor_to_position":
            # The production ARM owner is the retained guard-damage slice.
            # Keep the unchanged body here only for self-contained host links.
            pieces.extend((
                "#if defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
                body,
                "#endif",
            ))
        elif name == "chrlvInitActAttack":
            # The original arithmetic intentionally casts ROM pointers through
            # N64 s32. Preserve that body below for ARM; the 64-bit sanitizer
            # fixture needs only the pointer-width ABI spelling of the same
            # table-index calculation.
            host_body = body.replace(
                "(s32)arg1[anim_index]->table + (s32)((s32)next_anim * (s32)sizeof(struct weapon_firing_animation_table))",
                "(uintptr_t)arg1[anim_index]->table + (uintptr_t)((s32)next_anim * (s32)sizeof(struct weapon_firing_animation_table))")
            host_body = host_body.replace(
                "(s32)arg1[anim_index]->table + (s32)(((next_anim + 1) % arg1[anim_index]->len) * (s32)sizeof(struct weapon_firing_animation_table))",
                "(uintptr_t)arg1[anim_index]->table + (uintptr_t)(((next_anim + 1) % arg1[anim_index]->len) * (s32)sizeof(struct weapon_firing_animation_table))")
            pieces.extend((
                "#if defined(GE_PORT_DAM_GUARD_AI_HOST_OFFSETS)",
                host_body,
                "#else",
                body,
                "#endif",
            ))
        else:
            pieces.append(body)
    for source, name, renamed in PRODUCTION_REMAINING_ACTION_HANDLERS:
        pieces.extend((f"#define {name} {renamed}",
                       function_text(sources[source], name),
                       f"#undef {name}"))
    pieces.append("#endif")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n\n".join(pieces) + "\n", encoding="utf-8")
    total = (len(FUNCTIONS) + len(HOST_ONLY_FUNCTIONS)
             + len(PRODUCTION_NAVIGATION_FUNCTIONS)
             + len(PRODUCTION_ROUTE_FUNCTIONS)
             + len(PRODUCTION_ACTION_FUNCTIONS)
             + len(PRODUCTION_STAND_ANIM_DEPENDENCIES)
             + len(PRODUCTION_STAND_ANIM_FUNCTIONS)
             + len(PRODUCTION_GRENADE_FUNCTIONS)
             + len(PRODUCTION_SIGHT_FUNCTIONS)
             + len(PRODUCTION_REMAINING_ACTION_HANDLERS)
             + len(PRODUCTION_ACTION_GRAPH_DEPENDENCIES))
    print(f"generated {total} exact Dam guard AI services")


if __name__ == "__main__":
    main()

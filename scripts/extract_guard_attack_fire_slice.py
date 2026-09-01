#!/usr/bin/env python3
"""Extract the unchanged guard attack/fire/player-damage path for host audit."""

from __future__ import annotations

import argparse
from pathlib import Path

from extract_dam_guard_chr_scheduler_slice import function_text


CHRACTION_FUNCTIONS = (
    "sub_GAME_7F02BFE4",
    "chrlvGetSubrotySideback",
    "sub_GAME_7F02C27C",
    "chrlvGetAimLimitAngle",
    "chrlvUpdateShotbondsum",
    "sub_GAME_7F02D630",
    "chrlvToggleHiddenRelated",
    "chrlvFireWeaponRelated",
    "chrlvTriggerFireWeapon",
    "chrlvTickAttackCommon",
    "chrlvTickAttack",
)

BONDVIEW_FUNCTIONS = (
    "record_damage_kills",
    "bondviewCallRecordDamageKills",
    "bondviewGetIfCurrentPlayerDamageShowTime",
)

GUN_FUNCTIONS = (
    "gunItemGetDestructionAmount",
    "bondwalkItemGetAutomaticFiringRate",
    "bondwalkItemGetSoundTriggerRate",
    "bondwalkItemGetSound",
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    chraction = (args.repo / "src/game/chraction.c").read_text()
    bondview = (args.repo / "src/game/bondview2.c").read_text()
    gun = (args.repo / "src/game/gun.c").read_text()
    pieces = [
        "/* Generated unchanged guard attack/fire path; do not hand-edit. */",
        "#include <ultra64.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#include <bondaicommands.h>",
        "#include <math.h>",
        '#include "game/bondinv.h"',
        '#include "game/bondview.h"',
        '#include "game/chraction.h"',
        '#include "game/chr.h"',
        '#include "game/file.h"',
        '#include "game/front.h"',
        '#include "game/gun.h"',
        '#include "game/lv.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/player.h"',
        '#include "game/propobj.h"',
        '#include "game/stan.h"',
        '#include "game/stanintersection.h"',
        '#include "music.h"',
        '#include "snd.h"',
        "#ifndef M_U16_MAX_VALUE_F",
        "#define M_U16_MAX_VALUE_F 65536.0f",
        "#endif",
        "#ifndef RUNTIMEBITFLAG_HASPROJECTILE",
        "#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)",
        "#endif",
        "extern void chrlvResetAimend(ChrRecord *self);",
        "extern void chrlvKneelingAnimationRelated7F023E48(ChrRecord *self);",
        "extern s32 chrlvAttackrollAnimationRelated7F02E2E0(ChrRecord *self);",
        "extern void chrlvAttackrollAnimationRelated7F02E3B8(ChrRecord *self);",
        "extern void sub_GAME_7F025560(ChrRecord *self, s32, s32);",
        "extern void sub_GAME_7F0256F0(ChrRecord *self, s32, s32);",
        "extern f32 chrlvGetGuard007SpeedRating(ChrRecord *self, f32, f32);",
        "extern s32 chrlvSetSubroty(ChrRecord *self, s32, f32, f32, f32);",
        "extern void chrlvSetTargetToPlayer(ChrRecord *self);",
        "extern s32 chrlvUpdateAimendsideback(ChrRecord *self,",
        "    struct weapon_firing_animation_table *, s32, s32, f32);",
        "extern void chrSetFiring(ChrRecord *self, s32 hand, s32 firing);",
        "extern void chrlvStanLineDirIntersection(coord3d *, coord3d *, coord3d *);",
        "extern s32 chrlvAttackRelated7F0292A8(ChrRecord *, coord3d *, StandTile *);",
        "extern void bullet_spark_create(coord3d *, s32, f32, s16);",
        "extern void CapBeamLengthAndDecideIfRendered(struct BeamRecord *,",
        "    ITEM_IDS, coord3d *, coord3d *);",
        "extern s32 get_cur_playernum(void);",
        "extern s32 g_stopPlayFlag;",
        "extern s32 g_gameOverFlag;",
        "extern void hudMakeDamageSegments(f32 *, s32, s32, f32);",
        "extern void joyRumblePakStart(s8 player, f32 strength);",
        "extern void increment_num_deaths(void);",
        "extern void increment_num_suicides_display_MP(void);",
        "extern void increment_num_times_killed_MwtGC(void);",
        "extern void bondviewKillCurrentPlayer(void);",
        "",
    ]
    pieces.extend(function_text(chraction, name)
                  for name in CHRACTION_FUNCTIONS)
    pieces.extend(function_text(bondview, name)
                  for name in BONDVIEW_FUNCTIONS)
    pieces.extend(function_text(gun, name) for name in GUN_FUNCTIONS)
    args.output.write_text("\n\n".join(pieces) + "\n")


if __name__ == "__main__":
    main()

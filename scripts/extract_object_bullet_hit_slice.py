#!/usr/bin/env python3
"""Extract the canonical ordinary-object bullet hit and damage dispatch."""

from __future__ import annotations

import argparse
from pathlib import Path

from extract_dam_guard_chr_scheduler_slice import function_text


PROPOBJ_FUNCTIONS = (
    "sub_GAME_7F04DD68",
    "objBreakCCTVGlass",
    "objBounce",
    "sub_GAME_7F04E720",
    "sub_GAME_7F04E9BC",
    "objHit",
)

GUNFIRE_FUNCTIONS = (
    "sub_GAME_7F064720",
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    propobj = (args.repo / "src/game/propobj.c").read_text()
    gunfire = (args.repo / "src/game/gunfire.c").read_text()
    bodies = [function_text(propobj, name) for name in PROPOBJ_FUNCTIONS]
    bodies.extend(function_text(gunfire, name) for name in GUNFIRE_FUNCTIONS)
    output = """/* Generated from canonical decompiled sources; do not hand-edit. */
#include <stddef.h>
#include <ultra64.h>
#include <PR/gbi.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "bondaicommands.h"
#include "music.h"
#define RUNTIMEBITFLAG_EMBEDDED (1u << 6)
#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/bondview.h"
#include "game/chr.h"
#include "game/explosion.h"
#include "game/glass.h"
#include "game/gun.h"
#include "game/image_bank.h"
#include "game/model.h"
#include "game/objecthandler.h"
#include "game/player.h"
#include "game/propobj.h"
#include "random.h"
#include "game/tex.h"

extern f32 chrpropSumMatrixNegZ(
    struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx);
extern void door7F0526EC(DoorRecord *door, Mtxf *rhs);
extern void explosionClearBulletImpactRoomByFlag(PropRecord *prop, s8 flag);
extern bool propobjFindHit(Model *model, ModelNode *node, coord3d *raypos,
    coord3d *raydir, HitThing *hit, s32 *mtxindex, ModelNode **hitnode);
extern u32 modelFindNextProjectileHitCandidate(Model *model, coord3d *raypos,
    coord3d *raydir, ModelNode **nodeptr);
extern ALSoundState *gunGetFreeSfxState(void);
extern void objBreakCCTVGlass(ObjectRecord *obj);
extern void objBounce(ObjectRecord *obj, coord3d *direction);
extern void objDropRecursively(PropRecord *prop);
extern void sub_GAME_7F064720(coord3d *pos);

/* 0x80030B18 */ f32 F_80030B18 = 1.0f;
/* 0x80030B24 */ f32 F_80030B24 = 1.0f;

""" + "\n\n".join(bodies) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    print(f"generated {len(bodies)} exact object bullet-hit bodies -> {args.output}")


if __name__ == "__main__":
    main()

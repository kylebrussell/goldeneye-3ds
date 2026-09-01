#!/usr/bin/env python3
"""Retain the canonical ACT_DIE -> ACT_DEAD action closure.

This intentionally lives outside the broader Dam AI/action extractor.  It is
used by the focused lethal-hit sanitizer harness, while the production build
gets the same unchanged bodies from the complete action-graph slice.
"""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path


def load_extractor(path: Path):
    spec = importlib.util.spec_from_file_location("guard_hit_extract", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    extract = load_extractor(
        args.repo / "scripts/extract_guard_bullet_hit_slice.py")
    action = (args.repo / "src/game/chraction.c").read_text()
    chr_source = (args.repo / "src/game/chr.c").read_text()

    pieces = [
        "/* Generated from canonical decompiled sources; do not hand-edit. */",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        '#include "game/chraction.h"',
        '#include "game/chr.h"',
        '#include "game/initanitable.h"',
        '#include "game/model.h"',
        '#include "game/propobj.h"',
        '#include "game/stan.h"',
        '#include "assets/animationtable_data.h"',
        '#include "music.h"',
        '#include "snd.h"',
        '#include "ge_original_dam_guard_ai_tick.h"',
        "extern ChrRecord *g_ChrSlots;",
        "extern s32 g_NumChrSlots;",
        "extern s32 g_ClockTimer;",
        "extern struct animation_table_data *ptr_animation_table;",
        "extern ALSoundState *sndPlaySfx(struct ALBankAlt_s *, s16, ALSoundState *);",
        "extern void chrobjSndCreatePostEventDefault(ALSoundState *, coord3d *);",
        "extern void chrStopFiring(ChrRecord *);",
        "extern void chrSetMoving(ChrRecord *, s32);",
        "extern f32 getsubroty(Model *);",
        "#define ANIM_DATA_death_left_leg "
        "(*(s32 *)(uintptr_t)PTR_ANIM_death_left_leg)",
        "#define ANIM_DATA_jump_backwards "
        "(*(s32 *)(uintptr_t)PTR_ANIM_jump_backwards)",
        "",
        extract.find_function(chr_source, "get_numguards"),
        extract.find_function(action, "check_if_position_in_same_room"),
        extract.find_function(action, "chrlvMaybeSameRoom"),
        extract.find_function(action, "chrlvActorFadeAway"),
        extract.find_function(action, "chrlvIterateGuardSeeShotDie"),
        "#define chrlvTickDie ge_original_dam_guard_tick_die_exact",
        extract.find_function(action, "chrlvTickDie"),
        "#undef chrlvTickDie",
        "#define chrlvTickDead ge_original_dam_guard_tick_dead_exact",
        extract.find_function(action, "chrlvTickDead"),
        "#undef chrlvTickDead",
    ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n\n".join(pieces) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

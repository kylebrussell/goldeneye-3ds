#!/usr/bin/env python3
"""Retain GoldenEye's exact live-character scheduling path for Dam guards."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


CHR_FUNCTIONS = ("chrDetectRooms", "chrUpdateAnim", "chrTick")
CHRPROP_FUNCTIONS = ("propsTick",)
CHRACTION_FUNCTIONS = ("chrlvAllChrTick",)


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
            if ((state == "string" and char == '"')
                    or (state == "char" and char == "'")):
                state = "code"
        pos += 1
    raise ValueError(f"unterminated function {name}")


def render(repo: Path) -> str:
    chr_source = (repo / "src/game/chr.c").read_text()
    chrprop_source = (repo / "src/game/chrprop.c").read_text()
    chraction_source = (repo / "src/game/chraction.c").read_text()
    pieces = [
        "/* Generated unchanged canonical character scheduler slice. */",
        '#include "ge_original_dam_guard_chr_scheduler.h"',
        '#include "ge_original_dam_guard_ai_tick.h"',
        "#include <ultra64.h>",
        "#include <PR/gbi.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#include <random.h>",
        "#include <snd.h>",
        "#include <math.h>",
        '#include "bondaicommands.h"',
        '#include "bg.h"',
        '#include "cheat.h"',
        '#include "chr.h"',
        '#include "chrai.h"',
        '#include "chraction.h"',
        '#include "chrobjdata.h"',
        '#include "debugmenu_handler.h"',
        '#include "dyn.h"',
        '#include "glass.h"',
        '#include "file2.h"',
        '#include "propobj.h"',
        '#include "explosion.h"',
        '#include "file.h"',
        '#include "gun.h"',
        '#include "initanitable.h"',
        '#include "joy.h"',
        '#include "lv.h"',
        '#include "language.h"',
        '#include "matrixmath.h"',
        '#include "objecthandler.h"',
        '#include "player.h"',
        '#include "stan.h"',
        '#include "model.h"',
        '#include "tex.h"',
        "",
        "#ifndef RUNTIMEBITFLAG_REMOVE",
        "#define RUNTIMEBITFLAG_REMOVE (1u << 2)",
        "#endif",
        "extern void chrUpdateAimProperties(ChrRecord *chr);",
        "extern void chrHandleJointPositioned(enum CHR_RENDER_PART bodypart,",
        "                                     Mtxf *matrix);",
        "extern s32 objTick(PropRecord *prop);",
        "extern s32 playerTick(PropRecord *prop);",
        "extern void handle_alarm_gas_timer_calldamage(void);",
        "extern void loop_set_sound_effect_all_slots(void);",
        "",
        "#define chrDetectRooms ge_original_dam_guard_chr_detect_rooms_exact",
        "#define chrUpdateAnim ge_original_dam_guard_chr_update_anim_exact",
        "#define chrTick ge_original_dam_guard_chr_tick_exact",
        "#define propsTick ge_original_dam_guard_props_tick_exact",
        "#define chrlvActionTick ge_original_dam_guard_action_tick_exact",
        "#define chrlvAllChrTick ge_original_dam_guard_all_chr_tick_exact",
        "",
    ]
    # Wrappers observe dispatch boundaries; retained source bodies remain
    # byte-for-byte unchanged and an unbound clock calls straight through.
    pieces.append(r"""
static GeOriginalPropsProfileClock props_profile_clock;
static void *props_profile_context;
uint64_t ge_original_props_profile[10];
void ge_original_props_profile_bind(GeOriginalPropsProfileClock clock, void *context)
{
    props_profile_clock = clock;
    props_profile_context = context;
    for (unsigned i = 0; i < 10; ++i) ge_original_props_profile[i] = 0;
}
""")
    pieces.extend(function_text(chr_source, name) for name in CHR_FUNCTIONS[:2])
    for function, target, params, args, category in (
        ("chrlvActionTick", "ge_original_dam_guard_action_tick_exact", "ChrRecord *chr", "chr", 6),
        ("chrUpdateAnim", "ge_original_dam_guard_chr_update_anim_exact", "ChrRecord *chr, s32 ticks", "chr, ticks", 7),
        ("subcalcmatrices", "subcalcmatrices", "ModelRenderData *data, Model *model", "data, model", 8),
        ("chrlvTriggerFireWeapon", "chrlvTriggerFireWeapon", "ChrRecord *chr", "chr", 9),
    ):
        pieces.append(f"""
static void profile_{function}({params})
{{
    if (props_profile_clock == NULL) {{ {target}({args}); return; }}
    uint64_t start = props_profile_clock(props_profile_context);
    {target}({args});
    ge_original_props_profile[{category}] += props_profile_clock(props_profile_context) - start;
}}
#undef {function}
#define {function} profile_{function}
""")
    pieces.append(function_text(chr_source, "chrTick"))
    pieces.extend(function_text(chraction_source, name) for name in CHRACTION_FUNCTIONS)
    for function, target, category in (
        ("chrTick", "ge_original_dam_guard_chr_tick_exact", 0),
        ("objTick", "objTick", 1),
        ("explosionChrpropExplosionTick", "explosionChrpropExplosionTick", 2),
        ("explosionChrpropSmokeTick", "explosionChrpropSmokeTick", 2),
        ("playerTick", "playerTick", 2),
    ):
        pieces.append(f"""
static s32 profile_{function}(PropRecord *prop)
{{
    if (props_profile_clock == NULL) return {target}(prop);
    uint64_t start = props_profile_clock(props_profile_context);
    s32 result = {target}(prop);
    ge_original_props_profile[{category}] += props_profile_clock(props_profile_context) - start;
    ge_original_props_profile[{category + 3}]++;
    return result;
}}
#undef {function}
#define {function} profile_{function}
""")
    pieces.extend(function_text(chrprop_source, name)
                  for name in CHRPROP_FUNCTIONS)
    pieces.append("\n".join("#undef " + name for name in (
        "objTick", "explosionChrpropExplosionTick", "explosionChrpropSmokeTick", "playerTick",
        "subcalcmatrices", "chrlvTriggerFireWeapon")))
    pieces.extend([
        "",
        "#undef chrlvAllChrTick",
        "#undef chrlvActionTick",
        "#undef propsTick",
        "#undef chrTick",
        "#undef chrUpdateAnim",
        "#undef chrDetectRooms",
        "",
    ])
    return "\n\n".join(pieces)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(render(args.repo))
    print("generated exact Dam guard propsTick/chrTick scheduler")


if __name__ == "__main__":
    main()

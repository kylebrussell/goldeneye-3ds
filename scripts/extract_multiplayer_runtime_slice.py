#!/usr/bin/env python3
"""Retain canonical interaction and viewport clocks for the shared match runtime."""
from pathlib import Path
import argparse
import hashlib
from extract_stage_pickup_slice import function_text
from extract_bond_move_state_slice import extract_function


def render(repo):
    source = (repo / "src/game/chrprop.c").read_text()
    bodies = "\n\n".join(function_text(source, name) for name in
                         ("propFindForInteract", "bond_interact_object"))
    bondview = (repo / "src/game/bondview2.c").read_text()
    viewport = extract_function(bondview,
                                "bondviewMovePlayerUpdateViewport")
    start = bondview.index("#if defined(VERSION_JP) || defined(VERSION_EU)",
                           bondview.index("s32 mission_timer;"))
    end = bondview.index("#endif", start) + len("#endif")
    watch_declaration = bondview[start:end]
    assert "f32 watch_time_0;" in watch_declaration and "s32 watch_time_0;" in watch_declaration
    condition = "    if ((g_CameraMode == CAMERAMODE_NONE) || ((g_CameraMode == CAMERAMODE_FP) && (is_timer_active != 0)) || (g_CameraMode == CAMERAMODE_FADE_TO_TITLE))"
    start = viewport.index(condition)
    move = viewport.index("        MoveBond(stick_x", start)
    mission = viewport[start:move].rstrip() + "\n    }"
    assert mission.count("mission_timer += g_ClockTimer;") == 1
    assert mission.count("if (get_cur_playernum() == 0)") == 1
    frozen = viewport.index("        bondviewFrozenMoveBond(stick_x", move)
    start = viewport.index("#if defined(BUGFIX_R1)", frozen)
    end = viewport.index("#endif", start) + len("#endif")
    watch = viewport[start:end]
    assert "watch_time_0 += speedgraphframes;" in watch
    assert "watch_time_0 += jpD_800484D0;" in watch
    return """/* Generated unchanged chrprop.c interaction; native resource seams. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/propobj.h"
#include "game/chrai.h"
#include "game/bondview.h"
#include "game/player.h"
#include "game/frametiming.h"
#include "ge_original_door_interaction.h"
extern PropRecord *g_InteractProp, *g_OnScreenPropList[], **g_LastOnScreenProp;
extern bool objTestForInteract(PropRecord *prop);
extern void propExecuteTickOperation(PropRecord *prop, TICKOP tickop);
extern TICKOP ge_original_mp_runtime_interact_object(PropRecord *prop);
extern TICKOP ge_original_mp_runtime_interact_door(PropRecord *prop);
static bool native_door_test(PropRecord *prop)
{
    void *selected = g_InteractProp;
    bool checkmore = ge_original_door_interaction_test_native(prop, &selected);
    g_InteractProp = selected;
    return checkmore;
}
#define propFindForInteract ge_original_mp_runtime_find_interactable_exact
#define bond_interact_object ge_original_mp_runtime_interaction_exact
#define doorTestForInteract native_door_test
#define propobjInteract ge_original_mp_runtime_interact_object
#define propdoorInteract ge_original_mp_runtime_interact_door
""" + bodies + "\n/* canonical sha256 " + hashlib.sha256(bodies.encode()).hexdigest() + " */\n" + \
        watch_declaration + "\n" + \
        "/* The committed clock is a typed external read. Native mission/watch\n" \
        " * scalar ownership and the original selected-viewport order remain. */\n" \
        "void ge_original_mp_runtime_mission_clock_exact(s32 g_ClockTimer)\n{\n" + mission + "\n}\n" + \
        "void ge_original_mp_runtime_watch_clock_exact(void)\n{\n" + watch + "\n}\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(args.repo))

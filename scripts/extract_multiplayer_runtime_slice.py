#!/usr/bin/env python3
"""Retain canonical mixed interaction traversal for the shared match runtime."""
from pathlib import Path
import argparse
import hashlib
from extract_stage_pickup_slice import function_text


def render(repo):
    source = (repo / "src/game/chrprop.c").read_text()
    bodies = "\n\n".join(function_text(source, name) for name in
                         ("propFindForInteract", "bond_interact_object"))
    return """/* Generated unchanged chrprop.c interaction; native resource seams. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/propobj.h"
#include "game/chrai.h"
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
""" + bodies + "\n/* canonical sha256 " + hashlib.sha256(bodies.encode()).hexdigest() + " */\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(args.repo))

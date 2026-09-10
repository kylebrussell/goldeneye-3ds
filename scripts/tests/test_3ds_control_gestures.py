#!/usr/bin/env python3
"""Run physical/touch mapping through joy.c and the canonical 2.x input branch.

This is input-dispatch evidence, not a complete MoveBond/hardware route.
"""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]

def block(source, start):
    first = source.index("{", start)
    depth = 0
    for end in range(first, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if not depth:
            return source[first:end + 1]
    raise AssertionError("Unclosed canonical block")

port = (repo / "port/src/ge_port.c").read_text()
start = port.index("static uint16_t ge_port_n64_buttons(")
mapping = port[start:port.index("\nstatic uint16_t ge_port_level_buttons_pressed", start)]
source = (repo / "src/game/bondview2.c").read_text()
start = source.index("void bondviewProcessInput(")
start = source.index("if (cur_player_get_control_type() == CONTROLLER_CONFIG_DOMINO", start)
dual_body = block(source, start)
move_data = "struct MoveData {" + (repo / "src/bondtypes.h").read_text().split("struct MoveData {", 1)[1].split("};", 1)[0] + "};\n"
harness = r'''
#include "ge_3ds_controls.h"
#include "ge_original_input.h"
#include <ultra64.h>
#undef MAXFLOAT
#include <bondconstants.h>
#include "joy.h"
#include <assert.h>
#include <string.h>
#define GE_PORT_CSTICK_BUTTON_THRESHOLD 0.35f
#define FLOAT_TEN_B 10.0f
''' + move_data + r'''
static struct { int insightaimmode, watch_animation_state; unsigned prev_buttons_pressed; } player;
#define g_CurrentPlayer (&player)
static int g_PlayerIsInTank, weapon, scoped, locked, actions_enabled=1;
static int cur_player_get_control_type(void) { return CONTROLLER_CONFIG_GALORE; }
static int cur_player_get_aim_control(void) { return 0; }
static int get_cur_playernum(void) { return 0; }
static int getPlayerCount(void) { return 1; }
static int lvlGetControlsLockedFlag(void) { return locked; }
static int disablePlayerActionsWhenPausedOrInMpMenu(void) { return actions_enabled; }
static int getCurrentPlayerWeaponId(int hand) { assert(hand==GUNRIGHT); return weapon; }
static int bondwalkItemCheckBitflags(int item, int flags)
{ assert(item==weapon && flags==WEAPONSTATBITFLAG_DISABLE_CROUCH); return scoped; }
''' + mapping + r'''
static struct MoveData decode(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons)
{
    struct MoveData moveData={0};
    s8 player_joyGetStickX, player_joyGetStickY;
    u16 player_joyGetButtons, copy_prev_buttons_pressed;
    s32 adjustedStickX, tmpc2sticky, sp10C, sp108, sp104, sp100;
''' + dual_body + r'''
    return moveData;
}
static Ge3dsControls controls;
static u16 oldbuttons;
static struct MoveData sample(Ge3dsControlSample sample)
{
    GePortInput logical=ge_3ds_controls_sample(&controls, &sample, true);
    GeOriginalInputSample native={0};
    GeOriginalBondInputFrame frame;
    native.move_x=logical.move_x; native.move_y=logical.move_y;
    native.look_x=logical.look_x; native.look_y=logical.look_y;
    native.buttons=ge_port_n64_buttons(&logical, &native.buttons_secondary);
    ge_original_input_tick(&native);
    ge_original_input_read_bond_frame(oldbuttons, &frame);
    oldbuttons=frame.buttons;
    return decode(frame.stick_x, frame.stick_y, frame.buttons, frame.oldbuttons);
}
static void release(void) { (void)sample((Ge3dsControlSample){0}); }
int main(void)
{
    Ge3dsControlSample pad={0};
    struct MoveData result;
    ge_original_input_init(); ge_original_input_set_dual_analog(1);
    ge_3ds_controls_init(&controls);
    pad.held=GE_PORT_ACTION_AIM; pad.look_x=156; pad.look_y=-156;
    result=sample(pad);
    assert(result.aimTurnRightSpeed==0 && result.speedVertaUp==0);
    assert(joyGetStickX(0)==60); /* Reproduce the unreachable pan threshold. */
    ge_original_input_set_full_stick_range(1);
    result=sample(pad);
    assert(joyGetStickX(0)==80 && joyGetStickY(0)==-80);
    assert(result.aimTurnRightSpeed==1 && result.speedVertaUp==1);
    pad.look_x=-156; pad.look_y=156;
    result=sample(pad);
    assert(result.aimTurnLeftSpeed==1 && result.speedVertaDown==1);
    release(); pad=(Ge3dsControlSample){0};
    pad.held=GE_PORT_ACTION_AIM; pad.move_y=-156;
    result=sample(pad); assert(result.crouchDown && !result.crouchUp);
    pad.move_y=156;
    result=sample(pad); assert(result.crouchUp && !result.crouchDown);
    scoped=1;
    result=sample(pad);
    assert(!result.crouchUp && result.zoomInFovPersec==2 && result.zooming);
    pad.move_y=-156;
    result=sample(pad);
    assert(!result.crouchDown && result.zoomOutFovPersec==2);
    scoped=0; release(); pad=(Ge3dsControlSample){0};
    pad.held=GE_PORT_ACTION_PREV_WEAPON;
    result=sample(pad);
    assert(result.weaponBackOffset && !result.weaponForwardOffset && !result.triggerOn);
    result=sample(pad); assert(!result.weaponBackOffset && !result.triggerOn);
    release(); pad.held=GE_PORT_ACTION_NEXT_WEAPON;
    result=sample(pad); assert(result.weaponForwardOffset && !result.triggerOn);
    release(); pad.held=GE_PORT_ACTION_FIRE;
    result=sample(pad); assert(result.triggerOn && !result.weaponBackOffset);
    release(); pad.held=GE_PORT_ACTION_NEXT_WEAPON|GE_PORT_ACTION_USE; weapon=ITEM_REMOTEMINE;
    result=sample(pad);
    assert(result.detonating && !result.btap && !result.weaponForwardOffset && !result.triggerOn);
    assert(!sample(pad).detonating); /* Held chord detonates once. */
    release(); weapon=0;
    result=sample(pad); assert(!result.detonating && result.btap);
    release(); pad=(Ge3dsControlSample){0}; pad.touching=true; pad.touch_x=220; pad.touch_y=150;
    pad.move_y=-156;
    result=sample(pad); assert(result.crouchDown && result.aiming);
    locked=1;
    result=sample(pad); assert(!result.crouchDown && !result.canManualAim);
    locked=0; actions_enabled=0;
    result=sample(pad); assert(!result.crouchDown && !result.canManualAim);
    ge_original_input_set_full_stick_range(0);
    puts("3DS canonical gestures: aim pan, crouch, scoped zoom, weapon chords, mine edges and locks passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="ge-control-gestures-") as directory:
    path=Path(directory)
    (path / "test.c").write_text(harness)
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-Wno-logical-not-parentheses", "-Wno-empty-body", "-Wno-unused-variable",
        "-Wno-unused-parameter", "-ffunction-sections", "-fdata-sections",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-I"+str(repo / "port/include"), "-I"+str(repo / "platform/3ds/include"),
        "-I"+str(repo / "src"), "-I"+str(repo), "-idirafter", str(repo / "include"), str(path / "test.c"),
        *[str(repo / item) for item in (
            "port/src/ge_services.c", "port/src/ge_libultra.c",
            "port/src/ge_original_input.c", "src/joy.c",
            "platform/3ds/source/ge_3ds_controls.c")],
        "-Wl,-dead_strip" if os.uname().sysname=="Darwin" else "-Wl,--gc-sections",
        "-lm", "-o", str(path / "test")
    ], check=True)
    subprocess.run([str(path / "test")], check=True,
        env={**os.environ, "ASAN_OPTIONS":"detect_leaks=0:halt_on_error=1",
             "UBSAN_OPTIONS":"halt_on_error=1"})

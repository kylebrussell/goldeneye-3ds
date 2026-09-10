#!/usr/bin/env python3
"""Probe target aim through actual logical mapping/joy and canonical HONEY.

This is controller dispatch evidence, not a complete MoveBond/AI mission run.
An old horizontal-output mutant must fail the same yaw/strafe assertion.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / "scripts"))
from extract_bond_input_live_state_slice import find_function


def block_end(source, start):
    first = source.index("{", start)
    depth = 0
    for end in range(first, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if not depth:
            return end + 1
    raise AssertionError("Unclosed canonical block")


main = (repo / "platform/3ds/source/main.c").read_text()
aim = find_function(main, "input_probe_apply_live_aim")
mapping = find_function((repo / "port/src/ge_port.c").read_text(), "ge_port_n64_buttons")
source = (repo / "src/game/bondview2.c").read_text()
function = source.index("void bondviewProcessInput(")
start = source.index("if (cur_player_get_control_type() == CONTROLLER_CONFIG_DOMINO", function)
dual_end = block_end(source, start)
single_start = source.index("else", dual_end)
single_end = block_end(source, single_start)
single = source[source.index("{", single_start):single_end]
initialization = source[source.index("    moveData.canSwivelGun = 0;", function):
    source.index("    if (g_CurrentPlayer->bonddead == FALSE", function)]
invert_start = source.index("if (moveData.invertPitch == 0)", single_end)
invert = source[invert_start:block_end(source, invert_start)]
turn_start = source.index("if (moveData.canNaturalTurn)", invert_start)
turn = source[turn_start:block_end(source, turn_start)]
pitch_start = source.index("else if (moveData.speedVertaDown > 0)", invert_start)
pitch_end = block_end(source, pitch_start)
pitch_end = block_end(source, source.index("else if (moveData.speedVertaUp > 0)", pitch_end))
pitch_end = block_end(source, source.index("else", pitch_end))
pitch = source[pitch_start:pitch_end].removeprefix("else ")
move_data = "struct MoveData {" + (repo / "src/bondtypes.h").read_text().split("struct MoveData {", 1)[1].split("};", 1)[0] + "};\n"
vertical_helpers = "\n".join(find_function(source, name) for name in (
    "sub_GAME_7F07FF74", "bondviewCurrentPlayerUpdateSpeedVerta"))

fov_define = next(line for line in (repo / "src/fr.h").read_text().splitlines()
    if line.startswith("#define FOV_Y_F "))
prefix = fov_define + r'''
#include "ge_port.h"
#include "ge_original_input.h"
#include "ge_original_player_spawn.h"
#include "ge_original_bond_input_provider.h"
#include <ultra64.h>
#undef MAXFLOAT
#include <bondconstants.h>
#include "joy.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GE_PORT_CSTICK_BUTTON_THRESHOLD 0.35f
#define FLOAT_TEN_B 10.0f
static struct {
    int insightaimmode, watch_animation_state, movecentrerelease;
    unsigned prev_buttons_pressed;
    float speedtheta, speedverta;
} player;
#define g_CurrentPlayer (&player)
static int g_PlayerIsInTank;
static float g_GlobalTimerDelta=1.0f;
static int cur_player_get_control_type(void)
{ return ge_original_bond_input_provider()->control_type; }
static int cur_player_get_aim_control(void)
{ return ge_original_bond_input_provider()->aim_control; }
static int get_cur_player_look_vertical_inverted(void)
{ return ge_original_bond_input_provider()->look_vertical_inverted; }
static int lvlGetControlsLockedFlag(void)
{ return ge_original_bond_input_provider()->controls_locked; }
static int disablePlayerActionsWhenPausedOrInMpMenu(void)
{ return ge_original_bond_input_provider()->player_actions_enabled; }
static int getCurrentPlayerWeaponId(int hand) { (void)hand; return ITEM_WPPKSIL; }
static int bondwalkItemCheckBitflags(int item,int flags)
{ (void)item; (void)flags; return 0; }
static float viGetFovY(void) { return 60.0f; }
static void require(int condition, const char *message)
{ if (!condition) { fprintf(stderr,"%s\n",message); exit(1); } }
''' + move_data + vertical_helpers + mapping

suffix = r'''
static struct MoveData decode(s8 stick_x,s8 stick_y,u16 buttons,u16 oldbuttons)
{
    struct MoveData moveData={0};
    u16 shootButtons,aimButtons,invButtons;
    f32 ftemp_nostack_sp8C,ftemp_nostack_sp84,ftemp_nostack_sp88;
''' + initialization + single + invert + turn + pitch + r'''
    return moveData;
}
static struct MoveData sample(const GePortInput *input)
{
    GeOriginalInputSample native={0};
    GeOriginalBondInputFrame frame;
    native.move_x=input->move_x; native.move_y=input->move_y;
    native.look_x=input->look_x; native.look_y=input->look_y;
    native.buttons=ge_port_n64_buttons(input,&native.buttons_secondary);
    ge_original_input_tick(&native);
    ge_original_input_read_bond_frame(0,&frame);
    player.speedtheta=player.speedverta=0;
    return decode(frame.stick_x,frame.stick_y,frame.buttons,frame.oldbuttons);
}
int main(void)
{
    GeOriginalPlayerViewState view={0};
    GePortInput input={0},before;
    struct MoveData decoded;
    float target[3];
    ge_original_input_init();
    ge_original_input_set_dual_analog(0);
    ge_original_input_set_full_stick_range(0);
    ge_original_bond_input_provider_reset_normal_dam();
    require(cur_player_get_control_type()==CONTROLLER_CONFIG_HONEY,"actual probe provider defaults to HONEY");
    require(get_cur_player_look_vertical_inverted()==0,"actual provider pitch default");
    view.initialized=1; view.camera_look[2]=-1;
    for (int sign=-1;sign<=1;sign+=2) {
        for (int offset=25;offset<=100;offset+=25) {
            input=(GePortInput){0};
            input.move_y=0.42f;
            input.held=GE_PORT_ACTION_FIRE;
            target[0]=(float)(sign*offset);target[1]=0;target[2]=-100;
            input_probe_apply_live_aim(&input,&view,target);
            decoded=sample(&input);
            require(decoded.canNaturalTurn && sign*player.speedtheta>0
                && !decoded.digitalStepLeft && !decoded.digitalStepRight,
                "HONEY yaw input must turn without strafing");
            require(sign*input.move_x>0 && input.look_x==0,"yaw stick polarity");
            require(fabsf(input.move_x)<=1,"yaw saturation");
            require(input.move_y==0.42f && joyGetStickY(0)==25
                && decoded.analogWalk==20,"navigation forward input preserved");
            require((joyGetButtons(0,ANY_BUTTON)&(L_CBUTTONS|R_CBUTTONS))==0,
                "probe aim never synthesizes horizontal C buttons");
            require(decoded.triggerOn && input.held==GE_PORT_ACTION_FIRE,
                "aim preserves firing action");
        }
    }
    target[0]=100;target[1]=0;target[2]=0;
    input=(GePortInput){0};
    input_probe_apply_live_aim(&input,&view,target);
    require(input.move_x==1.0f,"positive yaw clamps");
    target[0]=-100;
    input_probe_apply_live_aim(&input,&view,target);
    require(input.move_x==-1.0f,"negative yaw clamps");
    target[0]=0;target[2]=-100;
    input_probe_apply_live_aim(&input,&view,target);
    decoded=sample(&input);
    require(input.move_x==0 && player.speedtheta==0
        && !decoded.digitalStepLeft && !decoded.digitalStepRight,"aligned target holds yaw");
    /* The helper keeps its existing positive-up pitch error. Assert the
     * exact canonical dispatch under BOTH option settings; speedverta's
     * internal sign must not be confused with world-space camera look.y. */
    for (int invert=0;invert<=1;++invert) {
        ge_original_bond_input_provider()->look_vertical_inverted=(uint32_t)invert;
        for (int sign=-1;sign<=1;sign+=2) {
            target[1]=(float)(sign*100);
            input=(GePortInput){0};input.move_y=0.25f;
            input_probe_apply_live_aim(&input,&view,target);
            decoded=sample(&input);
            require(sign*input.look_y>0 && fabsf(input.look_y)<=1,"pitch target polarity/clamp");
            require(input.move_y==0.25f && input.look_x==0,"pitch preserves forward/yaw routing");
            require(sign*player.speedverta*(invert?1:-1)>0,"canonical pitch option dispatch");
            require((joyGetButtons(0,ANY_BUTTON)&(sign>0?U_CBUTTONS:D_CBUTTONS))!=0,
                "pitch uses original vertical C buttons");
            require(!decoded.digitalStepLeft&&!decoded.digitalStepRight,"pitch does not strafe");
        }
    }
    /* Existing view pitch is subtracted, rather than targeting absolute up. */
    view.camera_look[1]=1;view.camera_look[2]=-1;
    target[1]=100;target[2]=-100;
    input_probe_apply_live_aim(&input,&view,target);
    require(fabsf(input.look_y)<0.000001f,"equal current/target pitch holds");
    input=(GePortInput){0};input.move_x=0.7f;input.move_y=-0.4f;
    input.look_x=-0.6f;input.look_y=0.3f;
    input.held=GE_PORT_ACTION_FIRE;input.pressed=GE_PORT_ACTION_USE;
    before=input;
    input_probe_apply_live_aim(NULL,&view,target);
    input_probe_apply_live_aim(&input,NULL,target);
    require(memcmp(&input,&before,sizeof(input))==0,"null player preserves navigation");
    input_probe_apply_live_aim(&input,&view,NULL);
    require(memcmp(&input,&before,sizeof(input))==0,"missing guard target preserves navigation");
    view.initialized=0;
    input_probe_apply_live_aim(&input,&view,target);
    require(memcmp(&input,&before,sizeof(input))==0,"uninitialized player preserves navigation");
    view.initialized=1;
    input_probe_apply_live_aim(&input,&view,view.camera_position);
    require(memcmp(&input,&before,sizeof(input))==0,"degenerate target preserves navigation");
    memset(view.camera_look,0,sizeof(view.camera_look));
    input_probe_apply_live_aim(&input,&view,target);
    require(memcmp(&input,&before,sizeof(input))==0,"degenerate view preserves navigation");
    puts("Probe aim: canonical HONEY yaw/strafe/pitch dispatch and navigation guards passed");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="ge-probe-aim-") as directory:
    path=Path(directory)
    old_aim=aim.replace("input->move_x =", "input->look_x =",1)
    old_aim=old_aim.replace("    input->look_x = 0.0f;\n", "",1)
    assert old_aim!=aim,"horizontal producer mutation must match the current helper"
    for name, helper in (("current",aim),("old-horizontal-output",old_aim)):
        source_path=path/(name+".c")
        source_path.write_text(prefix+helper+suffix)
        subprocess.run([
            os.environ.get("CC","cc"),"-std=c11","-Wall","-Wextra","-Werror",
            "-Wno-logical-not-parentheses","-Wno-empty-body","-Wno-unused-variable",
            "-Wno-unused-parameter","-ffunction-sections","-fdata-sections",
            "-fsanitize=address,undefined","-fno-omit-frame-pointer",
            "-I"+str(repo/"port/include"),"-I"+str(repo/"src"),"-I"+str(repo),
            "-idirafter",str(repo/"include"),str(source_path),
            *[str(repo/item) for item in (
                "port/src/ge_services.c","port/src/ge_libultra.c",
                "port/src/ge_original_input.c","port/src/ge_original_bond_input_provider.c","src/joy.c")],
            "-Wl,-dead_strip" if os.uname().sysname=="Darwin" else "-Wl,--gc-sections",
            "-lm","-o",str(path/name)
        ],check=True)
        result=subprocess.run([str(path/name)],text=True,capture_output=True,
            env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0:halt_on_error=1",
                 "UBSAN_OPTIONS":"halt_on_error=1"})
        if name=="current":
            assert result.returncode==0,result.stdout+result.stderr
            print(result.stdout,end="")
        else:
            assert result.returncode==1,result.stdout+result.stderr
            assert "HONEY yaw input must turn without strafing" in result.stderr
            print("Previous look_x producer fails the same canonical yaw/strafe check, as required")

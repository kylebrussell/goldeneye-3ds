#!/usr/bin/env python3
"""Retain canonical match stop/reset and limit action order, without awards/UI."""
from pathlib import Path
import argparse
import hashlib
from extract_stage_pickup_slice import function_text
from extract_multiplayer_regen_slice import block


def render(repo):
    menu=(repo/'src/game/mpmenu.c').read_text()
    lv=(repo/'src/game/lv.c').read_text()
    out='''/* Generated canonical multiplayer session state. */
#include <ultra64.h>
#include <stdint.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/lv.h"
#include "game/mpmenu.h"
#include "ge_original_multiplayer_session.h"
extern s32 g_stopPlayFlag, g_gameOverFlag, g_pausedFlag;
extern s32 g_MpTime; /* lv.c timer slice owns this storage. */
s32 g_MpPoint = 0xA;
s32 chevron_glow;
s32 chevron_glow_timer;
s32 alt_gameover_msg;
s32 alt_gameover_msg_timer;
void set_missionstate(s32 state);
'''
    for source,names in ((menu,('mpwatchUnpauseGame','mpwatchSetStopPlayFlag','pauseAndLockControls')),
                         (lv,('lvlSetMpTime','lvlSetMpPoint'))):
        for name in names:
            body=function_text(source,name)
            out+='\n/* '+name+' sha256 '+hashlib.sha256(body.encode()).hexdigest()+' */\n'+body+'\n'
    awards=function_text(menu,'mpCalculateAwards')
    start=awards.index('    pauseAndLockControls();')
    end=awards.index('    prev_player_num =',start)
    out+='''
/* Exact state-only awards entry. Resource music/SFX and cosmetic award
 * calculation remain owned by the results presentation lifecycle. */
void ge_original_mp_session_results_exact(void)
{
    s32 i, player_count=getPlayerCount(), prev_player_num=get_cur_playernum();
    const bool gameoverdelay=FALSE;
'''
    out+='    '+awards[awards.index('set_missionstate('):awards.index('set_missionstate(')+len('set_missionstate(MISSION_STATE_0);')]+'\n'
    out+=awards[start:end]
    # Preserve the actual per-player menu publication prefix and current owner.
    menu_start=awards.index('        set_cur_player(i);')
    menu_end=awards.index('        bondinvGetWeaponOfChoice',menu_start)
    out+='    for (i=0; i<player_count; ++i)\n    {\n'+awards[menu_start:menu_end]+'    }\n    set_cur_player(prev_player_num);\n}\n'
    out+='/* mpCalculateAwards sha256 '+hashlib.sha256(awards.encode()).hexdigest()+' */\n'
    # Observe each unchanged lv.c terminal action. The source itself has no
    # cause field; retain all causes and their source order explicitly.
    time,_=block(lv,'if ((current_time < g_MpTime) && (sp180 >= g_MpTime))')
    point,_=block(lv,'if ((g_MpPoint > 0) && (g_ClockTimer != 0))')
    yolt,_=block(lv,'if ((get_scenario() == SCENARIO_YOLT) && (g_ClockTimer != 0))')
    # Native bounded totals avoid signed overflow while preserving decisions.
    yolt=yolt.replace('s32 killed_count;', 'int64_t killed_count;')
    out+='''
void ge_original_mp_session_decide_exact(s32 g_ClockTimer,s32 current_time,
    s32 g_MpTime,s32 g_MpPoint,uint32_t *results,uint32_t *freeze)
{
    s32 sp180=current_time+g_ClockTimer;
    *results=*freeze=0;
'''
    for cause,body in (('TIME',time),('POINT',point),('ELIMINATION',yolt)):
        out+='#define mpCalculateAwards(delay) (*results |= GE_ORIGINAL_MP_SESSION_CAUSE_'+cause+')\n'
        out+='#define mpwatchSetStopPlayFlag() (*freeze |= GE_ORIGINAL_MP_SESSION_CAUSE_'+cause+')\n'
        if cause=='TIME':out+='    if (g_MpTime > 0)\n'
        out+=body+'\n#undef mpCalculateAwards\n#undef mpwatchSetStopPlayFlag\n'
    out+='}\n'
    return out

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(render(a.repo))

#!/usr/bin/env python3
"""Retain native multiplayer menu and full RNG-consuming awards path."""
from pathlib import Path
import argparse
import hashlib
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import extract_data

def render(root):
    menu=(root/'src/game/mpmenu.c').read_text()
    front=(root/'src/game/front.c').read_text()
    out='''/* Generated native multiplayer watch and complete awards. */
#include <ultra64.h>
#include <music.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <joy.h>
#include <random.h>
#include <snd.h>
#include "game/player.h"
#include "game/bondinv.h"
#include "game/gun.h"
#include "game/front.h"
#include "game/file.h"
#include "game/lv.h"
#include "game/mpmenu.h"
#include "game/mp_music.h"
#include "game/language.h"
#include "game/frametiming.h"
#include "boss.h"
#include "assets/obseg/text/LmpmenuE.h"
extern s32 g_pausedFlag;
u8 *ge_original_mp_watch_language(s32 id);
#define langGet ge_original_mp_watch_language
void pauseAndLockControls(void);
void ge_original_mp_watch_quit_exact(bool delay);
void ge_original_mp_watch_publish_award(s32 player,s32 award,const char *text);
'''
    for name in ('who_paused','chevron_glow','chevron_glow_timer','alt_gameover_msg','alt_gameover_msg_timer','g_AwardNames'):
        out+=extract_data(menu,name)+'\n'
    for name in ('gamemode','selected_num_players','array_favweapon'):
        out+=extract_data(front,name)+'\n'
    for file,names in (('front.c',('get_selected_num_players',)),
                       ('file.c',('store_favorite_weapon_current_player',)),
                       ('bondinv.c',('bondinvGetWeaponOfChoice',)),
                       ('gunfire.c',('get_curplayer_shot_register',))):
        source=(root/'src/game'/file).read_text()
        for name in names:
            body=function_text(source,name)
            # Original caller ignores this decomp-declared return value.
            if name=='bondinvGetWeaponOfChoice':body=body[:-1]+'    return 0;\n}'
            out+=body+'\n'
    for name in ('mpFindMaxInt','mpFindMinInt','mpFindMaxFloat','mpFindMinFloat',
                 'mpwatchMenuCanGoRight','mpwatchMenuCanGoLeft','mpwatchIsPlayerPressingRight',
                 'mpwatchIsPlayerPressingLeft','mpwatchPlayBeep','mpCalculateAwards','mpwatchMenuTick'):
        body=function_text(menu,name)
        out+='\n/* '+name+' source sha256 '+hashlib.sha256(body.encode()).hexdigest()+' */\n'
        if name=='mpCalculateAwards':
            # All active metrics are overwritten. Original helpers ignore
            # inactive arguments; initialize scratch to avoid undefined C
            # reads while evaluating the unused third/fourth arguments.
            body=body.replace('struct AwardMetrics metrics[4];','struct AwardMetrics metrics[4] = {{0}};')
            for which,label in enumerate(('first','second')):
                original=f'g_playerPointers[i]->ptr_text_{label}_mp_award = langGet(g_AwardNames[awardindex]);'
                assert original in body
                body=body.replace(original,f'ge_original_mp_watch_publish_award(i, {which}, (const char *)langGet(g_AwardNames[awardindex]));')
        if name=='mpwatchMenuTick':out+='#define mpCalculateAwards ge_original_mp_watch_quit_exact\n'
        out+=body+'\n'
        if name=='mpwatchMenuTick':out+='#undef mpCalculateAwards\n'
    return out
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(render(a.root))

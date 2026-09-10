#!/usr/bin/env python3
"""Exact native watch fixture dependencies; audio sink is supplied by test."""
from pathlib import Path
import argparse
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import extract_data

def render(root):
    player=(root/'src/game/player.c').read_text()
    music=(root/'src/game/mp_music.c').read_text()
    out='''#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <music.h>
#include <random.h>
#include "game/player.h"
#include "game/mp_music.h"
#include "game/options.h"
#include "game/music_0D2720.h"
'''
    out+=extract_data(player,'array_PLAYER_IDs')+'\n'
    for name in ('get_player_position_in_shuffled','get_nth_player_from_shuffled','shuffle_player_ids'):
        out+=function_text(player,name)+'\n'
    for name in ('stageMusicID','mission_state'):
        out+=extract_data(music,name)+'\n'
    for name in ('sub_GAME_7F0C0BF0','sub_GAME_7F0C0C10','set_missionstate','sub_GAME_7F0C11FC'):
        body=function_text(music,name)
        if name=='sub_GAME_7F0C0BF0':body=body.replace('    get_mTrack2Vol();','    return get_mTrack2Vol();')
        out+=body+'\n'
    # Independent direct-source awards invocation for adapter equivalence.
    menu=(root/'src/game/mpmenu.c').read_text()
    out+='\n#include "game/mpmenu.h"\n#include "game/bondinv.h"\n#include "game/gun.h"\n#include "game/file.h"\n#include "game/language.h"\n#include <random.h>\n#include <snd.h>\n'
    out+='extern s32 g_gameOverFlag,alt_gameover_msg,alt_gameover_msg_timer,chevron_glow,chevron_glow_timer;\nextern u16 g_AwardNames[];\nvoid pauseAndLockControls(void);\nvoid ge_original_mp_watch_publish_award(s32,s32,const char *);\n'
    for helper in ('mpFindMaxInt','mpFindMinInt','mpFindMaxFloat','mpFindMinFloat'):
        out+=function_text(menu,helper).split('{',1)[0].rstrip()+';\n'
    body=function_text(menu,'mpCalculateAwards').replace('mpCalculateAwards(', 'fixture_reference_awards(')
    body=body.replace('struct AwardMetrics metrics[4];','struct AwardMetrics metrics[4] = {{0}};')
    for i,label in enumerate(('first','second')):
        body=body.replace(f'g_playerPointers[i]->ptr_text_{label}_mp_award = langGet(g_AwardNames[awardindex]);',f'ge_original_mp_watch_publish_award(i, {i}, (const char *)langGet(g_AwardNames[awardindex]));')
    out+=body+'\n'
    return out
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(render(a.root))

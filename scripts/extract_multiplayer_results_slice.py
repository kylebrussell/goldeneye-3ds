#!/usr/bin/env python3
"""Retain exact original ranking and award kill/death metrics for snapshots."""
from pathlib import Path
import argparse
import hashlib
import re
from extract_stage_pickup_slice import function_text
from extract_multiplayer_regen_slice import block


def render(repo):
    source=(repo/'src/game/mpmenu.c').read_text()
    rank=function_text(source,'write_playerrank_to_buffer')
    original=rank
    rank=rank.replace('void write_playerrank_to_buffer(char *buffer, s32 playernum)',
                      's32 ge_original_mp_result_rank_exact(s32 playernum)')
    for n, label in enumerate(('11_RANK1ST','12_RANK2ND','13_RANK3RD','14_RANK4TH'),1):
        rank=re.sub(r'sprintf\(buffer, langGet\(getStringID\(LMPMENU, MPMENU_STR_'+label+r'\)\)\);',
                    f'return {n};',rank)
    if 'sprintf' in rank or 'buffer' in rank:
        raise ValueError('unconverted ranking presentation')
    rank=rank[:-1]+'    return 0;\n}'
    awards=function_text(source,'mpCalculateAwards')
    loop,_=block(awards,'for (j = 0; j < get_selected_num_players(); j++)')
    return '''/* Generated canonical mpmenu.c rank and award metrics. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/mpmenu.h"
''' + rank + '\n/* ranking sha256 '+hashlib.sha256(original.encode()).hexdigest()+' */\n'+'''
void ge_original_mp_result_counts_exact(s32 i,s32 player_count,s32 *kills,s32 *deaths)
{
    struct AwardMetrics metrics[4] = {0};
    s32 j;
#define get_selected_num_players() player_count
''' + loop + '''
#undef get_selected_num_players
    *kills=metrics[i].num_kills;
    *deaths=metrics[i].num_deaths;
}
''' + '/* award counts sha256 '+hashlib.sha256(loop.encode()).hexdigest()+' */\n'


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--reference',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(render(a.repo))
    if a.reference:
        source=(a.repo/'src/game/mpmenu.c').read_text()
        a.reference.write_text('''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <stdio.h>
#include "game/player.h"
#include "game/front.h"
#include "game/mpmenu.h"
#include "game/language.h"
#include "assets/obseg/text/LmpmenuE.h"
'''+function_text(source,'write_playerrank_to_buffer'))

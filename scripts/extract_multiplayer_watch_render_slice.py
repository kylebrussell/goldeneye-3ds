#!/usr/bin/env python3
"""Retain native multiplayer menu presentation and exact text measurement."""
from pathlib import Path
import argparse,hashlib
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import extract_data

def render(root):
    source=(root/'src/game/mpmenu.c').read_text()
    out='''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <stdio.h>
#include "game/player.h"
#include "game/front.h"
#include "game/mpmenu.h"
#include "game/textrelated.h"
#include "ge_original_multiplayer_watch_render.h"
#include "assets/obseg/text/LmpmenuE.h"
#define MPMENU_YOFF 0
#define SPACE_WIDTH 5
extern s32 g_gameOverFlag,g_stopPlayFlag,g_pausedFlag,who_paused,chevron_glow,alt_gameover_msg,j_text_trigger;
s32 mpwatchMenuCanGoLeft(void);
s32 mpwatchMenuCanGoRight(void);
struct font *ge_original_mp_watch_render_font(void);
s16 ge_original_mp_watch_render_vi(unsigned);
s32 ge_original_mp_watch_render_spacing(void);
char *ge_original_mp_watch_render_language(s32);
char *ge_original_mp_watch_render_award(unsigned);
char *ge_original_mp_watch_render_favorite(int,int);
void ge_original_mp_watch_render_measure(s32*,s32*,char*,struct fontchar*,struct font*,s32);
Gfx *ge_original_mp_watch_render_text(Gfx*,s32*,s32*,char*,struct fontchar*,struct font*,u32,s32,s32,u32,s32);
Gfx *ge_original_mp_watch_render_outlined(Gfx*,s32*,s32*,char*,struct fontchar*,struct font*,u32,u32,s32,s32,s32,s32);
Gfx *ge_original_mp_watch_render_state(Gfx*,unsigned);
#define text_spacing ge_original_mp_watch_render_spacing()
'''
    body=function_text((root/'src/game/textrelated.c').read_text(),'textMeasure')
    out+=body.replace('void textMeasure(', 'void ge_original_mp_watch_text_measure_exact(')+'\n'
    out+='''#define textMeasure ge_original_mp_watch_render_measure
#define textRender ge_original_mp_watch_render_text
#define textRenderOutlined ge_original_mp_watch_render_outlined
#define langGet ge_original_mp_watch_render_language
#define ptrFontBankGothic ge_original_mp_watch_render_font()
#define ptrFontBankGothicChars (ge_original_mp_watch_render_font()->chars)
#define viGetX() ge_original_mp_watch_render_vi(0)
#define viGetY() ge_original_mp_watch_render_vi(1)
#define viGetViewLeft() ge_original_mp_watch_render_vi(2)
#define viGetViewTop() ge_original_mp_watch_render_vi(3)
#define viGetViewWidth() ge_original_mp_watch_render_vi(4)
#define viGetViewHeight() ge_original_mp_watch_render_vi(5)
#define microcode_constructor(gdl) ge_original_mp_watch_render_state(gdl,0)
#define combiner_bayer_lod_perspective(gdl) ge_original_mp_watch_render_state(gdl,1)
#define bgScissorCurrentPlayerViewDefault(gdl) ge_original_mp_watch_render_state(gdl,2)
#define frontGetPlayersFavoriteWeaponInHand ge_original_mp_watch_render_favorite
'''
    for name in ('ascii_MP_watch_menu_BLANK','ascii_MP_watch_menu_left_chevron','ascii_MP_watch_menu_right_chevron','ascii_pnum_KILLS','ascii_pnum_LOSSES'):
        out+=extract_data(source,name)+'\n'
    for name in ('display_text_for_playerdata_on_MP_menu','write_playerrank_to_buffer','mpwatchShouldDisplayRank','mpwatchShouldDisplayScore','mp_watch_menu_display'):
        body=function_text(source,name)
        out+='/* '+name+' sha256 '+hashlib.sha256(body.encode()).hexdigest()+' */\n'
        if name=='display_text_for_playerdata_on_MP_menu':
            assert 'u16 *text;' in body
            body=body.replace('u16 *text;','char text[32];').replace('sprintf(&text,','snprintf(text, sizeof(text),').replace('&text,','text,')
        if name=='write_playerrank_to_buffer':
            # Source rank strings have no format arguments.
            body=body.replace('sprintf(buffer, langGet(', 'snprintf(buffer, GE_ORIGINAL_MP_WATCH_TEXT_CAPACITY, "%s", langGet(')
        if name=='mp_watch_menu_display':
            body=body.replace('char *text;', 'char *text = NULL;')
            body=body.replace('char rankbuffer[4];','char rankbuffer[GE_ORIGINAL_MP_WATCH_TEXT_CAPACITY];\n    const char *label_prefix,*label_kind;')
            body=body.replace('q = (s32) langGet(', 'label_prefix = langGet(').replace('h2 = (s32) langGet(', 'label_kind = langGet(')
            body=body.replace('sprintf(rankbuffer, ascii_pnum_', 'snprintf(rankbuffer, sizeof(rankbuffer), ascii_pnum_').replace('(char *) q, curplayernum + 1, (char *) h2','label_prefix, curplayernum + 1, label_kind')
            # Decomp-only register-liveness expression reads uninitialized
            # colour for the score-suppressed YOLT/TLD pages, with no effect.
            body=body.replace('            if (colour);','')
            for n,word in enumerate(('first','second')):
                body=body.replace('(char *) g_CurrentPlayer->ptr_text_'+word+'_mp_award',f'ge_original_mp_watch_render_award({n})')
            body=body.replace('Gfx *mp_watch_menu_display(', 'Gfx *ge_original_mp_watch_menu_display_exact(')
        out+=body+'\n'
    return out
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(render(a.root))

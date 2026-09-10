#!/usr/bin/env python3
"""Retain CPU state and RNG from the original BG prop-render boundary.

GPU commands, temporary vertices, colour/scissor setup and destructive N64
float-to-fixed matrix output are presentation boundaries. Model relations,
sorted hit-list traversal/retirement, monitor controllers, impact flags and
every original render-side random call remain in source order.
"""
from pathlib import Path
import argparse
import hashlib
import re
from extract_bond_move_explosion_slice import extract_function, extract_data
from extract_dam_monitor_animation_slice import extract_braced


def cut(text, start, end, replacement=''):
    a = text.index(start)
    b = text.index(end, a)
    return text[:a] + replacement + text[b:]


def remove_block(text, marker):
    start = text.index(marker)
    block = extract_braced(text, start)
    return text[:start] + text[start+len(block):]


def generate(repo):
    model = (repo/'src/game/model.c').read_text()
    obj = (repo/'src/game/propobj.c').read_text()
    chr = (repo/'src/game/chr.c').read_text()
    handler = (repo/'src/game/objecthandler.c').read_text()
    names = ('drawjointlist','subdraw','sub_GAME_7F074534','chrRenderProp',
             'chrobjRenderProp','sub_GAME_7F04AC20','dogfnegx','dorottex',
             'process_monitor_animation_microcode')
    pieces = ['''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <stdint.h>
#include "game/model.h"
#include "game/objecthandler.h"
#include "game/propobj.h"
#include "game/bgfog.h"
#include "game/player.h"
#include "random.h"
#include "ge_original_effect_runtime.h"
extern f32 g_GlobalTimerDelta;
extern s32 g_ClockTimer;
extern ModelNode *sub_GAME_7F04B478(ObjectRecord *);
#ifndef M_U16_MAX_VALUE_F
#define M_U16_MAX_VALUE_F 65536.0f
#endif
extern void ge_original_dam_monitor_save_command_list(MonitorRecord *,u32 *);
extern void ge_original_dam_monitor_save_image_index(MonitorRecord *,void *);
static void *ge_render_context;
static void (*ge_render_flash)(void *,Model *,ModelNode *,u32,u32);
#define MONITOR_TIMER_DELTA g_GlobalTimerDelta
#define save_ptr_monitor_ani_code_to_obj_ani_slot ge_original_dam_monitor_save_command_list
#define save_img_index_to_obj_ani_slot(screen,index) ge_original_dam_monitor_save_image_index(screen,(void *)(uintptr_t)(u32)(index))
''']
    for name in names:
        pieces.append('#define '+name+' ge_render_'+name)
    pieces += ['static void sub_GAME_7F074534(ModelRenderData *,Model *,ModelNode *);',
               'static void subdraw(ModelRenderData *,Model *);',
               extract_data(chr,'D_8002CCBC').replace('D_8002CCBC','ge_render_chr_data'),
               extract_data(obj,'D_80031FD0').replace('D_80031FD0','ge_render_obj_data')]

    # Model gunfire output consumes only these two source expressions. The
    # Image gate is essential: an untextured flash takes only the scale sample.
    dog = extract_function(model,'dogfnegx')
    assert dog.count('randomGetNext()') == 2, 'audit changed gunfire RNG source'
    gate = dog[dog.index('    if ((renderdata->flags'):dog.index('        s32 index')]
    scale = re.search(r'        scale = [^;]+;',dog).group()
    angle = re.search(r'            sp62 = [^;]+;',dog).group()
    pieces.append('''static void dogfnegx(ModelRenderData *renderdata,Model *model,ModelNode *node)
{
    ModelRoData_GunfireRecord *rodata=&node->Data->Gunfire;
    union ModelRwData *rwdata=modelGetNodeRwData(model,node);
    u32 scale_random=0,angle_random=0;
    f32 scale;u16 sp62;
'''+gate+scale.replace('randomGetNext()', '(scale_random=randomGetNext())')+'''
        if (rodata->Image)
        {
'''+angle.replace('randomGetNext()', '(angle_random=randomGetNext())')+'''
        }
        if(ge_render_flash)ge_render_flash(ge_render_context,model,node,scale_random,angle_random);
    }
}
''')
    rot = extract_function(model,'dorottex')
    assert rot.count('randomGetNext()') == 3, 'audit changed primary-node RNG source'
    prefix = rot[:rot.index('            Vertex *src;')]
    loop = rot[rot.index('            for (i = 0;'):rot.index('                s32 corner1')]
    # Only local output arithmetic follows these original three samples.
    loop = re.sub(r'                s32 s[43] = [^;]+;\n','',loop)
    pieces.append(prefix+loop+'            }\n        }\n    }\n}\n')

    dispatch = extract_function(model,'sub_GAME_7F074534')
    # All these callees either return immediately or emit only GPU/temporary
    # vertices. Relations and both RNG-producing nodes remain actual calls.
    output = ('sub_GAME_7F0737FC','doshadow','sub_GAME_7F074514',
              'sub_GAME_7F074524','modelRenderNodeGundl','modelRenderNodeDl',
              'sub_GAME_7F072C10','sub_GAME_7F07306C','dotube','sub_GAME_7F0737EC')
    for name in output:
        dispatch = re.sub(r'        '+name+r'\([^;]*;', '        (void)0;', dispatch)
    pieces.append(dispatch)
    joint = extract_function(handler, 'drawjointlist')
    joint = remove_block(joint, '    if (data->gdl == NULL)')
    joint = remove_block(joint, '        if (entry->model->obj == NULL)')
    joint = remove_block(joint, '        if (!entry->model->obj->isLoaded)')
    joint = re.sub(r'            gdl = data->gdl\+\+;\n\s*gSPSegment\([^;]*;', '(void)0;', joint)
    pieces.append(joint)
    subdraw = extract_function(model,'subdraw')
    subdraw = cut(subdraw,'    #if defined(LEFTOVERDEBUG)','    while (root != NULL)')
    pieces.append(subdraw)

    # Reuse the existing relocated canonical monitor command/image owners.
    pieces.append(extract_braced(obj,obj.index('struct tvcmd {')))
    monitor = extract_function(obj,'process_monitor_animation_microcode')
    assert monitor.count('randomGetNext()') == 1, 'audit changed monitor RNG source'
    monitor = monitor[:monitor.index('        // Set up everything for rendering')]
    monitor = re.sub(r'        (?:Vertex \*vertices|Gfx \*savedgdl|union ModelRoData \*rodata|union ModelRwData \*rwdata|sImageTableEntry \*tconfig)[^;]*;\n','',monitor)
    pieces.append(monitor+'    }\n    return gdl;\n}\n')

    child = extract_function(obj,'sub_GAME_7F04AC20')
    child = cut(child,'        destroyed =','        if (obj->type == PROPDEF_MONITOR)')
    child = cut(child,'        if (obj->type == PROPDEF_DOOR)','        subdraw(mrData, model);',
                '        if(obj->type != PROPDEF_DOOR) (void)sub_GAME_7F04B478(obj);\n')
    child = cut(child,'        gdl = mrData->gdl;','        if (obj->state & (1 << arg2))')
    child = cut(child,'        if (destroyed)','        for (child = prop->child;')
    child = remove_block(child,'        if (arg2)')
    child = child.replace('gdl = explosionRenderBulletImpactOnProp(gdl, prop, arg2);',
                          'ge_original_effect_impact_semantic_exact(prop, arg2);')
    pieces.append(child)
    prop = extract_function(obj,'chrobjRenderProp')
    prop = cut(prop,'    if ((getPropCombinedRoomsBBox2D','    sub_GAME_7F04AC20(prop, &mrData, arg2);',
               '    mrData.flags = sp44;\n')
    pieces.append(prop.replace('D_80031FD0','ge_render_obj_data'))
    character = extract_function(chr,'chrRenderProp')
    character = cut(character,'            if ((getPropCombinedRoomsBBox2D','            g_playerPerm->time_other_players_on_screen',
                    '            mrData.flags = spB8;\n')
    character = remove_block(character,'            if (withalpha != 0)')
    character = re.sub(r'gdl = explosionRenderBulletImpactOnProp\(gdl, ([^,]+), withalpha\);',
                       r'ge_original_effect_impact_semantic_exact(\1, withalpha);',character)
    pieces.append(character.replace('D_8002CCBC','ge_render_chr_data'))
    pieces.append('''int ge_original_prop_render_semantic_exact(void *context,void *native_prop,int alpha,
    void (*flash)(void *,Model *,ModelNode *,u32,u32))
{
    PropRecord *prop=native_prop;
    if(!prop)return 0;
    ge_render_context=context;ge_render_flash=flash;
    switch(prop->type) {
    case PROP_TYPE_CHR: (void)chrRenderProp(prop,NULL,alpha);break;
    case PROP_TYPE_VIEWER: if(prop->chr)(void)chrRenderProp(prop,NULL,alpha);break;
    case PROP_TYPE_OBJ: case PROP_TYPE_WEAPON: case PROP_TYPE_DOOR:
        (void)chrobjRenderProp(prop,NULL,alpha);break;
    default:break;
    }
    ge_render_flash=NULL;ge_render_context=NULL;return 1;
}
''')
    body='\n\n'.join(pieces)
    return '/* Original render CPU semantics SHA256 '+hashlib.sha256(body.encode()).hexdigest()+' */\n'+body


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('repo',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();args.output.write_text(generate(args.repo))

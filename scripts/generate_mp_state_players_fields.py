#!/usr/bin/env python3
"""Generate explicit typed E6 accessors and a complete native member inventory.

The US player declaration is mechanically inventoried. Model/display-list
storage aliases are reviewed source boundaries, not integer payloads.
"""
from pathlib import Path
import argparse, hashlib, json, re

EXTERNAL_E2 = {'ammoheldarr','ptr_inventory_first_in_cycle','p_itemcur','equipmaxitems',
    'equipallguns','equipcuritem','textoverrides','gunheldarr','deathcount','num_suicides',
    'last_kill_time','lifestarttime60','kills_this_life'}
OVERLAYS = (
    ('something_with_watch_object_instance','buttons_pressed','WATCH_MODEL'),
    ('model','bondheadmatrices','GAIT_MODEL'),
    ('watch_body_armor_bar_gdl','watch_health_bar_gdl','DISPLAY_LIST'),
    ('watch_health_bar_gdl','buffer_for_watch_greenbackdrop_DL','DISPLAY_LIST'),
    ('buffer_for_watch_greenbackdrop_DL','buffer_for_watch_static_DL','DISPLAY_LIST'),
    ('buffer_for_watch_static_DL','healthdamagetype','DISPLAY_LIST'),
)
LEGACY_REFS={'field_5C':'FIXED_MATRIX','field_60':'FIXED_MATRIX',
    'field_64':'FLOAT_MATRIX','field_68':'FLOAT_MATRIX','field_10D0':'UNKNOWN',
    'field_10E0':'FIXED_MATRIX','field_10E4':'LOOKAT',
    'players_cur_animation':'ANIMATION'}
SCALARS={'s32':'I32','f32':'F32','bool':'I32','s16':'I16','s8':'I8','u8':'U8','u16':'U16','u32':'U32','ITEM_IDS':'I32'}
POINTERS={'StandTile *':'STAN','struct StandTile *':'STAN','PropRecord*':'PROP',
    'struct PropRecord *':'PROP','struct Model *':'MODEL','Mtx*':'FIXED_MATRIX',
    'Mtxf*':'FLOAT_MATRIX','u8 *':'IMAGE','ModelFileHeader *':'HAND_BUFFER'}

def generate(root):
    source=(root/'src/game/bondview.h').read_text()
    source=source[source.index('struct player\n{'):source.index('\nstruct firing_anim_struct')]
    clean=re.sub(r'/\*.*?\*/|//[^\n]*','',source,flags=re.S)
    clean=re.sub(r'#if defined\(VERSION_JP\).*?#else\n(.*?)#endif',r'\1',clean,flags=re.S)
    declarations=[]
    for line in clean.splitlines()[2:]:
        line=line.strip()
        if not line or line in ('{','};'):continue
        match=re.fullmatch(r'(.+?)\s*([a-zA-Z_][a-zA-Z_0-9]*)(\[[^]]+\])?;',line)
        if not match:raise ValueError('unclassified declaration: '+line)
        typ,name,dim=match.groups();count=int(dim[1:-1]) if dim and dim[1:-1].isdigit() else 5 if dim=='[WATCH_NUMBER_SCREENS]' else 1
        if dim and count==1 and dim!='[1]':raise ValueError(dim)
        declarations.append((typ.strip(),name,count,bool(dim)))
    names=[x[1] for x in declarations]
    assert len(names)==len(set(names))
    overlay_members={}
    for first,last,kind in OVERLAYS:
        for name in names[names.index(first):names.index(last)]:overlay_members[name]=(first,last,kind)
    fields=[];copies=[];inventory=[];offsets={x:0 for x in set(SCALARS.values())};refindex=0
    def scalar(path,typ,expressions):
        ident=256+len(fields);index=offsets[typ];offsets[typ]+=len(expressions)
        fields.append((ident,typ,len(expressions),path,typ.lower(),index,0))
        copies.extend(f'P_COPY({typ}, {index+i}, {e});' for i,e in enumerate(expressions))
    def ref(path,kind,expressions,extent='0',integer=False):
        nonlocal refindex
        ident=256+len(fields);index=refindex;refindex+=len(expressions)
        fields.append((ident,'ACTOR_REF',len(expressions),path,'refs',index,1))
        fields.append((ident+1,'U64',len(expressions),path+'.offset','offsets',index,1))
        for i,e in enumerate(expressions):
            pointer=f'(const void *)(uintptr_t)(uint32_t)({e})' if integer else f'(const void *)({e})'
            copies.append(f'P_REF({ident}, {index+i}, {kind}, {pointer}, {extent});')
    for typ,name,count,array in declarations:
        item={'member':name,'native_type':typ,'count':count}
        if name in overlay_members:
            first,last,kind=overlay_members[name];item.update(classification='external overlay owner',owner=kind,owner_path=first)
            if name==first:
                ref(name,kind,[f'&p->{name}'],f'offsetof(struct player,{last})-offsetof(struct player,{first})')
        elif name in EXTERNAL_E2:item.update(classification='external E2 inventory owner',owner_path='inventory_owner')
        elif name=='hands':
            item.update(classification='external E4 physical hand owner');ref(name,'HAND',[f'&p->hands[{i}]' for i in range(2)],'sizeof(struct hand)')
        else:
            expr=[f'p->{name}[{i}]' if array else f'p->{name}' for i in range(count)]
            item['classification']='typed native values'
            if name in LEGACY_REFS:
                item['classification']='legacy 32-bit '+LEGACY_REFS[name].lower()+' reference';ref(name,LEGACY_REFS[name],expr,integer=True)
            elif name.startswith('ptr_text_'):
                item['classification']='legacy 32-bit text reference';ref(name,'TEXT',expr,integer=True)
            elif typ in SCALARS:scalar(name,SCALARS[typ],expr)
            elif typ in POINTERS:ref(name,POINTERS[typ],expr)
            elif typ in ('coord3d','vec3d','struct coord3d'):scalar(name,'F32',[f'{e}.f[{j}]' for e in expr for j in range(3)])
            elif typ=='coord2d':scalar(name,'F32',[f'{e}.{j}' for e in expr for j in ('x','y')])
            elif typ=='struct rect4f':scalar(name,'F32',[f'{e}.f[{j}]' for e in expr for j in range(8)])
            elif typ=='struct bbox2d':scalar(name,'F32',[f'{e}.f[{j}][{k}]' for e in expr for j in range(2) for k in range(2)])
            elif typ=='Mtxf':scalar(name,'F32',[f'{e}.m[{j}][{k}]' for e in expr for j in range(4) for k in range(4)])
            elif typ=='Vp':
                for member in ('vscale','vtrans'):scalar(name+'.'+member,'I16',[f'{e}.vp.{member}[{j}]' for e in expr for j in range(4)])
            elif typ=='struct rgba_u8':scalar(name,'U8',[f'{e}.rgba[{j}]' for e in expr for j in range(4)])
            elif typ=='struct collision434':
                for member in ('current_tile_ptr','current_tile_ptr_for_portals'):ref(name+'.'+member,'STAN',[f'{e}.{member}' for e in expr])
                for member in ('collision_position','theta_transform','pos3','pos','applied_view','applied_view2'):scalar(name+'.'+member,'F32',[f'{e}.{member}.f[{j}]' for e in expr for j in range(3)])
                scalar(name+'.collision_radius','F32',[f'{e}.collision_radius' for e in expr])
            elif typ=='ModelFileHeader':
                for member,kind in (('RootNode','MODEL_NODE'),('Skeleton','SKELETON'),('Switches','MODEL_SWITCHES'),('Textures','MODEL_TEXTURES')):ref(name+'.'+member,kind,[f'{e}.{member}' for e in expr])
                for member in ('numSwitches','numMatrices','numRecords','numtextures'):scalar(name+'.'+member,'I16',[f'{e}.{member}' for e in expr])
                scalar(name+'.BoundingVolumeRadius','F32',[f'{e}.BoundingVolumeRadius' for e in expr]);scalar(name+'.isLoaded','I32',[f'{e}.isLoaded' for e in expr])
            elif typ=='struct texpool':
                for member in ('start','end','leftpos','rightpos'):ref(name+'.'+member,'TEXTURE_POOL',[f'{e}.{member}' for e in expr])
            elif typ=='struct damage_display_parent':
                for member in ('pos','normal'):scalar(name+'.'+member,'I16',[f'{e}.items[{j}].{member}.AsArray[{k}]' for e in expr for j in range(2) for k in range(3)])
                scalar(name+'.colour','U8',[f'{e}.items[{j}].colour.rgba[{k}]' for e in expr for j in range(2) for k in range(4)])
            elif typ=='struct WatchRectangle':
                for member in ('coord1','coord2'):scalar(name+'.'+member,'I16',[f'{e}.vtx[{j}].{member}.AsArray[{k}]' for e in expr for j in range(4) for k in range(3)])
                scalar(name+'.color','U8',[f'{e}.vtx[{j}].color.rgba[{k}]' for e in expr for j in range(4) for k in range(4)])
            else:raise ValueError('unclassified native type '+typ+' '+name)
        inventory.append(item)
    scalar('active_player','U8',['storage.active_player'])
    ref('inventory_owner','INVENTORY_OWNER',['p'])
    header=['/* Generated by scripts/generate_mp_state_players_fields.py; typed fields only. */',
        f'#define GE_MP_PLAYER_FIELD_COUNT {len(fields)}U',f'#define GE_MP_PLAYER_REF_COUNT {refindex}U']
    header += [f'#define GE_MP_PLAYER_{typ}_COUNT {offsets[typ]}U' for typ in sorted(offsets)]
    header += ['#define GE_MP_PLAYER_FIELDS(X) \\']
    for i,(ident,typ,count,path,storage,index,refkind) in enumerate(fields):
        header.append(f' X({ident}, {typ}, {count}, "{path}", {storage}, {index}, {refkind})'+(' \\' if i<len(fields)-1 else ''))
    metadata={'version':1,'source':'src/game/bondview.h: struct player','declaration_sha256':hashlib.sha256(source.encode()).hexdigest(),
        'native_members':len(inventory),'record_fields':len(fields),'reference_values':refindex,'members':inventory}
    scalar_copies=[c.replace('storage.active_player','active_player') for c in copies if c.startswith('P_COPY(')]
    copy_functions=['/* Generated explicit typed member access; never native blob reads. */','#ifdef GE_MP_PLAYER_COPY_FUNCTIONS']
    parts=[]
    for index in range(0,len(scalar_copies),128):
        name='ge_player_copy_part_'+str(index//128);parts.append(name)
        copy_functions += ['static __attribute__((noinline)) void '+name+'(GeMpPlayerValues *v,const struct player *p,uint8_t active_player) {','(void)active_player;']+scalar_copies[index:index+128]+['}']
    copy_functions += ['static void ge_player_copy_native(GeMpPlayerValues *v,const struct player *p,uint8_t active_player) {']
    copy_functions += [name+'(v,p,active_player);' for name in parts]
    copy_functions += ['}','#else']+[c for c in copies if c.startswith('P_REF(')]+['#endif']
    return {'port/include/ge_mp_state_players_fields.inc':'\n'.join(header)+'\n',
        'port/src/ge_mp_state_players_copy.inc':'\n'.join(copy_functions)+'\n',
        'port/tests/ge_mp_state_players_mutations.inc':'/* Generated mutations of each owned native typed scalar element. */\n'+'\n'.join('P_MUTATE('+re.fullmatch(r'P_COPY\(\w+, \d+, (.*)\);',c).group(1)+');' for c in copies if c.startswith('P_COPY(') and 'storage.active_player' not in c)+'\n',
        'docs/generated/multiplayer_player_field_inventory.json':json.dumps(metadata,indent=2)+'\n'}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('repo',type=Path);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    for relative,text in generate(args.repo).items():
        path=args.repo/relative
        if args.check:assert path.read_text()==text,relative+' needs regenerated field review'
        else:path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)

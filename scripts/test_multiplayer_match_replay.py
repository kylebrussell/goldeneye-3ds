#!/usr/bin/env python3
"""Independent-process/enum/restart controls for the actual native match.

Inputs are a frozen raw-pad tape. State witnesses name a bounded set of native
fields; neither their comparison nor file hashes are a complete state digest.
"""
from pathlib import Path
import argparse
from collections import Counter
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys

REPO=Path(__file__).resolve().parents[1]
ENTRYPOINTS=('MoveBond','ge_original_dam_guard_props_tick_exact',
 'ge_original_player_held_items_tick','gunTickGameplay','ge_original_gun_update_and_fire_both_hands_exact',
 'chrTestHit','chrHandleBulletHit','bheadStartDeathAnimation','modelTickAnim','mp_respawn_handler',
 'mpCalculateAwards','sub_GAME_7F0C11FC','sub_GAME_7F0C1364','musicFadeTick','mpwatchMenuTick','bossSetLoadedStage','getROOMID_isRendered','bgGet2dBboxByRoomId')
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def write(path,value):Path(path).write_text(json.dumps(value,indent=2)+'\n')
def compile_fixture(build):
 ledger=json.loads((build/'compile-ledger.json').read_text())
 expected=json.loads((build/'build-identity.json').read_text())['dependencies']
 for name in ('test_ge_original_multiplayer_match.c','ge_multiplayer_match_controls.inc'):
  path=REPO/'port/tests'/name;expected[str(path)]=sha(path)
 command=next(e for e in ledger if e['source'].endswith('/ge_original_multiplayer_runtime.c'))['command'][:]
 command[command.index('-MF')+1]=str(build/'match-controls.d')
 command[command.index('-c')+1]=str(REPO/'port/tests/test_ge_original_multiplayer_match.c')
 command[command.index('-o')+1]=str(build/'match-controls.o')
 command+=['-Wformat=2','-Werror=format','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types']
 subprocess.run(command,check=True)
 dependency_text=(build/'match-controls.d').read_text().replace(chr(92)+chr(10),' ')
 dependencies={name:sha(name) for name in shlex.split(dependency_text.split(':',1)[1])}
 assert all(value==expected.get(name,value) for name,value in dependencies.items()),'fixture dependency changed during compile'
 binary=build/'match-controls'
 link=[command[0],'-fsanitize=address,undefined',
       '-Wl,-dead_strip' if sys.platform=='darwin' else '-Wl,--gc-sections',
       str(build/'match-controls.o'),str(build/'libmultiplayer-native.a'),'-lm','-o',str(binary)]
 subprocess.run(link,check=True)
 symbols=subprocess.check_output(['nm','-g',str(binary)],text=True)
 for name in ENTRYPOINTS:
  assert any(line.endswith(' T _'+name) or line.endswith(' T '+name) for line in symbols.splitlines()),name
 (build/'controls-link-symbols.txt').write_text(symbols)
 return binary,dict(compile=command,link=link,binary_sha256=sha(binary),symbols_sha256=sha(build/'controls-link-symbols.txt'),
   fixture_sha256=sha(REPO/'port/tests/test_ge_original_multiplayer_match.c'),dependency_sha256=dependencies,
   witness_io_sha256=sha(REPO/'port/tests/ge_multiplayer_match_controls.inc'))
def run(binary,assets,tapes,out,name,rosters,record=False):
 command=[str(binary),*map(str,assets),'--combat','--rosters',rosters,
          '--record-tape' if record else '--replay-tape',str(tapes),'--witness',str(out/(name+'.jsonl'))]
 result=subprocess.run(command,cwd=REPO,text=True,capture_output=True,
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 (out/(name+'.log')).write_text(result.stdout+result.stderr)
 write(out/(name+'.invocation.json'),dict(command=command,returncode=result.returncode,
       witness_sha256=sha(out/(name+'.jsonl')) if (out/(name+'.jsonl')).exists() else None,transcript_sha256=sha(out/(name+'.log'))))
 if result.returncode:print(result.stdout+result.stderr,end='')
 result.check_returncode()
 return [json.loads(line) for line in (out/(name+'.jsonl')).read_text().splitlines()]
def diff(a,b,path=''):
 if type(a)!=type(b):return [(path,a,b)]
 if isinstance(a,dict):
  result=[]
  for key in sorted(a.keys()|b.keys()):
   result+=diff(a.get(key),b.get(key),path+'.'+key)
  return result
 if isinstance(a,list):
  if len(a)!=len(b):return [(path+'.length',len(a),len(b))]
  result=[]
  for i,(x,y) in enumerate(zip(a,b)):result+=diff(x,y,f'{path}[{i}]')
  return result
 return [] if a==b else [(path,a,b)]
def summarize(differences):
 return dict(count=len(differences),by_field=dict(Counter(re.sub(r"\[\d+\]","[]",path) for path,_,_ in differences)),first=differences[:100])
def audit_restart(cold,restarted,index):
 # These source-backed assertions preserve raw values. They do not make a
 # rewritten/normalized witness and do not label the whole state equal.
 assert len(cold)==len(restarted)
 delta=restarted[0]['state']['player_tick_count']-cold[0]['state']['player_tick_count']
 assert delta>=0 and (index==0 or delta>0)
 for path,a,b in diff([x['state'] for x in cold],[x['state'] for x in restarted]):
  if path.endswith('.player_tick_count'):
   assert b-a==delta;continue
  body=re.fullmatch(r'\[1\]\.players\[(\d+)\]\.body\[0\]',path)
  if body and cold[0]['roster']==2 and index>0:
   slot=int(body[1])
   assert cold[1]['tick']==0 and a is None
   assert b==cold[2]['state']['players'][slot]['body'][0]
   continue
  raise AssertionError('unexplained native restart difference: '+repr((path,a,b)))
 allocation_delta=restarted[0]['ownership_history']['allocations_at_reset']-cold[0]['ownership_history']['allocations_at_reset']
 for old,new in zip(cold,restarted):
  a,b=old['ownership_history'],new['ownership_history']
  assert b['pool_lifetime']==a['pool_lifetime']+index
  assert b['allocations_at_reset']==a['allocations_at_reset']+allocation_delta
  assert b['allocations_now']==a['allocations_now']+allocation_delta
  assert b['player_lifetimes']==[x+index for x in a['player_lifetimes']]
 return dict(player_tick_count_offset=delta,allocation_offset=allocation_delta,
             lifetime_offset=index,unexplained_differences=0)
def main():
 p=argparse.ArgumentParser();p.add_argument('--record',action='store_true')
 p.add_argument('--tapes',type=Path,default=REPO/'port/tests/fixtures/multiplayer-controls')
 p.add_argument('--output',type=Path,default=REPO/'build/host-tests/multiplayer-match-controls')
 p.add_argument('--abis',nargs='+',choices=('native','short'),default=['native','short'])
 p.add_argument('--assets',nargs=5,type=Path,default=[REPO/x for x in (
  'build/host-tests/multiplayer-assets/multiplayer.gepack','build/host-tests/multiplayer-bodies/multiplayer.gepack',
  'build/multiplayer-port-tests/original-dam-setup/first-person-pp7.gepack','build/3ds-animations/bond',
  'build/host-tests/multiplayer-supplies/supplies.gepack')]);args=p.parse_args()
 args.output.mkdir(parents=True,exist_ok=True);args.tapes.mkdir(parents=True,exist_ok=True)
 assets=[x.resolve() for x in args.assets]
 files=[x for x in assets if x.is_file()]
 files += [assets[3]/name for name in ('animation_data.bin','animation_entries.bin','bond_eye_walk.entry.bin','sprinting.entry.bin','idle.entry.bin')]
 identity=dict(schema='ge-native-match-controls-v1',assets={str(x):sha(x) for x in files},
  seeds=['000000000073d600','000000000073d601'],witness_scope='explicit fields only; not a complete native state digest',builds={})
 peers={};report={};binaries={}
 frozen_tapes={str(x):sha(x) for x in sorted(args.tapes.glob('*.tape'))}
 def persist():
  current_tapes={str(x):sha(x) for x in sorted(args.tapes.glob('*.tape'))}
  if not args.record:assert current_tapes==frozen_tapes,'input tape changed during acceptance'
  identity['tapes']=current_tapes
  assert all(sha(path)==digest for path,digest in identity['assets'].items()),'asset changed during acceptance' 
  write(args.output/'controls-identity.json',identity);write(args.output/'comparisons.json',report)
 persist()
 for abi in args.abis:
  build=REPO/('build/host-tests/multiplayer-match' if abi=='native' else 'build/host-tests/multiplayer-match-short')
  subprocess.run([sys.executable,str(REPO/'scripts/build_multiplayer_match_host.py'),'--enum-abi',abi,'--build',str(build)],check=True)
  binary,fixture=compile_fixture(build);identity['builds'][abi]=dict(build_identity=json.loads((build/'build-identity.json').read_text()),fixture=fixture)
  binaries[abi]=binary;persist()
 print("All requested native graphs and fixture binaries captured",flush=True)
 for abi,binary in binaries.items():
  if args.record:
   assert abi=='native' and len(args.abis)==1
   for count in (2,4):
    run(binary,assets,args.tapes,args.output,f'record-{count}',str(count),True);persist()
   continue
  peers[abi]={}
  for rosters in ('2','4','2,4,2'):
   name=rosters.replace(',','-');first=run(binary,assets,args.tapes,args.output,f'{abi}-{name}-peer0',rosters)
   second=run(binary,assets,args.tapes,args.output,f'{abi}-{name}-peer1',rosters)
   differences=diff(first,second)
   report[f'{abi}-{name}-independent_peers']=dict(equal=not differences,first_differences=differences[:20])
   peers[abi][rosters]=first
   persist()
   assert not differences,f'independent peer divergence: {abi} {rosters}'
  first=[row for row in peers[abi]['2,4,2'] if row['run']==0]
  last=[row for row in peers[abi]['2,4,2'] if row['run']==2]
  # Run number is a test label; every native state/history field is compared.
  state_diff=diff([row['state'] for row in first],[row['state'] for row in last])
  history_diff=diff([row['ownership_history'] for row in first],[row['ownership_history'] for row in last])
  report[f'{abi}-restart']=dict(state_equal=not state_diff,state_difference_count=len(state_diff),
    first_state_differences=state_diff[:100],ownership_history_difference_count=len(history_diff),
    first_ownership_history_differences=history_diff[:20])
  for index,count in enumerate((2,4,2)):
   cold=peers[abi][str(count)]
   restarted=[row for row in peers[abi]['2,4,2'] if row['run']==index]
   if index:
    previous=[row for row in peers[abi]['2,4,2'] if row['run']==index-1]
    assert restarted[0]['state']['player_tick_count']==previous[-1]['state']['player_tick_count']
    assert restarted[0]['ownership_history']['allocations_at_reset']==previous[-1]['ownership_history']['allocations_now']
   report[f'{abi}-cold-{count}-versus-chain-{index}']=dict(
    state=summarize(diff([x['state'] for x in cold],[x['state'] for x in restarted])),
    ownership_history=summarize(diff([x['ownership_history'] for x in cold],[x['ownership_history'] for x in restarted])))
   persist()
   report[f'{abi}-cold-{count}-versus-chain-{index}']['source_history_audit']=audit_restart(cold,restarted,index)
  report[f'{abi}-restart']['state_difference_fields']=summarize(state_diff)['by_field']
  persist()
 if not args.record and len(peers)==2:
  for rosters in ('2','4','2,4,2'):
   differences=diff(peers['native'][rosters],peers['short'][rosters])
   report[f'enum-{rosters}']=dict(equal=not differences,first_differences=differences[:100])
   persist()
   assert not differences,'enum ABI divergence: '+rosters
 identity['tapes']={str(x):sha(x) for x in sorted(args.tapes.glob('*.tape'))}
 write(args.output/'controls-identity.json',identity);write(args.output/'comparisons.json',report)
 if args.record:print('Recorded native raw-pad tapes; identities retained in '+str(args.output/'controls-identity.json'))
 else:print(json.dumps(report,indent=2))
if __name__=='__main__':main()

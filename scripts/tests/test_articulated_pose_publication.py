#!/usr/bin/env python3
"""Byte-exact articulated pose copies and complete-copy fallbacks."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
s=(repo/'platform/3ds/source/main.c').read_text();a=s.index('static bool copy_stage_articulated_vertices(');b=s.index('\nstatic bool refresh_stage_articulated_objects(',a);helper=s[a:b]
body=s[b:s.index('\nstatic bool refresh_stage_monitor_surfaces(',b)]
assert 'copy_stage_articulated_vertices(' in body
assert 'ge_dam_dynamic_scene_commit_overlay_rooms(' in body
assert 'batch_count, pose_only ? 2U : 0U)' in body
assert 'publication->force_copy = false;' in body
unit=r'''
#include "ge_original_model_scene.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
'''+helper+r'''
static uint32_t rng=853;
static void randomize(void *p,size_t n){unsigned char *b=p;for(size_t i=0;i<n;++i){rng=rng*1664525U+1013904223U;b[i]=(unsigned char)(rng>>24);}}
int main(void){
 for(size_t trial=0;trial<12000;++trial){
  size_t count=trial%333;bool force=trial%7==0;
  GeDamRoomWorldVertex *source=malloc((count+2)*sizeof(*source)),*target=malloc((count+2)*sizeof(*target)),*expected=malloc((count+2)*sizeof(*expected));assert(source&&target&&expected);
  randomize(target,(count+2)*sizeof(*target));memcpy(source,target,(count+2)*sizeof(*target));
  GeOriginalModelScenePublicationRange ranges[8]={{0}};GeOriginalModelSceneCache cache={0};cache.publication_ranges=ranges;cache.publication_range_count=trial%9;
  bool pose=!force&&cache.publication_range_count!=0;
  for(size_t i=0;i<cache.publication_range_count;++i){ranges[i].static_data_changed=(uint8_t)(trial%5==0 && i==cache.publication_range_count-1);if(ranges[i].static_data_changed)pose=false;}
  for(size_t i=1;i<=count;++i){
   if(pose){randomize(source[i].processed.eye,sizeof(source[i].processed.eye));randomize(source[i].world,sizeof(source[i].world));}
   else randomize(&source[i],sizeof(source[i]));
  }
  memcpy(expected,target,(count+2)*sizeof(*target));memcpy(expected+1,source+1,count*sizeof(*target));
  assert(copy_stage_articulated_vertices(&cache,force,target+1,source+1,count)==pose);
  assert(memcmp(target,expected,(count+2)*sizeof(*target))==0);
  free(source);free(target);free(expected);
 }
 puts("12000 articulated pose/static/force-copy/empty publications preserve every vertex byte and both range sentinels");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-artic-pose-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-O3','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(repo/'port/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

#!/usr/bin/env python3
"""Exact full-buffer comparison for immutable identity-outer publication."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
s=(repo/'port/src/ge_original_model_scene.c').read_text()
start=s.index('static void cache_transform_vertex(');end=s.index('typedef struct GeModelPublishedInput',start)
actual=s[start:end]
code='''#include "ge_dam_room.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef struct {const GeDamRoomWorldVertex *vertices;const uint16_t *matrix_indices;const uint32_t *transform_sources;} GeOriginalModelSceneTopologyComponent;
'''+actual+r'''
static uint32_t rng=97363;
static uint32_t next(void){rng=rng*1664525U+1013904223U;return rng;}
static GeDamRoomWorldVertex source[257],a[259],b[259];
static uint16_t indices[257];static uint32_t prior[257];
int main(void){
 unsigned long total=0,duplicates=0;
 for(unsigned run=0;run<3000;++run){
  float matrix[31][4][4];
  for(unsigned i=0;i<31;++i)for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
   matrix[i][r][c]=(float)(int32_t)next()/65536.0f;
  if(run%7==0)for(unsigned i=0;i<31;++i){matrix[i][0][3]=-0.0f;matrix[i][1][3]=0.0f;matrix[i][2][3]=-0.0f;matrix[i][3][3]=1.0f;}
  for(unsigned i=0;i<sizeof(source);++i)((unsigned char*)source)[i]=(unsigned char)next();
  for(unsigned i=0;i<sizeof(a);++i)((unsigned char*)a)[i]=(unsigned char)next();
  memcpy(b,a,sizeof(a));
  unsigned count=run%258U;
  for(unsigned i=0;i<count;++i){
   indices[i]=(uint16_t)(next()%31);prior[i]=i;
   if(i && run%3!=0){unsigned q=next()%i;prior[i]=prior[q];indices[i]=indices[q];source[i].source.x=source[q].source.x;source[i].source.y=source[q].source.y;source[i].source.z=source[q].source.z;}
  }
  GeOriginalModelSceneTopologyComponent component={source,indices,prior};
  size_t expected=0;
  for(unsigned i=0;i<count;++i){
   GeDamRoomWorldVertex *dst=&a[i+1];
   if(prior[i]<i){
    memcpy(dst->processed.eye,a[prior[i]+1].processed.eye,sizeof(dst->processed.eye));
    memcpy(dst->world,a[prior[i]+1].world,sizeof(dst->world));++expected;
   }else{
    float transformed[4];
    /* Independent original scalar order, including leading positive zero. */
    for(unsigned axis=0;axis<4;++axis){float value=0.0f;
     value+=(float)source[i].source.x*matrix[indices[i]][0][axis];
     value+=(float)source[i].source.y*matrix[indices[i]][1][axis];
     value+=(float)source[i].source.z*matrix[indices[i]][2][axis];
     value+=matrix[indices[i]][3][axis];transformed[axis]=value;}
    memcpy(dst->processed.eye,transformed,sizeof(transformed));
    for(unsigned axis=0;axis<3;++axis)dst->world[axis]=transformed[axis];
   }
  }
  size_t got=cache_publish_segment_vertices(&component,matrix,b+1,count);
  assert(expected==got);assert(memcmp(a,b,sizeof(a))==0);
  total+=count;duplicates+=got;
 }
 printf("Segment publication: %lu byte-exact vertices, %lu duplicate copies, arbitrary joint matrices, signed zeros, empty ranges and full-buffer sentinels passed\n",total,duplicates);
}
'''
with tempfile.TemporaryDirectory(prefix='ge-segment-publish-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-O3','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(repo/'port/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
assert 'if (publish_segment_space && input->segment4_vertex_count == 0U\n                && query->required_vertex_count != 0U)' in s

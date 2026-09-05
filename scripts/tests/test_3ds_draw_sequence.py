#!/usr/bin/env python3
"""Compare the actual batched encoder with retained upstream DrawArrays."""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
vendor = repo/'platform/3ds/vendor/citro3d-1.7.1'
actual = (repo/'platform/3ds/source/ge_3ds_draw_arrays.c').read_text()
actual = re.sub(r'^#include .*\n', '', actual, flags=re.M)
reference = (vendor/'drawArrays.c').read_text().replace('#include "internal.h"', '')
reference = reference.replace('void C3D_DrawArrays(', 'void reference_DrawArrays(')
unit = r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <setjmp.h>
typedef uint32_t u32;
typedef u32 GPU_Primitive_t;
typedef struct {u32 prefix[8];u32 flags;} C3D_Context;
static C3D_Context context;
static C3D_Context *C3Di_GetContext(void) {return &context;}
enum {C3DiF_DrawUsed=2};
enum {GPUREG_PRIMITIVE_CONFIG=0x25e,GPUREG_RESTART_PRIMITIVE=0x25f,
 GPUREG_INDEXBUFFER_CONFIG=0x227,GPUREG_NUMVERTICES=0x228,
 GPUREG_VERTEX_OFFSET=0x22a,GPUREG_GEOSTAGE_CONFIG2=0x253,
 GPUREG_START_DRAW_FUNC0=0x245,GPUREG_DRAWARRAYS=0x22e,GPUREG_VTX_FUNC=0x231};
#define GPUCMD_HEADER(i,m,r) (((i)<<31)|(((m)&15)<<16)|((r)&0x3ff))
static u32 *gpuCmdBuf,gpuCmdBufSize,gpuCmdBufOffset;
static jmp_buf panic;
static unsigned context_calls,context_words;
static void GPUCMD_Add(u32 h,const u32 *p,u32 n) {
 assert(n==1);
 if(!gpuCmdBuf || gpuCmdBufOffset+2>gpuCmdBufSize)longjmp(panic,1);
 gpuCmdBuf[gpuCmdBufOffset]=p[0];gpuCmdBuf[gpuCmdBufOffset+1]=h;
 gpuCmdBufOffset+=2;
}
#define GPUCMD_AddMaskedWrite(r,m,v) do {u32 param=(u32)(v);GPUCMD_Add(GPUCMD_HEADER(0,m,r),&param,1);} while(0)
#define GPUCMD_AddWrite(r,v) GPUCMD_AddMaskedWrite(r,15,v)
static void C3Di_UpdateContext(void) {
 ++context_calls;
 for(unsigned i=0;i<context_words;++i)GPUCMD_AddWrite(0x100+i,i*17U);
 context.flags &= ~0x80U;
}
''' + reference + actual + r'''
static u32 rng=23183;
static u32 next(void) {rng=rng*1664525U+1013904223U;return rng;}
static u32 buffers[2][128];
static int run(int which,u32 capacity,u32 offset,int null_buffer,
 u32 flags,u32 primitive,int first,int size) {
 gpuCmdBuf=null_buffer?NULL:buffers[which];gpuCmdBufSize=capacity;gpuCmdBufOffset=offset;
 memset(&context,0x59,sizeof(context));context.flags=flags;context_calls=0;
 if(setjmp(panic))return 1;
 if(which)C3D_DrawArrays(primitive,first,size);else reference_DrawArrays(primitive,first,size);
 return 0;
}
int main(void) {
 unsigned successful=0,failed=0;
 for(unsigned trial=0;trial<50000;++trial) {
  for(unsigned i=0;i<128;++i)buffers[0][i]=next();
  memcpy(buffers[1],buffers[0],sizeof(buffers[0]));
  u32 offset=next()%64,capacity=next()%100,flags=next(),primitive=next();
  int first=(int32_t)next(),size=(int32_t)next();context_words=next()%12;
  if(trial%7==0)capacity=offset+context_words*2+22;
  if(trial%11==0)capacity=offset+context_words*2+21;
  if(trial%17==0){first=0;size=0;}
  int null_buffer=trial%23==0;
  int a=run(0,capacity,offset,null_buffer,flags,primitive,first,size);
  u32 end=gpuCmdBufOffset;C3D_Context expected=context;unsigned calls=context_calls;
  int b=run(1,capacity,offset,null_buffer,flags,primitive,first,size);
  assert(a==b && end==gpuCmdBufOffset && calls==context_calls && calls==1);
  assert(memcmp(&expected,&context,sizeof(context))==0);
  assert(memcmp(buffers[0],buffers[1],sizeof(buffers[0]))==0);
  assert(gpuCmdBufSize==capacity);
  successful+=!a;failed+=a;
 }
 assert(successful>1000 && failed>1000);
 printf("Draw sequence: 50000 exact buffers, offsets, flags and context calls (%u valid, %u matching partial/panic outcomes)\n",successful,failed);
}
'''
with tempfile.TemporaryDirectory(prefix='ge-draw-sequence-') as temporary:
    p=Path(temporary)
    (p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-O3','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
makefile=(repo/'platform/3ds/Makefile').read_text()
assert '3f34eff859c1f1e60ebe597b6df9cf5e5e74563d9e64ca01a7b3292fde63016a' in makefile
assert '$(error Citro3D archive changed:' in makefile

# The oracle and private ABI declaration are unmodified pinned upstream files.
assert hashlib.sha256((vendor/'drawArrays.c').read_bytes()).hexdigest() == 'da8544e074e74d8d56cbffd200bd9df56e9d3164afa1c6f505c3af4ca8ee1904'
assert hashlib.sha256((vendor/'internal.h').read_bytes()).hexdigest() == 'ab1efd417fcb2247932c800c8080108c7f8a9443ad2d1f7d1f2597b718c10e83'
assert hashlib.sha256((vendor/'LICENSE').read_bytes()).hexdigest() == '4d8db8e7270929946a3439aef932e37c7356a83391e56006439087e1d685809d'

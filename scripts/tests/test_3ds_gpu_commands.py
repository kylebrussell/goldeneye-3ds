#!/usr/bin/env python3
"""Differential command bytes, offsets and failures against libctru 2.7.0.

The altered reference encoder below follows the upstream implementation.
Its original notice is preserved in licenses/libctru.txt.
"""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
assert '-Wl,--wrap=GPUCMD_Add' in (repo/'platform/3ds/Makefile').read_text()
unit=r'''
#include <3ds.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
u32 *gpuCmdBuf, gpuCmdBufSize, gpuCmdBufOffset;
void __wrap_GPUCMD_Add(u32,const u32*,u32);
static jmp_buf panic;
static unsigned fallbacks;
/* Independent transcription of the published v2.7.0 encoder. Avoid forming
 * a pointer past NULL in the test oracle; there is no subsequent use for the
 * single-chunk NULL-parameter cases supported by this test. */
void __real_GPUCMD_Add(u32 header,const u32 *param,u32 length){
 ++fallbacks;if(!length)length=1;
 while(length){
  u32 n=length>0x100?0x100:length;
  if(!gpuCmdBuf || gpuCmdBufOffset+n+1>gpuCmdBufSize)longjmp(panic,1);
  u32 extra=n-1,h=header|((extra&255)<<20);
  gpuCmdBuf[gpuCmdBufOffset]=param?param[0]:0;
  gpuCmdBuf[gpuCmdBufOffset+1]=h;
  if(extra){if(param)memcpy(gpuCmdBuf+gpuCmdBufOffset+2,param+1,extra*4);else memset(gpuCmdBuf+gpuCmdBufOffset+2,0,extra*4);}
  gpuCmdBufOffset+=extra+2;
  if(extra&1)gpuCmdBuf[gpuCmdBufOffset++]=0;
  if(param)param+=n;length-=n;if(header&(1U<<31))header+=n;
 }
}
static u32 seed=73419;
static u32 next(void){seed=seed*1664525U+1013904223U;return seed;}
static u32 initial[4096],actual[4096],expected[4096],params[1024];
static int run(int fast,u32 *buffer,u32 size,u32 offset,u32 header,u32 length,int parameter_mode){
 gpuCmdBuf=buffer;gpuCmdBufSize=size;gpuCmdBufOffset=offset;
 const u32 *p=parameter_mode<0?NULL:parameter_mode==0?params:buffer+parameter_mode;
 if(setjmp(panic))return 1;
 if(fast)__wrap_GPUCMD_Add(header,p,length);else __real_GPUCMD_Add(header,p,length);
 return 0;
}
int main(void){
 const u32 lengths[]={0,1,2,3,4,255,256,257,300,511,512,777};
 size_t fast_count=0,failed=0;
 for(unsigned trial=0;trial<50000;++trial){
  for(size_t i=0;i<4096;++i)initial[i]=next();for(size_t i=0;i<1024;++i)params[i]=next();
  memcpy(actual,initial,sizeof(actual));memcpy(expected,initial,sizeof(expected));
  u32 length=lengths[trial%12],header=next(),offset=next()%2000,size=next()%3000;
  if(trial%7==0)size=offset+2;if(trial%13==0)size=offset+1;if(trial%19==0)size=offset;
  int mode=trial%5==0&&length<=256?-1:0;
  if(length<=1&&trial%3==0)mode=(int)(next()%3000)+1;
  int null_buffer=trial%23==0;if(null_buffer)mode=0;
  int a=run(0,null_buffer?NULL:expected,size,offset,header,length,mode);u32 end=gpuCmdBufOffset;
  fallbacks=0;int b=run(1,null_buffer?NULL:actual,size,offset,header,length,mode);
  assert(a==b&&end==gpuCmdBufOffset);assert(memcmp(actual,expected,sizeof(actual))==0);
  assert(gpuCmdBufSize==size);failed+=a;fast_count+=fallbacks==0;
 }
 assert(fast_count>1000&&failed>1000);
 printf("50000 GPU command cases: %zu fast writes, %zu identical panic paths; exact entire buffers, offsets, masks, zero-length, aliases, null parameters, chunking/padding and capacity boundaries\n",fast_count,failed);
}
'''
with tempfile.TemporaryDirectory(prefix='ge-gpu-command-') as tmp:
 p=Path(tmp);(p/'3ds.h').write_text('#include <stdint.h>\n#include <stddef.h>\ntypedef uint32_t u32;\nextern u32 *gpuCmdBuf, gpuCmdBufSize, gpuCmdBufOffset;\n');(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-O3','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-I'+str(p),str(p/'test.c'),str(repo/'platform/3ds/source/ge_3ds_gpu_commands.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

#!/usr/bin/env python3
"""Exercise the actual runtime handoff functions with delayed/failing execution."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
source=(repo/'port/src/ge_original_music_runtime.c').read_text()
a=source.index('static void ge_music_execute_pending(')
b=source.index('\nvoid ge_original_music_runtime_stats(',a)
body=source[a:b]
header=r'''
#include "ge_original_music_runtime.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef int32_t s32;
typedef GeAudioAbiCommand Acmd;
#define GE_MUSIC_RUNTIME_MAX_SAMPLES 800U
#define GE_MUSIC_RUNTIME_MAX_COMMANDS 3000U
struct GeOriginalMusicRuntime {
 int initialized,pending;uint8_t retrace_phase;
 size_t pending_samples,pending_command_count;
 GeAudioAbiResult pending_result;
 GeOriginalMusicRuntimeStats stats;
 GeAudioAbiState abi;
 Acmd *commands;int16_t *frame_pcm;GeAudioOutput *output;
 int player;
};
static int available=1,produced,executed,next_count=3,playing=7,queue_result;
static int submitted,waited,main_work;
static size_t free_frames=4096;
static void (*job)(void *);static void *job_context;
static void *ge_music_resolve(void *ctx,uint32_t address,size_t bytes){(void)ctx;(void)address;(void)bytes;return NULL;}
size_t ge_audio_output_free(const GeAudioOutput *out){assert(out);return free_frames;}
static void alAudioFrame(Acmd *commands,s32 *count,int16_t *pcm,s32 samples){assert(!job);assert(samples>0);assert(pcm[0]==0);commands[0].word0=++produced;*count=next_count;}
static int alCSPGetState(int *p){(void)p;return playing;}
GeAudioAbiResult ge_audio_abi_execute_and_queue(GeAudioAbiState *abi,const GeAudioAbiCommand *commands,size_t count,GeAudioAbiResolve resolve,void *ctx,uint32_t address,size_t frames,GeAudioOutput *out){(void)abi;(void)resolve;(void)ctx;(void)address;assert(out && frames && count==3);assert(commands[0].word0==(unsigned)produced);++executed;return (GeAudioAbiResult)queue_result;}
static int ge_3ds_music_worker_submit(void (*fn)(void *),void *ctx){if(!available)return 0;assert(!job);job=fn;job_context=ctx;++submitted;return 1;}
static void ge_3ds_music_worker_wait(void){++waited;if(job){void (*fn)(void *)=job;job=NULL;fn(job_context);}}
'''
test=r'''
int main(void){
 Acmd commands[3];int16_t pcm[1600];GeAudioOutput output={0};
 GeOriginalMusicRuntime r={.initialized=1,.commands=commands,.frame_pcm=pcm,.output=&output};
 assert(ge_original_music_runtime_begin_tick_60hz(&r)==0);assert(produced==1 && executed==0 && r.pending && job);
 ++main_work;assert(main_work==1 && r.stats.rendered_frames==0);
 assert(ge_original_music_runtime_finish(&r)==0);assert(executed==1 && !job && !r.pending && r.stats.rendered_frames==1);
 assert(r.stats.generated_commands==3 && r.stats.rendered_samples==736 && r.stats.player_state==7);
 assert(ge_original_music_runtime_finish(&r)==0 && executed==1);
 assert(ge_original_music_runtime_begin_tick_60hz(&r)==0 && produced==1); // odd retrace
 assert(ge_original_music_runtime_begin_tick_60hz(&r)==0 && produced==2 && executed==1);
 assert(ge_original_music_runtime_begin_tick_60hz(&r)==0 && !job && executed==2); // drains before next tick
 available=0;assert(ge_original_music_runtime_begin_tick_60hz(&r)==0 && executed==3 && r.pending);assert(ge_original_music_runtime_finish(&r)==0);
 available=1;assert(ge_original_music_runtime_render(&r,736)==0 && !job && executed==4); // synchronous API remains synchronous
 free_frames=0;int before=produced;assert(ge_original_music_runtime_render(&r,736)==GE_AUDIO_ABI_OUTPUT_FULL && produced==before);free_frames=4096;
 assert(ge_original_music_runtime_render(&r,801)==GE_AUDIO_ABI_INVALID_ARGUMENT);
 assert(ge_original_music_runtime_render(&r,0)==GE_AUDIO_ABI_INVALID_ARGUMENT);
 next_count=-1;assert(ge_original_music_runtime_render(&r,736)==GE_AUDIO_ABI_INVALID_ARGUMENT && !r.pending);next_count=3001;assert(ge_original_music_runtime_render(&r,736)==GE_AUDIO_ABI_INVALID_ARGUMENT && !r.pending);next_count=3;
 r.retrace_phase=0;queue_result=GE_AUDIO_ABI_DMEM_RANGE;uint64_t rendered=r.stats.rendered_frames;
 assert(ge_original_music_runtime_begin_tick_60hz(&r)==0 && job);assert(ge_original_music_runtime_finish(&r)==GE_AUDIO_ABI_DMEM_RANGE);assert(!r.pending && r.stats.rendered_frames==rendered && r.stats.last_abi_result==GE_AUDIO_ABI_DMEM_RANGE);queue_result=0;
 r.retrace_phase=0;before=produced;
 for(int i=0;i<256;++i){assert(ge_original_music_runtime_begin_tick_60hz(&r)==0);assert(ge_original_music_runtime_finish(&r)==0);}
 assert(produced==before+128 && r.retrace_phase==0);
 assert(ge_original_music_runtime_finish(NULL)==GE_AUDIO_ABI_INVALID_ARGUMENT);
 puts("Music runtime: deferred ownership, serial fallback, exact cadence, idempotent join, bounds and delayed errors pass");
}
'''
# Teardown and mutations drain before their first libaudio mutation/free.
for name,stop in [('ge_original_music_runtime_close','alCSPStop('),('ge_original_music_runtime_set_layer','alCSPStop('),('ge_original_music_runtime_set_layer_volume','alCSPSetVol('),('ge_original_music_runtime_stop_layer','alCSPStop(')]:
 start=source.index(name+'(')
 section=source[start:]
 # close is forward-called during open; find its definition instead.
 if name=='ge_original_music_runtime_close':section=source[source.index('void '+name+'('):]
 assert section.index('ge_original_music_runtime_finish(runtime)')<section.index(stop),name
main=(repo/'platform/3ds/source/main.c').read_text()
start=main.index('const GeAudioAbiResult music_status = render_music')
end=main.index('fine_profile.gameplay_phase_ticks[0] +=',start)
assert 'ge_original_music_runtime_begin_tick_60hz(original_music)' in main[start:end]
assert 'ge_original_music_runtime_finish(original_music)' in main[start:end]
with tempfile.TemporaryDirectory(prefix='ge-music-handoff-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(header+body+test)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-DGE_PLATFORM_3DS','-fsanitize=address,undefined','-I'+str(repo/'port/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

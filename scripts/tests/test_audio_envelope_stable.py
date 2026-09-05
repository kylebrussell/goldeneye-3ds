#!/usr/bin/env python3
"""Compare the ramp/stable split with the original envelope loop, including aliases."""
from pathlib import Path
import re
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
source=(repo/'port/src/ge_audio_abi.c').read_text()
a=source.index('static GeAudioAbiResult ge_audio_abi_envmixer(')
b=source.index('\nstatic GeAudioAbiResult ge_audio_abi_polef(',a)
reference=source[:a]+r'''
static GeAudioAbiResult ge_audio_abi_envmixer(
        GeAudioAbiState *state,
        uint8_t flags,
        uint32_t state_address,
        GeAudioAbiResolve resolve,
        void *resolve_context)
{
    const size_t sample_count = state->count_bytes / sizeof(int16_t);
    const int auxiliary = (flags & GE_ABI_FLAG_AUX) != 0U;
    const uint16_t outputs[4] = {
        state->dmem_output,
        state->dmem_dry_right,
        state->dmem_wet_left,
        state->dmem_wet_right,
    };
    uint32_t resolved_state_address = ge_audio_abi_address(
            state, state_address);
    uint8_t *saved_state;
    GeAudioAbiRamp ramps[2];
    int16_t gains[4] = {0};
    int gains_stable = 0;
    int16_t dry = state->envelope_dry;
    int16_t wet = state->envelope_wet;
    size_t output_count = auxiliary ? 4U : 2U;
    size_t sample;
    size_t output;

    if (!ge_audio_abi_dmem_range(state->dmem_input, state->count_bytes)) {
        return GE_AUDIO_ABI_DMEM_RANGE;
    }
    for (output = 0U; output < output_count; ++output) {
        if (!ge_audio_abi_dmem_range(outputs[output], state->count_bytes)) {
            return GE_AUDIO_ABI_DMEM_RANGE;
        }
    }
    if (resolve == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }
    saved_state = resolve(resolve_context, resolved_state_address,
            GE_ABI_ENVMIX_STATE_BYTES);
    if (saved_state == NULL) {
        return GE_AUDIO_ABI_ADDRESS_UNMAPPED;
    }

    if ((flags & GE_ABI_FLAG_INIT) != 0U) {
        for (output = 0U; output < 2U; ++output) {
            ramps[output].value = (int32_t)state->envelope_volume[output]
                    * 65536;
            ramps[output].target = (int32_t)state->envelope_target[output]
                    * 65536;
            ramps[output].step = state->envelope_rate[output] / 8;
        }
    } else {
        wet = ge_audio_abi_load_s16(saved_state + 0U);
        dry = ge_audio_abi_load_s16(saved_state + 4U);
        ramps[0].target = (int32_t)ge_audio_abi_load_u32(saved_state + 8U);
        ramps[1].target = (int32_t)ge_audio_abi_load_u32(saved_state + 12U);
        ramps[0].step = (int32_t)ge_audio_abi_load_u32(saved_state + 16U);
        ramps[1].step = (int32_t)ge_audio_abi_load_u32(saved_state + 20U);
        ramps[0].value = (int32_t)ge_audio_abi_load_u32(saved_state + 32U);
        ramps[1].value = (int32_t)ge_audio_abi_load_u32(saved_state + 36U);
    }

    for (sample = 0U; sample < sample_count; ++sample) {
        const int16_t source = ge_audio_abi_load_s16(state->dmem
                + state->dmem_input + sample * sizeof(int16_t));
        if (!gains_stable) {
            const int16_t left_volume = ge_audio_abi_ramp_step(&ramps[0]);
            const int16_t right_volume = ge_audio_abi_ramp_step(&ramps[1]);
            gains[0] = ge_audio_abi_saturate(
                    ((int32_t)left_volume * dry + 0x4000) >> 15U);
            gains[1] = ge_audio_abi_saturate(
                    ((int32_t)right_volume * dry + 0x4000) >> 15U);
            if (auxiliary) {
                gains[2] = ge_audio_abi_saturate(
                        ((int32_t)left_volume * wet + 0x4000) >> 15U);
                gains[3] = ge_audio_abi_saturate(
                        ((int32_t)right_volume * wet + 0x4000) >> 15U);
            }
            /* Once both ramps stop, further ramp steps and gain multiplies
             * return the same values for the rest of this command. */
            gains_stable = ramps[0].step == 0 && ramps[1].step == 0;
        }
        /* Preserve source-before-output and dry-left/right, wet-left/right
         * write order, including when any of the DMEM buffers overlap. */
        ge_audio_abi_mix_sample(state->dmem + outputs[0] + sample * 2U,
                source, gains[0]);
        ge_audio_abi_mix_sample(state->dmem + outputs[1] + sample * 2U,
                source, gains[1]);
        if (auxiliary) {
            ge_audio_abi_mix_sample(state->dmem + outputs[2] + sample * 2U,
                    source, gains[2]);
            ge_audio_abi_mix_sample(state->dmem + outputs[3] + sample * 2U,
                    source, gains[3]);
        }
    }

    ge_audio_abi_store_s16(saved_state + 0U, wet);
    ge_audio_abi_store_s16(saved_state + 4U, dry);
    ge_audio_abi_store_u32(saved_state + 8U, (uint32_t)ramps[0].target);
    ge_audio_abi_store_u32(saved_state + 12U, (uint32_t)ramps[1].target);
    ge_audio_abi_store_u32(saved_state + 16U, (uint32_t)ramps[0].step);
    ge_audio_abi_store_u32(saved_state + 20U, (uint32_t)ramps[1].step);
    ge_audio_abi_store_u32(saved_state + 32U, (uint32_t)ramps[0].value);
    ge_audio_abi_store_u32(saved_state + 36U, (uint32_t)ramps[1].value);
    return GE_AUDIO_ABI_OK;
}
'''+source[b:]
public=re.findall(r'\b(ge_audio_abi_\w+)\s*\(', (repo/'port/include/ge_audio_abi.h').read_text())
test=r'''
#include "ge_audio_abi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
GeAudioAbiResult reference_ge_audio_abi_execute(GeAudioAbiState*, const GeAudioAbiCommand*,size_t,GeAudioAbiResolve,void*);
typedef struct {GeAudioAbiState *state; unsigned alias, fail; uint8_t bytes[128];} Memory;
static void *resolve(void *context,uint32_t address,size_t count) {
 Memory *m=context;
 if(m->fail || address>128 || count>128-address)return NULL;
 return m->alias ? m->state->dmem+address : m->bytes+address;
}
static uint32_t rng=93214;
static uint32_t next(void) {rng=rng*1664525U+1013904223U;return rng;}
int main(void) {
 size_t successful=0;
 for(unsigned test=0;test<30000;++test) {
  GeAudioAbiState a,b;ge_audio_abi_init(&a);
  for(size_t i=0;i<sizeof(a.dmem);++i)a.dmem[i]=(uint8_t)(next()>>24);
  uint16_t input=(uint16_t)(next()%4097U),output=(uint16_t)(next()%4097U);
  switch(test%8U) {
   case 0:input=0;output=0;break;
   case 1:input=1;output=3;break;
   case 2:input=2;output=0;break;
   case 3:input=0;output=2;break;
   case 4:input&=0xfffeU;output&=0xfffeU;break;
  }
  a.dmem_input=input;a.dmem_output=output;
  a.count_bytes=(uint16_t)(next()%1025U);
  if(test%19U==0)a.count_bytes=(uint16_t)(test%4U);
  if(test%23U==0)a.count_bytes=4096;
  a.dmem_dry_right=test%3U ? output : (uint16_t)(next()%4097U);
  a.dmem_wet_left=test%5U ? input : (uint16_t)(next()%4097U);
  a.dmem_wet_right=test%7U ? output : (uint16_t)(next()%4097U);
  a.envelope_dry=test%3U ? (int16_t)next() : 0;a.envelope_wet=test%5U ? (int16_t)next() : 0;
  for(unsigned i=0;i<2;++i){a.envelope_volume[i]=(int16_t)next();a.envelope_target[i]=(int16_t)next();a.envelope_rate[i]=(int32_t)next();}
  if(test%4U==0)for(unsigned i=0;i<2;++i){a.envelope_volume[i]=(int16_t)(test%128U);a.envelope_target[i]=(int16_t)(a.envelope_volume[i]+i+1);a.envelope_rate[i]=(int32_t)(65536U*8U/(test%257U+1));}
  if(test%17U==0)for(unsigned i=0;i<2;++i){a.envelope_volume[i]=a.envelope_target[i];a.envelope_rate[i]=0;}
  Memory ma={.state=&a,.alias=test%3U==0,.fail=test%29U==0},mb;
  for(size_t i=0;i<sizeof(ma.bytes);++i)ma.bytes[i]=(uint8_t)(next()>>24);
  if(test%7U==0) {uint8_t *saved=ma.alias?a.dmem:ma.bytes;memset(saved,0,8);}
  mb=ma;mb.state=&b;b=a;
  GeAudioAbiCommand cmd;
  cmd=(GeAudioAbiCommand){(3U<<24)|(((test%2U==0?1U:0U)|(test%3U==0?8U:0U))<<16),(test>>1U)%2U};
  int x=ge_audio_abi_execute(&a,&cmd,1,resolve,&ma);
  int y=reference_ge_audio_abi_execute(&b,&cmd,1,resolve,&mb);
  assert(x==y);assert(memcmp(&a,&b,sizeof(a))==0);assert(memcmp(ma.bytes,mb.bytes,sizeof(ma.bytes))==0);
  successful+=x==GE_AUDIO_ABI_OK;
 }
 printf("Envelope ramp/stable split: 30000 exact commands (%zu valid), odd/even offsets, aliases, saturation, ramps, saved state and range failures\n",successful);
}
'''
with tempfile.TemporaryDirectory(prefix='ge-env-split-') as tmp:
 p=Path(tmp);(p/'reference.c').write_text(reference);(p/'test.c').write_text(test)
 flags=['-std=c11','-O3','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(repo/'port/include')]
 subprocess.run(['cc',*flags,*['-D'+n+'=reference_'+n for n in public],'-c',str(p/'reference.c'),'-o',str(p/'reference.o')],check=True)
 subprocess.run(['cc',*flags,str(p/'test.c'),str(repo/'port/src/ge_audio_abi.c'),str(repo/'port/src/ge_audio_output.c'),str(p/'reference.o'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

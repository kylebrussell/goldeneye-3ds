#ifndef GE_LIBULTRA_AUDIO_H
#define GE_LIBULTRA_AUDIO_H

#include "ge_audio_output.h"
#include "ultra64.h"

#define AI_STATUS_FIFO_FULL UINT32_C(0x80000000)
#define AI_STATUS_DMA_BUSY UINT32_C(0x40000000)

void ge_libultra_audio_bind(GeAudioOutput *output);
GeAudioOutput *ge_libultra_audio_output(void);

u32 osAiGetStatus(void);
u32 osAiGetLength(void);
s32 osAiSetFrequency(u32 frequency);
s32 osAiSetNextBuffer(void *buffer, u32 size_bytes);

#endif

#ifndef GE_AUDIO_SMOKE_LIBAUDIO_H
#define GE_AUDIO_SMOKE_LIBAUDIO_H

#include "ultra64.h"

#define AL_EVTQ_END INT32_MAX

typedef s32 ALMicroTime;

typedef struct ALLink {
    struct ALLink *next;
    struct ALLink *prev;
} ALLink;

typedef struct ALEvent {
    s16 type;
    u16 padding;
    u64 payload[4];
} ALEvent;

typedef struct ALEventListItem {
    ALLink node;
    ALMicroTime delta;
    ALEvent evt;
} ALEventListItem;

typedef struct ALEventQueue {
    ALLink freeList;
    ALLink allocList;
    s32 eventCount;
} ALEventQueue;

typedef struct ALSynth {
    int unused;
} ALSynth;

typedef struct ALGlobals {
    ALSynth drvr;
} ALGlobals;

typedef struct ALSynConfig {
    int unused;
} ALSynConfig;

void alSynNew(ALSynth *synth, ALSynConfig *config);
void alSynDelete(ALSynth *synth);
void alLink(ALLink *link, ALLink *after);
void alUnlink(ALLink *link);
void alCopy(void *source, void *destination, s32 length);

void alEvtqNew(
        ALEventQueue *queue,
        ALEventListItem *items,
        s32 item_count);
ALMicroTime alEvtqNextEvent(ALEventQueue *queue, ALEvent *event);
void alEvtqPostEvent(
        ALEventQueue *queue,
        ALEvent *event,
        ALMicroTime delta);
void alEvtqFlush(ALEventQueue *queue);
void alEvtqFlushType(ALEventQueue *queue, s16 type);
f32 alCents2Ratio(s32 cents);

#endif

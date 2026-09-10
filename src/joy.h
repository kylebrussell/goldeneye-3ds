#ifndef _JOY_H_
#define _JOY_H_

#include <ultra64.h>

#ifdef GE_PORT_ULTRA64_H
#include "ge_original_input_state.h"
#endif


/**
 * Number of samples in contdata.
 */
#define CONTSAMPLE_LEN   20

/* Native playback presents two original pads per player. The hardware N64
 * build retains its four-pad ABI; no serial controller protocol is changed. */
#ifdef GE_PORT_ULTRA64_H
#define JOY_MAX_CONTROLLERS 8
#else
#define JOY_MAX_CONTROLLERS MAXCONTROLLERS
#endif

struct contsample {
	OSContPad pads[JOY_MAX_CONTROLLERS];
};

typedef s32 (*contplaybackfunc)(struct contsample*, s32);
typedef void (*contrecordfunc)(struct contsample*, s32, s32);

#ifdef GE_PORT_ULTRA64_H
/* Read-only identity check used by the owner of a private playback function. */
int ge_original_joy_playback_matches(contplaybackfunc func);
#endif

void joyInit(void);
void joyCheckStatusThreadSafe(void);
s8 joyGetControllerCount(void);
u8 joyGetConnectedControllers(void);
void joySetPlaybackFunc(contplaybackfunc func, s32 controllercount);
void joySetRecordFunc(contrecordfunc func);
void joyConsumeSamplesWrapper(void);
void joyPoll(void);
s8 joyGetStickX(s8 contpadnum);
s8 joy7000C174(s8 contpadnum);
s8 joyGetStickY(s8 contpadnum);
s8 joy7000C284(s8 contpadnum);
u16 joyGetButtons(s8 contpadnum, u16 mask);
u16 joyGetButtonsPressedThisFrame(s8 contpadnum, u16 mask);
s32 joyGetStickXInRange(s8 contpadnum, s32 rangemin, s32 rangemax);
s32 joyGetStickYInRange(s8 contpadnum, s32 rangemin, s32 rangemax);
f32 joyGetStickXInRangef(s8 contpadnum, f32 rangemin, f32 rangemax);
f32 joyGetStickYInRangef(s8 contpadnum, f32 rangemin, f32 rangemax);
s32 joyGamePakProbe(void);
s32 joyGamePakLongRead(u8 address, u8 *buffer, s32 nbytes);
s32 joyGamePakLongWrite(u8 address, u8 *buffer, s32 nbytes);
void joyRumblePakStart(s32 controller, f32 duration);
void joyRumblePakStop(void);
void joySetContDataIndex(s32 index);
s32 joyGetContDataIndex(void);

#endif

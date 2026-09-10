#ifndef GE_3DS_AUDIO_H
#define GE_3DS_AUDIO_H

#include "ge_audio_output.h"
#include "ge_audio_refill.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Owns NDSP channel zero.  Pump once per frontend frame; completed wave
 * buffers are refilled from output and resubmitted without allocating.
 * Calls and producer ring writes must be serialized (finish asynchronous
 * production before pumping). The caller owns each ring and its storage;
 * keep the primary alive until exit and unbind a secondary before freeing it.
 * Libctru owns DSP suspend/resume; retained buffers stay bound during sleep.
 */
int ge_3ds_audio_init(GeAudioOutput *output);
/* NULL unbinds, including after exit. Otherwise require a valid, disjoint ring
 * at the primary rate.
 * Rejected bindings preserve the existing secondary. A secondary invalidated
 * or retuned incompatibly after binding is detached by the next pump. */
int ge_3ds_audio_bind_secondary(GeAudioOutput *output);
/* An invalid primary or failed DSP cache flush stops the sink and releases its
 * resources. Recheck is_active after pumping when choosing producer fallback. */
void ge_3ds_audio_pump(void);
void ge_3ds_audio_exit(void);
bool ge_3ds_audio_is_active(void);
const GeAudioRefillState *ge_3ds_audio_refill_stats(void);
/* Retained through exit, reset by init: 0 means no recorded error, -1 invalid
 * ring/binding, -2 linear allocation failure, otherwise the failed DSP Result. */
int32_t ge_3ds_audio_last_error(void);

#endif

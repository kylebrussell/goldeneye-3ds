#ifndef GE_3DS_AUDIO_H
#define GE_3DS_AUDIO_H

#include "ge_audio_output.h"
#include "ge_audio_refill.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Owns NDSP channel zero.  Pump once per frontend frame; completed wave
 * buffers are refilled from output and resubmitted without allocating.
 */
int ge_3ds_audio_init(GeAudioOutput *output);
int ge_3ds_audio_bind_secondary(GeAudioOutput *output);
void ge_3ds_audio_pump(void);
void ge_3ds_audio_exit(void);
bool ge_3ds_audio_is_active(void);
const GeAudioRefillState *ge_3ds_audio_refill_stats(void);
int32_t ge_3ds_audio_last_error(void);

#endif

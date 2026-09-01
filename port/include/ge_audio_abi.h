#ifndef GE_AUDIO_ABI_H
#define GE_AUDIO_ABI_H

/* Portable interpreter frontier for the N64 ABI1 audio command stream. */

#include "ge_audio_output.h"

#include <stddef.h>
#include <stdint.h>

#define GE_AUDIO_ABI_DMEM_BYTES 4096U
#define GE_AUDIO_ABI_SEGMENTS 16U
#define GE_AUDIO_ABI_ADPCM_BOOK_SAMPLES 128U

typedef struct GeAudioAbiCommand {
    uint32_t word0;
    uint32_t word1;
} GeAudioAbiCommand;

typedef void *(*GeAudioAbiResolve)(
        void *context,
        uint32_t address,
        size_t size_bytes);

typedef enum GeAudioAbiResult {
    GE_AUDIO_ABI_OK = 0,
    GE_AUDIO_ABI_INVALID_ARGUMENT = -1,
    GE_AUDIO_ABI_DMEM_RANGE = -2,
    GE_AUDIO_ABI_ADDRESS_UNMAPPED = -3,
    GE_AUDIO_ABI_UNSUPPORTED_COMMAND = -4,
    GE_AUDIO_ABI_OUTPUT_FULL = -5,
    GE_AUDIO_ABI_CODEBOOK_RANGE = -6
} GeAudioAbiResult;

typedef struct GeAudioAbiState {
    uint8_t dmem[GE_AUDIO_ABI_DMEM_BYTES];
    uint32_t segments[GE_AUDIO_ABI_SEGMENTS];
    uint16_t dmem_input;
    uint16_t dmem_output;
    uint16_t count_bytes;
    uint16_t dmem_dry_right;
    uint16_t dmem_wet_left;
    uint16_t dmem_wet_right;
    int16_t envelope_dry;
    int16_t envelope_wet;
    int16_t envelope_volume[2];
    int16_t envelope_target[2];
    int32_t envelope_rate[2];
    int16_t adpcm_codebook[GE_AUDIO_ABI_ADPCM_BOOK_SAMPLES];
    size_t adpcm_codebook_bytes;
    uint32_t adpcm_loop_address;
    uint8_t unsupported_opcode;
    uint8_t direct_addresses;
    size_t commands_executed;
} GeAudioAbiState;

void ge_audio_abi_init(GeAudioAbiState *state);

/*
 * Executes the CPU-side subset emitted by libaudio's producer and bus/save
 * chain: SPNOOP, ADPCM, CLEARBUFF, LOADBUFF, RESAMPLE, SAVEBUFF, SEGMENT,
 * SETBUFF, SETVOL, DMEMMOVE, LOADADPCM, MIXER, INTERLEAVE, ENVMIXER, and
 * SETLOOP, and POLEF. These are all sixteen ABI1 opcodes emitted by the
 * original GoldenEye libaudio producer.
 */
GeAudioAbiResult ge_audio_abi_execute(
        GeAudioAbiState *state,
        const GeAudioAbiCommand *commands,
        size_t command_count,
        GeAudioAbiResolve resolve,
        void *resolve_context);

/* Execute a list, then queue its saved interleaved stereo buffer. */
GeAudioAbiResult ge_audio_abi_execute_and_queue(
        GeAudioAbiState *state,
        const GeAudioAbiCommand *commands,
        size_t command_count,
        GeAudioAbiResolve resolve,
        void *resolve_context,
        uint32_t output_address,
        size_t frame_count,
        GeAudioOutput *output);

#endif

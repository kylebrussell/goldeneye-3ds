#include "ge_libultra_audio.h"

#include <stddef.h>
#include <stdint.h>

static GeAudioOutput *g_audio_output;

void ge_libultra_audio_bind(GeAudioOutput *output)
{
    g_audio_output = output;
}

GeAudioOutput *ge_libultra_audio_output(void)
{
    return g_audio_output;
}

u32 osVirtualToPhysical(void *address)
{
    return (u32)(uintptr_t)address;
}

u32 osAiGetStatus(void)
{
    u32 status = 0U;

    if (g_audio_output == NULL) {
        return 0U;
    }
    if (ge_audio_output_queued(g_audio_output) != 0U) {
        status |= AI_STATUS_DMA_BUSY;
    }
    if (ge_audio_output_free(g_audio_output) == 0U) {
        status |= AI_STATUS_FIFO_FULL;
    }
    return status;
}

u32 osAiGetLength(void)
{
    size_t queued_bytes;

    if (g_audio_output == NULL) {
        return 0U;
    }
    queued_bytes = ge_audio_output_queued(g_audio_output)
        * 2U * sizeof(int16_t);
    return queued_bytes > UINT32_MAX ? UINT32_MAX : (u32)queued_bytes;
}

s32 osAiSetFrequency(u32 frequency)
{
    if (g_audio_output == NULL
            || ge_audio_output_set_sample_rate(g_audio_output, frequency) != 0) {
        return -1;
    }
    return (s32)frequency;
}

s32 osAiSetNextBuffer(void *buffer, u32 size_bytes)
{
    size_t frame_count;
    size_t accepted;

    if (g_audio_output == NULL || buffer == NULL || size_bytes == 0U
            || size_bytes % (2U * sizeof(int16_t)) != 0U) {
        return -1;
    }

    frame_count = size_bytes / (2U * sizeof(int16_t));
    if (ge_audio_output_free(g_audio_output) < frame_count) {
        return -1;
    }
    accepted = ge_audio_output_write(g_audio_output, buffer, frame_count);
    return accepted == frame_count ? 0 : -1;
}

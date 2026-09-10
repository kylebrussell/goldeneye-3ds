#ifndef GE_3DS_AUDIO_MOCK_H
#define GE_3DS_AUDIO_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t Result;
#define R_FAILED(result) ((result) < 0)

typedef enum { NDSP_INTERP_LINEAR = 1 } ndspInterpType;
typedef enum { NDSP_OUTPUT_STEREO = 1 } ndspOutputMode;
enum {
    NDSP_FORMAT_STEREO_PCM16 = 6,
    NDSP_WBUF_FREE = 0,
    NDSP_WBUF_QUEUED = 1,
    NDSP_WBUF_PLAYING = 2,
    NDSP_WBUF_DONE = 3
};
typedef struct {
    uint16_t index;
    int16_t history0;
    int16_t history1;
} ndspAdpcmData;
typedef struct tag_ndspWaveBuf {
    union {
        int8_t *data_pcm8;
        int16_t *data_pcm16;
        uint8_t *data_adpcm;
        const void *data_vaddr;
    };
    uint32_t nsamples;
    ndspAdpcmData *adpcm_data;
    uint32_t offset;
    bool looping;
    uint8_t status;
    uint16_t sequence_id;
    struct tag_ndspWaveBuf *next;
} ndspWaveBuf;

Result ndspInit(void);
void ndspExit(void);
void *linearAlloc(size_t size);
void linearFree(void *pointer);
Result DSP_FlushDataCache(const void *address, uint32_t size);
void ndspSetOutputMode(ndspOutputMode mode);
void ndspChnReset(int id);
void ndspChnSetInterp(int id, ndspInterpType type);
void ndspChnSetRate(int id, float rate);
void ndspChnSetFormat(int id, uint16_t format);
void ndspChnWaveBufClear(int id);
void ndspChnWaveBufAdd(int id, ndspWaveBuf *wave);

#endif

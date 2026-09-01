#ifndef GE_GBI_RSP_H
#define GE_GBI_RSP_H

#include "ge_gbi_decoder.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_GBI_VIEWPORT_BYTE_COUNT 16U
#define GE_GBI_LIGHT_BYTE_COUNT 16U
#define GE_GBI_LIGHT_COUNT 8U

typedef struct GeGbiViewport {
    int16_t scale[4];
    int16_t translation[4];
} GeGbiViewport;

typedef struct GeGbiLight {
    uint8_t color[3];
    uint8_t color_copy[3];
    int8_t direction[3];
} GeGbiLight;

typedef enum GeGbiRspPayloadStatus {
    GE_GBI_RSP_PAYLOAD_OK,
    GE_GBI_RSP_PAYLOAD_INVALID_ARGUMENT,
    GE_GBI_RSP_PAYLOAD_TRUNCATED
} GeGbiRspPayloadStatus;

GeGbiRspPayloadStatus ge_gbi_viewport_decode(const uint8_t *bytes,
                                              size_t byte_count,
                                              GeGbiByteOrder byte_order,
                                              GeGbiViewport *viewport);

GeGbiRspPayloadStatus ge_gbi_light_decode(const uint8_t *bytes,
                                           size_t byte_count,
                                           GeGbiLight *light);

#ifdef __cplusplus
}
#endif

#endif

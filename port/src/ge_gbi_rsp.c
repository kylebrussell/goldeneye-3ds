#include "ge_gbi_rsp.h"

static uint16_t ge_gbi_rsp_read_u16(const uint8_t *bytes,
                                    GeGbiByteOrder byte_order)
{
    if (byte_order == GE_GBI_BYTE_ORDER_BIG_ENDIAN) {
        return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
    }
    return (uint16_t)(((uint16_t)bytes[1] << 8) | (uint16_t)bytes[0]);
}

GeGbiRspPayloadStatus ge_gbi_viewport_decode(const uint8_t *bytes,
                                              size_t byte_count,
                                              GeGbiByteOrder byte_order,
                                              GeGbiViewport *viewport)
{
    size_t index;

    if (bytes == NULL || viewport == NULL
            || (byte_order != GE_GBI_BYTE_ORDER_BIG_ENDIAN
                && byte_order != GE_GBI_BYTE_ORDER_LITTLE_ENDIAN)) {
        return GE_GBI_RSP_PAYLOAD_INVALID_ARGUMENT;
    }
    if (byte_count < GE_GBI_VIEWPORT_BYTE_COUNT) {
        return GE_GBI_RSP_PAYLOAD_TRUNCATED;
    }

    for (index = 0U; index < 4U; ++index) {
        viewport->scale[index] = (int16_t)ge_gbi_rsp_read_u16(
            bytes + index * 2U, byte_order);
        viewport->translation[index] = (int16_t)ge_gbi_rsp_read_u16(
            bytes + 8U + index * 2U, byte_order);
    }
    return GE_GBI_RSP_PAYLOAD_OK;
}

GeGbiRspPayloadStatus ge_gbi_light_decode(const uint8_t *bytes,
                                           size_t byte_count,
                                           GeGbiLight *light)
{
    size_t index;

    if (bytes == NULL || light == NULL) {
        return GE_GBI_RSP_PAYLOAD_INVALID_ARGUMENT;
    }
    if (byte_count < GE_GBI_LIGHT_BYTE_COUNT) {
        return GE_GBI_RSP_PAYLOAD_TRUNCATED;
    }

    for (index = 0U; index < 3U; ++index) {
        light->color[index] = bytes[index];
        light->color_copy[index] = bytes[4U + index];
        light->direction[index] = (int8_t)bytes[8U + index];
    }
    return GE_GBI_RSP_PAYLOAD_OK;
}

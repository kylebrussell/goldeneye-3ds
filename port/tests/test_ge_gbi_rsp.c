#include "ge_gbi_rsp.h"

#include <assert.h>
#include <stdio.h>

static void test_viewport_decode(void)
{
    static const uint8_t viewport_be[] = {
        0x02, 0x80, 0x01, 0xe0, 0x03, 0xff, 0x00, 0x00,
        0x02, 0x80, 0x01, 0xe0, 0x00, 0x00, 0xff, 0xff
    };
    GeGbiViewport viewport;

    assert(ge_gbi_viewport_decode(viewport_be, sizeof(viewport_be),
                                  GE_GBI_BYTE_ORDER_BIG_ENDIAN, &viewport)
           == GE_GBI_RSP_PAYLOAD_OK);
    assert(viewport.scale[0] == 640);
    assert(viewport.scale[1] == 480);
    assert(viewport.scale[2] == 1023);
    assert(viewport.translation[0] == 640);
    assert(viewport.translation[1] == 480);
    assert(viewport.translation[3] == -1);
    assert(ge_gbi_viewport_decode(viewport_be, 15U,
                                  GE_GBI_BYTE_ORDER_BIG_ENDIAN, &viewport)
           == GE_GBI_RSP_PAYLOAD_TRUNCATED);
}

static void test_light_decode(void)
{
    static const uint8_t light_bytes[] = {
        0x10, 0x20, 0x30, 0,
        0x11, 0x21, 0x31, 0,
        0xff, 0x02, 0xfd, 0,
        0xaa, 0xbb, 0xcc, 0xdd
    };
    GeGbiLight light;

    assert(ge_gbi_light_decode(light_bytes, sizeof(light_bytes), &light)
           == GE_GBI_RSP_PAYLOAD_OK);
    assert(light.color[0] == UINT8_C(0x10));
    assert(light.color_copy[2] == UINT8_C(0x31));
    assert(light.direction[0] == -1);
    assert(light.direction[1] == 2);
    assert(light.direction[2] == -3);
    assert(ge_gbi_light_decode(light_bytes, 8U, &light)
           == GE_GBI_RSP_PAYLOAD_TRUNCATED);
}

int main(void)
{
    test_viewport_decode();
    test_light_decode();
    puts("GoldenEye RSP viewport and light payload tests passed");
    return 0;
}

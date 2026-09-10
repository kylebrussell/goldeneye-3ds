#include <mbi.h>
#include <gu.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    float source[4][4] = {
        {-1.5f, 2.25f, 0.0f, -0.0f},
        {-32768.0f, 32767.998046875f, -0.5f, 0.5f},
        {-1.0f/65536.0f, 1.0f/65536.0f, 123.25f, -321.75f},
        {1.0f, -1.0f, 16.0f, -16.0f}
    };
    const uint32_t expected[16] = {
        0xfffe0002, 0x00000000, 0x80007fff, 0xffff0000,
        0xffff0000, 0x007bfebe, 0x0001ffff, 0x0010fff0,
        0x80004000, 0x00000000, 0x0000ff80, 0x80008000,
        0xffff0001, 0x40004000, 0x00000000, 0x00000000
    };
    Mtx packed;
    uint32_t words[16];
    float restored[4][4];
    guMtxF2L(source, &packed);
    memcpy(words, &packed, sizeof(words));
    assert(!memcmp(words, expected, sizeof(words)));
    guMtxL2F(restored, &packed);
    for (unsigned row=0; row<4; ++row)
        for (unsigned column=0; column<4; ++column)
            assert(restored[row][column] == source[row][column]);
    puts("native matrix packing: negative/fractional/boundary words and exact round trip passed");
    return 0;
}

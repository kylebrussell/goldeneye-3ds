#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "game/chrObjRandom.h"

int main(void)
{
    static const uint64_t expected_seeds[] = {
        UINT64_C(0x140ec37cf), UINT64_C(0x1630aedd7),
        UINT64_C(0x11f58071e), UINT64_C(0x0fdde372),
        UINT64_C(0x0d9d9dc24), UINT64_C(0x0f12ea100),
        UINT64_C(0x0928759a8), UINT64_C(0x03cd92f19),
    };
    static const uint32_t expected_values[] = {
        UINT32_C(0x40ec37cf), UINT32_C(0x630aedd7),
        UINT32_C(0x1f58071e), UINT32_C(0x0fdde372),
        UINT32_C(0xd9d9dc24), UINT32_C(0xf12ea100),
        UINT32_C(0x928759a8), UINT32_C(0x3cd92f19),
    };
    size_t i;

    g_chrObjRandomSeed = UINT64_C(0xAB8D9F7781280783);
    for (i = 0; i < sizeof(expected_values) / sizeof(expected_values[0]); i++) {
        assert(chrObjRandomGetNext() == expected_values[i]);
        assert(g_chrObjRandomSeed == expected_seeds[i]);
    }

    chrObjRandomSetSeed(UINT32_C(0xffffffff));
    assert(g_chrObjRandomSeed == UINT64_C(0x100000000));
    assert(chrObjRandomGetNext() == UINT32_C(0x80000800));
    assert(g_chrObjRandomSeed == UINT64_C(0x80000800));

    puts("canonical chr/object MIPS RNG transcription passed");
    return 0;
}

#include <stdint.h>
#include <ultra64.h>

#include "game/chrObjRandom.h"

/*
 * Native transcription of src/game/chrObjRandom.s.  Each expression below
 * follows the original 64-bit MIPS instruction sequence in order; this is a
 * platform ISA adapter for the one canonical source that is still assembly,
 * not a replacement random-number generator.
 */
u64 g_chrObjRandomSeed = UINT64_C(0xAB8D9F7781280783);

u32 chrObjRandomGetNext(void)
{
    const u64 old = g_chrObjRandomSeed;
    u64 a2 = old << 63;              /* dsll32 a2,a0,31 */
    u64 a1 = old << 31;              /* dsll   a1,a0,31 */

    a2 >>= 31;                       /* dsrl   a2,a2,31 */
    a1 >>= 32;                       /* dsrl32 a1,a1,0  */
    a2 |= a1;
    a2 ^= (old << 44) >> 32;         /* dsll32/dsrl32 a0 */
    a2 ^= (a2 >> 20) & UINT64_C(0xfff);

    g_chrObjRandomSeed = a2;
    return (u32)a2;                  /* dsll32/dsra32 return preserves low 32 */
}

void chrObjRandomSetSeed(u32 seed)
{
    g_chrObjRandomSeed = (u64)seed + UINT64_C(1); /* daddiu then sd */
}

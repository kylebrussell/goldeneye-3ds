#include <random.h>

u64 g_randomSeed = 0xAB8D9F7781280783ULL;

static u64 random_step(u64 seed)
{
    const u64 rotated_33 = (seed >> 1) | ((seed & 1ULL) << 32);
    const u64 shifted_low_20 = (seed & 0xFFFFFULL) << 12;
    const u64 mixed = rotated_33 ^ shifted_low_20;

    return mixed ^ ((mixed >> 20) & 0xFFFULL);
}

void randomSetSeed(u32 seed)
{
    g_randomSeed = (u64)seed + 1ULL;
}

u32 randomGetNext(void)
{
    g_randomSeed = random_step(g_randomSeed);
    return (u32)g_randomSeed;
}

u32 randomGetNextFrom(u64 *seed)
{
    *seed = random_step(*seed);
    return (u32)*seed;
}

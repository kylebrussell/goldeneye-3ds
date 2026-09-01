#include <ultra64.h>

#ifdef __3DS__
#include <3ds.h>
#else
#include <time.h>
#endif

#define N64_CPU_COUNTER_HZ 46875000ULL

u32 osGetCount(void)
{
#ifdef __3DS__
    return (u32)((osGetTime() * N64_CPU_COUNTER_HZ) / 1000ULL);
#else
    struct timespec now;
    u64 nanoseconds;

    timespec_get(&now, TIME_UTC);
    nanoseconds = ((u64)now.tv_sec * 1000000000ULL) + (u64)now.tv_nsec;
    return (u32)((nanoseconds * N64_CPU_COUNTER_HZ) / 1000000000ULL);
#endif
}

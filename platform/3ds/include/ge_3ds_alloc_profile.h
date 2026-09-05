#ifndef GE_3DS_ALLOC_PROFILE_H
#define GE_3DS_ALLOC_PROFILE_H
#include <stdint.h>
/* Link-boundary allocator requests, not live/resident byte counts. */
void ge_3ds_alloc_profile_enable(int enabled);
void ge_3ds_alloc_profile_snapshot(uint32_t values[6]);
#endif

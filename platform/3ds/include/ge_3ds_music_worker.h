#ifndef GE_3DS_MUSIC_WORKER_H
#define GE_3DS_MUSIC_WORKER_H
#include <stdint.h>
typedef struct Ge3dsMusicWorkerStats {
    uint64_t submissions, execution_ticks, wait_ticks;
    int enabled;
} Ge3dsMusicWorkerStats;
/* One borrowed job at a time. Zero means the caller must execute serially. */
int ge_3ds_music_worker_submit(void (*execute)(void *), void *context);
void ge_3ds_music_worker_wait(void);
void ge_3ds_music_worker_close(void);
void ge_3ds_music_worker_stats(Ge3dsMusicWorkerStats *stats);
#endif

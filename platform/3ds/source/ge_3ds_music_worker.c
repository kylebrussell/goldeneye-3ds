#include "ge_3ds_music_worker.h"
#include <3ds.h>
#include <string.h>

/* The extra application core is available on New 3DS only, and may still be
 * denied by the launcher. Keep the ordinary synchronous path in that case.
 * No libaudio producer, allocator, renderer or NDSP operation runs here. */
static struct {
    Thread thread;
    LightLock lock;
    LightEvent ready, done;
    void (*execute)(void *);
    void *context;
    int attempted, busy, stopping;
    Ge3dsMusicWorkerStats stats;
} worker;

static void music_worker_main(void *unused)
{
    (void)unused;
    for (;;) {
        LightEvent_Wait(&worker.ready);
        LightLock_Lock(&worker.lock);
        int stopping = worker.stopping;
        void (*execute)(void *) = worker.execute;
        void *context = worker.context;
        LightLock_Unlock(&worker.lock);
        if (stopping) return;
        const uint64_t start = svcGetSystemTick();
        execute(context);
        const uint64_t elapsed = svcGetSystemTick() - start;
        LightLock_Lock(&worker.lock);
        worker.stats.execution_ticks += elapsed;
        worker.busy = 0;
        LightLock_Unlock(&worker.lock);
        LightEvent_Signal(&worker.done);
    }
}

static void music_worker_open(void)
{
    bool new_3ds = false;
    if (worker.attempted) return;
    worker.attempted = 1;
    if (R_FAILED(APT_CheckNew3DS(&new_3ds)) || !new_3ds) return;
    LightLock_Init(&worker.lock);
    LightEvent_Init(&worker.ready, RESET_ONESHOT);
    LightEvent_Init(&worker.done, RESET_ONESHOT);
    worker.thread = threadCreate(music_worker_main, NULL, 32U * 1024U,
            0x30, 2, false);
    worker.stats.enabled = worker.thread != NULL;
}

int ge_3ds_music_worker_submit(void (*execute)(void *), void *context)
{
    if (execute == NULL) return 0;
    music_worker_open();
    if (worker.thread == NULL) return 0;
    ge_3ds_music_worker_wait();
    LightLock_Lock(&worker.lock);
    worker.execute = execute;
    worker.context = context;
    worker.busy = 1;
    ++worker.stats.submissions;
    LightEvent_Clear(&worker.done);
    LightLock_Unlock(&worker.lock);
    LightEvent_Signal(&worker.ready);
    return 1;
}

void ge_3ds_music_worker_wait(void)
{
    if (worker.thread == NULL) return;
    const uint64_t start = svcGetSystemTick();
    for (;;) {
        LightLock_Lock(&worker.lock);
        int busy = worker.busy;
        LightLock_Unlock(&worker.lock);
        if (!busy) break;
        LightEvent_Wait(&worker.done);
    }
    worker.stats.wait_ticks += svcGetSystemTick() - start;
}

void ge_3ds_music_worker_close(void)
{
    if (worker.thread != NULL) {
        ge_3ds_music_worker_wait();
        LightLock_Lock(&worker.lock);
        worker.stopping = 1;
        LightLock_Unlock(&worker.lock);
        LightEvent_Signal(&worker.ready);
        threadJoin(worker.thread, U64_MAX);
        threadFree(worker.thread);
    }
    memset(&worker, 0, sizeof(worker));
}

void ge_3ds_music_worker_stats(Ge3dsMusicWorkerStats *stats)
{
    if (stats == NULL) return;
    if (worker.thread != NULL) LightLock_Lock(&worker.lock);
    *stats = worker.stats;
    if (worker.thread != NULL) LightLock_Unlock(&worker.lock);
}

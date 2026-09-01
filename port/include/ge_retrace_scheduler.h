#ifndef GE_RETRACE_SCHEDULER_H
#define GE_RETRACE_SCHEDULER_H

/*
 * Fixed-storage retrace clock for the native frontend.  One instance owns the
 * singleton portable-libultra runtime at a time.  Pumping is deterministic:
 * the frontend supplies elapsed microseconds and chooses how many queued
 * ticks to consume on each call.
 */

#include "ge_libultra_services.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GE_RETRACE_QUEUE_CAPACITY 4U

typedef struct GeRetracePumpReport {
    uint64_t generated_ticks;
    uint64_t delivered_ticks;
    uint64_t dropped_ticks;
    uint64_t pending_ticks;
    uint64_t vi_events_delivered;
    uint64_t vi_events_dropped;
} GeRetracePumpReport;

typedef struct GeRetraceScheduler {
    OSMesg messages[GE_RETRACE_QUEUE_CAPACITY];
    OSMesgQueue queue;
    OSTimer timer;
    GeServiceWaitFn wait;
    void *wait_context;
    uint64_t interval_us;
    uint64_t phase_us;
    uint64_t total_generated_ticks;
    uint64_t total_delivered_ticks;
    uint64_t total_dropped_ticks;
    uint64_t total_vi_events_delivered;
    uint64_t total_vi_events_dropped;
    bool initialized;
    bool running;
    bool forward_vi_events;
} GeRetraceScheduler;

/* Initializes and takes ownership of the portable-libultra singleton. */
int ge_retrace_scheduler_init(
        GeRetraceScheduler *scheduler,
        GeServiceWaitFn wait,
        void *wait_context);

/* Stops, clears counters/queued ticks, and preserves the configured wait hook. */
int ge_retrace_scheduler_reset(GeRetraceScheduler *scheduler);
int ge_retrace_scheduler_start(
        GeRetraceScheduler *scheduler,
        uint64_t interval_us);
int ge_retrace_scheduler_stop(GeRetraceScheduler *scheduler);

/*
 * Advances services, then consumes at most max_ticks queued retraces.  Use
 * SIZE_MAX to drain the complete backlog.  Forwarding emits OS_EVENT_VI once
 * for every consumed tick; a missing/full VI event binding is reported as a
 * dropped VI event and does not affect retrace delivery.
 */
int ge_retrace_scheduler_pump(
        GeRetraceScheduler *scheduler,
        uint64_t elapsed_us,
        size_t max_ticks,
        GeRetracePumpReport *report);
void ge_retrace_scheduler_set_vi_forwarding(
        GeRetraceScheduler *scheduler,
        bool enabled);
bool ge_retrace_scheduler_is_running(const GeRetraceScheduler *scheduler);
size_t ge_retrace_scheduler_pending(const GeRetraceScheduler *scheduler);

#endif

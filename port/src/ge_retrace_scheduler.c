#include "ge_retrace_scheduler.h"

#include <limits.h>
#include <string.h>

#define GE_RETRACE_MESSAGE ((OSMesg)(uintptr_t)1U)

static uint64_t ge_retrace_add_saturating(uint64_t left, uint64_t right)
{
    if (UINT64_MAX - left < right) {
        return UINT64_MAX;
    }
    return left + right;
}

static bool ge_retrace_usec_to_cycles(uint64_t microseconds, OSTime *cycles)
{
    uint64_t seconds = microseconds / UINT64_C(1000000);
    uint64_t remaining_us = microseconds % UINT64_C(1000000);
    uint64_t whole_cycles;
    uint64_t partial_cycles;

    if (cycles == NULL
            || seconds > UINT64_MAX / GE_LIBULTRA_CPU_COUNTER_HZ) {
        return false;
    }
    whole_cycles = seconds * GE_LIBULTRA_CPU_COUNTER_HZ;
    partial_cycles = (remaining_us * GE_LIBULTRA_CPU_COUNTER_HZ)
        / UINT64_C(1000000);
    if (UINT64_MAX - whole_cycles < partial_cycles) {
        return false;
    }

    *cycles = whole_cycles + partial_cycles;
    return *cycles != 0U;
}

static void ge_retrace_queue_init(GeRetraceScheduler *scheduler)
{
    osCreateMesgQueue(
            &scheduler->queue,
            scheduler->messages,
            (s32)GE_RETRACE_QUEUE_CAPACITY);
}

int ge_retrace_scheduler_init(
        GeRetraceScheduler *scheduler,
        GeServiceWaitFn wait,
        void *wait_context)
{
    if (scheduler == NULL) {
        return -1;
    }

    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->wait = wait;
    scheduler->wait_context = wait_context;
    scheduler->timer.native_timer_id = GE_SERVICE_INVALID_TIMER;
    ge_libultra_services_init(wait, wait_context);
    ge_retrace_queue_init(scheduler);
    scheduler->initialized = true;
    return 0;
}

int ge_retrace_scheduler_reset(GeRetraceScheduler *scheduler)
{
    GeServiceWaitFn wait;
    void *wait_context;

    if (scheduler == NULL || !scheduler->initialized) {
        return -1;
    }

    wait = scheduler->wait;
    wait_context = scheduler->wait_context;
    return ge_retrace_scheduler_init(scheduler, wait, wait_context);
}

int ge_retrace_scheduler_start(
        GeRetraceScheduler *scheduler,
        uint64_t interval_us)
{
    OSTime interval_cycles;

    if (scheduler == NULL || !scheduler->initialized || interval_us == 0U) {
        return -1;
    }
    if (!ge_retrace_usec_to_cycles(interval_us, &interval_cycles)) {
        return -1;
    }

    if (scheduler->running && ge_retrace_scheduler_stop(scheduler) != 0) {
        return -1;
    }

    /* Starting a new cadence intentionally discards an old stopped backlog. */
    ge_retrace_queue_init(scheduler);
    scheduler->phase_us = 0U;
    scheduler->interval_us = interval_us;
    if (osSetTimer(
                &scheduler->timer,
                interval_cycles,
                interval_cycles,
                &scheduler->queue,
                GE_RETRACE_MESSAGE) != 0) {
        scheduler->interval_us = 0U;
        return -1;
    }

    scheduler->running = true;
    return 0;
}

int ge_retrace_scheduler_stop(GeRetraceScheduler *scheduler)
{
    if (scheduler == NULL || !scheduler->initialized) {
        return -1;
    }
    if (!scheduler->running) {
        return 0;
    }

    if (osStopTimer(&scheduler->timer) != 0) {
        scheduler->running = false;
        scheduler->interval_us = 0U;
        scheduler->phase_us = 0U;
        return -1;
    }

    scheduler->running = false;
    scheduler->interval_us = 0U;
    scheduler->phase_us = 0U;
    return 0;
}

static uint64_t ge_retrace_due_ticks(
        GeRetraceScheduler *scheduler,
        uint64_t elapsed_us)
{
    uint64_t whole_ticks;
    uint64_t remainder;

    if (!scheduler->running) {
        return 0U;
    }

    whole_ticks = elapsed_us / scheduler->interval_us;
    remainder = elapsed_us % scheduler->interval_us;
    if (remainder >= scheduler->interval_us - scheduler->phase_us) {
        whole_ticks = ge_retrace_add_saturating(whole_ticks, 1U);
        scheduler->phase_us = remainder
            - (scheduler->interval_us - scheduler->phase_us);
    } else {
        scheduler->phase_us += remainder;
    }
    return whole_ticks;
}

int ge_retrace_scheduler_pump(
        GeRetraceScheduler *scheduler,
        uint64_t elapsed_us,
        size_t max_ticks,
        GeRetracePumpReport *report)
{
    GeRetracePumpReport local_report;
    uint64_t due_ticks;
    uint64_t pending_before;
    uint64_t queue_space;
    size_t ignored_service_drops = 0U;
    size_t ticks_to_drain;
    size_t index;

    if (scheduler == NULL || !scheduler->initialized) {
        return -1;
    }

    memset(&local_report, 0, sizeof(local_report));
    pending_before = (uint64_t)ge_service_queue_count(
            &scheduler->queue.native_queue);
    due_ticks = ge_retrace_due_ticks(scheduler, elapsed_us);
    queue_space = GE_RETRACE_QUEUE_CAPACITY - pending_before;
    local_report.generated_ticks = due_ticks;
    local_report.dropped_ticks = due_ticks > queue_space
        ? due_ticks - queue_space
        : 0U;

    (void)ge_libultra_services_pump(elapsed_us, &ignored_service_drops);

    ticks_to_drain = ge_service_queue_count(&scheduler->queue.native_queue);
    if (ticks_to_drain > max_ticks) {
        ticks_to_drain = max_ticks;
    }
    for (index = 0U; index < ticks_to_drain; index++) {
        OSMesg message = NULL;

        if (osRecvMesg(
                    &scheduler->queue,
                    &message,
                    OS_MESG_NOBLOCK) != 0) {
            break;
        }
        (void)message;
        local_report.delivered_ticks++;
        if (scheduler->forward_vi_events) {
            if (ge_libultra_services_emit_event(OS_EVENT_VI) == 0) {
                local_report.vi_events_delivered++;
            } else {
                local_report.vi_events_dropped++;
            }
        }
    }

    local_report.pending_ticks = (uint64_t)ge_service_queue_count(
            &scheduler->queue.native_queue);
    scheduler->total_generated_ticks = ge_retrace_add_saturating(
            scheduler->total_generated_ticks,
            local_report.generated_ticks);
    scheduler->total_delivered_ticks = ge_retrace_add_saturating(
            scheduler->total_delivered_ticks,
            local_report.delivered_ticks);
    scheduler->total_dropped_ticks = ge_retrace_add_saturating(
            scheduler->total_dropped_ticks,
            local_report.dropped_ticks);
    scheduler->total_vi_events_delivered = ge_retrace_add_saturating(
            scheduler->total_vi_events_delivered,
            local_report.vi_events_delivered);
    scheduler->total_vi_events_dropped = ge_retrace_add_saturating(
            scheduler->total_vi_events_dropped,
            local_report.vi_events_dropped);

    if (report != NULL) {
        *report = local_report;
    }
    return 0;
}

void ge_retrace_scheduler_set_vi_forwarding(
        GeRetraceScheduler *scheduler,
        bool enabled)
{
    if (scheduler != NULL && scheduler->initialized) {
        scheduler->forward_vi_events = enabled;
    }
}

bool ge_retrace_scheduler_is_running(const GeRetraceScheduler *scheduler)
{
    return scheduler != NULL && scheduler->initialized && scheduler->running;
}

size_t ge_retrace_scheduler_pending(const GeRetraceScheduler *scheduler)
{
    if (scheduler == NULL || !scheduler->initialized) {
        return 0U;
    }
    return ge_service_queue_count(&scheduler->queue.native_queue);
}

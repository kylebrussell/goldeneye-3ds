#include "ge_retrace_scheduler.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define MESSAGE(value) ((OSMesg)(uintptr_t)(value))

static void test_cadence_backlog_and_drops(void)
{
    GeRetraceScheduler scheduler;
    GeRetracePumpReport report;

    assert(ge_retrace_scheduler_init(&scheduler, NULL, NULL) == 0);
    assert(ge_retrace_scheduler_start(&scheduler, 100U) == 0);
    assert(ge_retrace_scheduler_is_running(&scheduler));

    assert(ge_retrace_scheduler_pump(&scheduler, 99U, SIZE_MAX, &report) == 0);
    assert(report.generated_ticks == 0U);
    assert(report.delivered_ticks == 0U);
    assert(report.pending_ticks == 0U);

    assert(ge_retrace_scheduler_pump(&scheduler, 1U, SIZE_MAX, &report) == 0);
    assert(report.generated_ticks == 1U);
    assert(report.delivered_ticks == 1U);
    assert(report.dropped_ticks == 0U);

    assert(ge_retrace_scheduler_pump(&scheduler, 350U, 2U, &report) == 0);
    assert(report.generated_ticks == 3U);
    assert(report.delivered_ticks == 2U);
    assert(report.pending_ticks == 1U);

    assert(ge_retrace_scheduler_pump(&scheduler, 500U, 0U, &report) == 0);
    assert(report.generated_ticks == 5U);
    assert(report.delivered_ticks == 0U);
    assert(report.dropped_ticks == 2U);
    assert(report.pending_ticks == GE_RETRACE_QUEUE_CAPACITY);

    assert(ge_retrace_scheduler_pump(&scheduler, 0U, SIZE_MAX, &report) == 0);
    assert(report.generated_ticks == 0U);
    assert(report.delivered_ticks == GE_RETRACE_QUEUE_CAPACITY);
    assert(report.pending_ticks == 0U);
    assert(scheduler.total_generated_ticks == 9U);
    assert(scheduler.total_delivered_ticks == 7U);
    assert(scheduler.total_dropped_ticks == 2U);
}

static void test_vi_forwarding(void)
{
    GeRetraceScheduler scheduler;
    GeRetracePumpReport report;
    OSMesg vi_storage[1];
    OSMesgQueue vi_queue;
    OSMesg message = NULL;

    assert(ge_retrace_scheduler_init(&scheduler, NULL, NULL) == 0);
    osCreateMesgQueue(&vi_queue, vi_storage, 1);
    osSetEventMesg(OS_EVENT_VI, &vi_queue, MESSAGE(0x5649U));
    ge_retrace_scheduler_set_vi_forwarding(&scheduler, true);
    assert(ge_retrace_scheduler_start(&scheduler, 50U) == 0);
    assert(ge_retrace_scheduler_pump(&scheduler, 100U, SIZE_MAX, &report) == 0);
    assert(report.delivered_ticks == 2U);
    assert(report.vi_events_delivered == 1U);
    assert(report.vi_events_dropped == 1U);
    assert(osRecvMesg(&vi_queue, &message, OS_MESG_NOBLOCK) == 0);
    assert(message == MESSAGE(0x5649U));
}

static void test_start_stop_reset(void)
{
    GeRetraceScheduler scheduler;
    GeRetracePumpReport report;

    assert(ge_retrace_scheduler_init(&scheduler, NULL, NULL) == 0);
    assert(ge_retrace_scheduler_start(&scheduler, 100U) == 0);
    assert(ge_retrace_scheduler_pump(&scheduler, 200U, 0U, &report) == 0);
    assert(ge_retrace_scheduler_pending(&scheduler) == 2U);
    assert(ge_retrace_scheduler_stop(&scheduler) == 0);
    assert(ge_retrace_scheduler_stop(&scheduler) == 0);
    assert(!ge_retrace_scheduler_is_running(&scheduler));

    assert(ge_retrace_scheduler_pump(&scheduler, 500U, SIZE_MAX, &report) == 0);
    assert(report.generated_ticks == 0U);
    assert(report.delivered_ticks == 2U);
    assert(report.pending_ticks == 0U);

    assert(ge_retrace_scheduler_start(&scheduler, 25U) == 0);
    assert(ge_retrace_scheduler_pump(&scheduler, 25U, 0U, &report) == 0);
    assert(ge_retrace_scheduler_pending(&scheduler) == 1U);
    assert(ge_retrace_scheduler_reset(&scheduler) == 0);
    assert(!ge_retrace_scheduler_is_running(&scheduler));
    assert(ge_retrace_scheduler_pending(&scheduler) == 0U);
    assert(scheduler.total_generated_ticks == 0U);
    assert(scheduler.total_delivered_ticks == 0U);
}

static void test_invalid_calls(void)
{
    GeRetraceScheduler scheduler = {0};

    assert(ge_retrace_scheduler_init(NULL, NULL, NULL) == -1);
    assert(ge_retrace_scheduler_start(&scheduler, 100U) == -1);
    assert(ge_retrace_scheduler_stop(&scheduler) == -1);
    assert(ge_retrace_scheduler_reset(&scheduler) == -1);
    assert(ge_retrace_scheduler_pump(&scheduler, 1U, 1U, NULL) == -1);
    assert(ge_retrace_scheduler_init(&scheduler, NULL, NULL) == 0);
    assert(ge_retrace_scheduler_start(&scheduler, 0U) == -1);
    assert(ge_retrace_scheduler_start(&scheduler, UINT64_MAX) == -1);
}

int main(void)
{
    test_cadence_backlog_and_drops();
    test_vi_forwarding();
    test_start_stop_reset();
    test_invalid_calls();
    puts("portable retrace scheduler bridge tests passed");
    return 0;
}

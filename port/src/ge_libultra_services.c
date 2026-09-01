#include "ge_libultra_services.h"

#include <string.h>

u64 osClockRate = GE_LIBULTRA_CPU_COUNTER_HZ;

static GeServiceRuntime g_libultra_runtime;
static OSTimer *g_timer_owners[GE_SERVICE_MAX_TIMERS];
static bool g_libultra_initialized;

static uint32_t ge_libultra_find_timer(const OSTimer *timer)
{
    uint32_t timer_id;

    for (timer_id = 0U; timer_id < GE_SERVICE_MAX_TIMERS; timer_id++) {
        if (g_timer_owners[timer_id] == timer) {
            return timer_id;
        }
    }
    return GE_SERVICE_INVALID_TIMER;
}

static void ge_libultra_ensure_initialized(void)
{
    if (!g_libultra_initialized) {
        ge_libultra_services_init(NULL, NULL);
    }
}

static GeServiceWaitMode ge_libultra_wait_mode(s32 flags, bool *valid)
{
    if (flags == OS_MESG_NOBLOCK) {
        *valid = true;
        return GE_SERVICE_NOBLOCK;
    }
    if (flags == OS_MESG_BLOCK) {
        *valid = true;
        return GE_SERVICE_BLOCK;
    }

    *valid = false;
    return GE_SERVICE_NOBLOCK;
}

static uint64_t ge_libultra_cycles_to_us_ceil(OSTime cycles)
{
    /* 46,875,000 cycles/s is exactly 375 cycles per 8 microseconds. */
    uint64_t whole_groups = cycles / UINT64_C(375);
    uint64_t remainder = cycles % UINT64_C(375);
    uint64_t microseconds = whole_groups * UINT64_C(8);

    if (remainder != 0U) {
        microseconds += (remainder * UINT64_C(8) + UINT64_C(374))
            / UINT64_C(375);
    }
    return microseconds;
}

void ge_libultra_services_init(GeServiceWaitFn wait, void *wait_context)
{
    /* Reset is intended for startup/tests and invalidates all prior handles. */
    memset(g_timer_owners, 0, sizeof(g_timer_owners));
    ge_service_runtime_init(&g_libultra_runtime, wait, wait_context);
    g_libultra_initialized = true;
}

size_t ge_libultra_services_pump(
        uint64_t elapsed_us,
        size_t *dropped_messages)
{
    size_t delivered;
    uint32_t timer_id;

    ge_libultra_ensure_initialized();
    delivered = ge_service_advance(
            &g_libultra_runtime,
            elapsed_us,
            dropped_messages);

    for (timer_id = 0U; timer_id < GE_SERVICE_MAX_TIMERS; timer_id++) {
        if (g_timer_owners[timer_id] != NULL
                && !g_libultra_runtime.timers[timer_id].active) {
            g_timer_owners[timer_id]->active = false;
            g_timer_owners[timer_id]->native_timer_id =
                GE_SERVICE_INVALID_TIMER;
            g_timer_owners[timer_id] = NULL;
        }
    }
    return delivered;
}

int32_t ge_libultra_services_emit_event(OSEvent event)
{
    ge_libultra_ensure_initialized();
    return ge_service_event_emit(
            &g_libultra_runtime,
            (GeServiceEvent)event) == GE_SERVICE_OK ? 0 : -1;
}

uint64_t ge_libultra_services_now_us(void)
{
    ge_libultra_ensure_initialized();
    return ge_service_now_us(&g_libultra_runtime);
}

void osCreateMesgQueue(OSMesgQueue *queue, OSMesg *messages, s32 message_count)
{
    ge_libultra_ensure_initialized();
    if (queue == NULL) {
        return;
    }

    if (message_count <= 0
            || ge_service_queue_init(
                &queue->native_queue,
                messages,
                (size_t)message_count) != GE_SERVICE_OK) {
        memset(&queue->native_queue, 0, sizeof(queue->native_queue));
    }
}

s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags)
{
    GeServiceWaitMode wait_mode;
    bool valid;

    ge_libultra_ensure_initialized();
    wait_mode = ge_libultra_wait_mode(flags, &valid);
    if (!valid || queue == NULL) {
        return -1;
    }

    return ge_service_queue_send(
            &g_libultra_runtime,
            &queue->native_queue,
            message,
            wait_mode) == GE_SERVICE_OK ? 0 : -1;
}

s32 osJamMesg(OSMesgQueue *queue, OSMesg message, s32 flags)
{
    GeServiceWaitMode wait_mode;
    bool valid;

    ge_libultra_ensure_initialized();
    wait_mode = ge_libultra_wait_mode(flags, &valid);
    if (!valid || queue == NULL) {
        return -1;
    }

    return ge_service_queue_jam(
            &g_libultra_runtime,
            &queue->native_queue,
            message,
            wait_mode) == GE_SERVICE_OK ? 0 : -1;
}

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags)
{
    GeServiceWaitMode wait_mode;
    bool valid;

    ge_libultra_ensure_initialized();
    wait_mode = ge_libultra_wait_mode(flags, &valid);
    if (!valid || queue == NULL) {
        return -1;
    }

    return ge_service_queue_receive(
            &g_libultra_runtime,
            &queue->native_queue,
            message,
            wait_mode) == GE_SERVICE_OK ? 0 : -1;
}

void osSetEventMesg(OSEvent event, OSMesgQueue *queue, OSMesg message)
{
    ge_libultra_ensure_initialized();
    if (queue == NULL) {
        (void)ge_service_event_unbind(
                &g_libultra_runtime,
                (GeServiceEvent)event);
        return;
    }

    (void)ge_service_event_bind(
            &g_libultra_runtime,
            (GeServiceEvent)event,
            &queue->native_queue,
            message);
}

int osSetTimer(
        OSTimer *timer,
        OSTime value,
        OSTime interval,
        OSMesgQueue *queue,
        OSMesg message)
{
    OSTime first_expiration;
    uint32_t previous_timer_id;
    uint32_t timer_id;

    ge_libultra_ensure_initialized();
    if (timer == NULL || queue == NULL) {
        return -1;
    }

    /* libultra callers are not required to zero-initialize OSTimer storage. */
    previous_timer_id = ge_libultra_find_timer(timer);
    if (previous_timer_id != GE_SERVICE_INVALID_TIMER) {
        (void)ge_service_timer_cancel(
                &g_libultra_runtime,
                previous_timer_id);
        g_timer_owners[previous_timer_id] = NULL;
    }

    first_expiration = value != 0U ? value : interval;
    timer_id = ge_service_timer_start(
            &g_libultra_runtime,
            ge_libultra_cycles_to_us_ceil(first_expiration),
            ge_libultra_cycles_to_us_ceil(interval),
            &queue->native_queue,
            message);
    if (timer_id == GE_SERVICE_INVALID_TIMER) {
        timer->native_timer_id = GE_SERVICE_INVALID_TIMER;
        timer->active = false;
        return -1;
    }

    timer->native_timer_id = timer_id;
    timer->active = true;
    g_timer_owners[timer_id] = timer;
    return 0;
}

int osStopTimer(OSTimer *timer)
{
    uint32_t timer_id;
    GeServiceResult result;

    ge_libultra_ensure_initialized();
    if (timer == NULL) {
        return -1;
    }

    timer_id = ge_libultra_find_timer(timer);
    if (timer_id == GE_SERVICE_INVALID_TIMER) {
        timer->native_timer_id = GE_SERVICE_INVALID_TIMER;
        timer->active = false;
        return -1;
    }

    result = ge_service_timer_cancel(&g_libultra_runtime, timer_id);
    g_timer_owners[timer_id] = NULL;
    timer->native_timer_id = GE_SERVICE_INVALID_TIMER;
    timer->active = false;
    return result == GE_SERVICE_OK ? 0 : -1;
}

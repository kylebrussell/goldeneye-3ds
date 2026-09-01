#include "ge_services.h"

#include <limits.h>
#include <string.h>

static bool ge_service_queue_is_valid(const GeServiceQueue *queue)
{
    return queue != NULL && queue->messages != NULL && queue->capacity != 0U;
}

static bool ge_service_wait_once(GeServiceRuntime *runtime)
{
    return runtime != NULL && runtime->wait != NULL
        && runtime->wait(runtime->wait_context);
}

static uint64_t ge_service_add_saturating(uint64_t left, uint64_t right)
{
    if (UINT64_MAX - left < right) {
        return UINT64_MAX;
    }

    return left + right;
}

void ge_service_runtime_init(
        GeServiceRuntime *runtime,
        GeServiceWaitFn wait,
        void *wait_context)
{
    if (runtime == NULL) {
        return;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->wait = wait;
    runtime->wait_context = wait_context;
}

GeServiceResult ge_service_queue_init(
        GeServiceQueue *queue,
        GeServiceMessage *storage,
        size_t capacity)
{
    if (queue == NULL || storage == NULL || capacity == 0U) {
        return GE_SERVICE_INVALID;
    }

    queue->messages = storage;
    queue->capacity = capacity;
    queue->count = 0U;
    queue->first = 0U;
    return GE_SERVICE_OK;
}

GeServiceResult ge_service_queue_send(
        GeServiceRuntime *runtime,
        GeServiceQueue *queue,
        GeServiceMessage message,
        GeServiceWaitMode wait_mode)
{
    size_t last;

    if (!ge_service_queue_is_valid(queue)
            || (wait_mode != GE_SERVICE_NOBLOCK
                && wait_mode != GE_SERVICE_BLOCK)) {
        return GE_SERVICE_INVALID;
    }

    while (queue->count == queue->capacity) {
        if (wait_mode == GE_SERVICE_NOBLOCK || !ge_service_wait_once(runtime)) {
            return GE_SERVICE_WOULD_BLOCK;
        }
    }

    last = (queue->first + queue->count) % queue->capacity;
    queue->messages[last] = message;
    queue->count++;
    return GE_SERVICE_OK;
}

GeServiceResult ge_service_queue_jam(
        GeServiceRuntime *runtime,
        GeServiceQueue *queue,
        GeServiceMessage message,
        GeServiceWaitMode wait_mode)
{
    if (!ge_service_queue_is_valid(queue)
            || (wait_mode != GE_SERVICE_NOBLOCK
                && wait_mode != GE_SERVICE_BLOCK)) {
        return GE_SERVICE_INVALID;
    }

    while (queue->count == queue->capacity) {
        if (wait_mode == GE_SERVICE_NOBLOCK || !ge_service_wait_once(runtime)) {
            return GE_SERVICE_WOULD_BLOCK;
        }
    }

    queue->first = (queue->first + queue->capacity - 1U) % queue->capacity;
    queue->messages[queue->first] = message;
    queue->count++;
    return GE_SERVICE_OK;
}

GeServiceResult ge_service_queue_receive(
        GeServiceRuntime *runtime,
        GeServiceQueue *queue,
        GeServiceMessage *message,
        GeServiceWaitMode wait_mode)
{
    if (!ge_service_queue_is_valid(queue)
            || (wait_mode != GE_SERVICE_NOBLOCK
                && wait_mode != GE_SERVICE_BLOCK)) {
        return GE_SERVICE_INVALID;
    }

    while (queue->count == 0U) {
        if (wait_mode == GE_SERVICE_NOBLOCK || !ge_service_wait_once(runtime)) {
            return GE_SERVICE_WOULD_BLOCK;
        }
    }

    if (message != NULL) {
        *message = queue->messages[queue->first];
    }
    queue->first = (queue->first + 1U) % queue->capacity;
    queue->count--;
    return GE_SERVICE_OK;
}

size_t ge_service_queue_count(const GeServiceQueue *queue)
{
    if (!ge_service_queue_is_valid(queue)) {
        return 0U;
    }

    return queue->count;
}

GeServiceResult ge_service_event_bind(
        GeServiceRuntime *runtime,
        GeServiceEvent event,
        GeServiceQueue *queue,
        GeServiceMessage message)
{
    if (runtime == NULL || (unsigned int)event >= GE_SERVICE_EVENT_COUNT
            || !ge_service_queue_is_valid(queue)) {
        return GE_SERVICE_INVALID;
    }

    runtime->events[event].queue = queue;
    runtime->events[event].message = message;
    return GE_SERVICE_OK;
}

GeServiceResult ge_service_event_unbind(
        GeServiceRuntime *runtime,
        GeServiceEvent event)
{
    if (runtime == NULL || (unsigned int)event >= GE_SERVICE_EVENT_COUNT) {
        return GE_SERVICE_INVALID;
    }

    runtime->events[event].queue = NULL;
    runtime->events[event].message = NULL;
    return GE_SERVICE_OK;
}

GeServiceResult ge_service_event_emit(
        GeServiceRuntime *runtime,
        GeServiceEvent event)
{
    GeServiceEventBinding *binding;

    if (runtime == NULL || (unsigned int)event >= GE_SERVICE_EVENT_COUNT) {
        return GE_SERVICE_INVALID;
    }

    binding = &runtime->events[event];
    if (binding->queue == NULL) {
        return GE_SERVICE_INVALID;
    }

    return ge_service_queue_send(
            runtime,
            binding->queue,
            binding->message,
            GE_SERVICE_NOBLOCK);
}

uint32_t ge_service_timer_start(
        GeServiceRuntime *runtime,
        uint64_t delay_us,
        uint64_t interval_us,
        GeServiceQueue *queue,
        GeServiceMessage message)
{
    uint32_t timer_id;

    if (runtime == NULL || !ge_service_queue_is_valid(queue)) {
        return GE_SERVICE_INVALID_TIMER;
    }

    for (timer_id = 0U; timer_id < GE_SERVICE_MAX_TIMERS; timer_id++) {
        GeServiceTimer *timer = &runtime->timers[timer_id];

        if (!timer->active) {
            timer->queue = queue;
            timer->message = message;
            timer->deadline_us = ge_service_add_saturating(
                    runtime->now_us,
                    delay_us);
            timer->interval_us = interval_us;
            timer->active = true;
            return timer_id;
        }
    }

    return GE_SERVICE_INVALID_TIMER;
}

GeServiceResult ge_service_timer_cancel(
        GeServiceRuntime *runtime,
        uint32_t timer_id)
{
    if (runtime == NULL || timer_id >= GE_SERVICE_MAX_TIMERS
            || !runtime->timers[timer_id].active) {
        return GE_SERVICE_INVALID;
    }

    memset(&runtime->timers[timer_id], 0, sizeof(runtime->timers[timer_id]));
    return GE_SERVICE_OK;
}

static size_t ge_service_deliver_timer(
        GeServiceRuntime *runtime,
        GeServiceTimer *timer,
        size_t occurrences,
        size_t *dropped_messages)
{
    size_t delivered = 0U;
    size_t available = timer->queue->capacity - timer->queue->count;
    size_t to_deliver = occurrences < available ? occurrences : available;
    size_t index;

    for (index = 0U; index < to_deliver; index++) {
        if (ge_service_queue_send(
                    runtime,
                    timer->queue,
                    timer->message,
                    GE_SERVICE_NOBLOCK) == GE_SERVICE_OK) {
            delivered++;
        }
    }

    if (dropped_messages != NULL) {
        *dropped_messages += occurrences - delivered;
    }
    return delivered;
}

size_t ge_service_advance(
        GeServiceRuntime *runtime,
        uint64_t delta_us,
        size_t *dropped_messages)
{
    size_t delivered = 0U;
    uint32_t timer_id;

    if (dropped_messages != NULL) {
        *dropped_messages = 0U;
    }
    if (runtime == NULL) {
        return 0U;
    }

    runtime->now_us = ge_service_add_saturating(runtime->now_us, delta_us);

    for (timer_id = 0U; timer_id < GE_SERVICE_MAX_TIMERS; timer_id++) {
        GeServiceTimer *timer = &runtime->timers[timer_id];
        size_t occurrences;

        if (!timer->active || runtime->now_us < timer->deadline_us) {
            continue;
        }

        if (timer->interval_us == 0U) {
            occurrences = 1U;
            delivered += ge_service_deliver_timer(
                    runtime,
                    timer,
                    occurrences,
                    dropped_messages);
            memset(timer, 0, sizeof(*timer));
            continue;
        }

        {
            uint64_t overdue = runtime->now_us - timer->deadline_us;
            uint64_t occurrences_u64 = 1U + overdue / timer->interval_us;
            uint64_t phase = overdue % timer->interval_us;

            occurrences = occurrences_u64 > (uint64_t)SIZE_MAX
                ? SIZE_MAX
                : (size_t)occurrences_u64;
            if (UINT64_MAX - runtime->now_us < timer->interval_us - phase) {
                timer->active = false;
            } else {
                timer->deadline_us = runtime->now_us
                    + timer->interval_us - phase;
            }
        }

        delivered += ge_service_deliver_timer(
                runtime,
                timer,
                occurrences,
                dropped_messages);
    }

    return delivered;
}

uint64_t ge_service_now_us(const GeServiceRuntime *runtime)
{
    return runtime != NULL ? runtime->now_us : 0U;
}

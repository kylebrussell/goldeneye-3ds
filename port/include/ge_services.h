#ifndef GE_SERVICES_H
#define GE_SERVICES_H

/*
 * Cooperative, allocation-free services used while moving GoldenEye's
 * libultra scheduler onto native platforms.  Time is expressed in
 * microseconds and messages are pointer-sized, matching libultra's OSMesg.
 *
 * Queue operations are intentionally not internally synchronized.  A native
 * frontend may call them from one scheduler thread, or put a lock around the
 * service boundary if producers run on multiple threads.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GE_SERVICE_EVENT_COUNT 23U
#define GE_SERVICE_MAX_TIMERS 16U
#define GE_SERVICE_INVALID_TIMER UINT32_MAX

typedef void *GeServiceMessage;

typedef enum GeServiceResult {
    GE_SERVICE_OK = 0,
    GE_SERVICE_WOULD_BLOCK = -1,
    GE_SERVICE_INVALID = -2
} GeServiceResult;

typedef enum GeServiceWaitMode {
    GE_SERVICE_NOBLOCK = 0,
    GE_SERVICE_BLOCK = 1
} GeServiceWaitMode;

typedef enum GeServiceEvent {
    GE_SERVICE_EVENT_SW1 = 0,
    GE_SERVICE_EVENT_SW2,
    GE_SERVICE_EVENT_CART,
    GE_SERVICE_EVENT_COUNTER,
    GE_SERVICE_EVENT_SP,
    GE_SERVICE_EVENT_SI,
    GE_SERVICE_EVENT_AI,
    GE_SERVICE_EVENT_VI,
    GE_SERVICE_EVENT_PI,
    GE_SERVICE_EVENT_DP,
    GE_SERVICE_EVENT_CPU_BREAK,
    GE_SERVICE_EVENT_SP_BREAK,
    GE_SERVICE_EVENT_FAULT,
    GE_SERVICE_EVENT_THREAD_STATUS,
    GE_SERVICE_EVENT_PRE_NMI,
    GE_SERVICE_EVENT_RDB_READ_DONE,
    GE_SERVICE_EVENT_RDB_LOG_DONE,
    GE_SERVICE_EVENT_RDB_DATA_DONE,
    GE_SERVICE_EVENT_RDB_REQ_RAMROM,
    GE_SERVICE_EVENT_RDB_FREE_RAMROM,
    GE_SERVICE_EVENT_RDB_DBG_DONE,
    GE_SERVICE_EVENT_RDB_FLUSH_PROF,
    GE_SERVICE_EVENT_RDB_ACK_PROF
} GeServiceEvent;

typedef struct GeServiceQueue {
    GeServiceMessage *messages;
    size_t capacity;
    size_t count;
    size_t first;
} GeServiceQueue;

/*
 * Called when a blocking queue operation cannot make immediate progress.
 * The callback should yield or pump the native event source.  Returning false
 * aborts the operation with GE_SERVICE_WOULD_BLOCK.
 */
typedef bool (*GeServiceWaitFn)(void *context);

typedef struct GeServiceEventBinding {
    GeServiceQueue *queue;
    GeServiceMessage message;
} GeServiceEventBinding;

typedef struct GeServiceTimer {
    GeServiceQueue *queue;
    GeServiceMessage message;
    uint64_t deadline_us;
    uint64_t interval_us;
    bool active;
} GeServiceTimer;

typedef struct GeServiceRuntime {
    GeServiceWaitFn wait;
    void *wait_context;
    uint64_t now_us;
    GeServiceEventBinding events[GE_SERVICE_EVENT_COUNT];
    GeServiceTimer timers[GE_SERVICE_MAX_TIMERS];
} GeServiceRuntime;

void ge_service_runtime_init(
        GeServiceRuntime *runtime,
        GeServiceWaitFn wait,
        void *wait_context);

GeServiceResult ge_service_queue_init(
        GeServiceQueue *queue,
        GeServiceMessage *storage,
        size_t capacity);
GeServiceResult ge_service_queue_send(
        GeServiceRuntime *runtime,
        GeServiceQueue *queue,
        GeServiceMessage message,
        GeServiceWaitMode wait_mode);
GeServiceResult ge_service_queue_jam(
        GeServiceRuntime *runtime,
        GeServiceQueue *queue,
        GeServiceMessage message,
        GeServiceWaitMode wait_mode);
GeServiceResult ge_service_queue_receive(
        GeServiceRuntime *runtime,
        GeServiceQueue *queue,
        GeServiceMessage *message,
        GeServiceWaitMode wait_mode);
size_t ge_service_queue_count(const GeServiceQueue *queue);

GeServiceResult ge_service_event_bind(
        GeServiceRuntime *runtime,
        GeServiceEvent event,
        GeServiceQueue *queue,
        GeServiceMessage message);
GeServiceResult ge_service_event_unbind(
        GeServiceRuntime *runtime,
        GeServiceEvent event);
GeServiceResult ge_service_event_emit(
        GeServiceRuntime *runtime,
        GeServiceEvent event);

uint32_t ge_service_timer_start(
        GeServiceRuntime *runtime,
        uint64_t delay_us,
        uint64_t interval_us,
        GeServiceQueue *queue,
        GeServiceMessage message);
GeServiceResult ge_service_timer_cancel(
        GeServiceRuntime *runtime,
        uint32_t timer_id);

/* Advances the deterministic service clock and enqueues all due timer events. */
size_t ge_service_advance(
        GeServiceRuntime *runtime,
        uint64_t delta_us,
        size_t *dropped_messages);
uint64_t ge_service_now_us(const GeServiceRuntime *runtime);

#endif

#ifndef GE_LIBULTRA_SERVICES_H
#define GE_LIBULTRA_SERVICES_H

/*
 * Source-compatibility adapter for the message, event, and timer portion of
 * libultra.  This is a native-port ABI: its queue/timer structures are not
 * binary-compatible with the Nintendo 64 structures.
 *
 * Blocking calls are cooperative.  The wait callback installed by
 * ge_libultra_services_init() must yield or pump native events and return true
 * while the operation should keep waiting.  Without a callback, a blocking
 * call that cannot progress returns -1 instead of spinning forever.
 */

#include "ge_services.h"
#include "ultra64.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GE_LIBULTRA_CPU_COUNTER_HZ UINT64_C(46875000)

#ifndef OS_MESG_NOBLOCK
#define OS_MESG_NOBLOCK 0
#endif
#ifndef OS_MESG_BLOCK
#define OS_MESG_BLOCK 1
#endif

#ifndef OS_CLOCK_RATE
#define OS_CLOCK_RATE UINT64_C(62500000)
#endif
#ifndef OS_CPU_COUNTER
#define OS_CPU_COUNTER GE_LIBULTRA_CPU_COUNTER_HZ
#endif
#ifndef OS_USEC_TO_CYCLES
#define OS_USEC_TO_CYCLES(usec) \
    (((uint64_t)(usec) * GE_LIBULTRA_CPU_COUNTER_HZ) / UINT64_C(1000000))
#endif
#ifndef OS_CYCLES_TO_USEC
#define OS_CYCLES_TO_USEC(cycles) \
    (((uint64_t)(cycles) * UINT64_C(1000000)) \
        / GE_LIBULTRA_CPU_COUNTER_HZ)
#endif

typedef void *OSMesg;
typedef uint32_t OSEvent;
typedef uint64_t OSTime;

typedef struct OSMesgQueue {
    GeServiceQueue native_queue;
} OSMesgQueue;

typedef struct OSTimer {
    uint32_t native_timer_id;
    bool active;
} OSTimer;

#define OS_EVENT_SW1 ((OSEvent)GE_SERVICE_EVENT_SW1)
#define OS_EVENT_SW2 ((OSEvent)GE_SERVICE_EVENT_SW2)
#define OS_EVENT_CART ((OSEvent)GE_SERVICE_EVENT_CART)
#define OS_EVENT_COUNTER ((OSEvent)GE_SERVICE_EVENT_COUNTER)
#define OS_EVENT_SP ((OSEvent)GE_SERVICE_EVENT_SP)
#define OS_EVENT_SI ((OSEvent)GE_SERVICE_EVENT_SI)
#define OS_EVENT_AI ((OSEvent)GE_SERVICE_EVENT_AI)
#define OS_EVENT_VI ((OSEvent)GE_SERVICE_EVENT_VI)
#define OS_EVENT_PI ((OSEvent)GE_SERVICE_EVENT_PI)
#define OS_EVENT_DP ((OSEvent)GE_SERVICE_EVENT_DP)
#define OS_EVENT_CPU_BREAK ((OSEvent)GE_SERVICE_EVENT_CPU_BREAK)
#define OS_EVENT_SP_BREAK ((OSEvent)GE_SERVICE_EVENT_SP_BREAK)
#define OS_EVENT_FAULT ((OSEvent)GE_SERVICE_EVENT_FAULT)
#define OS_EVENT_THREADSTATUS ((OSEvent)GE_SERVICE_EVENT_THREAD_STATUS)
#define OS_EVENT_PRENMI ((OSEvent)GE_SERVICE_EVENT_PRE_NMI)
#define OS_EVENT_RDB_READ_DONE ((OSEvent)GE_SERVICE_EVENT_RDB_READ_DONE)
#define OS_EVENT_RDB_LOG_DONE ((OSEvent)GE_SERVICE_EVENT_RDB_LOG_DONE)
#define OS_EVENT_RDB_DATA_DONE ((OSEvent)GE_SERVICE_EVENT_RDB_DATA_DONE)
#define OS_EVENT_RDB_REQ_RAMROM ((OSEvent)GE_SERVICE_EVENT_RDB_REQ_RAMROM)
#define OS_EVENT_RDB_FREE_RAMROM ((OSEvent)GE_SERVICE_EVENT_RDB_FREE_RAMROM)
#define OS_EVENT_RDB_DBG_DONE ((OSEvent)GE_SERVICE_EVENT_RDB_DBG_DONE)
#define OS_EVENT_RDB_FLUSH_PROF ((OSEvent)GE_SERVICE_EVENT_RDB_FLUSH_PROF)
#define OS_EVENT_RDB_ACK_PROF ((OSEvent)GE_SERVICE_EVENT_RDB_ACK_PROF)

extern u64 osClockRate;

void ge_libultra_services_init(GeServiceWaitFn wait, void *wait_context);
size_t ge_libultra_services_pump(
        uint64_t elapsed_us,
        size_t *dropped_messages);
int32_t ge_libultra_services_emit_event(OSEvent event);
uint64_t ge_libultra_services_now_us(void);

void osCreateMesgQueue(OSMesgQueue *queue, OSMesg *messages, s32 message_count);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);
s32 osJamMesg(OSMesgQueue *queue, OSMesg message, s32 flags);
s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
void osSetEventMesg(OSEvent event, OSMesgQueue *queue, OSMesg message);
int osSetTimer(
        OSTimer *timer,
        OSTime value,
        OSTime interval,
        OSMesgQueue *queue,
        OSMesg message);
int osStopTimer(OSTimer *timer);

#endif

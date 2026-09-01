#include "ge_libultra_services.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MESSAGE(value) ((OSMesg)(uintptr_t)(value))

typedef struct WaitFixture {
    OSMesgQueue *queue;
    OSMesg message;
    unsigned int calls;
} WaitFixture;

static bool send_during_wait(void *context)
{
    WaitFixture *fixture = context;

    fixture->calls++;
    return osSendMesg(
            fixture->queue,
            fixture->message,
            OS_MESG_NOBLOCK) == 0;
}

static void test_queue_compatibility(void)
{
    OSMesg storage[3];
    OSMesg received = NULL;
    OSMesgQueue queue;

    ge_libultra_services_init(NULL, NULL);
    osCreateMesgQueue(&queue, storage, 3);
    assert(osSendMesg(&queue, MESSAGE(10U), OS_MESG_NOBLOCK) == 0);
    assert(osSendMesg(&queue, MESSAGE(20U), OS_MESG_NOBLOCK) == 0);
    assert(osJamMesg(&queue, MESSAGE(5U), OS_MESG_NOBLOCK) == 0);
    assert(osSendMesg(&queue, MESSAGE(30U), OS_MESG_NOBLOCK) == -1);

    assert(osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) == 0);
    assert(received == MESSAGE(5U));
    assert(osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) == 0);
    assert(received == MESSAGE(10U));
    assert(osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) == 0);
    assert(received == MESSAGE(20U));
    assert(osRecvMesg(&queue, NULL, OS_MESG_NOBLOCK) == -1);
    assert(osSendMesg(&queue, NULL, 99) == -1);
}

static void test_cooperative_blocking(void)
{
    OSMesg storage[1];
    OSMesg received = NULL;
    OSMesgQueue queue;
    WaitFixture fixture = {&queue, MESSAGE(42U), 0U};

    osCreateMesgQueue(&queue, storage, 1);
    ge_libultra_services_init(send_during_wait, &fixture);
    assert(osRecvMesg(&queue, &received, OS_MESG_BLOCK) == 0);
    assert(received == MESSAGE(42U));
    assert(fixture.calls == 1U);
}

static void test_event_compatibility(void)
{
    OSMesg storage[1];
    OSMesg received = NULL;
    OSMesgQueue queue;

    ge_libultra_services_init(NULL, NULL);
    osCreateMesgQueue(&queue, storage, 1);
    osSetEventMesg(OS_EVENT_VI, &queue, MESSAGE(0x5649U));
    assert(ge_libultra_services_emit_event(OS_EVENT_VI) == 0);
    assert(ge_libultra_services_emit_event(OS_EVENT_VI) == -1);
    assert(osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) == 0);
    assert(received == MESSAGE(0x5649U));
    osSetEventMesg(OS_EVENT_VI, NULL, NULL);
    assert(ge_libultra_services_emit_event(OS_EVENT_VI) == -1);
}

static void test_timer_compatibility(void)
{
    OSMesg storage[4];
    OSMesg received = NULL;
    OSMesgQueue queue;
    OSTimer one_shot;
    OSTimer periodic;
    size_t dropped = 99U;

    ge_libultra_services_init(NULL, NULL);
    osCreateMesgQueue(&queue, storage, 4);
    assert(osSetTimer(
                &one_shot,
                OS_USEC_TO_CYCLES(100U),
                0U,
                &queue,
                MESSAGE(1U)) == 0);
    /* Like libultra, value == 0 uses interval as the first expiration. */
    assert(osSetTimer(
                &periodic,
                0U,
                OS_USEC_TO_CYCLES(50U),
                &queue,
                MESSAGE(2U)) == 0);

    assert(ge_libultra_services_pump(49U, &dropped) == 0U);
    assert(dropped == 0U);
    assert(ge_libultra_services_pump(1U, &dropped) == 1U);
    assert(ge_libultra_services_now_us() == 50U);
    assert(ge_libultra_services_pump(50U, &dropped) == 2U);

    assert(osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) == 0);
    assert(received == MESSAGE(2U));
    assert(osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) == 0);
    assert(received == MESSAGE(1U));
    assert(osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) == 0);
    assert(received == MESSAGE(2U));
    assert(osStopTimer(&periodic) == 0);
    assert(osStopTimer(&periodic) == -1);
    assert(osStopTimer(&one_shot) == -1);
}

static void test_timer_capacity(void)
{
    OSMesg storage[1];
    OSMesgQueue queue;
    OSTimer timers[GE_SERVICE_MAX_TIMERS + 1U] = {{0U, false}};
    size_t index;

    ge_libultra_services_init(NULL, NULL);
    osCreateMesgQueue(&queue, storage, 1);
    for (index = 0U; index < GE_SERVICE_MAX_TIMERS; index++) {
        assert(osSetTimer(
                    &timers[index],
                    OS_USEC_TO_CYCLES(100U),
                    0U,
                    &queue,
                    NULL) == 0);
    }
    assert(osSetTimer(
                &timers[GE_SERVICE_MAX_TIMERS],
                OS_USEC_TO_CYCLES(100U),
                0U,
                &queue,
                NULL) == -1);
}

int main(void)
{
    test_queue_compatibility();
    test_cooperative_blocking();
    test_event_compatibility();
    test_timer_compatibility();
    test_timer_capacity();
    puts("portable libultra queue/event/timer adapter tests passed");
    return 0;
}

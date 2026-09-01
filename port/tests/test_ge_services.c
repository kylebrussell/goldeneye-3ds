#include "ge_services.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MESSAGE(value) ((GeServiceMessage)(uintptr_t)(value))

typedef struct WaitFixture {
    GeServiceQueue *queue;
    GeServiceMessage message;
    unsigned int calls;
    bool send_on_wait;
    bool receive_on_wait;
} WaitFixture;

static bool fixture_wait(void *context)
{
    WaitFixture *fixture = context;

    fixture->calls++;
    if (fixture->send_on_wait) {
        fixture->send_on_wait = false;
        return ge_service_queue_send(
                NULL,
                fixture->queue,
                fixture->message,
                GE_SERVICE_NOBLOCK) == GE_SERVICE_OK;
    }
    if (fixture->receive_on_wait) {
        fixture->receive_on_wait = false;
        return ge_service_queue_receive(
                NULL,
                fixture->queue,
                NULL,
                GE_SERVICE_NOBLOCK) == GE_SERVICE_OK;
    }
    return false;
}

static void test_fifo_and_jam(void)
{
    GeServiceMessage storage[3];
    GeServiceMessage message = NULL;
    GeServiceQueue queue = {0};

    assert(ge_service_queue_init(&queue, storage, 3U) == GE_SERVICE_OK);
    assert(ge_service_queue_send(NULL, &queue, MESSAGE(10U), GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(ge_service_queue_send(NULL, &queue, MESSAGE(20U), GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(ge_service_queue_jam(NULL, &queue, MESSAGE(5U), GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(ge_service_queue_send(NULL, &queue, MESSAGE(30U), GE_SERVICE_NOBLOCK)
            == GE_SERVICE_WOULD_BLOCK);
    assert(ge_service_queue_count(&queue) == 3U);

    assert(ge_service_queue_receive(NULL, &queue, &message, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(5U));
    assert(ge_service_queue_receive(NULL, &queue, &message, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(10U));
    assert(ge_service_queue_receive(NULL, &queue, &message, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(20U));
    assert(ge_service_queue_receive(NULL, &queue, NULL, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_WOULD_BLOCK);
}

static void test_cooperative_blocking(void)
{
    GeServiceMessage storage[1];
    GeServiceMessage message = NULL;
    GeServiceQueue queue;
    GeServiceRuntime runtime;
    WaitFixture fixture = {&queue, MESSAGE(42U), 0U, true, false};

    assert(ge_service_queue_init(&queue, storage, 1U) == GE_SERVICE_OK);
    ge_service_runtime_init(&runtime, fixture_wait, &fixture);
    assert(ge_service_queue_receive(&runtime, &queue, &message, GE_SERVICE_BLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(42U));
    assert(fixture.calls == 1U);

    assert(ge_service_queue_send(NULL, &queue, MESSAGE(7U), GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    fixture.receive_on_wait = true;
    assert(ge_service_queue_send(&runtime, &queue, MESSAGE(8U), GE_SERVICE_BLOCK)
            == GE_SERVICE_OK);
    assert(fixture.calls == 2U);
    assert(ge_service_queue_receive(NULL, &queue, &message, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(8U));
}

static void test_events(void)
{
    GeServiceMessage storage[1];
    GeServiceMessage message = NULL;
    GeServiceQueue queue;
    GeServiceRuntime runtime;

    assert(ge_service_queue_init(&queue, storage, 1U) == GE_SERVICE_OK);
    ge_service_runtime_init(&runtime, NULL, NULL);
    assert(ge_service_event_emit(&runtime, GE_SERVICE_EVENT_VI)
            == GE_SERVICE_INVALID);
    assert(ge_service_event_bind(
                &runtime,
                GE_SERVICE_EVENT_VI,
                &queue,
                MESSAGE(0x5649U)) == GE_SERVICE_OK);
    assert(ge_service_event_emit(&runtime, GE_SERVICE_EVENT_VI)
            == GE_SERVICE_OK);
    assert(ge_service_event_emit(&runtime, GE_SERVICE_EVENT_VI)
            == GE_SERVICE_WOULD_BLOCK);
    assert(ge_service_queue_receive(NULL, &queue, &message, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(0x5649U));
    assert(ge_service_event_unbind(&runtime, GE_SERVICE_EVENT_VI)
            == GE_SERVICE_OK);
}

static void test_timers(void)
{
    GeServiceMessage storage[4];
    GeServiceMessage message = NULL;
    GeServiceQueue queue;
    GeServiceRuntime runtime;
    uint32_t one_shot;
    uint32_t periodic;
    size_t dropped = 99U;

    assert(ge_service_queue_init(&queue, storage, 4U) == GE_SERVICE_OK);
    ge_service_runtime_init(&runtime, NULL, NULL);
    one_shot = ge_service_timer_start(
            &runtime, 100U, 0U, &queue, MESSAGE(1U));
    periodic = ge_service_timer_start(
            &runtime, 50U, 50U, &queue, MESSAGE(2U));
    assert(one_shot != GE_SERVICE_INVALID_TIMER);
    assert(periodic != GE_SERVICE_INVALID_TIMER);

    assert(ge_service_advance(&runtime, 49U, &dropped) == 0U);
    assert(dropped == 0U);
    assert(ge_service_advance(&runtime, 1U, &dropped) == 1U);
    assert(ge_service_now_us(&runtime) == 50U);
    assert(ge_service_advance(&runtime, 50U, &dropped) == 2U);
    assert(ge_service_queue_count(&queue) == 3U);

    assert(ge_service_queue_receive(NULL, &queue, &message, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(2U));
    assert(ge_service_queue_receive(NULL, &queue, &message, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(1U));
    assert(ge_service_queue_receive(NULL, &queue, &message, GE_SERVICE_NOBLOCK)
            == GE_SERVICE_OK);
    assert(message == MESSAGE(2U));

    /* Five periodic expirations, but the bounded queue can retain only four. */
    assert(ge_service_advance(&runtime, 250U, &dropped) == 4U);
    assert(dropped == 1U);
    assert(ge_service_queue_count(&queue) == 4U);
    assert(ge_service_timer_cancel(&runtime, periodic) == GE_SERVICE_OK);
    assert(ge_service_timer_cancel(&runtime, one_shot) == GE_SERVICE_INVALID);
}

static void test_invalid_inputs(void)
{
    GeServiceMessage storage[1];
    GeServiceQueue queue = {0};
    GeServiceRuntime runtime;

    ge_service_runtime_init(&runtime, NULL, NULL);
    assert(ge_service_queue_init(NULL, storage, 1U) == GE_SERVICE_INVALID);
    assert(ge_service_queue_init(&queue, NULL, 1U) == GE_SERVICE_INVALID);
    assert(ge_service_queue_init(&queue, storage, 0U) == GE_SERVICE_INVALID);
    assert(ge_service_event_bind(
                &runtime,
                (GeServiceEvent)GE_SERVICE_EVENT_COUNT,
                &queue,
                NULL) == GE_SERVICE_INVALID);
    assert(ge_service_timer_start(&runtime, 0U, 0U, &queue, NULL)
            == GE_SERVICE_INVALID_TIMER);
}

int main(void)
{
    test_fifo_and_jam();
    test_cooperative_blocking();
    test_events();
    test_timers();
    test_invalid_inputs();
    puts("portable queue, event, and timer service tests passed");
    return 0;
}

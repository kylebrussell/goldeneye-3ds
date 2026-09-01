#include <libaudio.h>

#include <assert.h>
#include <stdio.h>

void alSynNew(ALSynth *synth, ALSynConfig *config)
{
    (void)synth;
    (void)config;
}

void alSynDelete(ALSynth *synth)
{
    (void)synth;
}

static ALEvent make_event(s16 type)
{
    ALEvent event = {0};

    event.type = type;
    return event;
}

static void test_original_event_order(void)
{
    ALEventQueue queue;
    ALEventListItem items[4];
    ALEvent late = make_event(10);
    ALEvent early = make_event(20);
    ALEvent end = make_event(30);
    ALEvent received;

    alEvtqNew(&queue, items, 4);
    alEvtqPostEvent(&queue, &late, 100);
    alEvtqPostEvent(&queue, &early, 40);
    alEvtqPostEvent(&queue, &end, AL_EVTQ_END);

    assert(alEvtqNextEvent(&queue, &received) == 40);
    assert(received.type == 20);
    assert(alEvtqNextEvent(&queue, &received) == 60);
    assert(received.type == 10);
    assert(alEvtqNextEvent(&queue, &received) == 0);
    assert(received.type == 30);
    assert(alEvtqNextEvent(&queue, &received) == 0);
    assert(received.type == -1);
}

static void test_original_event_flush_type(void)
{
    ALEventQueue queue;
    ALEventListItem items[4];
    ALEvent removed_first = make_event(1);
    ALEvent kept = make_event(2);
    ALEvent removed_last = make_event(1);
    ALEvent received;

    alEvtqNew(&queue, items, 4);
    alEvtqPostEvent(&queue, &removed_first, 20);
    alEvtqPostEvent(&queue, &kept, 30);
    alEvtqPostEvent(&queue, &removed_last, 40);
    alEvtqFlushType(&queue, 1);
    assert(alEvtqNextEvent(&queue, &received) == 30);
    assert(received.type == 2);
    alEvtqFlush(&queue);
    assert(alEvtqNextEvent(&queue, &received) == 0);
    assert(received.type == -1);
}

static void test_original_pitch_ratio(void)
{
    f32 octave_up = alCents2Ratio(1200);
    f32 octave_down = alCents2Ratio(-1200);
    f32 unison = alCents2Ratio(0);

    assert(octave_up > 1.998f && octave_up < 2.002f);
    assert(octave_down > 0.499f && octave_down < 0.501f);
    assert(unison == 1.0f);
}

int main(void)
{
    test_original_event_order();
    test_original_event_flush_type();
    test_original_pitch_ratio();
    puts("original libaudio event queue and pitch tests passed");
    return 0;
}

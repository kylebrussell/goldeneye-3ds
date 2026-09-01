#include "ge_libultra_scheduler.h"
#include "sched.h"
#include <bondgame.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

extern s32 __scSchedule(
        OSSched *scheduler,
        OSScTask **sp,
        OSScTask **dp,
        s32 available_rcp);
extern void __scExec(OSSched *scheduler, OSScTask *sp, OSScTask *dp);
extern void __scHandleRetrace(OSSched *scheduler);
extern void __scHandleRSP(OSSched *scheduler);
extern void __scHandleRDP(OSSched *scheduler);

u8 sp_shed[SP_SHED_SZ] = {0};
u8 cfb_16[2][320 * 240 * 2] = {{0}};
OSViMode *viMode;
u32 g_viOriginalHstart;
u32 g_viOriginalVstart0;
u32 g_viOriginalVstart1;

void *setSPToEnd(u8 *stack, u32 size)
{
    return stack + size - 8U;
}

void crashRenderFrame(u16 *buffer)
{
    (void)buffer;
}

void joyRumblePakStop(void)
{
}

void joyPoll(void)
{
}

void musicFadeTick(void)
{
}

void viVsyncRelated(void)
{
}

void speedgraphMarkerUpdate(void)
{
}

void speedgraphMarkerHandler(s32 marker)
{
    (void)marker;
}

#define ORIGINAL_OS_SC_SP 0x0002
#define ORIGINAL_OS_SC_DP 0x0001
#define ORIGINAL_VIDEO_MSG 666U
#define ORIGINAL_RSP_DONE_MSG 667U
#define ORIGINAL_RDP_DONE_MSG 668U

static void test_original_append_lists(void)
{
    OSSched scheduler;
    OSScTask audio;
    OSScTask graphics;

    memset(&scheduler, 0, sizeof(scheduler));
    memset(&audio, 0, sizeof(audio));
    memset(&graphics, 0, sizeof(graphics));
    audio.list.t.type = M_AUDTASK;
    audio.flags = OS_SC_NEEDS_RSP;
    graphics.list.t.type = M_GFXTASK;
    graphics.flags = OS_SC_NEEDS_RSP | OS_SC_NEEDS_RDP;

    __scAppendList(&scheduler, &audio);
    assert(scheduler.audioListHead == &audio);
    assert(scheduler.audioListTail == &audio);
    assert(scheduler.doAudio == 1);
    assert(audio.state == OS_SC_NEEDS_RSP);

    __scAppendList(&scheduler, &graphics);
    assert(scheduler.gfxListHead == &graphics);
    assert(scheduler.gfxListTail == &graphics);
    assert(graphics.state == (OS_SC_NEEDS_RSP | OS_SC_NEEDS_RDP));
}

static void test_original_audio_priority(void)
{
    OSSched scheduler;
    OSScTask audio;
    OSScTask *sp = NULL;
    OSScTask *dp = NULL;
    s32 remaining;

    memset(&scheduler, 0, sizeof(scheduler));
    memset(&audio, 0, sizeof(audio));
    audio.list.t.type = M_AUDTASK;
    audio.flags = OS_SC_NEEDS_RSP;
    __scAppendList(&scheduler, &audio);

    remaining = __scSchedule(
            &scheduler,
            &sp,
            &dp,
            ORIGINAL_OS_SC_SP | ORIGINAL_OS_SC_DP);
    assert(sp == &audio);
    assert(dp == NULL);
    assert(remaining == ORIGINAL_OS_SC_DP);
    assert(scheduler.audioListHead == NULL);
    assert(scheduler.audioListTail == NULL);
    assert(scheduler.doAudio == 0);
}

static void test_original_graphics_selection(void)
{
    OSSched scheduler;
    OSScTask graphics;
    OSScTask *sp = NULL;
    OSScTask *dp = NULL;
    int framebuffer;
    s32 remaining;

    memset(&scheduler, 0, sizeof(scheduler));
    memset(&graphics, 0, sizeof(graphics));
    ge_libultra_scheduler_reset();
    osViSwapBuffer(&framebuffer);
    ge_libultra_scheduler_complete_swap();
    graphics.list.t.type = M_GFXTASK;
    graphics.flags = OS_SC_NEEDS_RSP | OS_SC_NEEDS_RDP;
    __scAppendList(&scheduler, &graphics);

    remaining = __scSchedule(
            &scheduler,
            &sp,
            &dp,
            ORIGINAL_OS_SC_SP | ORIGINAL_OS_SC_DP);
    assert(sp == &graphics);
    assert(dp == &graphics);
    assert(remaining == 0);
    assert(scheduler.gfxListHead == NULL);
    assert(scheduler.gfxListTail == NULL);
}

static void test_original_task_dispatch(void)
{
    OSSched scheduler;
    OSScTask graphics;
    const GeLibultraSchedulerState *native_state;

    memset(&scheduler, 0, sizeof(scheduler));
    memset(&graphics, 0, sizeof(graphics));
    ge_libultra_scheduler_reset();
    graphics.list.t.type = M_GFXTASK;
    graphics.state = OS_SC_NEEDS_RSP | OS_SC_NEEDS_RDP;

    __scExec(&scheduler, &graphics, &graphics);
    native_state = ge_libultra_scheduler_state();
    assert(scheduler.curRSPTask == &graphics);
    assert(scheduler.curRDPTask == &graphics);
    assert(native_state->loaded_sp_task == &graphics.list);
    assert(native_state->sp_started);
    assert(native_state->dp_status == 0x3c0U);
}

static void test_original_client_list(void)
{
    OSSched scheduler;
    OSScClient clients[2];
    OSMesg storage[1];
    OSMesgQueue queue;

    memset(&scheduler, 0, sizeof(scheduler));
    memset(clients, 0, sizeof(clients));
    ge_libultra_services_init(NULL, NULL);
    ge_libultra_scheduler_reset();
    osCreateMesgQueue(&queue, storage, 1);

    osScAddClient(&scheduler, &clients[0], &queue, NULL);
    assert(scheduler.clientList == &clients[0]);
    assert(clients[0].msgQ == &queue);
    assert(osScGetCmdQ(&scheduler) == &scheduler.cmdQ);
    osScRemoveClient(&scheduler, &clients[0]);
    assert(scheduler.clientList == NULL);
}

static void test_original_retrace_to_completion(void)
{
    OSSched scheduler;
    OSThread scheduler_thread;
    OSScClient clients[2];
    OSMesg client_storage[2];
    OSMesg done_storage[1];
    OSMesgQueue client_queue;
    OSMesgQueue done_queue;
    OSScTask graphics;
    OSMesg message = NULL;
    int done_marker;

    memset(&scheduler, 0, sizeof(scheduler));
    memset(&scheduler_thread, 0, sizeof(scheduler_thread));
    memset(clients, 0, sizeof(clients));
    memset(&graphics, 0, sizeof(graphics));
    ge_libultra_services_init(NULL, NULL);
    ge_libultra_scheduler_reset();
    osCreateScheduler(&scheduler, &scheduler_thread, OS_VI_NTSC_LAN1, 1U);
    assert(scheduler_thread.started);
    assert(scheduler_thread.entry == __scMain);

    osCreateMesgQueue(&client_queue, client_storage, 2);
    osCreateMesgQueue(&done_queue, done_storage, 1);
    osScAddClient(&scheduler, &clients[0], &client_queue, NULL);
    graphics.list.t.type = M_GFXTASK;
    graphics.flags = OS_SC_NEEDS_RSP | OS_SC_NEEDS_RDP;
    graphics.msgQ = &done_queue;
    graphics.msg = &done_marker;
    /* Force the unconditional even-frame client notification on host64. */
    scheduler.frameCount = 1U;
    assert(osSendMesg(
                osScGetCmdQ(&scheduler),
                &graphics,
                OS_MESG_NOBLOCK) == 0);

    assert(ge_libultra_services_emit_event(OS_EVENT_VI) == 0);
    assert(osRecvMesg(
                &scheduler.interruptQ,
                &message,
                OS_MESG_NOBLOCK) == 0);
    assert((uintptr_t)message == ORIGINAL_VIDEO_MSG);
    __scHandleRetrace(&scheduler);
    assert(scheduler.frameCount == 2U);
    assert(scheduler.curRSPTask == &graphics);
    assert(scheduler.curRDPTask == &graphics);
    assert(osRecvMesg(&client_queue, &message, OS_MESG_NOBLOCK) == 0);
    assert(message == &scheduler.retraceMsg);

    assert(ge_libultra_scheduler_complete_sp() == 0);
    assert(osRecvMesg(
                &scheduler.interruptQ,
                &message,
                OS_MESG_NOBLOCK) == 0);
    assert((uintptr_t)message == ORIGINAL_RSP_DONE_MSG);
    __scHandleRSP(&scheduler);
    assert(scheduler.curRSPTask == NULL);
    assert(scheduler.curRDPTask == &graphics);
    assert(osRecvMesg(&done_queue, NULL, OS_MESG_NOBLOCK) == -1);

    assert(ge_libultra_scheduler_complete_dp() == 0);
    assert(osRecvMesg(
                &scheduler.interruptQ,
                &message,
                OS_MESG_NOBLOCK) == 0);
    assert((uintptr_t)message == ORIGINAL_RDP_DONE_MSG);
    __scHandleRDP(&scheduler);
    assert(scheduler.curRDPTask == NULL);
    assert(osRecvMesg(&done_queue, &message, OS_MESG_NOBLOCK) == 0);
    assert(message == &done_marker);
    assert(ge_libultra_scheduler_complete_sp() == -1);
    assert(ge_libultra_scheduler_complete_dp() == -1);
}

static void finite_thread_entry(void *argument)
{
    unsigned int *runs = argument;

    (*runs)++;
}

static void test_finite_cooperative_thread(void)
{
    OSThread thread;
    unsigned int runs = 0U;

    memset(&thread, 0, sizeof(thread));
    ge_libultra_scheduler_reset();
    osCreateThread(&thread, 9, finite_thread_entry, &runs, NULL, 10);
    assert(ge_libultra_scheduler_run_thread_once(&thread) == -1);
    osStartThread(&thread);
    assert(ge_libultra_scheduler_run_thread_once(&thread) == 0);
    assert(runs == 1U);
    assert(ge_libultra_scheduler_state()->thread_entries_completed == 1U);
    osStopThread(&thread);
    assert(ge_libultra_scheduler_run_thread_once(&thread) == -1);
}

int main(void)
{
    test_original_append_lists();
    test_original_audio_priority();
    test_original_graphics_selection();
    test_original_task_dispatch();
    test_original_client_list();
    test_original_retrace_to_completion();
    test_finite_cooperative_thread();
    puts("original GoldenEye scheduler retrace/dispatch/completion tests passed");
    return 0;
}

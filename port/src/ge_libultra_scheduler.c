#include "ge_libultra_scheduler.h"

#include <string.h>

u32 osTvType = OS_TV_NTSC;
OSViMode osViModeTable[2];

static GeLibultraSchedulerState g_scheduler_state;

void ge_libultra_scheduler_reset(void)
{
    memset(&g_scheduler_state, 0, sizeof(g_scheduler_state));
}

const GeLibultraSchedulerState *ge_libultra_scheduler_state(void)
{
    return &g_scheduler_state;
}

void ge_libultra_scheduler_complete_swap(void)
{
    g_scheduler_state.current_framebuffer =
        g_scheduler_state.next_framebuffer;
}

int ge_libultra_scheduler_complete_sp(void)
{
    int result;

    if (!g_scheduler_state.sp_completion_pending) {
        return -1;
    }
    g_scheduler_state.sp_completion_pending = false;
    result = ge_libultra_services_emit_event(OS_EVENT_SP);
    return result == 0 ? 0 : -1;
}

int ge_libultra_scheduler_complete_dp(void)
{
    int result;

    if (!g_scheduler_state.dp_completion_pending) {
        return -1;
    }
    g_scheduler_state.dp_completion_pending = false;
    result = ge_libultra_services_emit_event(OS_EVENT_DP);
    return result == 0 ? 0 : -1;
}

int ge_libultra_scheduler_run_thread_once(OSThread *thread)
{
    if (thread == NULL || !thread->started || thread->entry == NULL
            || g_scheduler_state.running_thread_entry) {
        return -1;
    }

    g_scheduler_state.running_thread_entry = true;
    thread->entry(thread->argument);
    g_scheduler_state.running_thread_entry = false;
    g_scheduler_state.thread_entries_completed++;
    return 0;
}

void osCreateThread(
        OSThread *thread,
        OSId id,
        void (*entry)(void *),
        void *argument,
        void *stack_pointer,
        OSPri priority)
{
    if (thread == NULL) {
        return;
    }
    thread->id = id;
    thread->priority = priority;
    thread->entry = entry;
    thread->argument = argument;
    thread->stack_pointer = stack_pointer;
    thread->started = false;
}

void osStartThread(OSThread *thread)
{
    if (thread != NULL) {
        /* Cooperative native scheduling does not invoke the entry inline. */
        thread->started = true;
    }
}

void osStopThread(OSThread *thread)
{
    if (thread != NULL) {
        thread->started = false;
    }
}

void osSetThreadPri(OSThread *thread, OSPri priority)
{
    if (thread != NULL) {
        thread->priority = priority;
    }
}

OSIntMask osSetIntMask(OSIntMask mask)
{
    OSIntMask previous = g_scheduler_state.interrupt_mask;

    g_scheduler_state.interrupt_mask = mask;
    return previous;
}

void osCreateViManager(OSPri priority)
{
    g_scheduler_state.vi_manager_priority = priority;
}

void osViSetEvent(OSMesgQueue *queue, OSMesg message, u32 retrace_count)
{
    g_scheduler_state.vi_retrace_count = retrace_count;
    osSetEventMesg(OS_EVENT_VI, queue, message);
}

void osViSetMode(OSViMode *mode)
{
    g_scheduler_state.vi_mode = mode;
}

void osViSetXScale(f32 scale)
{
    (void)scale;
}

void osViSetYScale(f32 scale)
{
    (void)scale;
}

void osViRepeatLine(s32 enabled)
{
    (void)enabled;
}

void osViBlack(u8 active)
{
    g_scheduler_state.vi_black = active != 0U;
}

void osViSwapBuffer(void *framebuffer)
{
    g_scheduler_state.next_framebuffer = framebuffer;
}

void *osViGetCurrentFramebuffer(void)
{
    return g_scheduler_state.current_framebuffer;
}

void *osViGetNextFramebuffer(void)
{
    return g_scheduler_state.next_framebuffer;
}

void osWritebackDCacheAll(void)
{
}

void osSpTaskLoad(OSTask *task)
{
    g_scheduler_state.loaded_sp_task = task;
    g_scheduler_state.sp_started = false;
}

void osSpTaskStartGo(OSTask *task)
{
    g_scheduler_state.loaded_sp_task = task;
    g_scheduler_state.sp_started = task != NULL;
    g_scheduler_state.sp_completion_pending = task != NULL;
    if (task != NULL && task->t.type == M_GFXTASK) {
        g_scheduler_state.dp_completion_pending = true;
    }
}

void osSpTaskYield(void)
{
    g_scheduler_state.sp_yield_requested = true;
}

s32 osSpTaskYielded(OSTask *task)
{
    (void)task;
    return g_scheduler_state.sp_yield_requested ? 1 : 0;
}

s32 osDpSetNextBuffer(void *buffer, u64 size)
{
    g_scheduler_state.dp_buffer = buffer;
    g_scheduler_state.dp_buffer_size = size;
    g_scheduler_state.dp_completion_pending = true;
    return 0;
}

void osDpSetStatus(u32 status)
{
    g_scheduler_state.dp_status = status;
}

void osDpGetCounters(u32 counters[4])
{
    if (counters != NULL) {
        memset(counters, 0, sizeof(u32) * 4U);
    }
}

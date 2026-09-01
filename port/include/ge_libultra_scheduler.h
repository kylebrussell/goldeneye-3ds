#ifndef GE_LIBULTRA_SCHEDULER_H
#define GE_LIBULTRA_SCHEDULER_H

/*
 * Native definitions required by GoldenEye's original src/sched.c.  These are
 * source-compatible service objects, not Nintendo 64 binary ABI structures.
 * RSP/RDP entry points record lightweight state only; a renderer backend must
 * replace them before graphics tasks can execute.
 */

#include "ge_libultra_services.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t OSId;
typedef int32_t OSPri;
typedef uint32_t OSIntMask;

typedef struct OSThread {
    OSId id;
    OSPri priority;
    void (*entry)(void *);
    void *argument;
    void *stack_pointer;
    bool started;
} OSThread;

typedef union OSTask {
    struct {
        uint32_t type;
        uint32_t flags;
        void *ucode_boot;
        uint32_t ucode_boot_size;
        void *ucode;
        uint32_t ucode_size;
        void *ucode_data;
        uint32_t ucode_data_size;
        void *dram_stack;
        uint32_t dram_stack_size;
        void *output_buff;
        uint64_t *output_buff_size;
        void *data_ptr;
        uint32_t data_size;
        void *yield_data_ptr;
        uint32_t yield_data_size;
    } t;
    uint64_t force_alignment;
} OSTask;

typedef struct OSViCommonRegs {
    uint32_t hStart;
} OSViCommonRegs;

typedef struct OSViFieldRegs {
    uint32_t vStart;
} OSViFieldRegs;

typedef struct OSViMode {
    OSViCommonRegs comRegs;
    OSViFieldRegs fldRegs[2];
} OSViMode;

typedef struct Gfx {
    uint32_t words[2];
} Gfx;

typedef struct Mtx {
    uint64_t words[8];
} Mtx;

#define M_GFXTASK 1U
#define M_AUDTASK 2U

#define OS_IM_NONE 0U
#define OS_IM_VI UINT32_C(0x00000801)
#define OS_PRIORITY_VIMGR 254

#define TV_TYPE_PAL 0U
#define TV_TYPE_NTSC 1U
#define TV_TYPE_MPAL 2U
#define OS_TV_PAL TV_TYPE_PAL
#define OS_TV_NTSC TV_TYPE_NTSC
#define OS_TV_MPAL TV_TYPE_MPAL

#define OS_VI_NTSC_LAN1 0U
#define OS_VI_MPAL_LAN1 1U

extern u32 osTvType;
extern OSViMode osViModeTable[2];

void osCreateThread(
        OSThread *thread,
        OSId id,
        void (*entry)(void *),
        void *argument,
        void *stack_pointer,
        OSPri priority);
void osStartThread(OSThread *thread);
void osStopThread(OSThread *thread);
void osSetThreadPri(OSThread *thread, OSPri priority);
OSIntMask osSetIntMask(OSIntMask mask);

void osCreateViManager(OSPri priority);
void osViSetEvent(OSMesgQueue *queue, OSMesg message, u32 retrace_count);
void osViSetMode(OSViMode *mode);
void osViSetXScale(f32 scale);
void osViSetYScale(f32 scale);
void osViRepeatLine(s32 enabled);
void osViBlack(u8 active);
void osViSwapBuffer(void *framebuffer);
void *osViGetCurrentFramebuffer(void);
void *osViGetNextFramebuffer(void);

void osWritebackDCacheAll(void);
void osSpTaskLoad(OSTask *task);
void osSpTaskStartGo(OSTask *task);
void osSpTaskYield(void);
s32 osSpTaskYielded(OSTask *task);
s32 osDpSetNextBuffer(void *buffer, u64 size);
void osDpSetStatus(u32 status);
void osDpGetCounters(u32 counters[4]);

typedef struct GeLibultraSchedulerState {
    OSIntMask interrupt_mask;
    OSPri vi_manager_priority;
    u32 vi_retrace_count;
    OSViMode *vi_mode;
    void *current_framebuffer;
    void *next_framebuffer;
    void *dp_buffer;
    u64 dp_buffer_size;
    OSTask *loaded_sp_task;
    u32 dp_status;
    bool vi_black;
    bool sp_started;
    bool sp_yield_requested;
    bool sp_completion_pending;
    bool dp_completion_pending;
    bool running_thread_entry;
    uint64_t thread_entries_completed;
} GeLibultraSchedulerState;

void ge_libultra_scheduler_reset(void);
const GeLibultraSchedulerState *ge_libultra_scheduler_state(void);
void ge_libultra_scheduler_complete_swap(void);

/* Explicit hardware-boundary signals; they enqueue OS_EVENT_SP/DP messages. */
int ge_libultra_scheduler_complete_sp(void);
int ge_libultra_scheduler_complete_dp(void);

/*
 * Runs one finite cooperative entry function.  Original infinite thread
 * entries such as __scMain must be driven through finite handlers instead.
 */
int ge_libultra_scheduler_run_thread_once(OSThread *thread);

#endif

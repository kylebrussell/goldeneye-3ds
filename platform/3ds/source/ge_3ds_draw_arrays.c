#include <3ds.h>
#include <citro3d.h>
#include <stddef.h>
#include <string.h>
#include "ge_3ds_draw_trace.h"
#include "../vendor/citro3d-1.7.1/internal.h"

/* Altered Citro3D 1.7.1 drawArrays.c; upstream source and license are retained
 * in vendor/citro3d-1.7.1. Only encoding is batched: context update, all eleven
 * commands, primitive restart, vertex-cache clear and DrawUsed are unchanged.
 * The Makefile pins the linked archive because this uses its internal ABI. */
_Static_assert(offsetof(C3D_Context, flags) == 32U,
    "Reverify the pinned Citro3D context ABI");

/* Diagnostic state only: no GPU writes and no allocation. A SELECT capture
 * reads the last completed left-eye world pass after FrameBegin's GPU fence. */
static const uint16_t trace_register_ids[GE_3DS_DRAW_TRACE_REGISTERS] = {
    0x107, 0x114, 0x115, 0x100, 0x104, 0x112, 0x113, 0x126
};
typedef struct Ge3dsDrawTraceRecord {
    uint32_t first, count, context_depth, context_flags;
    uint32_t begin, end;
    bool valid;
    Ge3dsDrawTraceRegisters registers;
} Ge3dsDrawTraceRecord;
typedef struct Ge3dsDrawTracePass {
    Ge3dsDrawTraceRecord records[GE_3DS_DRAW_TRACE_CAPACITY];
    size_t count;
    bool complete;
} Ge3dsDrawTracePass;
static Ge3dsDrawTracePass trace_work, trace_completed;
static Ge3dsDrawTraceRegisters trace_registers;
static bool trace_active, trace_valid;
static const uint32_t *trace_buffer;
static size_t trace_cursor;

bool ge_3ds_draw_trace_scan(Ge3dsDrawTraceRegisters *state,
    const uint32_t *words, size_t capacity, size_t begin, size_t end)
{
    if (state == NULL || words == NULL || begin > end || end > capacity
            || (begin & 1U) != 0U || (end & 1U) != 0U) return false;
    memset(state->emitted, 0, sizeof(state->emitted));
    while (begin < end) {
        if (end - begin < 2U) return false;
        const uint32_t header = words[begin + 1U];
        const size_t parameters = 1U + ((header >> 20U) & 0x7ffU);
        const size_t length = (parameters + 2U) & ~(size_t)1U;
        if (length > end - begin) return false;
        const uint32_t byte_mask = (header >> 16U) & 15U;
        uint32_t mask = 0U;
        for (unsigned b = 0; b < 4U; ++b)
            if (byte_mask & (1U << b)) mask |= UINT32_C(0xff) << (b * 8U);
        for (size_t p = 0; p < parameters; ++p) {
            const uint32_t reg = (header & 0xffffU)
                + ((header & UINT32_C(0x80000000)) ? (uint32_t)p : 0U);
            const uint32_t value = words[begin + (p == 0U ? 0U : p + 1U)];
            for (size_t r = 0; r < GE_3DS_DRAW_TRACE_REGISTERS; ++r)
                if (reg == trace_register_ids[r]) {
                    state->value[r] = (state->value[r] & ~mask) | (value & mask);
                    state->known[r] |= (uint8_t)byte_mask;
                    state->emitted[r] |= (uint8_t)byte_mask;
                }
        }
        begin += length;
    }
    return true;
}
void ge_3ds_draw_trace_reset(void)
{
    memset(&trace_work, 0, sizeof(trace_work));
    memset(&trace_completed, 0, sizeof(trace_completed));
    trace_active = false;
}
void ge_3ds_draw_trace_begin(void)
{
    memset(&trace_work, 0, sizeof(trace_work));
    memset(&trace_registers, 0, sizeof(trace_registers));
    trace_buffer = gpuCmdBuf;
    trace_cursor = 0U;
    trace_valid = gpuCmdBuf != NULL;
    trace_active = true;
}
void ge_3ds_draw_trace_end(void)
{
    if (!trace_active) return;
    trace_work.complete = true;
    trace_completed = trace_work;
    trace_active = false;
}
static void ge_3ds_draw_trace_record(int first, int count,
    uint32_t context_depth, uint32_t context_flags)
{
    if (!trace_active || trace_work.count >= GE_3DS_DRAW_TRACE_CAPACITY) return;
    Ge3dsDrawTraceRecord *record = &trace_work.records[trace_work.count++];
    record->first = (uint32_t)first;
    record->count = (uint32_t)count;
    record->context_depth = context_depth;
    record->context_flags = context_flags;
    record->begin = (uint32_t)trace_cursor;
    record->end = gpuCmdBufOffset;
    trace_valid = trace_valid && trace_buffer == gpuCmdBuf
        && ge_3ds_draw_trace_scan(&trace_registers, gpuCmdBuf,
            gpuCmdBufSize, trace_cursor, gpuCmdBufOffset);
    record->valid = trace_valid;
    record->registers = trace_registers;
    trace_cursor = gpuCmdBufOffset;
}
void ge_3ds_draw_trace_write(FILE *stream)
{
    if (stream == NULL) return;
    fprintf(stream, "gpu_depth_trace=left source=previous_submission complete=%u count=%lu cap=%u\n",
        trace_completed.complete, (unsigned long)trace_completed.count,
        GE_3DS_DRAW_TRACE_CAPACITY);
    for (size_t i = 0; i < trace_completed.count; ++i) {
        const Ge3dsDrawTraceRecord *r = &trace_completed.records[i];
        fprintf(stream, "gpu_depth_draw=%lu first=%lu count=%lu context=%08lx flags=%08lx words=%lu,%lu valid=%u",
            (unsigned long)i, (unsigned long)r->first, (unsigned long)r->count,
            (unsigned long)r->context_depth, (unsigned long)r->context_flags,
            (unsigned long)r->begin,
            (unsigned long)r->end, r->valid);
        for (size_t j = 0; j < GE_3DS_DRAW_TRACE_REGISTERS; ++j)
            fprintf(stream, " reg%03x=%08lx/%x/%x", trace_register_ids[j],
                (unsigned long)r->registers.value[j], r->registers.known[j],
                r->registers.emitted[j]);
        fputc('\n', stream);
    }
}

_Static_assert(offsetof(C3D_Context, effect.depthTest) == 0xfcU,
    "Reverify pinned Citro3D depth context ABI");

static inline void ge_3ds_encode_draw(u32 *commands,
    GPU_Primitive_t primitive, int first, int size)
{
    commands[0] = (u32)primitive;
    commands[1] = GPUCMD_HEADER(0, 2, GPUREG_PRIMITIVE_CONFIG);
    commands[2] = 1;
    commands[3] = GPUCMD_HEADER(0, 15, GPUREG_RESTART_PRIMITIVE);
    commands[4] = 0x80000000;
    commands[5] = GPUCMD_HEADER(0, 15, GPUREG_INDEXBUFFER_CONFIG);
    commands[6] = (u32)size;
    commands[7] = GPUCMD_HEADER(0, 15, GPUREG_NUMVERTICES);
    commands[8] = (u32)first;
    commands[9] = GPUCMD_HEADER(0, 15, GPUREG_VERTEX_OFFSET);
    commands[10] = 1;
    commands[11] = GPUCMD_HEADER(0, 1, GPUREG_GEOSTAGE_CONFIG2);
    commands[12] = 0;
    commands[13] = GPUCMD_HEADER(0, 1, GPUREG_START_DRAW_FUNC0);
    commands[14] = 1;
    commands[15] = GPUCMD_HEADER(0, 15, GPUREG_DRAWARRAYS);
    commands[16] = 1;
    commands[17] = GPUCMD_HEADER(0, 1, GPUREG_START_DRAW_FUNC0);
    commands[18] = 0;
    commands[19] = GPUCMD_HEADER(0, 1, GPUREG_GEOSTAGE_CONFIG2);
    commands[20] = 1;
    commands[21] = GPUCMD_HEADER(0, 15, GPUREG_VTX_FUNC);
}

void C3D_DrawArrays(GPU_Primitive_t primitive, int first, int size)
{
    const C3D_Context *context = C3Di_GetContext();
    const uint32_t trace_depth = context->effect.depthTest;
    const uint32_t trace_flags = context->flags;
    C3Di_UpdateContext();
    ge_3ds_draw_trace_record(first, size, trace_depth, trace_flags);

    if (gpuCmdBuf != NULL && gpuCmdBufOffset <= gpuCmdBufSize
            && gpuCmdBufSize - gpuCmdBufOffset >= 22U) {
        /* Preserve individual command headers and parameter word order. */
        ge_3ds_encode_draw(gpuCmdBuf + gpuCmdBufOffset, primitive, first, size);
        gpuCmdBufOffset += 22U;
    } else {
        /* Preserve libctru's exact partial stream and panic point when the
         * sequence does not fit, rather than imposing an all-or-none write. */
        u32 commands[22];
        ge_3ds_encode_draw(commands, primitive, first, size);
        for (unsigned i = 0U; i < 22U; i += 2U)
            GPUCMD_Add(commands[i + 1U], &commands[i], 1U);
    }
    C3Di_GetContext()->flags |= C3DiF_DrawUsed;
}

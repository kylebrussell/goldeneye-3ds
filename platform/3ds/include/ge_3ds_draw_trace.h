#ifndef GE_3DS_DRAW_TRACE_H
#define GE_3DS_DRAW_TRACE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define GE_3DS_DRAW_TRACE_CAPACITY 32U
#define GE_3DS_DRAW_TRACE_REGISTERS 8U
/* Values are GPU register words; known/emitted contain byte masks (0..15). */
typedef struct Ge3dsDrawTraceRegisters {
    uint32_t value[GE_3DS_DRAW_TRACE_REGISTERS];
    uint8_t known[GE_3DS_DRAW_TRACE_REGISTERS];
    uint8_t emitted[GE_3DS_DRAW_TRACE_REGISTERS];
} Ge3dsDrawTraceRegisters;
bool ge_3ds_draw_trace_scan(Ge3dsDrawTraceRegisters *state,
    const uint32_t *words, size_t capacity, size_t begin, size_t end);
void ge_3ds_draw_trace_reset(void);
void ge_3ds_draw_trace_begin(void);
void ge_3ds_draw_trace_end(void);
void ge_3ds_draw_trace_write(FILE *stream);
#endif

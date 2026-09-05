#include <3ds.h>
#include <citro3d.h>
#include <stddef.h>
#include "../vendor/citro3d-1.7.1/internal.h"

/* Altered Citro3D 1.7.1 drawArrays.c; upstream source and license are retained
 * in vendor/citro3d-1.7.1. Only encoding is batched: context update, all eleven
 * commands, primitive restart, vertex-cache clear and DrawUsed are unchanged.
 * The Makefile pins the linked archive because this uses its internal ABI. */
_Static_assert(offsetof(C3D_Context, flags) == 32U,
    "Reverify the pinned Citro3D context ABI");

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
    C3Di_UpdateContext();

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

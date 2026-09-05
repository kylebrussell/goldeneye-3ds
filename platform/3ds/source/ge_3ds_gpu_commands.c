#include <3ds.h>

/* Keep Citro3D's context updates and draw commands intact. libctru 2.7.0's
 * GPUCMD_Add single-parameter encoding is exactly [value, header], including
 * its documented zero-length-as-one behavior. Long streams, exhausted or
 * invalid buffers use the library implementation (and its panic path).
 * Reference: devkitPro/libctru v2.7.0, libctru/source/gpu/gpu.c. */
void __real_GPUCMD_Add(u32 header, const u32 *param, u32 paramlength);

void __wrap_GPUCMD_Add(u32 header, const u32 *param, u32 paramlength)
{
    if (paramlength <= 1U && gpuCmdBuf != NULL
            && gpuCmdBufOffset <= gpuCmdBufSize
            && gpuCmdBufSize - gpuCmdBufOffset >= 2U) {
        /* Read before either write, preserving parameter/buffer aliases. */
        gpuCmdBuf[gpuCmdBufOffset] = param != NULL ? param[0] : 0U;
        gpuCmdBuf[gpuCmdBufOffset + 1U] = header;
        gpuCmdBufOffset += 2U;
        return;
    }
    __real_GPUCMD_Add(header, param, paramlength);
}

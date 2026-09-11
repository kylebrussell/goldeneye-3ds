#!/usr/bin/env python3
"""Run the real SELECT depth capture with tracked GPU/CPU buffer ownership."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()
start = source.index("static void capture_world_screen(")
begin = source.index("{", start)
depth = 1
end = begin + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
capture = source[start:end]

prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
typedef uint32_t u32;
typedef int32_t Result;
#define R_SUCCEEDED(x) ((x) >= 0)
#define GPU_RB_DEPTH24_STENCIL8 3
enum { BYTES = 240 * 400 * 4 };
static uint32_t gpu_words[BYTES / 4];
#define OS_VRAM_VADDR ((uintptr_t)gpu_words)
#define OS_VRAM_SIZE ((uintptr_t)sizeof(gpu_words))
typedef struct {
    void *depthBuf, *colorBuf;
    unsigned width, height, depthFmt, colorMask, depthMask, block32;
} C3D_FrameBuf;
typedef struct { C3D_FrameBuf frameBuf; bool ownsDepth; } C3D_RenderTarget;
static uint8_t framebuffer;
static uint8_t *world_last_framebuffer = &framebuffer;
static void *cpu_buffer;
static size_t cpu_size, bytes_written, write_calls, allocation_size = BYTES;
static int failure;
static char log_text[4096];
static FILE *file_token;
static void *tracked_malloc(size_t size) {
    assert(cpu_buffer == NULL);
    if (failure == 1) return NULL;
    cpu_buffer = malloc(size); cpu_size = size; assert(cpu_buffer);
    return cpu_buffer;
}
static void tracked_free(void *p) {
    if (p) { assert(p == cpu_buffer); free(p); cpu_buffer = NULL; cpu_size = 0; }
}
static FILE *tracked_fopen(const char *path, const char *mode) {
    assert(strstr(path, ".left.depth.bin") && strcmp(mode, "wb") == 0);
    return failure == 3 ? NULL : file_token;
}
static size_t tracked_fwrite(const void *p, size_t size, size_t count, FILE *f) {
    size_t bytes = size * count;
    assert(f == file_token);
    /* A CPU-readable VRAM pointer is NOT safe to pass to FS IPC. Require
     * this exact capture's allocated heap storage, not merely any other ptr. */
    assert(cpu_buffer && (uintptr_t)p >= (uintptr_t)cpu_buffer);
    assert(bytes <= cpu_size && (uintptr_t)p - (uintptr_t)cpu_buffer <= cpu_size - bytes);
    assert(bytes_written + bytes <= BYTES);
    assert(memcmp(p, (const unsigned char *)gpu_words + bytes_written, bytes) == 0);
    ++write_calls; bytes_written += bytes;
    return failure == 4 ? count - 1 : count;
}
static int tracked_fclose(FILE *f) { assert(f == file_token); return failure == 5 ? -1 : 0; }
static int tracked_fprintf(FILE *f, const char *format, ...) {
    (void)f;
    va_list args; va_start(args, format);
    int n = vsnprintf(log_text + strlen(log_text), sizeof(log_text) - strlen(log_text), format, args);
    va_end(args); return n;
}
static u32 C3D_CalcDepthBufSize(unsigned w, unsigned h, unsigned fmt) {
    assert(fmt == GPU_RB_DEPTH24_STENCIL8); return w * h * 4;
}
static size_t vramGetSize(void *p) { assert(p == gpu_words); return allocation_size; }
static Result GSPGPU_InvalidateDataCache(const void *p, u32 bytes) {
    if (p == world_last_framebuffer) { assert(bytes == 240 * 400 * 3); return 0; }
    assert(p == gpu_words && bytes == BYTES); return failure == 2 ? -1 : 0;
}
static bool ge_3ds_frontend_probe_write_ppm(const char *path, const uint8_t *pixels) {
    assert(strstr(path, ".left.ppm") && pixels == world_last_framebuffer); return true;
}
#define malloc tracked_malloc
#define free tracked_free
#define fopen tracked_fopen
#define fwrite tracked_fwrite
#define fclose tracked_fclose
#define fprintf tracked_fprintf
'''
suffix = r'''
static void reset(void) {
    assert(cpu_buffer == NULL); bytes_written = write_calls = 0; log_text[0] = 0;
}
int main(void) {
    (void)tracked_malloc; (void)tracked_free;
    file_token = stdout;
    for (size_t i = 0; i < BYTES / 4; ++i) gpu_words[i] = (uint32_t)i * 2654435761U;
    C3D_RenderTarget target = {.frameBuf = {.depthBuf = gpu_words, .width = 240,
        .height = 400, .depthFmt = GPU_RB_DEPTH24_STENCIL8, .colorMask = 15, .depthMask = 3}, .ownsDepth = true};
    for (failure = 0; failure <= 5; ++failure) {
        reset(); capture_world_screen(stdout, 123, &target);
        assert(cpu_buffer == NULL);
        if (!failure) { assert(bytes_written == BYTES && write_calls); }
        if (failure <= 3 && failure != 0) assert(write_calls == 0);
        const char *record = strstr(log_text, "depth=left"); assert(record);
        assert(strstr(record, failure ? "saved=0" : "saved=1"));
    }
    failure = 0; allocation_size = BYTES - 4;
    reset(); capture_world_screen(stdout, 123, &target); assert(write_calls == 0);
    allocation_size = BYTES; target.frameBuf.width = 239;
    reset(); capture_world_screen(stdout, 123, &target); assert(write_calls == 0);
    target.frameBuf.width = 240; target.frameBuf.depthFmt = 0;
    reset(); capture_world_screen(stdout, 123, &target); assert(write_calls == 0);
    target.frameBuf.depthFmt = GPU_RB_DEPTH24_STENCIL8; target.ownsDepth = false;
    reset(); capture_world_screen(stdout, 123, &target); assert(write_calls == 0);
    target.ownsDepth = true; target.frameBuf.depthBuf = NULL;
    reset(); capture_world_screen(stdout, 123, &target); assert(write_calls == 0);
    reset(); capture_world_screen(stdout, 123, NULL); assert(write_calls == 0);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="ge-depth-capture-") as directory:
    tmp = Path(directory)
    def run(body, name, expected_success=True):
        c = tmp / (name + ".c")
        binary = tmp / name
        c.write_text(prefix + body + suffix)
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        str(c), "-o", str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        if expected_success and result.returncode:
            raise AssertionError(result.stderr)
        if not expected_success:
            assert result.returncode != 0 and "tracked_fwrite" in result.stderr, result.stderr

    run(capture, "safe-capture")
    # The original failing implementation passed the raw attachment to
    # fwrite. Prove that changing only the I/O source reintroduces detection.
    mutated, count = re.subn(r"fwrite\(\s*[^,]+,", "fwrite(fb->depthBuf,", capture)
    assert count == 1
    run(mutated, "unsafe-vram-mutation", expected_success=False)
print("SELECT depth capture: heap-only FS payload, exact 384000 bytes, failure cleanup, validation guards; direct-VRAM mutation rejected")

#ifndef GE_PORT_ULTRA64_H
#define GE_PORT_ULTRA64_H

/*
 * Minimal libultra-facing surface for portable GoldenEye modules.
 * Keep this header deliberately small: subsystems move onto ARM only after
 * every N64 service they use has an explicit implementation in port/src.
 */

#include <stddef.h>
#include <stdint.h>
#include <math.h>

/* Native builds have no KSEG0 alias; the portable ABI carries host addresses. */
#ifndef OS_K0_TO_PHYSICAL
#define OS_K0_TO_PHYSICAL(address) ((uintptr_t)(address))
#endif

#ifdef GE_PORT_USE_ORIGINAL_TYPES
#include <PR/ultratypes.h>
#else
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
#endif

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

typedef float f32;
typedef double f64;
typedef ptrdiff_t ssize_t;

/*
 * Controller structures used by the original joy.c.  These keep the source
 * layout and names, but the native frontend supplies samples rather than the
 * N64 serial-interface manager.
 */
#ifndef MAXCONTROLLERS
#define MAXCONTROLLERS 4
#endif

typedef struct OSContPad {
    u16 button;
    s8 stick_x;
    s8 stick_y;
    u8 errno;
} OSContPad;

typedef struct OSContStatus {
    u16 type;
    u8 status;
    u8 errno;
} OSContStatus;

typedef struct OSPfs {
    u32 native_port;
} OSPfs;

#define A_BUTTON 0x8000U
#define B_BUTTON 0x4000U
#define Z_TRIG 0x2000U
#define START_BUTTON 0x1000U
#define U_JPAD 0x0800U
#define D_JPAD 0x0400U
#define L_JPAD 0x0200U
#define R_JPAD 0x0100U
#define L_TRIG 0x0020U
#define R_TRIG 0x0010U
#define U_CBUTTONS 0x0008U
#define D_CBUTTONS 0x0004U
#define L_CBUTTONS 0x0002U
#define R_CBUTTONS 0x0001U
#define ANY_BUTTON 0xffffU

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef M_PI_F
#define M_PI_F 3.1415927f
#endif

#ifndef M_TAU_F
#define M_TAU_F 6.2831855f
#endif

u32 osGetCount(void);

/* Native source-compatible queue/event/timer surface for migrated modules. */
#include "ge_libultra_services.h"

/* Original game translation units declare their own 32-bit bool. Portable
 * frontend files keep stdbool's native bool. */
#ifdef GE_PORT_USE_ORIGINAL_TYPES
#ifdef bool
#undef bool
#endif

/* bondconstants.h owns this original-game constant. */
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#endif

#endif

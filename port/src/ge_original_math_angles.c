/*
 * Compile GoldenEye's angle implementation after the native math declarations
 * are visible. The N64 integer helpers were named acos/asin; rename only those
 * two private entry points so they do not collide with the host C library.
 * All three decompiled function bodies and lookup tables remain the source of
 * behavior, and the public acosf/asinf/atan2f names replace libm at link time.
 */
#include <ultra64.h>

#ifndef M_HALF_PI
#define M_HALF_PI (M_PI_F / 2.0f)
#endif
#ifndef M_THREE_HALF_PI
#define M_THREE_HALF_PI (3.0f * M_HALF_PI)
#endif

#define acos ge_original_acos_u16
#define asin ge_original_asin_s16
#include "../../src/game/math_asinacos.c"
#include "../../src/game/math_asinfacosf.c"
#undef asin
#undef acos

#include "../../src/game/math_atan2f.c"

/* Compile the canonical GoldenEye RZIP inflater against the native fixed-width
 * ABI.  Its source explicitly includes the N64 SDK string headers after
 * ultra64.h; their declarations conflict with the native ABI, so provide the
 * one used C library declaration and mark those two headers as already read. */
#include <stddef.h>
extern void *memcpy(void *destination, const void *source, size_t size);
extern void bzero(void *destination, int size);
#define _STRING_H_
#define __BSTRING_H__
#include "../../src/game/zlib.c"

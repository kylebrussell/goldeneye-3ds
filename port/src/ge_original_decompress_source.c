/* Compile the canonical GoldenEye RZIP decompressor.  decompress.c omits the
 * prototype supplied by the original build's aggregate zlib translation unit;
 * state it here without changing the decompiled body. */
extern int zlib_inflate(void);
#include "../../src/game/decompress.c"

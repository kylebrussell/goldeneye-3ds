#ifndef GE_ORIGINAL_EMBEDMENT_POOL_H
#define GE_ORIGINAL_EMBEDMENT_POOL_H

#include <stddef.h>

/* Exact initobjects free-marker initialization for the canonical embedment
 * array retained by the live prop/object slice. */
void ge_original_embedment_pool_reset_exact(void);
size_t ge_original_embedment_pool_capacity(void);

#endif

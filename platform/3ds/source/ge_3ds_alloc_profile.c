#include "ge_3ds_alloc_profile.h"
#include <stddef.h>
#include <stdatomic.h>
static atomic_uint enabled;
static atomic_uint counters[6]; /* malloc, calloc, realloc, free, requested bytes, failures */
extern void *__real_malloc(size_t);
extern void *__real_calloc(size_t, size_t);
extern void *__real_realloc(void *, size_t);
extern void __real_free(void *);
void ge_3ds_alloc_profile_enable(int value) { atomic_store(&enabled, value != 0); }
void ge_3ds_alloc_profile_snapshot(uint32_t values[6])
{
    for (unsigned i = 0; i < 6; ++i)
        values[i] = atomic_load_explicit(&counters[i], memory_order_relaxed);
}
static void record(unsigned kind, size_t bytes, const void *result)
{
    if (!atomic_load_explicit(&enabled, memory_order_relaxed)) return;
    atomic_fetch_add_explicit(&counters[kind], 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&counters[4], (uint32_t)bytes, memory_order_relaxed);
    if (bytes && !result) atomic_fetch_add_explicit(&counters[5], 1, memory_order_relaxed);
}
void *__wrap_malloc(size_t n) { void *p = __real_malloc(n); record(0, n, p); return p; }
void *__wrap_calloc(size_t n, size_t s) { void *p = __real_calloc(n, s); record(1, n*s, p); return p; }
void *__wrap_realloc(void *old, size_t n) { void *p = __real_realloc(old, n); record(2, n, p); return p; }
void __wrap_free(void *p)
{
    if (atomic_load_explicit(&enabled, memory_order_relaxed))
        atomic_fetch_add_explicit(&counters[3], 1, memory_order_relaxed);
    __real_free(p);
}

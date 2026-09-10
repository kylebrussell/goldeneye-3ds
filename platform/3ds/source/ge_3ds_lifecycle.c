#include "ge_3ds_lifecycle.h"

#include <3ds.h>
#include <stdatomic.h>

static aptHookCookie g_lifecycle_hook;
static bool g_lifecycle_hooked;
static atomic_bool g_resume_pending;

static void ge_3ds_lifecycle_hook(APT_HookType event, void *parameter)
{
    (void)parameter;
    if (event == APTHOOK_ONRESTORE || event == APTHOOK_ONWAKEUP) {
        atomic_store_explicit(&g_resume_pending, true, memory_order_release);
    }
}

void ge_3ds_lifecycle_init(void)
{
    if (g_lifecycle_hooked) return;
    atomic_store_explicit(&g_resume_pending, false, memory_order_release);
    aptHook(&g_lifecycle_hook, ge_3ds_lifecycle_hook, NULL);
    g_lifecycle_hooked = true;
}

void ge_3ds_lifecycle_close(void)
{
    if (!g_lifecycle_hooked) return;
    aptUnhook(&g_lifecycle_hook);
    g_lifecycle_hooked = false;
    atomic_store_explicit(&g_resume_pending, false, memory_order_release);
}

bool ge_3ds_lifecycle_take_resume(void)
{
    return atomic_exchange_explicit(
            &g_resume_pending, false, memory_order_acq_rel);
}

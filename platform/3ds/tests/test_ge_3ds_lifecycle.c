#include "ge_3ds_lifecycle.h"
#include <3ds.h>

#include <assert.h>
#include <stdio.h>

static aptHookCookie first_hook;
static unsigned registrations, removals;

/* Mirror libctru's displaced-head cookie list, including unhooking a hook
 * that has since been displaced by another module's registration. */
void aptHook(aptHookCookie *cookie, aptHookFn callback, void *parameter)
{
    assert(cookie != NULL && callback != NULL);
    *cookie = first_hook;
    first_hook.next = cookie;
    first_hook.callback = callback;
    first_hook.param = parameter;
    ++registrations;
}

void aptUnhook(aptHookCookie *cookie)
{
    aptHookCookie *hook;
    for (hook = &first_hook; hook != NULL; hook = hook->next) {
        if (hook->next == cookie) {
            *hook = *cookie;
            ++removals;
            return;
        }
    }
    assert(!"Removed a hook that was never registered or was already removed");
}

static void notify(APT_HookType event)
{
    aptHookCookie *hook;
    for (hook = &first_hook; hook != NULL; hook = hook->next) {
        if (hook->callback != NULL) hook->callback(event, hook->param);
    }
}

static void unrelated_hook(APT_HookType event, void *parameter)
{
    unsigned *calls = parameter;
    (void)event;
    ++*calls;
}

static void test_event_consumption(void)
{
    unsigned repeat;
    assert(!ge_3ds_lifecycle_take_resume());
    ge_3ds_lifecycle_close();
    assert(removals == 0U);
    ge_3ds_lifecycle_init();
    ge_3ds_lifecycle_init();
    assert(registrations == 1U && removals == 0U);
    assert(!ge_3ds_lifecycle_take_resume());
    notify(APTHOOK_ONSUSPEND);
    notify(APTHOOK_ONSLEEP);
    notify(APTHOOK_ONEXIT);
    notify(APTHOOK_COUNT);
    assert(!ge_3ds_lifecycle_take_resume());
    notify(APTHOOK_ONRESTORE);
    assert(ge_3ds_lifecycle_take_resume());
    assert(!ge_3ds_lifecycle_take_resume());
    notify(APTHOOK_ONWAKEUP);
    ge_3ds_lifecycle_init(); /* Repeated init must not discard a pending event. */
    assert(ge_3ds_lifecycle_take_resume());
    assert(!ge_3ds_lifecycle_take_resume());
    for (repeat = 0U; repeat < 100U; ++repeat) {
        notify(APTHOOK_ONWAKEUP);
        notify(APTHOOK_ONRESTORE);
    }
    assert(ge_3ds_lifecycle_take_resume());
    assert(!ge_3ds_lifecycle_take_resume());
    notify(APTHOOK_ONWAKEUP);
    ge_3ds_lifecycle_close();
    ge_3ds_lifecycle_close();
    assert(removals == 1U && first_hook.callback == NULL);
    assert(!ge_3ds_lifecycle_take_resume());
    notify(APTHOOK_ONRESTORE);
    assert(!ge_3ds_lifecycle_take_resume());
    ge_3ds_lifecycle_init();
    assert(!ge_3ds_lifecycle_take_resume());
    notify(APTHOOK_ONWAKEUP);
    assert(ge_3ds_lifecycle_take_resume());
    ge_3ds_lifecycle_close();
    assert(registrations == 2U && removals == 2U);
}

static void test_other_hooks(void)
{
    aptHookCookie earlier = {0};
    aptHookCookie later = {0};
    unsigned earlier_calls = 0U, later_calls = 0U;
    aptHook(&earlier, unrelated_hook, &earlier_calls);
    ge_3ds_lifecycle_init();
    aptHook(&later, unrelated_hook, &later_calls);
    notify(APTHOOK_ONRESTORE);
    assert(earlier_calls == 1U && later_calls == 1U);
    assert(ge_3ds_lifecycle_take_resume());
    ge_3ds_lifecycle_close();
    notify(APTHOOK_ONWAKEUP);
    assert(earlier_calls == 2U && later_calls == 2U);
    assert(!ge_3ds_lifecycle_take_resume());
    aptUnhook(&later);
    notify(APTHOOK_ONSLEEP);
    assert(earlier_calls == 3U && later_calls == 2U);
    aptUnhook(&earlier);
    assert(first_hook.next == NULL && first_hook.callback == NULL);
    assert(registrations == removals);
}

int main(void)
{
    test_event_consumption();
    test_other_hooks();
    puts("3DS lifecycle: resume coalescing, consumption and hook ownership tests passed");
    return 0;
}

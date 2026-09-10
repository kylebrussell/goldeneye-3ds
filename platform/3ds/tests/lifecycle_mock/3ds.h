#ifndef GE_3DS_LIFECYCLE_MOCK_H
#define GE_3DS_LIFECYCLE_MOCK_H

#include <stddef.h>

typedef enum {
    APTHOOK_ONSUSPEND = 0,
    APTHOOK_ONRESTORE,
    APTHOOK_ONSLEEP,
    APTHOOK_ONWAKEUP,
    APTHOOK_ONEXIT,
    APTHOOK_COUNT
} APT_HookType;
typedef void (*aptHookFn)(APT_HookType hook, void *parameter);
typedef struct tag_aptHookCookie {
    struct tag_aptHookCookie *next;
    aptHookFn callback;
    void *param;
} aptHookCookie;

void aptHook(aptHookCookie *cookie, aptHookFn callback, void *parameter);
void aptUnhook(aptHookCookie *cookie);

#endif

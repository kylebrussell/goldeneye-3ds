# 3DS lifecycle boundaries — September 10, 2026

`ge_3ds_lifecycle` records `APTHOOK_ONRESTORE` and `APTHOOK_ONWAKEUP` in an
atomic pending flag. Repeated notifications coalesce until the main thread
consumes the flag with `ge_3ds_lifecycle_take_resume()`. The callback performs
no allocation, service calls, audio work, input polling or timing changes.
Repeated initialization does not duplicate the hook or erase a pending event;
close unregisters the hook and discards the pending event.

## Main-loop integration contract

1. Call `ge_3ds_lifecycle_init()` after APT initialization, before any frontend,
   lobby or gameplay loop. Call `ge_3ds_lifecycle_close()` before APT shutdown
   on each common cleanup path.
2. Immediately after each `aptMainLoop()` returns, consume the resume flag
   before reading elapsed time or processing input. Treat it as an epoch
   change: set that loop's previous wall-clock value to the current time and
   exclude suspended time from the next scheduler pump. Preserve the existing
   retrace scheduler's phase and counters; restarting it clears its phase.
3. Cancel captured touch actions and suppress physical/touch inputs held across
   the transition until release. Clear pending action edges without generating
   a new fire/use/watch press.
4. Invalidate any active timing/capture probe explicitly and retain a reported
   interruption reason. Do not remove the interrupted frames and then declare
   a continuous 60 Hz capture successful.
5. Finish any outstanding audio worker and original music runtime work before
   changing their rings. Call `ge_3ds_audio_resume()` at this safe main-thread
   boundary, before fresh synthesis and the next normal audio pump. It clears
   the NDSP queue and discards both valid rings' queued pre-sleep samples,
   preserving synth state, bindings and diagnostic counters. Invalid rings
   follow the same stop/detach policy as the normal pump. Recheck
   `ge_3ds_audio_is_active()` after the drain/pump when choosing producer
   fallback. The APT callback must never manipulate NDSP or producer storage.

The integrated main loop implements these policies in `handle_runtime_resume`
and the gameplay-loop rebase block. Both frontend and gameplay resynchronize
their sink-active flag after a pump failure. Gameplay records `suspend_events`
and fails any interrupted probe; frontend probes record the explicit APT
interruption reason. `test_3ds_lifecycle_integration.py` executes the actual
helper with ordered service doubles and checks its surrounding time/probe
boundary. No ordinary slow-frame elapsed time is clamped.

Renderer initialization now checks shader instance allocation. Startup failure
uses common stage cleanup to release already-loaded collision/model assets,
while a readiness guard prevents freeing a renderer that never initialized.
That full allocation-failure path has source/ARM review, not physical fault
injection.

Libctru already owns DSP suspension and restoration. Its
[NDSP service hook](https://github.com/devkitPro/libctru/blob/master/libctru/source/ndsp/ndsp.c)
preserves and restores DSP state, so the application must not add an unrelated
NDSP teardown/reinitialization inside an APT callback. Its
[APT implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/apt.c)
invokes sleep and restoration hooks around the main-thread applet transitions.

## Software evidence and physical acceptance

`sh scripts/test_3ds_lifecycle.sh` compiles the real helper under ASan and UBSan
with an APT double matching libctru's displaced-head hook-cookie list. Tests
cover sleep/suspend/exit neutrality, duplicate wake/restore coalescing,
one-shot consumption, repeated registration and close, pending-event cleanup,
reinitialization and preservation of unrelated hooks registered before and
after this module.

The module also compiles with `-Wall -Wextra -Werror` against the installed
`devkitpro/devkitarm:latest` libctru headers for ARMv6K. Its object has only
`aptHook` and `aptUnhook` as unresolved symbols; atomic flag operations require
no additional runtime library with this toolchain.

`sh scripts/test_3ds_audio_lifecycle.sh` exercises NDSP initialization failure,
linear allocation failure, each initial cache-flush failure, runtime failure,
idempotent cleanup, ring validation, overlapping bindings, rate changes and
owned-buffer preservation. Its simulated stalled queue is an ownership check,
not a physical lid-sleep test. Resume tests queue negative sentinel samples in
NDSP and both rings, drain them, and then require every resubmitted sample to
be freshly produced mixed PCM or zero-filled underflow. They also check
preserved counters, repeated drains, and inactive/failed/invalid-ring drains.

Real-console lid close/open and HOME/restore still need testing during menus,
gameplay, worker activity and transitions. Verify no simulation catchup, held
input action, stale queued audio, device error, leak or hang. Repeat cycles and
exit after restoration. Neither mocked event delivery nor ARM compilation
establishes physical audio quality, frame pacing or suspend behavior.

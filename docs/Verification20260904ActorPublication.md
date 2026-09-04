# 2026-09-04 actor publication performance checkpoint

This checkpoint keeps the original `chrlvAllChrTick`, `propsTick`, `chrTick`,
animation, visibility, model-matrix, weapon/hat parent, and draw-order
semantics.  The changes are confined to native renderer metadata and
instrumentation.

## Attachment indexing

`GeOriginalStageGuardRuntime` now builds stable per-guard linked indices when
authored weapons/hats are bound (and when an original runtime hat is added).
The render-matrix retention and scene-input publication paths walk only that
guard's attachments, in their original global setup order.  The canonical
prop parent relation still decides whether a dropped attachment is live.

This removes the former `guards * (all weapons + all hats)` scan from every
displayed guard-matrix pass.  In byte-matched 750-tick Azahar probes:

| Stage | guard matrix ticks before | after | reduction | total frame time |
|---|---:|---:|---:|---:|
| Dam | 23,227,549 | 7,252,803 | 68.8% | 7,274 -> 7,221 ms |
| Caverns | 96,592,847 | 15,335,310 | 84.1% | 5,601 -> 5,296 ms |

Dam's movement, room, visibility, scene-cache, guard-combat, and draw counters
are unchanged in `active-vis-dam750.result` and
`actor-index-cache-dam750.result`.  Caverns completed 750 ticks with no frame
over 16 ms after warmup in `actor-index-cache-caverns750.result` (the earlier
run had one 17 ms frame).

## Topology working set

Immutable actor geometry remains deduplicated in the component cache.  The
compact aggregate topology ring now retains 32 states rather than eight.  A
Dam combat route had previously reached 33 rebuilds and 24 evictions; the
larger cache reduces repeated metadata reconstruction when visibility,
weapons, hats, injury, and death relations cycle.  The sanitizer test now
exercises a 24-state room-scale working set without eviction, while the
existing 40-component stress test continues to exercise eviction/lifetime
correctness.

## Gameplay attribution

The input-probe result now publishes `gameplay_phase_ticks` for whole original
tick, background character pass, unchanged MoveBond, unchanged gun update,
mixed props/post services, diagnostic combat audit, and props region.  On the
Dam 750-tick route, MoveBond and gun update together are below 0.2 ms/tick;
the next CPU target is the unchanged `propsTick`/`chrTick` graph at roughly
2.2 ms/tick.  A trial of blanket `-O3` on the generated canonical units did
not provide a meaningful gain and was rejected; the normal strict build flags
remain in use.

## Caverns visual investigation

The two actors seen against the elevator at spawn are exact authored guards,
not synthetic placement: Caverns starts at pad 368 and guard pads 0/1 are on
the same floor immediately ahead.  The remaining visual question is the
authored elevator-door/script/portal presentation, and should be fixed at that
original door/mission boundary rather than hiding or moving the guards.

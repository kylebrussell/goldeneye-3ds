# Watch crash and completed depth captures — 2026-09-11

The tested build was `2079d8b5e1f0cbeb967b27df6e9bb4016b867af3`, executable
SHA-256 `9777652ed9e940a758aacaaaebeafff62373a1ee118e8913a77716cc5c035a3d`.
The SD evidence and its verified ELF were preserved in
`build/hardware-captures/watch-gate-20260911-195036/` before deployment changes.

## Watch crash

Luma dump `crash_dump_00000001.dmp` records a data abort at PC `0x00223050`,
writing FAR `0x0061c314`. The matching ELF resolves the PC to
`ge_original_gun_update_and_fire_exact`, generated line 493: `*flashvisptr = 0`.
The original player storage is `0x0060c070`; the invalid flash pointer is outside
its storage and beyond the executable's BSS. The boot log ends during Watch
state 0→1. The fault is in model preparation, not touchscreen hit testing.

Startup preloaded a plain PP7 into the right model region and a silenced PP7
into the left. The item acquisition cache could lend the left model to the
right hand when binding Dam's silenced PP7. Opening Watch replaces the left
region with `Csuit_lf_handZ`, freeing the native model that the right hand's
copied header still referenced. An unchanged item ID prevented the renderer's
sync helper from rebinding it. Reused memory supplied an invalid flash index.

Acquisition now reuses only the requested physical hand's model region. Each
hand can load/change its own model without invalidating the other's copied
header. This also separates identical dual weapons' mutable model metadata.
The regression follows startup, dual silenced-PP7 acquisition, and the actual
`load_object_fill_header` Watch load. With the old acquisition implementation,
a private regression variant reproduces ASan heap-use-after-free; with the fix,
the right copied header, flash node, and data remain valid and unchanged.
Evidence: `watch-ownership-uaf-repro.log` in the capture backup.

## Gate evidence

The new log contains 28 accepted activations of panel command 269, runtime
flags `0x4011`, four switch links and no omissions. One activation edge is
reported per press. The last SELECT capture precedes the first gate attempt,
so there is no post-activation gate state in this session's captures.

A bounded automatic trace now starts after accepted Dam gate/panel activation.
It records both gates immediately, after the first prop tick, at state changes,
every 15 ticks, and at the 120-tick endpoint. Records include displacement,
state, speed, last native calculation tick, links, prop tick status and collision
counters. The read-only native snapshot does not publish matrices or change
runtime state. No gate behavior fix is claimed without this missing evidence.

## Rendering evidence

Five complete 384,000-byte depth files confirm the SELECT filesystem fix worked
in this hardware session. Four broadly match depth replay of the captured
vertices. The tunnel capture at tick 54782008618 has only 25,750 nonzero depth
pixels where the replay expects 76,800. Near opaque surfaces have visible color
but zero depth, allowing later distant blue geometry to overwrite them.

Suppressing only depth writes for draws 0–12 in the replay, restoring writes
at draw 13 after the first translucent-to-opaque transition, matches 95,991 of
96,000 actual depth pixels within 1e-5. Thus the issue is intermittent initial
depth-write state; a global projection flip or missing room submission does not
explain these data. The exact GPU command cause remains unproven. Detailed
replay scripts/results: `build/host-tests/depth-evidence-20260911/`.

Hardware Watch, gate motion and rendering retesting are still required.

## Next capture and validation

SELECT now also preserves the first 32 left-eye world draw records: the CPU
context's depth word and the actual emitted depth/color-mask, framebuffer access,
fragment-operation and stencil register words. Known/emitted byte masks distinguish
inherited values from partial writes. A bounds-checked parser reads the existing
command buffer and writes no GPU state. A static assertion verifies the pinned
SDK's depth-field offset. Records are retained until the next completed left pass
and cleared on stage restart; filesystem output still occurs only on SELECT.

Focused ASan/UBSan checks passed: first-person model acquisition/Watch replacement,
first-person pose, gate trace lifecycle/nonmutation, depth capture, and command
trace parser/lifecycle. The renderer cache check and 50,000 exact draw-stream
comparisons also passed, including tracing enabled and disabled. The actual
2/4-player gun producer passed using the installed asset pack; native door
interaction checks passed. The full campaign suite was not repeated for these
changes. Forced ARM build and SD deployment have separate receipts.

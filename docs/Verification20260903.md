# Native 3DS performance checkpoint — 2026-09-03

This follows source checkpoint `f7ce9667` and its separately saved candidate
`421c99d1...`. Sustained 60 FPS across levels, hardware performance, and
end-to-end high-fidelity playability are still not demonstrated. macOS
remained locked during this pass; no new Azahar measurements are claimed.

## Prepared batch visibility

The last measured Dam profile (`dam-material-key-8f8d05d5-aim.result`) recorded
781,479 batch-frustum tests in 12.594 seconds. The old composition repeatedly
validated the same matrix for each batch and could transform its first vertex
again when falling through to the scalar walk.

The renderer now snapshots and validates the authored-world, runtime-world,
and eye-space clip matrices once per draw pass. A prepared visibility query
retains the first outcode when continuing the original scalar AND walk.
Bounds classification uses the already-validated matrix. Floating-point
operation order, tangent-plane treatment, invalid/nonfinite fail-open behavior,
and the five profiling outcomes are unchanged. No extracted frustum planes,
approximate rejection, gameplay visibility changes, or new draw sorting.
Contexts are recreated after camera publication, not cached across frames.

Validation includes 20,000 randomized comparisons with and without bounds,
negative/zero W and one-ULP boundary values, nonfinite matrix elements and
vertices, overflow, invalid ranges, and context snapshot lifetime. The
authored Dam scene also matches all 55,936 batch decisions over 64 camera
angles. The focused `-O2` ASan/UBSan host scene run measured 0.022 seconds for
the old composition versus 0.018 seconds prepared over 256 repetitions;
this is a host microbenchmark, not an emulator frame-rate improvement.

Evidence: `build/host-tests/prepared-visibility-20260903.log` and
`build/host-tests/dam-prepared-visibility-20260903.log`.

## HUD vertex-storage clearing

The ordinary gameplay render path cleared two 4,096-vertex HUD arrays and
two copies of the 768-vertex gameplay-HUD storage each frame: 342 KiB of
vertex storage, even with empty message queues. Builders now reset only
their publication metadata, then fully initialize each emitted vertex.
The renderer uses the same metadata-only reset when the gameplay-HUD builder
is not called. Unused capacity is explicitly unspecified and is never drawn
or uploaded. Original HUD timers, glyphs, colors, positions, gauges, ammo,
watch state, frontend behavior and draw ordering are unchanged.

Every existing font/gauge/menu test now builds into both poisoned and zeroed
storage. All published vertices and metadata match byte-for-byte, while
unused storage retains its poison pattern. These checks cover upper/lower
messages, gameplay gauges/ammo, watch objectives, credits, and both frontend
font paths. Focused ASan/UBSan passed.

Host-only `-O2` benchmark, 20,000 iterations of all three HUD builders:

| Input | Original full clearing | Metadata-only reset |
| --- | ---: | ---: |
| Hidden HUD/messages | 47.02 ms | 0.07 ms |
| Visible messages and ammo | 63.65 ms | 17.00 ms |

This compares the actual implementation from `f7ce9667` with the new source,
using identical ROM-backed fonts and inputs. It excludes the extra old clear
in `renderer_draw`, and is not a measurement of gameplay FPS. Private driver
and evidence: `build/host-tests/hud_publication_bench.c`,
`hud-fullclear-baseline-20260903.log`, `hud-publication-optimized-20260903.log`.

## Next emulator measurement

Input-probe reports now split `renderer_phase_ticks` into four sequential
regions: CPU preparation/vertex flushing, sky/world submission,
effects/first-person submission, and final HUD submission. The
`renderer_peak_phase_ticks` line records the slowest renderer pass after
120 presentations (total followed by its four regions). Counts are system
ticks at the existing reported tick frequency, not milliseconds or GPU time.
These distinguish the remaining renderer cost when Azahar becomes available.

ARM/3DSX passed (`arm-visibility-hud-20260903.log`). The linked ELF retains
`MoveBond`, `bondviewProcessInput`, `ge_original_gun_live_tick`,
`ge_original_stage_active_props_tick_exact`, and the new visibility/reset
entry points. Original gameplay code was not changed.

The complete host suite passed after integration
(`visibility-hud-full-20260903.log`). Focused ASan/UBSan visibility, authored
Dam, and HUD publication tests passed, as did the runtime coordinate-space,
renderer-submission-cache and frame-pacing source checks.

Candidate executable SHA-256:
`c8593eee734030268dce8e525f4b00dd3bad138e36a4f1fbb3c80dd255ba0201`.
Saved at `build/3ds-candidates/visibility-hud-c8593eee/goldeneye-3ds.3dsx`.
It pairs with unchanged assets SHA-256
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
The hardware staging directory and Azahar virtual SD remain at verified
executable `0797edaa...`; no temporary stage/input configurations were added,
and saves/DSP settings are untouched. This candidate includes the earlier
unvalidated idle-presentation gate and texture-index changes.

Once unlocked, run the existing Facility movement and Dam aim/combat traces
on this exact candidate/asset pair, then another stage. Compare work per
simulation interval as well as presentation count, because the idle gate
changes that denominator. Check movement, HUD/menus, audio, clipping,
scene-publication peaks, and the new renderer phase breakdown. Do not promote
the candidate or infer reliable 60 FPS from these host microbenchmarks alone.

## Follow-up: in-place middle-overlay topology publication

Source checkpoint `ce0f9e99` was followed by an optimization of articulated
prop publication. Previously only the final guard segment could reuse scene
capacity. A middle prop change allocated all three scene buffers and recopied
the resident room prefix even when the existing allocations were large enough.

Disjoint replacement inputs now reuse that capacity. Only the retained
overlay suffix moves; replacement batches keep their authored order, and
local/global vertex offsets are rebased exactly. Equal-sized changes do not
move or republish an unchanged suffix. The resident room prefix and unrelated
prefix props remain byte-identical. Allocation limits and existing headroom
policy are unchanged; previously allocated capacity is retained across these
changes, as it already was for guard-tail updates.

All range, capacity and triangle-overflow validation completes before mutation.
Overlapping middle inputs use the old allocate-before-publish path so suffix
movement cannot overwrite their source. The existing alias-safe guard-tail
path remains supported. Geometry growth beyond retained capacity still uses
the allocating path; this change does **not** eliminate cold guard-growth
copies or claim to remove every scene-publication hitch.

The live input report adds `overlay_publication_paths`: successful in-place
replacements, successful allocating replacements, and vertices moved in
overlay suffixes. These include startup publications and are not gameplay
tick counts. The small part-record table may still allocate independently;
this optimization removes scene-buffer allocations, not every allocation in
the articulated-prop workflow.

Verification:

- Focused ASan/UBSan: insertion, growth, shrinkage, deletion, retained pointer
  identity when capacity fits, exact room/peer/door/guard data, middle vertex
  and batch source aliasing, triangle-overflow rollback, and existing tail
  alias behavior (`middle-overlay-sanitizer-20260903.log`).
- An additional ASan/UBSan stress run covers the initial authored room sets
  of all 21 stage descriptors, with 300 synthetic middle-topology changes per
  stage after warmup. All 6,300 changes preserve room/prefix/suffix data and
  use zero new vertex-buffer allocations
  (`middle-overlay-all-stages-sanitizer-20260903.log`). These are renderer
  transactions, not AI/gameplay replays or completed missions.
- Full host suite passed (`middle-overlay-full-20260903.log`). ARM/3DSX passed
  (`arm-middle-overlay-20260903.log`). Original gameplay bodies are unchanged.

Host-only `-O2` comparison of the actual `ce0f9e99` implementation and the final
new implementation, each with identical authored room data and synthetic
replacement inputs (totals for 300 changes, **not frame times**):

| Stage room prefix | Previous | In-place | Vertex-buffer replacements |
| --- | ---: | ---: | ---: |
| Dam | 7.548 ms | 0.375 ms | 300 → 0 |
| Facility | 3.907 ms | 0.321 ms | 300 → 0 |
| Control | 6.506 ms | 0.350 ms | 300 → 0 |
| Egyptian | 9.511 ms | 0.388 ms | 300 → 0 |

All 21 stages showed the same 300-to-zero allocation reduction. Private
evidence: `middle-overlay-baseline-final-20260903.log`,
`middle-overlay-optimized-final-20260903.log`, and the driver
`build/host-tests/middle_overlay_bench.c`.

New candidate executable SHA-256:
`0f305d530343c1b7cdd8d6bbaaacaa51fc8c653a3fc3dbbc582330b979b4eccd`.
Saved separately at
`build/3ds-candidates/middle-overlay-0f305d53/goldeneye-3ds.3dsx`, with the same
`938536d4...` assets. macOS remained locked, so the verified hardware stage
and Azahar virtual SD still contain `0797edaa...`. No stage/input config,
save, DSP setting, or emulator instance was changed.

Next unlocked verification should use this latest candidate, including all
preceding pending optimizations. Repeat Facility movement and Dam aim/combat,
check exact prop/guard suffix publication and allocation-path counters, then
use renderer phase timing to select the next measured bottleneck. Reliable
60 FPS and high-fidelity gameplay remain unverified in this candidate.

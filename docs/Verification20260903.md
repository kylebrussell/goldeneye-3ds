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

## Follow-up: independent scene-buffer growth

After `de7e316d`, a topology change that exceeded any one capacity still
allocated all three buffers. The fallback now retains each buffer that fits:
vertices, published batches, and overlay-local batches. A batch-only growth
does not copy the resident room vertices, and vertex-only growth can keep both
batch views. A retained published batch view also avoids rewriting its
unchanged prop prefix. Authored geometry, material bytes, ordering, room
residency, and original gameplay bodies are unchanged.

Every range/triangle check and every needed allocation completes before any
retained buffer is modified. Overlapping inputs keep the prior all-buffer
transactional fallback (and the existing alias-safe, no-growth tail path).
Allocation failure frees only newly owned buffers. Optional 25% overlay
headroom still falls back to exact-size allocation under memory pressure;
there is no larger guessed guard reserve or change to scene limits.

The new `overlay_buffer_growth` report contains successful topology
replacements of vertex/published-batch/local-batch buffers, followed by the
number of resident room vertices copied by those replacements. Like
`overlay_publication_paths`, this includes startup operations and is not a
gameplay-tick counter. Room residency transactions are not included. Use it
to distinguish unavoidable vertex growth from batch-only growth in the next
live trace; old reports cannot resolve that split.

Validation:

- `scripts/test_scene_overlay_growth.sh` runs without ROM assets and is now
  part of the full suite. Its 416 ASan/UBSan cases cover all eight capacity
  combinations, tail/middle changes, vertex/batch source aliases, each
  allocation-failure boundary, exact-size retry, asymmetric vertex/batch
  growth and shrinkage, and invalid/overflow input. Failure checks compare
  the complete scene metadata and allocated storage, not just logical counts.
  Allocator fault injection exists only in the test translation unit.
- The authored Dam scene transaction test passes, including previous middle,
  tail, part-table, visibility/residency, and rollback coverage.
- A private all-stage ASan/UBSan stress test performs 5,376 cold renderer
  publications against the initial authored room sets of all 21 descriptors.
  Room/overlay bytes, local/global offsets, counts, and triangles match. Each
  stage's 128 batch-only changes replace zero vertex buffers (formerly 128);
  its 128 vertex-only changes replace neither batch buffer (formerly 128 of
  each). These are synthetic renderer transactions, **not** gameplay traces.
- Full host suite and ARM/3DSX pass. The linked ELF retains `MoveBond`,
  `bondviewProcessInput`, `ge_original_gun_live_tick`, and
  `ge_original_stage_active_props_tick_exact`.

Host-only `-O2` sample, totals for 128 cold batch-growth transactions:

| Authored room prefix | `de7e316d` | Independent growth |
| --- | ---: | ---: |
| Dam | 3.562 ms | 1.007 ms |
| Facility | 2.484 ms | 0.777 ms |
| Control | 2.926 ms | 0.951 ms |
| Egyptian | 4.991 ms | 1.148 ms |

These exclude preparing each cold scene and are not emulator frame times.
Host load causes noticeable run-to-run variation, and vertex-growth timings
did **not** improve consistently. Actual vertex-capacity growth still copies
the full room prefix; this optimization does not eliminate that remaining
hitch. No live performance gain or reliable 60 FPS is asserted.

Evidence under `build/host-tests/`: `independent-growth-faults-20260903.log`,
`independent-growth-authored-20260903.log`,
`independent-growth-final-20260903.log`,
`arm-independent-growth-final-20260903.log`, and
`independent-growth-final-{baseline,optimized,sanitizer}-20260903.log`.
The private comparison driver is `independent_growth_bench.c`.

Latest candidate executable SHA-256:
`54edff694e27a3e7c26ad13be86a7d650ad9bee6be7eda4fcfbc82d6b708f42c`.
Saved at `build/3ds-candidates/independent-growth-54edff69/goldeneye-3ds.3dsx`.
Assets are unchanged at `938536d4...`. macOS remains locked. Hardware staging
and Azahar's virtual SD retain the verified `0797edaa...` executable; no
temporary stage/input config, save, DSP setting, or emulator instance changed.

Next unlocked step: test this latest candidate (including all preceding
pending optimizations) on Facility movement and Dam aim/combat, then another
stage. Check allocation counters alongside the renderer phase breakdown,
original input/AI, moving geometry, HUD/menus, and audio. Only then choose the
next dominant bottleneck or promote the candidate. The remaining true vertex
growth, renderer submission cost, all-level performance, Dam completion, and
New 3DS XL fidelity/performance still need live validation.

## Follow-up: room membership and matrix-bank owner lookup

Starting from `35fece21`, the world-render pass now snapshots membership of
the original visible-room result into a 256-byte map once per pass. The old
lookup scanned that same ordered list for each batch and merge candidate.
Authored room IDs are uint8_t; wider nonmatching query IDs remain rejected.
Unready visibility remains fail-open. The ordered result and all non-renderer
users are unchanged. Nothing is cached across frames, and no portal, room
residency, or gameplay visibility policy changes.

The model-scene preparation pass now checks whether the preceding part uses
the same matrix-bank pointer and count. If so, it reuses that part's already
resolved **earliest** owner; otherwise it performs the previous first-match
search. The temporary owner array, offsets, quantization count, matrix hashes,
publication signatures, and floating-point operation order retain their old
values. This avoids repeated prefix scans for consecutive body/head/weapon
parts without new allocations or skipping original animation/model ticks.

A proposed narrow-value hash rewrite was tested and **not retained**. ARM GCC
already folds the four zero-byte FNV steps into the same constant multiply.
The old/new helper assemblies are identical apart from labels, so there is
no claimed hash optimization. Private evidence: `hash_codegen.c` and
`hash_codegen.arm.s` under `build/host-tests/`.

Validation:

- `scripts/tests/test_renderer_room_membership.py` compiles the actual
  platform/model helper bodies with ASan/UBSan. It compares 10,240,000 room
  queries against the old list scan, including all byte-valued IDs, duplicates,
  empty/unready lists, large query IDs, changed next-pass inputs, and snapshot
  lifetime. Source checks retain rejection before material/projection changes
  and preserve authored merge ordering.
- The same test verifies 262,144 exact earliest-owner results for consecutive,
  interleaved, randomized, null-bank, and changed-count inputs. For 256 parts
  grouped eight per bank, actual helper comparisons fall from 31,968 to 4,223.
  This is an algorithm-work count, not an FPS or whole-model speedup.
- The existing authored PP7, window, and Dam gate model-scene ASan/UBSan tests
  pass, including shared-bank reuse, pose publication, topology variants,
  unchanged inputs, and dirty ranges (`bank-owner-model-scene-20260903.log`).
- A private room-membership stress run uses the initial authored geometry of
  all 21 stage descriptors with 1/4/10-ID subsets (clamped to available rooms),
  changing one sampled ID each pass. Old/new decisions match. The ASan/UBSan
  run passes. These synthetic sets are **not** original portal-visibility
  gameplay traces or evidence that missions have been completed.
- Final combined host suite and ARM/3DSX pass. The ELF retains original
  `MoveBond`, `bondviewProcessInput`, gun and active-prop dispatch entry points.

Host-only `-O2` sample for 512 membership passes over the ten-ID cases,
including rebuilding the map on each pass:

| Authored geometry | Previous list scan | Membership map |
| --- | ---: | ---: |
| Dam, 874 batches | 0.810 ms | 0.163 ms |
| Facility, 547 batches | 0.455 ms | 0.110 ms |
| Control, 739 batches | 0.553 ms | 0.137 ms |
| Egyptian, 1,031 batches | 1.041 ms | 0.191 ms |

These totals measure only lookups, not drawing or emulator frame times.
Evidence under `build/host-tests/`: `room-bank-helpers-20260903.log`,
`room-membership-{optimized,sanitizer}-20260903.log`,
`room-bank-final-20260903.log`, and `arm-room-bank-20260903.log`.
Private driver: `room_membership_bench.c`, with extracted actual helpers in
`room_membership_bodies.inc`.

Latest candidate SHA-256:
`079305a8393a6b876d59872373a3d6d1c298c046037d3ba4a49be0745bba2696`.
Saved separately at `build/3ds-candidates/room-bank-079305a8/goldeneye-3ds.3dsx`.
The asset pack remains `938536d4...`. macOS stayed locked, so hardware staging
and Azahar still contain verified executable `0797edaa...` (distinct from this
new `079305a8...` candidate). No saves, input/stage configs, DSP settings, or
emulator instances changed.

Next: run the latest combined candidate in Azahar when UI access returns,
starting with Facility movement and Dam aim/combat. Read renderer phase and
allocation counters alongside simulation work and frame tails before selecting
the next optimization. Actual vertex-buffer growth still copies room geometry.
Reliable 60 FPS, all-level gameplay, Dam completion, and New 3DS XL behavior
remain unverified; host tests do not satisfy those goals.

## Follow-up: exact color-upload lookup candidate — not performance-validated

At `fb3276ca`, ARM disassembly of `upload_dam_gpu_world_scene_range` showed
four `vcvt.f32.u32` and four `vdiv.f32` operations per vertex (in each of its
UV-mapping/nonmapping loop variants). The old Dam combat trace uploaded
4,910,934 guard vertices, so this is a recurring target-side arithmetic cost,
not just a startup conversion.

The candidate replaces these byte-to-float divisions with a 1 KiB read-only
table. Its 256 entries are constant expressions using the same `/ 255.0f`
division, not a rounded reciprocal multiply. Current RGBA bytes are still
sampled on every upload, including topology-stable dynamic monitor colors.
Position, UV preservation/remapping, upload ranges, bounds invalidation, dirty
range accumulation, and first-person/HUD paths are unchanged.

Correctness and generated-code evidence:

- All 256 normalization results match byte-for-byte. The test deliberately
  checks that reciprocal multiplication differs for 126 values; that shortcut
  is not used.
- The actual helper passes ASan/UBSan over 1,074,920 vertices in 2,048 changing
  color/range/UV passes. Complete output buffers match the old arithmetic,
  including untouched prefixes/suffixes and zero-length ranges.
- The initial authored geometry of all 21 stage descriptors passes 256
  changing-color publications per stage against the old helper, with exact
  output bytes under ASan/UBSan. This is a renderer test, not gameplay or
  original mission completion.
- Cross-compiling the actual extracted helper/table and inspecting `.rodata`
  confirms all 256 ARM binary32 words (1,024 bytes). The final ARM world-upload
  loop has zero `vdiv.f32` and zero `vcvt.f32` instructions.
- Full host suite and ARM/3DSX pass. Evidence under `build/host-tests/`:
  `renderer-vertex-colors-20260903.log`, `color-upload-sanitizer-20260903.log`,
  `vertex-colors-full-20260903.log`, and `arm-vertex-colors-20260903.log`.
  Generated audit inputs/data are under `renderer-vertex-colors/`.

**No speedup is established.** The Mac-native `-O2` packing microbenchmark was
slower with the lookup; disabling vectorization did not reverse that result.
For Dam's 9,129 vertices over 256 publications, the default build measured
3.112 ms with divisions versus 3.868 ms with the lookup; scalar compilation
measured 3.440 ms versus 3.835 ms. This host does not execute the same code as
the ARM11 target, but the regression must not be presented as a performance
gain. Fewer ARM divisions alone also do not prove faster gameplay. Logs:
`color-upload-optimized-20260903.log` and `color-upload-scalar-20260903.log`;
private driver: `color_upload_bench.c`.

The two exact candidates are retained for an isolated target A/B test:

- Control, before color lookup:
  `build/3ds-candidates/room-bank-079305a8/goldeneye-3ds.3dsx`, SHA-256
  `079305a8393a6b876d59872373a3d6d1c298c046037d3ba4a49be0745bba2696`.
- Experimental color lookup:
  `build/3ds-candidates/color-lookup-bdd7f472/goldeneye-3ds.3dsx`, SHA-256
  `bdd7f472bb4bbfd5a6c28dc08526fe004ea8807b15453df2157a6d6ee8a18fb6`.

Both use unchanged assets `938536d4...`. macOS remained locked, so neither was
promoted to hardware staging or Azahar. Those still contain verified
`0797edaa...`; saves, input/stage configs, DSP settings, and emulator instances
were untouched.

Next unlocked work should compare these two builds on the same Dam combat
and Facility movement traces without concurrent host builds. Compare guard
upload ticks per vertex, frame tails, and simulation work, not just a single
FPS average. If target timings regress or fail to improve, drop the lookup
change; do not stack further assumed wins on top of it. The earlier pending
optimizations also still require combined target validation. Sustained 60 FPS,
all-level gameplay, and high-fidelity mission completion remain unproven.

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

## Follow-up: reject mixed experiments; remove material-state copying

The normal build now defaults `GE_3DS_EXPERIMENT_COLOR_LOOKUP` to zero.
The saved color-lookup A/B pair above remains available, but its unproven
lookup is not the default for subsequent work. Both settings pass the actual
vertex-upload helper's sanitizer/byte-equivalence tests. ARM disassembly
confirms the normal build uses the original divisions and omits the lookup
table. This reverses an unvalidated experiment, not an established regression
in emulator performance.

Several further experiments were **not retained**:

- Reading clip-test XYZ from the 36-byte GPU records instead of the 132-byte
  decoded records produced exact decisions, but mixed host timings. Removing
  its temporary XYZ stack copies did not establish a consistent gain. The
  normal visibility API/renderer remain unchanged. Private source patch:
  `build/host-tests/strided-position-experiment-20260903.patch`; comparison
  evidence: `strided-visibility-xyz-20260903.log`.
- An exact most-recent preparation-key check avoided hashes but added a
  comparison on misses. Dam's authored material sequence was slower, so that
  extra cache state/check was removed. Evidence:
  `material-locality-{optimized,sanitizer}-20260903.log`.
- Alternative per-word hashes initially looked faster with host enum layout,
  but short-enum tests revealed mixed timings and additional conflicts in
  several stages. The previous hash, capacity, key and replacement policy are
  retained exactly. Do not promote the rotation or shift variants from the
  private `material-hash-*` / `material-shift-hash-*` probes.

The retained renderer change compares the GPU-state suffix directly, skipping
only the same two diagnostic words that were previously zeroed in temporary
copies. A layout assertion prevents silently skipping newly added fields.
Texture binding equality, all effective state bytes (including padding),
draw decisions/order and diagnostic results are unchanged. This does not
borrow mutable materials or add a cache lifetime/invalidation requirement.

On ARM, each such comparison no longer copies two 32-byte state records or
performs four diagnostic-word stores; `memcmp` reads the original 24-byte
suffixes. The containing apply helper's stack reservation fell from 84 to
52 bytes. This is a small CPU-copy reduction, not a new gameplay feature or
evidence of reaching 60 FPS.

Verification:

- The actual comparison helper matches the prior copy/zero implementation
  for all 256 mutations of every state byte, both operand orders, null inputs,
  different texture bindings and diagnostic-only differences. Inputs remain
  byte-identical. Tests run under ASan/UBSan with both native and short enums.
- Existing exact preparation, deliberate collision/eviction, all 65,536
  texture IDs, fallback and material-byte mutation tests pass in both layouts.
- All 21 initial authored stage room sets match baseline preparation results,
  complete cache entry bytes, LRU bytes, hit/miss totals and GPU-state equality
  under short-enum ASan/UBSan. These are renderer inputs, not level completion.
- Full host suite and ARM/3DSX pass. The linked ELF retains original MoveBond,
  input, gun and active-prop dispatch. Final evidence under `build/host-tests/`:
  `material-state-compare-20260903.log`, `color-default-off-20260903.log`,
  `material-state-all-stages-20260903.log`,
  `material-state-compare-final-20260903.log`,
  `arm-material-state-compare-20260903.log`, and
  `material-state-compare-arm-disassembly-20260903.log`.

A host-only comparison microbenchmark alternated old/new measurement order
over 6,291,456 calls. With short enums (32-byte state), copies took 11.445 ms
versus 10.583 ms direct; native 64-byte state took 21.099 versus 12.868 ms.
These isolate the comparison helpers, exclude the renderer and do not measure
emulator or hardware FPS. Private driver: `material_state_compare_bench.c`;
logs: `material-state-compare-bench{,-host}-20260903.log`.

New candidate saved separately:
`build/3ds-candidates/material-state-ad4136ef/goldeneye-3ds.3dsx`, SHA-256
`ad4136ef6efc2cda4e8362176ea7c3c66eb36a6b42674ee16636817f1b74b0c3`.
Assets remain `938536d4...`. macOS stayed locked. Verified hardware staging
and Azahar still contain `0797edaa...`; no emulator instance, saves, input/stage
configuration or DSP configuration changed.

Next: prioritize the accumulated candidate's actual Azahar Facility movement
and Dam aim/combat comparisons as soon as UI access returns. Use the renderer
phase counters and slow-frame tails to select the next substantial bottleneck;
do not spend another locked turn retuning the rejected hashes or stacking
unmeasured micro-optimizations. Sustained 60 FPS and end-to-end high-fidelity
mission completion remain unverified.

## Follow-up: incremental resident-room geometry preparation

Continued from `1de6c80c`. The room-streaming transaction still reread all
retained room blobs and ran their sizing/emission GBI passes every time one
new room was requested (or an old room was evicted). This is movement/room
transition work, separate from the earlier per-draw micro-optimizations.

The dynamic scene now owns a small range/statistics record per resident room.
A prepared transaction copies the immutable decoded slices of retained rooms
and reads/decodes only new rooms. Original room/primary/secondary list order,
all vertex fields, material state, commands/triangle counts and overlay order
are preserved. Surviving batches are rebased by the exact old/new room offsets.
The original queue, room ages, eviction decision and generation/commit boundary
are unchanged. No original gameplay system was replaced or reticked.

Range metadata follows the same ownership as candidate geometry: allocated
off to the side, freed on failure/abort, transferred on commit, and released
when the scene closes. No extra copy of resident geometry is retained between
transactions. Existing combined buffers still allocate/copy at prepare time;
this removes redundant blob I/O/GBI decoding, not all allocation or upload cost.
The input report adds `room_geometry_work=decodes,reuses`, counting room work
in successful publications, including startup. Failed/aborted attempts do not
inflate these committed-work counts.

Validation:

- Dam sanitizer tests compare fresh and incremental scene metadata, all
  defined vertex fields and batch/material fields. Fresh C alignment padding
  after vertex cache slots and command-address fields is unspecified; it is
  not treated as authored data. Retained room slices are copied byte-for-byte.
- A noninitial room survives deletion of its old prefix with the asset-pack
  file deliberately unavailable; rebased geometry matches a cold build after
  I/O is restored. This proves eviction-only reuse performs no asset reads.
- Missing new assets, vertex/batch/room limits, abort/retry, missing transaction
  metadata, invalid retained ranges and overflowing statistics preserve the
  published scene. Existing alias-safe overlay/growth tests still pass.
- Private old/new comparison uses the actual `1de6c80c` implementation. All 21
  stage samples match geometry/materials, scene statistics, generation, room
  ages/residency and queue state. Eighteen connected samples exercise 12
  install/evict cycles (1,944 room installs total); Frigate, Cradle and Cuba's
  sampled connected sets contain one room and exercise initialization only.
  ASan/UBSan passes for both implementations. These synthetic residency
  transactions are not player movement replays or level completions.
- Complete host suite and ARM/3DSX pass. The ARM ELF retains original MoveBond,
  input, gun and active-prop dispatch alongside prepare/commit. Evidence under
  `build/host-tests/`: `dynamic-room-reuse{,-o0}-20260903.log`,
  `room-reuse-sanitizer-20260903.log`, `room-reuse-verified-20260903.log`, and
  `arm-room-reuse-20260903.log`.

The repeated full-suite run also exposed a preexisting wildcard-link problem
in `scripts/test_port.sh`: stale `player-gait/chain_*.o` files from a previous
run entered the standalone gait link before its movement dependencies existed.
The runner now links the explicit standalone object list. Verification uses
the already-populated directory, not a cleanup that would mask the problem.
The failed repeat remains recorded in `room-reuse-final-20260903.log`; this was
a test-link failure, not a regression in original movement code.

Each multi-room sample performed 661 room decodes with the old implementation
versus 109 with the new implementation, reusing 552 retained slices. Host-only
`-O2` timing totals for 108 installs (excluding initial load and eviction-only
ticks; **not gameplay frame times**):

| Stage sample | Previous | Reused room slices |
| --- | ---: | ---: |
| Dam | 58.456 ms | 10.711 ms |
| Facility | 40.524 ms | 7.175 ms |
| Control | 49.927 ms | 9.044 ms |
| Egyptian | 79.265 ms | 13.516 ms |

Private driver: `room_reuse_bench.c`; final timings:
`room-reuse-bench-final-20260903.log`. Host load varied between runs; work-count
reductions and exact outputs are stronger evidence than these timing totals.

Candidate saved separately at
`build/3ds-candidates/room-reuse-6f190388/goldeneye-3ds.3dsx`, SHA-256
`6f190388cc85d4e95717f70492b868e7cf081134cba13fee2cc8415d9d0c8e2b`.
Assets remain `938536d4...`; hardware staging and Azahar remain at verified
`0797edaa...`. macOS was still locked, so no live FPS/visual claim is made and
no saves, stage/input configs, DSP settings or emulator instances changed.

Next: measure this candidate in unlocked Azahar on the existing Facility
movement and Dam aim/combat traces, including room transitions and the new
work counters. Another concrete streaming cost remains in
`update_original_dam_camera`: its candidate texture setup still calls the
full scene texture loader. Evaluate using the existing transactional texture
reconciliation API there, with correct borrowed ownership, hidden character
dependencies and geometry/texture rollback; do not replace the call without
closing that transaction boundary. Reliable 60 FPS across levels and
end-to-end high-fidelity gameplay remain unverified.

## Follow-up: transactional room texture reuse and live preload draining

Continued from `0949d643`. The queued room camera path now uses the existing
texture reconciliation machinery: retained GPU images are borrowed during
preparation, newly required images are imported, and obsolete images are
released only at commit. Loaded guard body/head tables supply hidden texture
dependencies, including inactive draw switches. Bootstrap camera calls pass
no object provider; gameplay, POSEND and live tours pass the stage provider.

The texture API adds a commit gate. It validates borrowed ownership and set
capacities before calling the related fallible geometry publication. A failed
geometry/queue commit leaves both texture sets unchanged; a successful callback
is followed only by ownership transfers and obsolete-texture deletion. The
callback cannot mutate texture sets, and this is a single-threaded ownership
transaction, not GPU synchronization. Existing ordinary-object texture callers
retain their original ungated API. Candidate preview allocation is now zeroed:
the old allocation/import failure path could free an uninitialized render-batch
pointer before the preview copy had been assigned.

Inspection also found that normal gameplay passed `project_scene=false` and
returned before processing the visibility preload queue. Only bootstrap and
diagnostic cameras could install additional rooms. Matrix-only gameplay now
enters the same room transaction when a request is pending, and pending work
keeps the camera update eligible even with an unchanged player generation.
No-request camera updates still avoid geometry upload. This is platform scene
publication, not a replacement for original movement, collision or visibility.
The original room aging/eviction selection remains unchanged. This fix needs a
live traversal beyond the initial ten-room set; short opening-area traces are
not sufficient evidence for it.

Input and tour reports add `room_texture_work=retained,imported,released` for
successfully committed **room** transactions. These counters exclude initial
loads, ordinary-object rebuilds and aborted candidates; they are not total GPU
activity or gameplay ticks.

Verification:

- Texture sanitizer tests now exercise the real room/queue commit behind the
  gate: stale borrowed image, invalid geometry metadata, mismatched queue head,
  failure after queue pop with rollback, invalid texture capacity, and successful
  mixed retained/new/missing publication. They verify queue/residency/generation
  preservation, ownership transfer and exactly-once destruction. Existing hidden
  dependencies, empty sets, capacity, index and slot-relocation coverage passes.
- A 96-step synthetic sliding residency test performs 159 imports and retains
  5,985 images, versus 6,144 imports for full reload. Handles, dimensions and UV
  metadata match. Tests pass at default sanitizer settings and `-O2` with
  sanitizers and `-fshort-enums`; an optimized native run also passes.
- A source-contract check guards gameplay queue draining, explicit live guard
  providers, prepare/projection/gated-commit ordering, rollback and ownership
  handoff. It does not substitute for live execution of main.c in Azahar.
- Private actual-pack comparison exercises 21 authored stage samples. Eighteen
  connected samples perform 41 texture-set publications each (initial set plus
  four install/evict cycles within up to ten rooms). Frigate, Cradle and Cuba's
  connected samples have one room and perform five unchanged publications.
  Cold/reused sets match image IDs, dimensions and FNV-1a hashes/lengths of every
  source blob passed to the importer, with no missing images. Real asset-pack,
  catalog, cache, geometry and queue implementations run under ASan/UBSan;
  Citro3D/Tex3DS calls are instrumented host doubles, not GPU execution. Every
  mock allocation is deleted exactly once. No guards/player input are simulated
  by this comparison; the separate hidden-dependency tests cover that contract.

| Authored room sample | Full-load import calls | Reuse import calls |
| --- | ---: | ---: |
| Dam | 811 | 91 |
| Facility | 750 | 86 |
| Control | 1,218 | 93 |
| Aztec | 1,964 | 119 |

This is a reduction in repeated import work, **not measured FPS or GPU time**.
Private source/runner: `build/host-tests/room_texture_pack_probe.c` and
`run_room_texture_pack_probe.sh`. Evidence: `room-texture-pack-final-20260903.log`,
`room-texture-reconcile-20260903.log`, `room-texture-short-enums-20260903.log`.
The earlier pack-probe logs include intermediate link/setup attempts; the final
log is the completed 21-stage comparison.

The complete host suite passes (`room-texture-reuse-full-20260903.log`), and now
includes texture reconciliation and runtime wiring checks. ARM/3DSX passes
(`arm-room-texture-reuse{,-final}-20260903.log`); the ELF retains MoveBond,
bondviewProcessInput, the live original gun and active-prop dispatch, room
prepare/commit and the new gated texture commit.

Candidate:
`build/3ds-candidates/room-texture-d49118d4/goldeneye-3ds.3dsx`, SHA-256
`d49118d4cfce3553d8929c3e203a911c3d96f121b76857871324c0d6ac89c2b4`.
Assets remain `938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
Hardware staging and Azahar still match verified `0797edaa...`; macOS remained
locked. No save/config/audio/emulator-instance changes or public pushes were
made. Reliable 60 FPS and end-to-end mission fidelity remain unverified.

Next: compare this accumulated candidate against `0797edaa...` on the existing
Facility movement and Dam aim/combat traces once unlocked, plus a traversal
beyond initial residency. Inspect frame-time tails and the room work counters,
guard textures, portal transitions and scene-install failures. A further known
publication boundary to tighten is ordinary-object installation: it currently
sets the overlay before reconciling its textures, and a camera room install
followed by the object refresh can upload the combined geometry twice. Make
that boundary failure-safe before attempting deferred/coalesced upload; do not
just suppress one upload when a following rebuild can fail.

## Follow-up: atomic object overlays and unchanged-room upload reuse

Continued from `3d5725be`. This cycle tightens the object-publication boundary
and removes redundant CPU-side world-vertex preparation during overlay rebuilds.
It does **not** defer the first room upload, skip simulation ticks, change room
selection, or replace any canonical gameplay system.

The texture preparation API accepts ordered batch ranges, with one deduplicated
capacity preflight before importing. The ordinary-object installer supplies the
retained room prefix and its candidate overlay separately; it no longer changes
the live overlay just to discover the candidate texture set. Hidden character
dependencies are included before the existing commit gate invokes the fallible
overlay replacement. Both empty and nonempty object installs share this path.
After success, texture ownership and preview pointers transfer without another
fallible operation. Guard scene metadata is staged locally until commit. Failed
installs invalidate only the model cache's abandoned output publication (not its
decoded topology or original actor state), preventing freed-buffer address reuse
from being mistaken for an unchanged output buffer.

The optimization captures proof that the room portion of the vertex buffer is
current **before** an overlay-only rebuild: GPU-world rendering is active, the
uploaded scene generation/count match, and the current texture set has no missing
images. A successful overlay replacement preserves the room prefix, room batch
offsets and retained texture dimensions/orientation. The following publication
can therefore rewrite/remap just the new overlay, retaining room vertices and
bounds. Growing, shrinking and empty overlays advance exact draw counts and
upload generations. Stale generation/count, missing textures, or a non-GPU-world
path use the existing full upload. In particular, a newly recovered missing room
texture can require different UVs, so missing-image recovery never takes the
room-reuse path.

This applies to room-triggered ordinary-object rebuilds and full overlay rebuilds
in the live actor publication path. The first room publication still completes
as before; it is the following redundant room-prefix preparation that is avoided.
These routines write the shared/linear vertex buffer and prepare UVs/bounds.
Cache flushing occurs later in the display frame, so the work reduction must not
be described as a measured reduction in GPU transfers or GPU time.

The input report adds `overlay_gpu_room_reuse=overlay_only_publications,room_vertices_reused`.
These are successful renderer publications, not gameplay tick counts. Install
phase counters now mean query, ordinary build, guard build, overlay staging, and
texture/geometry/metadata commit; the last phase includes the gated geometry work.

Verification:

- ASan/UBSan texture tests add ordered-range deduplication, all-range capacity
  preflight, empty/invalid ranges, and real overlay replacement behind the gate.
  Texture-capacity and geometry-capacity failures preserve the old geometry,
  generation and GPU ownership. Success preserves the room prefix, rebases the
  replacement, retains hidden textures and releases obsolete handles once.
  The 14-case suite also passes optimized with `-fshort-enums`.
- `scripts/tests/test_renderer_overlay_room_reuse.py` compiles the actual
  main.c upload functions and vertex-color helper against host buffers and the
  real room-bounds builder. Across 240 changing overlay publications, full and
  partial paths match complete vertex-buffer bytes, active bounds, colors, UVs,
  draw counts and generations. It checks growth, shrinkage, empty overlays,
  dirty ranges, stale-upload fallback, invalid arguments, and a recovered room
  texture with a changed UV scale. Texture lookup/UV mapping are deterministic
  test seams; this is not PICA/emulator rendering verification.
- That comparison performs **125,973 full-path vertex writes versus 19,893
  optimized-path writes**, including conservative fallbacks. This is a synthetic
  work count, not elapsed time or FPS. The same script checks actual installer
  wiring and failure invalidation so a detached helper alone cannot satisfy it.
- The prior actual-pack texture comparison was recompiled and rerun against the
  ranged API. All 21 stage samples retain the same image IDs, dimensions and
  importer source-byte hashes/lengths, zero missing images and exactly-once mock
  handle destruction. It does not execute player movement or GPU rendering.

Evidence under `build/host-tests/`: `overlay-publication-textures-final-20260903.log`,
`overlay-texture-short-enums-20260903.log`, `overlay-room-upload-final-20260903.log`,
`overlay-ranges-pack-20260903.log`, and `arm-overlay-room-reuse-final-20260903.log`.
The complete host suite includes both new publication checks; its first run is
`overlay-room-reuse-full-20260903.log`, and the populated-directory repeat is
`overlay-room-reuse-verified-20260903.log`. The ARM ELF retains original MoveBond,
input, gun and active-prop dispatch along with room and texture transaction APIs.
Both complete host runs passed; the final focused upload test also passes with
the recovered-room-texture UV-scale case.

Candidate: `build/3ds-candidates/overlay-room-reuse-e3680b9d/goldeneye-3ds.3dsx`,
SHA-256 `e3680b9d037021b5d9fdd4333487d1a2382785209f38a294911202b217caf493`.
The matching asset pack remains `938536d4...`. Verified hardware staging and
Azahar remain `0797edaa...`. macOS stayed locked, and a nonblocking unlock request
was sent; no lock bypass, emulator/save/config change, or public push occurred.

Next is target validation, not another speculative micro-optimization: compare
the saved accumulated candidate to `0797edaa...` in Facility movement, Dam combat
and travel beyond the initially resident rooms. Check frame-time tails, room and
overlay work counters, textures after topology/room changes and install-failure
diagnostics. Reliable 60 FPS across levels and faithful mission completion are
still unverified. The first room upload is deliberately retained; coalescing it
with later actor work would need a wider safe publication boundary, not merely
removal of a call.

## Follow-up: conservative whole-room frustum proofs

Continued from `1401b0c7`. The recorded Dam profile above still identifies
world-frustum work as a concrete optimization target; it is not a measurement
of the new candidate. Static batches now share a conservative whole-room proof
before falling back to the existing prepared per-batch test. This removes
repeated transforms, without reducing simulation frequency or changing the
original portal visibility list, renderer order, geometry, or material state.

Each resident room range caches an AABB built from its actual decoded immutable
vertices. It does not use the authored portal/AI volume as a geometry bound.
Newly decoded rooms build this metadata once; reused rooms copy it through the
existing prepare/abort/commit/eviction lifecycle. Nonfinite/empty bounds remain
invalid. A room containing a batch with a different room ID or coordinate
space cannot supply a proof.

Each GPU-world draw uses the existing authored camera snapshot to classify
original-visible resident rooms. A proof covers only the static room prefix;
guards, doors and other overlays, including authored-space overlays with the
same room ID, retain their existing batch tests. Inconclusive, invalid or stale
scene metadata falls back as well. The camera's own room skips the usually
inconclusive whole-room test. If no room yields a proof, the draw loop passes
NULL to avoid a redundant per-batch room lookup. This last condition avoids
the first prototype's unnecessary single-room overhead.

Classification preserves the existing scalar floating-point grouping, finite
checks and tangent-plane rules. It does not extract/reassociate frustum planes
or approximate rejection. Proofs are scratch data rebuilt after the camera
snapshot, never cached across frames. The only new draw-stack storage is a
256-byte classification array; cached bounds live in resident-room metadata.

The input report adds
`draw_profile_room_frustum=room_tests,proven_visible_batches,proven_culled_batches`.
The latter two count avoided per-batch clip calculations, not new visibility
decisions or simulation ticks. Existing per-batch reason counters only cover
the fallback path, while total tested/culled counts still include both paths.

Verification:

- The existing 20,000 randomized/boundary tests compare the prepared bounds
  API to the old classifier. Another 10,000 groups exercise 80,000 contained
  batches: all 78,088 conclusive group proofs match the exact per-batch result.
  Strict ASan/UBSan and optimized `-O3 -fshort-enums` runs pass.
- `scripts/tests/test_renderer_room_frustum.py` compiles the actual renderer
  helpers with the portable visibility implementation under ASan/UBSan. It
  checks original room membership, current-room/no-proof fallbacks, three
  overlay coordinate spaces, cache hits, invalid bounds/ranges, stale scene
  publication, and changed/nonfinite cameras, plus both live call sites.
- Dynamic-scene cold/incremental comparisons now include bounds metadata and
  independently rebuild each room's bound from the committed vertex slice.
  The existing abort, eviction, no-I/O reuse and overlay tests still pass.
- A private actual-pack driver compiles the extracted renderer helpers and
  compares every sampled result against the old per-batch calculation across
  all 21 stage records. Each uses 64 diagnostic headings at the authored spawn
  STAN polygon centroid, with masks of the first 1, 4 and 10 connected rooms
  where available. These masks are diagnostic subsets, not recorded portal
  traces; the camera is not a live MoveBond camera or a level playthrough.
  All sampled decisions and independently rebuilt room bounds match.
- In the ten-room samples, Dam proves 25,882/55,936 batch decisions and Facility
  17,770/35,008. Host timings for 512 passes of the actual helper were
  8.566 → 4.795 ms (Dam) and 6.133 → 3.208 ms (Facility). Single-room cases use
  no proofs; Egyptian measured 3.974 → 4.137 ms in this run. These short host
  microbenchmarks include proof preparation but not GPU rendering, gameplay,
  or frame pacing; they are not emulator/3DS FPS claims.

Evidence under `build/host-tests/`: `room-frustum-full-bounds-20260903.log`,
`room-frustum-short-enums-20260903.log`, and
`room-frustum-adapter-pack-final-20260903.log`. The private driver and runner
are `room_bounds_probe.c` and `run_room_bounds_probe.sh`; they read the local
pack and do not redistribute assets. Complete host runs are
`room-frustum-full-20260903.log` and `room-frustum-verified-20260903.log`.
ARM/3DSX verification is `arm-room-frustum-verified-20260903.log`; the linked
ELF retains original MoveBond, input, gun/active-prop dispatch, room and texture
commits, and the new prepared bounds API.

Candidate: `build/3ds-candidates/room-frustum-2d043492/goldeneye-3ds.3dsx`,
SHA-256 `2d043492b8d31085c4017a45b991c31c915e6938ae4135fb0c2e3f6ea8c1a1cd`.
Assets remain `938536d4...`; hardware staging and Azahar remain the verified
`0797edaa...` pair. macOS is still locked. No save/config/emulator changes,
lock bypass or public push occurred.

Next: unlock-dependent comparison of the accumulated candidate against the
verified build on Facility movement, Dam aim/combat and room-boundary traversal.
Include the new room-frustum counters alongside renderer phase timing and
room/overlay reuse counters. Check both views with many visible rooms and
single-room views, dynamic guard/door occlusion and texture continuity. Keep
the verified executable available until frame-time tails and images pass;
sustained 60 FPS and high-fidelity mission completion remain unverified.

## Follow-up: prepare texture coordinates once per publication batch

Continued from `b2afcb7a`. UV remapping in world/overlay and first-person
publication repeated material validation, integer-to-float scale/dimension
conversions, four Tex3DS atlas-corner queries and corner differences for every
vertex. These values are constant for each batch. The new prepared contexts
snapshot them once, with no retained material, slot or texture-handle pointer.
Contexts are stack-local and recreated for every remapped batch, so room
transactions, recovered textures, monitor/weapon switches and material changes
cannot reuse a stale atlas transform.

Per-vertex normalization preserves the original scalar grouping and division;
it does not combine scales, replace non-power-of-two divisions with
reciprocals, change the tile/detail interpretation, or approximate UVs.
Interpolation still uses all four corners supplied by Tex3DS, including rotated
and padded atlas layouts. Invalid/missing textures skip mapping just as before,
without changing destination UVs. Existing single-coordinate APIs remain
available for other callers. The outdated public normalization comment about
applying detail-tile shifts was corrected to match the already-existing
unshifted base-image behavior; the behavior itself was not changed.

The live world-range uploader and first-person UV-invalidated path now use
prepared contexts. Pose-only uploads still avoid UV remapping entirely. Color,
position, topology, dirty ranges, texture residency and canonical game state
are unchanged. Input reports add
`texture_uv_work=world_batch_preparations,world_vertices,first_person_batch_preparations,first_person_vertices`.
These count only remapped batches/vertices, not frames or GPU transfers.

Verification:

- `scripts/test_3ds_texture_uv.sh` checks 4,194,304 exact mappings: exhaustive
  signed-16-bit ST domains over 64 scale/dimension/atlas combinations, zero
  and maximum scales, all valid shifts, one-ULP atlas edges, padded/rotated
  corner fixtures, non-power-of-two dimensions, and `UINT32_MAX` dimensions.
  Normalization matches an independent copy of the original scalar formula;
  full atlas results match the existing single-coordinate implementation
  byte-for-byte. Invalid arguments, failure invalidation and snapshot lifetime
  after the source slot/material change are covered. ASan/UBSan passes.
- The optimized `-O3 -fshort-enums -flto` run also passes. LTO permits the mock
  corner accessors to inline like the SDK, avoiding an artificial four-call
  penalty in the baseline. Across 50,000 batches, host timings including
  preparation were 0.313 → 0.195 ms for 3 vertices, 1.180 → 0.601 ms for 12,
  4.890 → 2.390 ms for 48, and 18.877 → 9.615 ms for 192. These are short host
  mapping microbenchmarks, not game frame times or target FPS.
- The actual-pack residency driver compares UVs for every batch/vertex in its
  existing incremental/eviction sequences across all 21 stage records. It uses
  real decoded ST/material data and catalog dimensions, with a mock unit atlas
  and mock GPU imports. Every sampled UV matches. Dam's 41 sets replace
  241,191 per-vertex preparations with 22,787 per-batch preparations; Facility
  replaces 160,599 with 15,034. Image IDs, dimensions, payload hashes and exact
  ownership tests still pass with no missing images. This is not traversal or
  GPU texture-rendering verification.
- The actual upload-helper test still matches entire world/overlay buffers,
  bounds, colors, counts and fallback publication across 240 transitions.
  Source checks require preparation outside the vertex loop, respect the
  no-UV-remap path, and retain first-person topology invalidation ordering.

Evidence: `build/host-tests/prepared-uv-verified-20260903.log`,
`prepared-uv-stage-pack-20260903.log`, and the complete host-suite pass
`prepared-uv-full-20260903.log`. ARM/3DSX passes in
`arm-prepared-uv-20260903.log`; the ELF retains the new prepared APIs and
original MoveBond, input, gun and active-prop dispatch. Disassembly of the
prepared atlas mapper is `arm-prepared-uv-disassembly-20260903.log`.

Candidate: `build/3ds-candidates/prepared-uv-3ae75757/goldeneye-3ds.3dsx`,
SHA-256 `3ae757578c19f7d167a0e1d360b33ec20d5b70aac45c0aa2e12081c7f677d40e`.
Asset pack remains `938536d4...`; verified hardware staging and Azahar remain
`0797edaa...`. macOS is still locked. No public push, save/config change or
lock bypass occurred. Next remains an unlocked accumulated-candidate A/B on
Facility movement, Dam combat and streaming beyond initial residency, checking
texture continuity, room proofs and UV counters alongside frame-time tails.
This cycle does not establish reliable 60 FPS or complete level playability.

## Follow-up: reject affine-W speculation; prepare opening-screen transforms

Continued from `75f49a25`. A proposed model-publication shortcut proved W=1
for exact affine matrices, with a full projective fallback and per-input
matrix-run invalidation. It passed 600,004 byte-exact comparisons in both model
and first-person adapters, including signed zero and one-ULP proof rejection.
It was nevertheless **removed**: normal host vectorization favored the old
four-component transform (1.274 → 1.956 ms for 48-vertex matrix runs), while a
scalar-only compilation gave only a modest, workload-dependent gain
(2.648 → 2.292 ms, but a regression at one-vertex runs). There is no target
measurement establishing a win. Both production model-scene files are restored
exactly; do not enable/retry this experiment merely because its scalar math is
correct. Private evidence: `affine-transform-prototype-20260903.log`, saved
standalone `affine-transform-candidate.c`, and `affine-transform-candidate.patch`
under `build/host-tests/`. The archived extraction script there describes the
rejected source and is not part of the test suite.

The retained change instead removes repeated constant work in animated opening
screens. `ge_original_frontend_lighting_prepare` snapshots rotation sine/cosine,
normalized light direction and authored ambient/diffuse bytes once per
publication. The prepared vertex function retains the original per-normal
rotation/normalization, diffuse arithmetic, color rounding/clamping and
generated reflection coordinates. Nintendo and GoldenEye logo publication
prepares from the same canonical presentation fields before its vertex loops.
The Rareware front/body share one frame-local lighting context; front, body and
letters share one prepared rotation/camera projection. No context survives into
the next frame. Single-vertex APIs remain available and match the old output.

This changes platform fixed-function realization only. Canonical menu timing,
logo poses, geometry, texture atlas arithmetic, primitive colors, letter/body
ordering and gameplay are unchanged. It targets opening-screen CPU cost, not
the remaining gameplay/combat frame-time gap.

Verification:

- `port/tests/reference_frontend_visuals.c` preserves the independent scalar
  implementation from `75f49a25`. 40,000 cases compare normals, lit RGBA,
  generated UVs and projected positions byte-for-byte against both prepared
  and single-vertex APIs. Coverage includes signed packed normals, zero
  normals/lights, varied angles/cameras, clipping-depth clamps, snapshot
  lifetime, invalid input and failed preparation without stale publication.
- The ROM-backed Rareware passes add 51,456 vertex comparisons over 64
  rotations: 3,456 front, 1,536 letters and 46,464 body. The reference and
  candidate match. This is CPU data equivalence, not a PICA screenshot test.
- Renderer source checks require one preparation before the logo vertex loops,
  no static across-frame contexts, and unchanged Nintendo/title input choice.
- The complete host suite passes in `prepared-frontend-full-20260903.log`.
  Final focused ASan/UBSan and source-wiring checks are in
  `prepared-frontend-sanitizer-verified-20260903.log`.
- Host `-O3` benchmark, 2,000 frames × 780 lit/projected vertices, including
  context preparation: **49.015 ms scalar → 13.138 ms prepared**. The scalar
  oracle is compiled in a separate translation unit like the production
  adapter. An earlier same-unit experiment allowed the compiler to inline and
  hoist baseline work that the production build cannot hoist; it measured
  8.635 → 9.042 ms and is not a like-for-like production-call comparison.
  Both logs are preserved; the comparable run is
  `prepared-frontend-bench-final-20260903.log`. Neither proves target FPS.
- ARM/3DSX passes (`arm-prepared-frontend-20260903.log`). The ELF retains the
  prepared frontend APIs plus original MoveBond, input, gun and active-prop
  dispatch. The rejected affine shortcut is not in this executable.

Candidate: `build/3ds-candidates/frontend-prepared-04d7b78d/goldeneye-3ds.3dsx`,
SHA-256 `04d7b78d9c77973e28e5c4f607efa38734589f64ee9318f30f442cd078aaca23`.
Assets remain `938536d4...`; hardware staging and Azahar remain verified
`0797edaa...`. macOS remained locked; no emulator, save/config or public-repo
mutation occurred. The next validation remains the accumulated-candidate A/B
against that verified build, including the full opening sequence, Facility
movement, Dam combat, and streaming beyond initial rooms. Sustained 60 FPS,
hardware timing and high-fidelity mission completion are still unverified.

## Unlocked emulator verification: accumulated `04d7b78d` candidate

macOS was unlocked on the next check. Reused the single Azahar 2126.0 process,
with Vulkan, no save resets or manual save edits, and no DSP configuration
changes; no concurrent build.
Used a bounded one-hour `caffeinate -di` session. The baseline executable was
saved separately under `build/3ds-candidates/verified-0797edaa/` before switching
the virtual-SD executable. Asset SHA-256 remains `938536d4...` throughout.

Fresh, same-config Dam aim/fire runs (750 simulation ticks each):

| Metric | Baseline `0797edaa` | Candidate `04d7b78d` |
| --- | ---: | ---: |
| Presented frames | 836 | 750 |
| Total measured frame work | 12,576 ms | 10,888 ms |
| Measured work / simulation tick | 16.768 ms | 14.517 ms |
| Measured work / presentation | 15.043 ms | 14.517 ms |
| Post-warmup samples over 16 ms | 99 / 716 | 21 / 630 |
| Post-warmup peak | 28 ms | 27 ms |
| Movement / actor / gun ticks | 750 / 750 / 750 | 750 / 750 / 750 |
| PP7 shots / decoded sound starts | 5 / 21 | 5 / 21 |
| NDSP queued blocks | 549 | 548 |

The 13.4% reduction is accumulated measured work per equal simulation interval,
not a 13.4% FPS claim. Candidate skipped 1,615 idle polls and retained the same
player endpoint and shot count. End camera recoil/guard states are not byte
identical across runs; this is not a deterministic full-state replay proof.
Both runs report zero guard matrix, gun-sight and overlay publication failures.
Evidence: `build/visual-probe/check-now-{baseline-0797,candidate-04d7}-aim.result`.

Candidate Facility stick/look/fire trace: 750 simulation/presented frames,
seven PP7 shots, exact previous stage-capable endpoint
`-199.956543,292.746887,-193.684525`, 7.576 ms measured work per simulation tick,
14 ms post-warmup maximum, zero samples over 16 ms, zero matrix/overlay errors.
Evidence: `check-now-candidate-04d7-facility.result`. A fresh Facility timing
comparison against `0797edaa` is **not available**: that old build gates input
probes to Dam (`706aa4d6` source), and ran interactively rather than executing
the trace. An accidentally copied stale candidate result was renamed
`check-now-invalid-facility-stale-result.txt` and excluded. Screenshots of both
builds show substantial blue gaps around the Facility vent geometry. These
remain a fidelity issue, not proof that this stage renders correctly.

Same authored 160-target combat/travel config, no invincibility or gameplay
changes: baseline reached 11 targets before death at 4,300 simulation ticks
(8,752 presentations); candidate reached 13 before death at 5,851 simulation
ticks (5,850 presentations; one pass serviced multiple ready ticks). Neither
route completed; both reports correctly say `status=failed` and mission failed.
Baseline fired 25 PP7 shots, candidate 50; both registered three damaging guard
hits. Candidate handled 2,187 guard-fire dispatches, 738 decoded sounds,
4,258 NDSP blocks and 45 guard topology replacements with zero reported matrix,
overlay, AI-opcode or sound decode failures. Screen captures show textured
guards, reloading/ammo changes, truck movement and colored damage gauges.
This verifies those observed paths, not complete visual/audio fidelity.

Combat post-warmup peak was 38 ms baseline / 31 ms candidate, with 27 / 10
samples over 25 ms respectively. Candidate still had 733 / 5,730 presented
samples over 16 ms. Different survival lengths, encounters and idle-presentation
denominators prevent a controlled combat FPS ratio. Candidate guard-refresh
peak was 13.85 ms (including 6.94 ms replacement work, 5.50 ms cache build,
and zero texture import); the worst renderer submission region was 8.77 ms, including
5.58 ms sky/world work and 2.29 ms CPU prep. These remain useful optimization
targets. Movement only reached rooms 135, 133, 132 and 124, within the initial
resident set: room/texture reuse counters were not exercised by this route.
Evidence: `check-now-{baseline-0797,candidate-04d7}-combat.result` and matching
private screenshots/config under `build/visual-probe/`.

The existing 177-view Dam authored-pad tour then completed all camera/visibility
checks with 62 successful streamed room installations, zero stream failures,
10-room peak residency, 114 peak textures, and 39 peak visible rooms. All 138
ready materializer records constructed with zero failures. This exercises
room eviction/reinstallation beyond the initial live-combat residency; it is a
diagnostic camera tour, not player traversal or mission completion, and the
few inspected captures are not an exhaustive visual comparison. Evidence:
`check-now-candidate-04d7-tour.result`, matching `.diag` and screenshots.

Normal boot was restored by moving the temporary stage/input/tour configs to
private evidence filenames. Captures show classification, rotating Nintendo
and Rareware models, and the gunbarrel animation. Some logo angles are very
dark; an additional boot of the immediately preceding `3ae75757` executable
shows the same dark/banded appearance before the prepared-lighting change.
The wall-clock captures do not match exact poses, so this is not a pixel-exact
regression test or an N64-reference fidelity certification. Retain an opening
reflection/lighting audit as a remaining fidelity task.

The accumulated candidate is now installed in Azahar and hardware staging:
code `04d7b78d9c77973e28e5c4f607efa38734589f64ee9318f30f442cd078aaca23`,
assets `938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
Both copies were hash-checked. The verified baseline remains recoverable in
`build/3ds-candidates/verified-0797edaa/`. Hardware staging's existing `cradle`
stage selection was not changed; Azahar has no active stage/input/tour override
and boots the normal opening. No public push or new production-code change
was made in this verification pass; the previously tested ARM binary is exact.

Next priorities: the measured guard-refresh and sky/world submission peaks;
Facility's vent visibility gaps; an N64-referenced opening lighting/reflection
comparison; and a real Dam playthrough through both gates and the objectives.
The combat probes still die before leaving initial room residency, and the
camera tour does not substitute for that playthrough. Sustained 60 FPS,
hardware performance, audible fidelity and mission completion remain unproven.

## Combat publication optimization: bulk copy and cold capacity reserve

This pass preserves original simulation, animation, input and rendering data.
Published overlay batches now use one contiguous copy followed by adjustment
of only the vertex-index origin, instead of repeated large struct copies.
At cold stage load, an optional transaction reserves up to one resident
scene's worth of overlay capacity, bounded by the existing scene limits.
Allocation failure leaves the old scene intact and retains normal growth.
Successful relocation changes the generation before runtime pointers are
published. No geometry, game ticks or visibility work is omitted.

The reserve is deliberately **cold-load only**. Later streamed installations
can replace storage with exact-sized buffers; this does not promise that
growth will never occur after room transitions. On Dam it costs 1,421,780
additional target bytes (1.36 MiB); Facility uses 903,896 bytes. The 21-stage
host audit's largest extra allocation is 1,926,320 bytes (Egyptian, host ABI,
not a target-memory measurement).

Fresh Azahar runs used the same 160-target combat config, with no concurrent
builds, invincibility or gameplay changes:

| Metric | Prior `04d7b78d` | Bulk copy `c2fdf516` | Reserve + bulk `952088b7` |
| --- | ---: | ---: | ---: |
| Simulation ticks before death | 4,996 | 5,825 | 6,365 |
| Route targets reached / 160 | 12 | 11 | 14 |
| PP7 shots / damaging hits | 35 / 3 | 37 / 5 | 51 / 5 |
| Worst measured guard refresh | 13.79 ms | 13.45 ms | 10.33 ms |
| Replacement work at that guard peak | 6.83 ms | 6.54 ms | 1.12 ms |
| Allocating overlay publications | 4 | 4 | 0 |
| Room vertices copied during growth | 36,516 | 36,516 | 0 |
| Post-warmup frame-work peak | 38 ms | 31 ms | 28 ms |
| Post-warmup samples over 16 ms | 642 / 4,875 | 514 / 5,704 | 576 / 6,244 |
| Post-warmup samples over 25 ms | 10 | 6 | 6 |

These are observed peaks, not deterministic encounter replays or a controlled
whole-run FPS comparison: survival, firing and targets differ. Every route
ended in death before completion, reporting `status=failed`, not an emulator
crash. All stayed within initial room residency. The reserve run performed
51 guard topology replacements with zero buffer-growth events; all 6,365
original movement, actor and gun ticks ran. It decoded 739 sounds with zero
decode failures and queued 4,627 NDSP blocks, with no reported guard matrix or
overlay errors. Evidence: private
`build/visual-probe/bulk-publish-baseline-04d7-combat.result`,
`bulk-publish-candidate-c2fd-combat.result`, and
`startup-reserve-9520-combat.result`.

At the latest worst guard-refresh frame, cache work is 8.29 ms, including
5.66 ms topology work and 1.79 ms vertex transformation. These nested fields
are measurements at the worst total guard frame, not independent component
maxima. Renderer submission still peaks at 8.74 ms. Model topology/template
work and sky/world submission remain the next measured targets. The 28 ms
post-warmup frame demonstrates that sustained 60 FPS is **not** achieved.

The latest Facility movement/look/fire probe completed all 750 simulation,
movement, actor, gun and presented frames, seven PP7 shots and the exact prior
endpoint `-199.956543,292.746887,-193.684525`. Post-warmup maximum is 14 ms,
with zero of 630 samples over 16 ms. This is a narrow vents test, not a
Facility playthrough or resolution of its known geometry gaps. Evidence:
`build/visual-probe/startup-reserve-9520-facility.result`.

Validation passed:

- Focused ASan/UBSan overlay tests: 416 existing capacity/alias/failure/fallback
  cases, 32 independent reserve-capacity/allocation-failure cases, empty-scene
  publication, no-op/invalid requests, and 1,024 distinct byte-checked batch
  payloads across growth/shrink/empty/refill transitions.
- Complete host suite and ARM/3DSX build for the production candidate.
- Additional ASan/UBSan authored cold-reserve audit across all 21 stages,
  verifying unchanged vertex/batch bytes and metadata, followed by the existing
  Silo eviction and Streets retained-overlay streaming checks. This test-only
  extension was added after the full suite; production code did not change.

Logs: `build/host-tests/startup-reserve-sanitizer.log`,
`startup-reserve-full.log`, `arm-startup-reserve.log`, and
`startup-reserve-all-stages.log`. ARM ELF retains `MoveBond`,
`bondviewProcessInput`, `ge_original_stage_active_props_tick_exact`,
`ge_original_gun_live_tick`, and the new reserve function. The exact latest
executable is `952088b77885010490e5ddb7658ccdb0dfb7dc8977aa185901aaeb1ef0182def`;
asset pack remains
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
The executable is installed in Azahar and hardware staging; private fallback
copies of the preceding candidates remain available. No public push was made.

The latest candidate also completed the existing 177-view authored-pad Dam
tour: 62 streamed installations, 34 evictions, zero camera/visibility/stream
or guard/door/monitor/articulated publication failures; peak residency 10
rooms, peak textures 114, peak visible rooms 39. All 138 ready materializer
records constructed. Evidence: `startup-reserve-9520-tour.result` and `.diag`
in `build/visual-probe/`. The tour deliberately jumps cameras and synchronously
waits for requested room sets: its 564 ms average / 1,008 ms peak displayed
intervals are not combat-frame measurements and do not establish smooth
player-driven streaming. It proves reserve-to-streamed-transaction compatibility,
not complete visual fidelity or mission completion. A sustained live traversal
beyond initial residency remains necessary to measure actual streaming dips.

Azahar's temporary Facility and input/tour overrides were removed after the
checks and normal boot restored, leaving the hash-checked latest code/data
pair installed. Hardware staging's pre-existing `cradle` selection is unchanged.

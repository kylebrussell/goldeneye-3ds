# Stage bootstrap correction and prepared original visibility

Continues [the model-publication performance pass](Verification20260904Performance.md).
The preceding work through `8dbbdf60` was pushed to `origin/master` before
this pass. No ROM, asset pack, save or DSP data is included in source commits.

## Stage coordinates were being normalized twice

The packaged setup parser exposes runtime-space pads and bound-pad boxes.
The live bootstrap then passed those normalized arrays to unchanged original
`proplvreset2PadSlice`, which applies the level reciprocal again. Its early
spawn/camera path also expects authored coordinates. Dam used a different,
static bootstrap: its first load was correct, but reusing its mutated pad
arrays on restart risked cumulative rescaling.

The live bootstrap now restores exact authored pad-position and bound-box
float bytes from the immutable setup source before the original pad loader
runs. It preserves relocated pointers, STAN bindings, orientation and
sentinels. It does not multiply rounded runtime coordinates to approximate
the originals. All stages, including Dam, use the fresh stage-owned setup
allocation. The standalone parser's existing normalized-data contract is
unchanged.

The regression test loads all 21 packaged stages and executes three cycles
of source restoration followed by the actual original pad loader. Each cycle
must reproduce the complete normalized pad and bound-pad arrays byte for
byte. A source-order check pins the live bootstrap call before spawn lookup
and prevents reinstating the static Dam branch.

This fixes the measured Runway door-query failure: command 121 / model 155
had a matrix translation of approximately `(85691, -3926, -55528)`, outside
the original s15.16 matrix range. No matrix-clamping or rebasing workaround
is retained. The corrected start is `(7301.435547, -282.735229, -5414.672852)`
instead of `(81515.226562, -282.735229, -60450.890625)`. All six Runway doors
also pass native scene-query tests, alongside the existing stage door audits.

Cradle's prior camera started at approximately `(-29895, 1602, -31461)`.
It now starts at `(-7046.666504, 342.058960, -7415.757324)`. Its repeated
751 empty scene installations / 750 door-refresh failures become two
successful installations and no refresh failures. The older report's
"zero rooms" description was imprecise: original visibility forced room 9,
but no world geometry was submitted. Corrected traces draw world geometry;
this alone is not a fidelity or mission-completion certification.

## Move immutable decoding to stage loading

`GeOriginalBgVisibilityProgram` prepares the validated portal polygons,
native portal headers, relocated global-visibility commands and source hash
once per immutable background. The runtime owns this allocation and closes
it before freeing the background. Raw-input execution remains available
for mutable fixtures. A program rejects a different source pointer, size or
room count.

Every frame still restores mutable execution state, applies live portal
controls, computes original portal order and executes unchanged
`bgDetermineVisibleRooms`, global commands and preload callbacks. Camera,
room bounds, fog range and door controls remain live. No visible-room result,
AI, animation or simulation tick is cached or skipped.

Three iterations were measured: relocated commands only, commands plus all
portal storage, then only the authored portal range plus its sentinel. The
last avoids copying 200 polygons on zero-portal stages. The fixed allocation
is approximately 27 KiB for Dam; there is no per-frame allocation. Tests
compare complete raw/prepared results across 96 packaged Dam camera inputs,
32 live portal-control combinations and interleaved stage geometries. A
populated-to-empty-stage test also verifies the original line/portal lookup
sees the restored terminator.

## Controlled emulator measurements

Azahar used the unchanged `ee769251` asset pack and the same 750-frame
move/look/fire input. Builds were not running during the traces. The
correctly scaled, uncached baseline was `b9f6f085`; command-only was
`0af7c9a2`; the full-storage cache was `54168859`.

| Stage | Baseline total frame work | Full-storage cache | Camera/visibility work, baseline → cache |
| --- | ---: | ---: | ---: |
| Dam | 7,496 ms | 7,278 ms | 571 → 355 ms |
| Facility | 5,408 ms | 5,324 ms | 312 → 218 ms |
| Cradle | 2,991 ms | 3,021 ms | 218 → 255 ms |
| Runway | 3,638 ms | 3,645 ms | 132 → 155 ms |

Dam's visibility region improves about 38%, but total instrumented work
only about 3%. The small-stage regressions prompted the active-range copy
iteration; do not advertise the full-storage cache as universally faster.
Dam's final endpoint and non-timing gameplay/draw/sound counters match the
command-only run exactly. Facility's gameplay/draw counters match; its NDSP
service counter varies with scheduling. These timings measure emulated CPU
work, not guaranteed displayed FPS or New 3DS hardware performance.

Evidence is private under `build/visual-probe/`:
`once-only-pad-scale-{dam,facility,cradle,runway}750.result`,
`vis-program-{dam,facility}750.result`, and
`prepared-vis-final-{dam,facility,cradle,runway}750.result`.
Screenshots confirm textured Dam, Facility and Runway geometry and the
first-person weapon. Cradle submits world geometry, but the captured
movement view is not sufficient to certify correct presentation throughout
the stage. The brief traces stay near the start and are not completion runs.

## Verification checkpoint

- Full host suite with source-restoration and command-only cache:
  `build/host-tests/scale-and-vis-full.log`, exit 0.
- Full host suite with full portal preparation:
  `build/host-tests/scale-prepared-vis-full.log`, exit 0.
- Final Dam-bootstrap source-order check passes; ARM build
  `arm-scale-prepared-vis-final.log` exits 0.
- Active-range ASan/UBSan visibility fixture passes, including raw/prepared
  equivalence and populated-to-zero-portal switching.

## Final active-range candidate

Executable SHA-256:
`2298144d9979098f71f14059753edc15c7ef1e5937a62c9ddf8861c697790465`.
Unchanged asset SHA-256:
`ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`.

The complete host suite exits 0 in
`build/host-tests/active-portal-final-full.log`; ARM/3DSX build exits 0 in
`build/host-tests/arm-active-portal-final.log`. The separately compiled
`test-bg-active-program` ASan/UBSan fixture also exits 0. The installed
Azahar executable and pack hashes were verified before the final traces.

| Stage | Total measured frame work, uncached → final | Camera/visibility, uncached → final | Final post-warmup >16 ms / peak |
| --- | ---: | ---: | ---: |
| Dam | 7,496 → 7,274 ms | 571 → 342 ms | 7/630 / 19 ms |
| Facility | 5,408 → 5,322 ms | 312 → 200 ms | 0/630 / 13 ms |
| Runway | 3,638 → 3,605 ms | 132 → 79 ms | 1/630 / 19 ms |
| Cradle | 2,991 → 2,965 ms | 218 → 192 ms | 0/630 / 10 ms |

Each final trace completes 750 movement/actor ticks, with matching
non-timing gameplay and geometry counters versus the full-storage candidate.
NDSP service timing/counters can vary; original sound event/decode counters
match. Evidence: `build/visual-probe/active-vis-{stage}750.result`.
An early final Cradle screenshot confirms the authored walkway, floor,
railings and PP7 at the start. It does not certify every texture, distant
geometry or mission behavior.

Remaining priorities are sustained combat frame tails, broader stage
traversal/fidelity, active music synthesis profiling (these traces have zero
music-render calls), and hardware testing. Extended combat validation and
final deployment are recorded below.

## Extended Dam combat

`build/visual-probe/active-vis-dam-combat.result` records 4,899 presented
frames / 4,900 original movement and actor ticks. Bond dies at route target
11/160, so the route report correctly says `status=failed`; this is not a
completed mission. Actor status remains healthy and no scene-install or
door-refresh error is recorded. There are 425 original sound events/decodes,
zero decode failures and 3,550 NDSP blocks. Music-render calls remain zero.

Post-warmup, 508/4,779 frames exceed 16 ms and three exceed 25 ms; peak is
31 ms. A live firefight screenshot reads 51 FPS / 87% speed. Thus sustained
60 FPS is still unmet. Aggregate measured frame work is 50,163 ms, including
2,359 ms in the camera/visibility region. World submission is about 13.7 s
over the trace. Original actor/guard work and world submission remain the
highest-value targets for the slow tail.

The previous `29b97c6a` combat trace had 613/4,584 post-warmup frames over
16 ms, but different guard encounters, sound counts and death timing. The
new trace is useful stability/performance evidence, not a controlled
whole-combat percentage speedup. No enemies, ticks, detail or sound were
removed to obtain these measurements.

## Additional stages and handoff

Control and Caverns each complete the same 750-frame trace with healthy
actor status and successful scene installation. Control reports four of
630 post-warmup frames over 16 ms, one over 25 ms, peak 26 ms. Caverns
reports one over 16 ms, peak 17 ms. Evidence:
`active-vis-{control,caverns}750.result`.

Caverns was also rerun on the correctly scaled uncached `b9f6f085` build:
`once-only-pad-scale-caverns750.result`. Non-timing gameplay, world/guard
draw counts, geometry publication and sound counters match the candidate
(code-address diagnostics differ, as expected). Total frame work is
5,723 → 5,601 ms; visibility/camera work is 262 → 175 ms. Its 16 → 17 ms
peak illustrates why small aggregate gains do not prove eliminated dips.

The candidate's Caverns screenshot shows guards apparently over the
elevator-door region. This presentation/occlusion issue needs diagnosis;
matching draw counters do not prove visual correctness. The uncached
screenshot attempt returned a blank captured window, so a visual A/B
comparison is **not** claimed. A clean idle Azahar restart was used to
recover capture afterward. Control's test has report evidence only.

The all-21-stage, three-cycle pad-bootstrap test was additionally compiled
with ASan/UBSan and passes: `build/host-tests/once-only-pad-asan.log`.

Final executable `2298144d…` is byte-identical in `platform/3ds`, the
Azahar SDMC installation and `build/3ds-sd/3ds/goldeneye-3ds`. Both installed
asset packs remain `ee769251…`. The old hardware-staging executable is
preserved in `build/3ds-candidates/hardware-before-active-vis`; the final
candidate is preserved in `build/3ds-candidates/active-vis-2298144d`.
Hardware staging retains its existing `cradle` selection. This is staging,
not a physical New 3DS test.

Temporary emulator stage/input overrides were moved out of SDMC to
`build/visual-probe/active-vis-final-used-{stage,input}.cfg`, restoring
normal frontend boot. A final live capture confirms the original gunbarrel
sequence runs from this normal boot; frontend 60 FPS is not certified.
Save files and DSP data were not manually changed.
Next work: reduce original actor-service/model-publication and world-submit
cost in Dam's recorded slow frames, investigate Caverns guard/door depth,
verify active music cost, and extend real mission traversal and hardware
coverage. Neither 60 FPS across all levels nor end-to-end completion of
these six missions is certified by this pass.

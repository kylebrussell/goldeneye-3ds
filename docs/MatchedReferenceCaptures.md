# Reproducible N64 and 3DS capture comparisons

`scripts/compare_fidelity_captures.py` seals existing capture evidence with
SHA-256 hashes, checks the recorded conditions, and produces a JSON report,
cropped PNGs, absolute RGB difference images, and an HTML contact sheet.
It does not launch an emulator, alter an SD card, control another process,
or acquire ROMs. Run captures separately through the integration coordinator.

There are two deliberately distinct results:

- **matched-recorded-state**: an original N64 execution and a 3DS port execution
  were declared, their required evidence is present and unchanged, and their
  recorded initial/current state, camera, input, retraces, ROM and mono settings
  satisfy the checks below. This is eligibility for examining image differences;
  it is not a claim that the images are identical or the port is complete.
- **unmatched-exploratory**: the tool can inspect pixels, but original execution
  or matching conditions are absent. Port-versus-port comparisons, missing
  instrumentation, and nonzero stereo always fall into this category.

Hashes prove file identity, **not the authenticity or completeness of a producer's
execution/state declarations**. Review the execution log, capture adapter and
recipe before accepting a recorded-state result as actual N64 reference evidence.
Copying an N64 label or hash onto a port screenshot does not establish reference
execution. The tool cannot detect that deception. No pixel threshold measures
stereo comfort, physical GPU cost, audio, or unrecorded gameplay state.

## Existing evidence, inspected September 10, 2026

The shared private build contains `build/u/ge007.u.z64` and many port Azahar PPM
captures. The inspected build paths contained no N64 reference image set.
`mupen64plus`, `simple64`, `ares`, and `retroarch` were not on PATH, and no N64
emulator application appeared in `/Applications` or the user's `Applications`
directory. This is a bounded local availability check, not proof that no other
reference setup exists. No emulator was started, stopped or installed.

The existing `frontend-probe.result` and `gameplay.result` provide useful port
diagnostics but do not supply the complete cross-runtime snapshot contract
below. A file called `mono`, a live-clock route, or a visually similar position
cannot fill that gap. Original ROM execution with an instrumented adapter and
matching port instrumentation remain required for actual matched captures.

One existing private 400 x 240 port PPM was copied into this task's local build
directory and compared with itself. Its SHA-256 was
`165b690c586def3a53fc63a372bd5534147799a5d78509ba243294d543ab20ac`.
The tool measured zero changed pixels but correctly produced
`unmatched-exploratory` and exit status 1 with `--require-matched`, because
original execution and state evidence were absent. Slider and simulation tick
were recorded as unknown, rather than guessed from the directory name.
The private report is under
`build/fidelity-comparison-selfcheck-20260910/report/index.html` in the renderer
task worktree; neither this image nor the original ROM is checked into Git.
This run verifies tool behavior on a real port capture, not N64 fidelity.

## Workflow

1. Agree on a case and freeze its ROM revision, difficulty, stage/menu, reset or
   save state, player slots, canonical controller timeline and retrace schedule.
   Capture after the same canonical simulation tick, with zero interpolation.
   Give the original emulator, RDP backend, VI filtering/output mode and version
   explicit identities. A hardware capture also needs capture-device settings
   and the mapping from recorded video frame to simulation tick.
2. Execute the original ROM and port independently. Save instrumented canonical
   initial state and the state actually consumed by each rendered capture. Keep
   the original execution log, settings, replay input, full retrace schedule,
   reset/save inputs, adapter revision and reproduction instructions. Do not
   regenerate metadata later by looking at a screenshot.
3. On 3DS capture both eyes at slider zero first; compare each to N64 mono. Then
   record both eyes at nonzero slider depth as a separate stereo case. The
   nonzero case can expose missing geometry or incorrect layers, but an N64
   mono image is not a pixel-identical stereo-eye target.
4. Copy immutable capture inputs into a private, worktree-local evidence
   directory. Create descriptors using the schema below; seal them using
   `record`. Sealing reads and hashes files without copying or modifying them.
   Freeze them after sealing. A changed image, executable, log, replay or other
   recorded artifact causes verification to fail, including same-size changes.
5. Compare without hidden transforms. An explicit crop can remove the 3DS
   viewport's pillarbox or a capture device's border. Both crop dimensions and
   pixel aspect ratios must agree. Export matching dimensions at capture time;
   the tool refuses to resize, register, blur, gamma-correct or choose a nearby
   frame to improve the score. Settings and export steps belong in the recipe.

```sh
python3 scripts/compare_fidelity_captures.py record \
  build/fidelity/case/n64.descriptor.json build/fidelity/case/n64.manifest.json
python3 scripts/compare_fidelity_captures.py record \
  build/fidelity/case/port.descriptor.json build/fidelity/case/port.manifest.json
python3 scripts/compare_fidelity_captures.py compare \
  build/fidelity/case/n64.manifest.json build/fidelity/case/port.manifest.json \
  --out-dir build/fidelity/case/comparison --require-matched
```

`record` refuses an existing output manifest. `compare` refuses an existing
output directory, so a rerun cannot overwrite prior evidence or source inputs.
Descriptors resolve relative paths against their own directory; sealed
manifests record absolute paths. To move a bundle, update its descriptor and
seal a new manifest, preserving the old one. Keep all ROM-derived inputs and
outputs private. The HTML report uses only local image files.

Exit status 0 means the requested checks passed, 1 means a requested acceptance
check failed, and 2 means invalid/missing/changed input or an I/O error. Without
`--require-matched`, successful exploratory analysis returns 0 and explicitly
reports that it is unmatched. Optional `--max-mae 0` requires both matched state
and exact RGB pixels. Any `--max-mae` implicitly requires matched state, so
even a generous threshold cannot accept a port-only comparison. The threshold,
matched-state requirement and acceptance result are saved in the report.
There is no default visual tolerance purporting to establish N64/PICA parity.

For an existing port capture whose original settings or tick are unknown,
retain `kind: "port-3ds-emulator"`, use `null` for the slider/tick, and record
unknown execution fields as `"unknown"`. Omit unavailable artifacts. Compare
using `--reference-eye left` (or `right`) when the reference is another port
eye. This remains exploratory even if every pixel is identical.

## Descriptor contract

This is a **schema illustration**, not completed execution evidence. Replace
all example values with observations, and paths with the actual private files.
Original N64 uses `eye: "mono"` and its actual image crop. The 3DS descriptor
normally has separate `left` and `right` captures and an `assets` artifact.

```json
{
  "schema": 1,
  "execution": {
    "kind": "port-3ds-emulator",
    "producer": "Azahar",
    "version": "exact tested build identity",
    "video_backend": "exact backend and configuration identity",
    "state_method": "runtime-instrumented"
  },
  "stereo_slider": 0,
  "artifacts": {
    "source_rom": {"path": "source.z64"},
    "binary": {"path": "goldeneye-3ds.3dsx"},
    "assets": {"path": "goldeneye.u.gepack"},
    "input": {"path": "canonical-input.json"},
    "retrace_trace": {"path": "canonical-retraces.json"},
    "initial_state": {"path": "initial-state.json"},
    "state": {"path": "capture-state.json"},
    "execution_log": {"path": "execution.log"},
    "settings": {"path": "settings.json"},
    "recipe": {"path": "reproduce.md"}
  },
  "captures": [
    {"eye": "left", "path": "left.ppm", "crop": [40, 0, 320, 240],
     "pixel_aspect": [1, 1], "tick": 120},
    {"eye": "right", "path": "right.ppm", "crop": [40, 0, 320, 240],
     "pixel_aspect": [1, 1], "tick": 120}
  ]
}
```

Allowed execution kinds are `original-n64-emulator`, `original-n64-hardware`,
`port-3ds-emulator`, `port-3ds-hardware`, `synthetic`, and `unknown`. Only the
first two are eligible as original references. A reference's `binary` must
hash identically to its `source_rom`; the two sides' source ROM, canonical input
and retrace-trace hashes must agree. This contract therefore excludes a patched
reference ROM; use external instrumentation that preserves executed ROM bytes.
The port executable hash must be recorded but is expected to differ from N64.

All listed artifacts are checked. Every artifact listed above is required for
a matched-state result; `assets` is required only on the port side. Additional
artifact names are allowed: record all consumed texture sidecars, save inputs,
capture adapters, conversion manifests and shader/settings files. An `assets`
pack hash alone does not establish identity of separate sidecars, and the tool
cannot discover an unrecorded runtime dependency. The reviewed recipe must
enumerate the complete asset set and its source-ROM conversion provenance.
Files must be nonempty. Extra artifacts are hashed just like required artifacts.

Input images can be opaque 8-bit RGB P6 PPM (no Python dependencies) or opaque
RGB PNG (requires Pillow). The tool deliberately rejects alpha/paletted PNGs
instead of silently choosing a compositing background or color conversion.
N64 VI filtering, gamma/dither, PAL/NTSC pixel aspect, framebuffer sampling and
capture-device conversion can explain differences; retain these details rather
than treating raw encoded RGB error as a physical light measurement.

## Canonical snapshot contract

Both `initial_state` and `state` are JSON documents with schema 1. All values
must match exactly across executions; differing paths are reported. There are
no float tolerances hiding camera drift. Serialize after mapping each native
runtime's coordinate layout into the following shared convention. This is an
adapter requirement, not permission to change the captured camera.

| Field | Required meaning |
| --- | --- |
| `scene` | Object containing `stage_id`, `difficulty`, `screen`; include local player slot, mode, scenario, and menu identifiers as applicable |
| `clock` | Object with nonnegative integer `tick`, positive integer `hz`, `phase: "render-input"`, and `retrace_trace_sha256`; the hash names the full common schedule, and the current state's hash must equal the recorded artifact |
| `camera` | `convention: "n64-authoring-row-major"`, `view` and `projection`, each 16 finite numbers; original authored coordinates, row-major matrices, with the actual off-axis projection and viewport mapping accounted for |
| `rng` | Nonempty ordered array of nonnegative integer states for all PRNG streams, preserving full integer precision |
| `channels` | Objects or arrays under every key listed below; no missing channels |

The required channels are `players`, `actors`, `doors`, `objectives`, `lighting`,
`effects`, `frontend`, and `transitions`. Their serialized contents must cover
the state affecting this image: player ownership/pose/weapon, actor identity and
animation frame, door fractions and visibility flags, mission state, room light
values, effect ages/seeds, watch/page/cursor/results state, and transition/fade
timers. Canonical stable actor IDs must replace addresses. Use an explicit empty
array only when a channel has no state in that case. Any additional channels
or fields are also compared. Matching empty arrays cannot prove that the
producer captured real state; adapter review is part of reference acceptance.

Each image's `tick` must equal the current snapshot's clock tick. A port output
frame counter, delayed framebuffer dump request, or VI presentation counter
cannot be substituted without establishing which simulation state produced
the image. Keep that mapping in the instrumented adapter and execution log.
Hand-entered snapshots can aid exploration, but `state_method: "manual"`
cannot pass the matched-state check.

## Finite outstanding reference cases

| Case | Capture and acceptance focus |
| --- | --- |
| Dam doorway and streaming edges | Closed/open authored doors; mono baseline and both eyes at slider zero/max; newly resident rooms and glass, unchanged canonical AI/room state |
| Depth and decals | Same surface/overlay depths at near, medium and far range; coplanar bullet decals, windows, fences and intersecting effects; quantify errors before choosing depth tolerance |
| Materials and coverage | Window opacity, guard/prop vertex color and lighting, cutouts, fog, smoke, muzzle flashes and explosions at identical age/RNG |
| Frontend | Legal/logos/file/mission/difficulty/briefing and cast at declared animation ticks; original crop/aspect and intended stereo layer |
| Watch and results | Each watch page and cursor; objective updates, pause/resume, abort, death, complete/failure statistics and fades at matching state/tick |
| Campaign transitions | Representative indoor/outdoor loading, exit and restart cases, followed by per-mission cases when canonical route evidence is available |

None of these actual N64 comparisons was completed by the tooling pass.
Acquiring the original execution adapter/captures, instrumenting a complete
cross-runtime state record, resolving measured N64 RDP/PICA differences, and
reviewing physical stereo comfort/GPU cost remain separate work. Existing port
captures and these synthetic tests cannot stand in for those acceptance steps.

## Tests

```sh
python3 scripts/tests/test_fidelity_capture_comparison.py
```

The tests use tiny synthetic images and dummy files, not private assets. They
exercise exact/known RGB errors, threshold boundaries, both zero-slider eyes,
explicit crop/aspect constraints, missing evidence, altered same-size files,
camera/projection/RNG/door/effect/frontend/clock mismatches, initial-state and
ROM/input/retrace identity, stale capture ticks, missing channels, exploratory
port/synthetic/manual/stereo rejection, no-overwrite behavior, malformed JSON
and PPM, optional PNG roundtrip, and command exit/report consistency.

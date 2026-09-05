# Guard attachment preparation and world traversal — 2026-09-05

Continues the verified [ARM music saturation candidate](Verification20260905MusicSaturation.md).
Evidence is in `build/host-tests/texture-warmup/`. Performance numbers are
Azahar measurements using the existing New 3DS configuration, not physical
console results. Both original and New 3DS use the same portable rendering
change. The exact pack remains
`7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`.

## First-use Caverns texture import

The buffered import trace identifies image IDs 1777 and 1778 in
`PhatberetblueZ.bin` as the 5.376 ms first-use import at frame 1101.
The prior loading preparation visited guard bodies and heads but omitted
weapons and hats. The new read-only model dependency visitor enumerates
already-loaded attachment texture tables, including hidden parts. Stage
installation and the existing room texture transaction include these images
before gameplay. Neither model poses nor visibility, AI or RNG advance.

The visitor validates the entire table before callbacks, does not load or
allocate models, and leaves embedded segment-5 images under model ownership.
The texture transaction retains the existing 192-slot limit, rollback and
handle borrowing. No compressed texture pins are retained after import.
Unknown future/dynamically selected models still have the existing runtime
fallback; this is not a claim that every possible future attachment is resident.

The first matched prewarm-only comparison has zero runtime guard texture
imports, versus 13 in the traced control (11 at the first frame and the two
beret images at frame 1101). Warm work-budget misses fall from 3 to 2 of 2880;
zero submission intervals are skipped. This fixes the demonstrated hitch,
not the remaining music/props spikes. Cold here means a fresh process with
empty application/GPU texture caches. Host filesystem caches were not flushed.

## Dam experiments and retained traversal approach

The world diagnostic observes 1,166,194 batch frustum tests on the route.
Whole-room bounds resolve none. Sampled visibility averages about 1.06 ms,
material application 0.34 ms and draws 0.96 ms; these nested timings include
measurement overhead. Sparse compatibility sampling then identifies substantial
repeated comparisons/traversal. Its extrapolation includes clock overhead and
is not an exact standalone phase measurement.

Rejected experiments remain in the evidence folder, not production code:

- Early bounds rejection: 11.4277 to 11.4229 ms mean work, 413 to 412 budget
  misses, unchanged 46 skipped intervals. Too small to justify complexity.
- Groups of eight bounds: reduced individual bounds rejections from 488,015
  to 244,535 and world mean by 0.067 ms, but added about 2 ms to some upload
  phases and worsened frame tails. Removed.
- The earlier static-camera cache and equivalent-GPU-state merging also
  failed to improve this route and remain removed.

The retained candidate prepares the existing contiguous draw runs during
immutable room publication. A run requires the same room, coordinate space,
texture validity/ID, complete material bytes, and contiguous vertex range.
The renderer jumps over the interior of such a run. It preserves the previous
scalar merging across culled incompatible ranges, visibility decisions, exact
draw ranges and order. Dynamic overlays keep the scalar traversal.

The plan uses four bytes per immutable batch and falls back on allocation
failure. Counts come from the owning dynamic scene: during overlay publication,
the preview summary can still contain the old count. Mixing the old preview
count with the new overlay count spuriously rebuilt the plan and could include
overlay batches in its temporary prefix. The final candidate avoids that mix
and falls back to scalar traversal if a plan exceeds the current draw count. Room transactions replace their range allocation before freeing the
previous one. That identity allows redundant full uploads after overlay or
texture changes to reuse the plan; new room publications rebuild it. Stage
initialization clears the preview, and teardown frees the plan. Noncanonical
preview data uses publication-based rebuilding rather than that identity proof.

The first instrumented run reduces world mean 2.3812 to 2.0788 ms, overall
mean 11.4696 to 11.1705 ms, budget misses 424 to 354, and skipped intervals
47 to 36. Recorded gameplay and draw counts match. Its worst frame regresses
31.3223 to 32.7686 ms. A buffered preparation diagnostic and the transitional
count test identify redundant plan building during overlay publication. The
final candidate derives the prefix from its owner and retains the immutable-room
identity check described above. It also reuses the plan in the existing streaming
material-group count, preserving that diagnostic exactly. Final normal-profile
measurements below, rather than this prototype, determine the delivered result.

## Verification and final measurements

Final candidate executable:
`efce164d5cffccfb45569364cb727de457da3a11cee71f6ee415f54686bdda7d`.
The first final Dam capture builds the plan once during loading (0.828 ms),
versus 29 builds / 24.034 ms total in the transitional-count prototype. Its
mean world phase is 2.0008 ms versus 2.2495 ms in the music-saturation control.

The normal-profile comparisons trim the first 120 presentations for steady
frame statistics, but every run starts in a fresh process. Runtime guard-import
traces cover the complete run, including its first frame. Neither detailed
opcode/PCM diagnostics nor world-submission sampling is enabled for this table.

| Route/build | Mean ms | p95 ms | p99 ms | Peak ms | >16.67 ms | Skipped intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Dam control | 11.3385 | 18.5354 | 21.3103 | 31.4393 | 376 / 4250 | 42 / 4249 |
| Dam final, first run | 11.0882 | 18.0080 | 20.8648 | 31.1168 | 319 / 4250 | 31 / 4249 |
| Dam final, repeat | 11.0882 | 18.0080 | 20.8647 | 31.1167 | 319 / 4250 | 31 / 4249 |
| Caverns traced control | 7.7721 | 12.6176 | 14.4578 | 17.9928 | 3 / 2880 | 0 / 2879 |
| Caverns final, cold process | 7.7824 | 12.6135 | 14.4591 | 17.6437 | 2 / 2880 | 0 / 2879 |
| Caverns final, repeat process | 7.7824 | 12.6136 | 14.4592 | 17.6436 | 2 / 2880 | 0 / 2879 |

Dam world mean falls 2.2495 to 2.0008 ms (11.1%). Both final repeats build
exactly one plan, during loading. Budget misses fall 15.2% and skipped intervals
26.2%. The early-rejection and grouped-bounds experiments remain removed.

Caverns frame 1101 falls 17.9928 to 12.5702 ms, with guard import 5.3755 to
0 ms. Only frames 2039 and 2343 remain over budget, combining music and props
work. No runtime guard import occurs anywhere in either final capture. World
mean in Caverns rises slightly, 0.7954 to 0.8072 ms; the prepared traversal is
a Dam improvement, not a universal recurring speedup. Retaining its small
Caverns overhead is justified by the repeatable Dam gain; the attachment fix
independently removes Caverns' demonstrated first-use hitch.

“Repeat” does not mean a warm application cache: the emulator process is
restarted each time. The host filesystem cache may be warm in all runs, and
was not purged. Existing ownership/reconciliation tests separately validate
borrowing/reuse through synthetic room transitions. These captures do not
prove a physical-console or campaign-wide 60 fps lock.

All normal comparison gates pass: initial RNG, recorded retraces, complete
input, intermediate checkpoints and ending gameplay state match. Draw calls,
material-state counts and frustum counts also match exactly. The Dam combat
verifier retains 26 PP7 shots, two damaging guard hits, 1174 guard-fire dispatches,
one opening-guard death, player damage and zero unknown AI commands. The route
ends with Bond dead at frame 4370, not with a completed mission.

Both deep comparisons also pass their complete gameplay/draw gates and exact
generated music PCM checks:

| Route | Generated bytes | Matching FNV-1a 64-bit digest |
| --- | ---: | --- |
| Dam | 6,435,584 | `1d505126fa091a5c` |
| Caverns | 4,416,000 | `3fb892d7b6dacc8c` |

These are complete generated music-stream checks for these routes, not a claim
of identical real-time speaker delivery. The existing exact host PCM and SFX
cache/lifecycle tests cover the retained sound changes. Deep diagnostics add
timing overhead and are excluded from the normal performance table above.
The Caverns audio pair deliberately uses the older `24f41d5b...` recorded
cadence matching its audio control; the normal/cold pair uses `7662e338...`.
Dam uses the same `b731bbc5...` cadence in both modes. Every final provenance
file records requested detail/world modes, installed executable/pack/input
hashes, stage selection and cadence hash.

## Regression coverage and reproduction

- `scripts/tests/test_renderer_prepared_world_runs.py`: 90,197 starting points
  match the prior scalar loop's exact ranges, merged counts and visibility
  calls. Includes room/material/texture/coordinate boundaries, gaps, hidden
  incompatible batches, animated overlays, publication changes, allocation
  failure, room identity replacement and transient stale preview counts.
- `scripts/tests/test_renderer_overlay_room_reuse.py`: 240 overlay transitions,
  exact buffers/bounds/UVs/colors with empty, shrink, growth and failure paths.
- Attachment dependency tests verify the actual blue-beret IDs, hidden-part
  enumeration, unchanged model/provider state, repeated calls, callback
  failure, whole-table validation and embedded-image ownership. Real assets
  cover 21 stages, 665 body/head pairs and 300 hats across nine models.
- Texture reconciliation retains the fixed capacity and transaction behavior:
  96 synthetic room sets, 159 imports and 5985 retained handles, exact UVs;
  14 capacity/ownership/rollback cases.
- Final ARM build: `scripts/build_3ds.sh -j4`, `final-build.log`.

The final `scripts/test_port.sh` suite passes, exit 0 (`full-suite.log`), and
`git diff --check` is clean. The actual ARM executable matches every final
capture's hash. No build or regression process ran during a performance capture.

The candidate is preserved at `build/3ds-candidates/texture-world-runs-efce164d/`
and staged at `build/3ds-sd/3ds/goldeneye-3ds/`, with the exact pack, SMDH,
catalog, hashes, source patch/archive, toolchain image identity and performance
evidence. It is also installed in Azahar. Temporary stage, input, retrace,
detail and world-profile overrides are removed, and the original configuration
inventory is restored. No commit, push or physical-console deployment is made.
The user's existing caffeinate wake protection remains active.

Remaining acceptance work: Dam still has 319 / 4250 measured frames over the
60 Hz work budget; Caverns has 2 / 2880. Canonical ticking, props and music
remain substantial CPU costs. This pass preserves exact draw ordering and
original gameplay/audio behavior in the tested routes, not complete campaign
parity or physical New/original 3DS performance certification.

Normal-startup smoke test passed after configuration restoration: the staged
candidate reached the original Rareware logo in Azahar. The emulator was
stopped afterward; wake protection was retained.

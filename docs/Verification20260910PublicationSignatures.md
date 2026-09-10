# Model placement signature reuse — 10 September 2026

This change starts from `d0c3e8d2869341b9fee832caf7848a9862690705`,
including the September 9 reverb byte-order correction. It reduces repeated
CPU signature work without changing geometry, publication signatures, room
membership, rendering, canonical ticks, synthesis or cache ownership. Route
timing and physical New 3DS XL acceptance remain pending.

## Change and bounded cost

The model scene cache hashes room ID, three placement floats and sixteen
outer-matrix floats before appending the joint-bank hash and mutable vertex
bytes. Every field occupies eight bytes in the existing FNV encoding. Guard
body/head/attachment input construction copies the same placement for related
parts; repeated parts therefore redo the same 160 byte steps.

`cache_placement_hash` reuses only the immediately previous input's placement
prefix, within the current preparation call. It checks the actual room ID and
compares all 76 float bytes. Signed zeros remain distinct. Different bank IDs,
bank contents, mutable vertices and final input order still undergo the existing
independent hashing. No pointer or prefix is retained across calls, and no
allocation, public structure or hash format changes.

For a consecutive run of N equal placements, the prefix work falls from
160N to 160 hash byte steps, plus N−1 equality checks. A run of 24 parts saves
3,680 FNV steps. A room mismatch exits before the float comparisons; different
placements retain their original hashing plus the equality-check cost. This is
an exact operation bound, **not a measured frame-time or whole-game speedup**.
It does not revive the previously rejected shortened float-hash experiment.

## Exactness

`python3 scripts/tests/test_model_placement_signature.py` passes at both `-O2`
and `-O3` with ASan/UBSan. The test extracts and executes the production helpers,
but independently serializes the old signature format as a byte-stream oracle.
It covers:

- 100,000 placement comparisons: 24,193 reuse cases and 75,807 recomputations,
  arbitrary float representations, invalidated prefixes and first-input resets.
- All 19 signed-zero positions, all 76 float-byte changes and all 32 room-ID bits.
- 95,725 complete input signatures across 3,000 empty/growing/shrinking input
  sets, shared and null joint banks, distinct bank lengths, animated matrices,
  mutable vertex bytes and ordered aggregate signatures. Quantized matrix bytes
  match independent double-precision fixed-point conversion.
- Exactly zero hash byte steps for a reusable prefix, and the original 160
  otherwise. This counter exists only in the test's generated source.

The existing authentic PP7, window and gate model tests also pass at `-O3`
under ASan/UBSan, including 40 original components, 120 reordered/empty/growing
publications, dirty ranges, buffer changes and complete vertex/batch comparisons.
For this initial isolated check, the private test driver replaces its benchmark
clock reads with zero; its printed timing fields are not measurements. The
unchanged segment-publication test additionally passes 377,724 complete-buffer
vertex cases, including 250,816 duplicate copies. Topology-storage and first-person
layout tests pass. The production model source compiles for ARMv6K/VFP at `-O3`.

Private evidence is in the task worktree's `build/performance-20260910/`:
`placement-signatures.log`, `model-correctness.log`, and `model-candidate.s`.
Source assets were copied read-only from the shared build inputs into this
worktree; no shared emulator, SD/config files or assets were modified.

## Reproducible measurement request

Prepare a host-only synthetic signature benchmark without running it:

```sh
python3 scripts/benchmark_model_publication.py \
  --baseline-ref d0c3e8d2869341b9fee832caf7848a9862690705 \
  --output build/performance-20260910/signature-benchmark
```

The printed command runs an immutable executable after the integration
coordinator schedules a quiet window. Its separate source objects execute the
baseline and candidate preparation routines on 192 inputs, eight banks and
20 bones per bank. It includes same-room, 24-parts-per-room and unique-room
control cases; seven alternating-order pairs; 2,000 preparations per sample;
one declared scratch-allocation warmup; and matching signature checksums.
It records compiler/platform/source/driver/binary provenance. These synthetic
host measurements exclude geometry transformation, GPU submission, audio,
loading and presentation, and cannot be converted into hardware FPS.

The integration coordinator must also compare the audio-corrected checkpoint
and candidate on the same heavy Dam and established Dam inputs/cadences, plus
a Caverns route extended to at least 3,600 measured frames after a declared
warmup. Use audible original music/SFX and maximum stereo. Run cold first/repeat
processes with deep/world/PCM/capture I/O diagnostics disabled; preserve
ordinary streaming, first-use costs and gameplay work. Check full input/assets/
cadence provenance, recorded gameplay and complete publication/draw equality.
Run separate PCM/diagnostic comparisons and a separate natural-scheduler route;
different combat histories are not matched speed comparisons. Report startup
and explicit transitions separately, without removing in-route stalls.

Retain this optimization only if matched route evidence supports its net value.
Even a passing emulator route cannot establish physical 60 Hz. Physical CPU/GPU
work, complete presentation intervals, audio stability and two-eye rendering
must still pass sustained representative New 3DS XL routes; no device is known
to be available in this task.

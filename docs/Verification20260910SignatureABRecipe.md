# Isolated placement-signature A/B recipe — 10 September 2026

The baseline is the final integrated source with only the successful reuse
return in `cache_placement_hash` disabled. It always computes the original
room/position/outer-matrix prefix. All source outside that helper remains
byte-identical, including the callback invalidation, shared matrix-bank owner
optimization, publication format, audio, renderer, controls and multiplayer.
The helper's parameters and call site remain intact; the compiler can eliminate
unused previous-placement bookkeeping in the baseline.

This isolates the net work avoided by placement reuse, including its equality
checks. It does not revert an older game checkpoint or undo subsequent fixes.
Both variants must be built from the same final integration freeze, with the
same toolchain and private build inputs. The prior standalone performance
binary is not the baseline for this final integrated comparison.

## Prepare after final source freeze

`scripts/prepare_3ds_signature_baseline.py` reads a source checkout and writes
only a new evidence directory. It generates `baseline.patch`, both complete
model-source variants, captured headers, the existing scalar-oracle generator,
four correctness executables and their compiler/run logs, plus `receipt.json`.
It applies and reverses the patch against a private copy and checks the complete
file bytes in both directions. Changes to the expected fast path or missing
callback invalidation fail preparation. Input bytes are read without newline
normalization; unsupported line-ending changes fail rather than silently
altering source outside the helper.

After the coordinator has committed the final clean integration freeze:

```sh
integration_root=/absolute/path/to/final/integration-checkout
recipe_root=/absolute/path/to/new-signature-ab-recipe
baseline_root=/absolute/path/to/new-isolated-baseline-checkout
final_commit=$(git -C "$integration_root" rev-parse HEAD)
test -z "$(git -C "$integration_root" status --porcelain)" || exit 1

python3 scripts/prepare_3ds_signature_baseline.py \
  --source-root "$integration_root" --output "$recipe_root"

git -C "$integration_root" worktree add --detach "$baseline_root" "$final_commit"
git -C "$baseline_root" apply --check "$recipe_root/baseline.patch"
git -C "$baseline_root" apply "$recipe_root/baseline.patch"

python3 - "$integration_root" "$baseline_root" "$recipe_root" <<'PY'
import hashlib, json, pathlib, subprocess, sys
candidate, baseline, recipe = map(pathlib.Path, sys.argv[1:])
receipt = json.loads((recipe / 'receipt.json').read_text())
model = 'port/src/ge_original_model_scene.c'
assert receipt['status'] == 'complete'
assert hashlib.sha256((candidate / model).read_bytes()).hexdigest() == receipt['candidate_sha256']
assert hashlib.sha256((baseline / model).read_bytes()).hexdigest() == receipt['baseline_sha256']
def inventory(root):
    names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '--cached', '--others', '--exclude-standard', '-z'])
    return {name.decode(): hashlib.sha256((root / name.decode()).read_bytes()).hexdigest()
            for name in names.split(b'\0') if name and (root / name.decode()).is_file()}
left, right = inventory(candidate), inventory(baseline)
assert set(left) == set(right)
assert {name for name in left if left[name] != right[name]} == {model}
print('Only the intended model-source file differs')
PY
```

The recipe never modifies the integration checkout or launches an emulator.
The worktree commands above are for the coordinator after final freeze. Build
each variant in its own checkout with the pinned toolchain, binding all private
build inputs and the resulting binary hashes in its build receipt. Compare the
complete source inventories again after the builds. The source inventory check
above covers tracked and non-ignored source files; ignored/generated build inputs
still require the existing build and asset receipts.

## Correctness verification and review

The generated tests execute the existing independent scalar-byte oracle from
`scripts/tests/test_model_placement_signature.py` against each captured source.
Only the baseline's reuse-work counter assertion changes: an eligible repeated
placement must execute all 160 original FNV byte steps. Signature expectations,
matrix quantization checks and the opaque mutable-callback regression remain
unchanged. At both `-O2` and `-O3`, with ASan/UBSan and warnings as errors:

| Variant | Reused prefixes | Rebuilt prefixes | Complete input signatures matching scalar bytes |
| --- | ---: | ---: | ---: |
| Integrated candidate | 24,193 | 75,807 | 95,725 |
| Reuse-disabled baseline | 0 | 100,000 | 95,725 |

Coverage includes all signed-zero positions, differing placement bytes and room
bits, first-input/reset cases, shared and null matrix banks, changing input
counts, mutable vertices, ordered aggregate signatures and the callback that
mutates its own/next placement and its eligibility marker. No clock-based timing
driver runs. These extracted-helper checks and a reviewed one-hunk patch verify
the intended source/functional difference; full ARM builds and matched gameplay,
publication/draw and audio comparisons remain required after final freeze.

## Existing route files, verified byte-for-byte

The current files are in the performance worktree at
`/Users/kyle/.codex/worktrees/2c7c/goldeneye-3ds/build/performance-20260910/routes/`.
They were rehashed and checked against their existing `manifest.json`. Copy them
without editing when preparing the final requests.

| Route | Input | Cadence | Configured capacity |
| --- | --- | --- | ---: |
| Heavy Dam | `dam-input.cfg` | `stress.cadence` | 4,500 |
| Established Dam | `dam-input.cfg` | `dam-control.cadence` | 4,500 |
| Extended Caverns | `caverns-4500-input.cfg` | `caverns-4500.cadence` | 4,500 |

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `dam-input.cfg` | 671 | `ea3ea43e4de2b5eb74355bf213980452010217ee942a8dcdfcb86bb291c10fe6` |
| `stress.cadence` | 9,032 | `afed049bb9d8d6a57dab39a64f22b85bdb2218b9f473855ba789f7424d5429ab` |
| `dam-control.cadence` | 9,032 | `b731bbc5ddd95515de21c950cb67c41796aab8ee21607bd81332b8ee4f573bca` |
| `caverns-4500-input.cfg` | 239 | `eb2558e3502cae4679a6901f37c14eb85fd41dff492f617a3494ad3c2b854cc3` |
| `caverns-4500.cadence` | 9,032 | `52129042936d8bbed229e67cef5aad6724062b3d56c7779330e211c037f00dfc` |

Both Dam routes use the same 11-target version-7 authored input and preserve
their different recorded retrace histories. Heavy cadence contains 4,485 ones,
13 twos and two threes; established cadence contains 4,407 ones, 91 twos,
one three and one four. Configured capacity is not a promise of actual displayed
frames: authored routes can finish early, and replay files retain unused padded
capacity. Compare actual completion/checkpoints across the final A/B pair.

The 4,500-frame Caverns route preserves the established first six input segments
(750 frames) and extends the final stationary segment from 2,250 to 3,750 frames.
Its first 3,000 cadence entries exactly match the old cadence
`7662e338e4dd3d17556b84e1cec7b8e43493259f8922e7d7458d33b2c4d57cdd`;
the final 1,500 entries are explicitly one retrace each. This suffix is a
declared deterministic extension, **not recorded natural scheduling evidence**.
The complete extended cadence has 4,490 ones, nine twos and one five.
`scripts/performance-probes/caverns-extended.cfg` is a different 3,721-frame
route and must not be substituted for these requested 4,500-frame files.

## Requests and coordinator gates

After live probe correctness closes, prepare each route with
`prepare_3ds_benchmark.py prepare` from the correct variant checkout, passing
the table's exact `--input`, `--cadence` and `--stage`. Use that variant's newly
built `--binary`, the same regenerated fullpack/assets and actual toolchain and
environment descriptions. Existing requests under
`build/performance-20260910/final/requests/` reference obsolete binary/assets
metadata; they identify prior intent and must be regenerated.

The minimum clean matrix is three routes × two variants × cold/independent
repeat: **12 complete runs**. Restart the process each time and declare reset
semantics. Use a 120-frame warmup and the default
`all-after-warmup-except-final` policy, retaining at least 3,600 measured frames
and exactly one trailing GPU-observation frame. Report every post-warmup CPU
frame, including that final guard, and all presentation intervals. Retain
in-route first-use/streaming stalls and late combat; report startup separately.
Do not crop the route to a chosen 3,600-frame interval.

All emulator/timing runs require the coordinator's exclusive quiet window.
Prepare separate PCM correctness pairs for the same three routes, and separate
natural-scheduler routes without a cadence override; different natural combat
histories cannot support an isolated speedup claim. Compare complete final A/B
gameplay/checkpoint/cadence, publication/draw/frustum and music-byte/hash evidence
with the existing analyzers, retaining failed runs and outliers.

The final Azahar pair must use the corrected, explicitly receipted
`--azahar-hle-dsp-marker` environment with `audio_emulation=0`. Require actual
NDSP enabled/error zero, music worker enabled, maximum slider and both eyes
(slider 1, eyes 2), along with observed audible music/SFX and complete active
NDSP/music-rendered pipeline rows. Earlier silent runs cannot be reused. The zero-byte
HLE component is emulator configuration and makes no physical-firmware claim.
Preserve each actual per-run profile byte hash. Since isolated output paths can
change profile bytes, compare the common source-profile identity and relevant
audio/stereo semantics across variants; do not require the full path-dependent
profile hashes to be identical. Emulator results remain separate from physical
New 3DS XL acceptance.

## Current preflight evidence

The final helper was run read-only against clean integration commit
`fe981c00d95880b4f05cd5a03423ebcedf0a3603`, capturing 200 header/include files.
This is the current recipe preflight, not the forthcoming final game freeze.
The integration model source remained unchanged during preparation.

| Artifact | SHA-256 |
| --- | --- |
| Candidate model source | `276a429b4abcf727c16a113132409de9de2f4dcad0bbe2e2a4544df97c671870` |
| Baseline model source | `9b6e4eb5f4532b52020acfecd80714ae2bcd2ca4b978682877dde3b7c369d6a3` |
| One-hunk baseline patch | `b846e49decc3f667612652a4915d5cdaedfff9ca3050f77639278b3cae6b0385` |
| Preparation receipt | `6bcf1557233b3a0cf3cf6df7ead785e5df62db7af2227a8012e552e2a9a1b98b` |
| Exact route inventory | `6c5bd9df97d648ddf848890850438250910c091eaf55818d5aa80e927e6e0c69` |

All 11 command exit statuses and log hashes verified. Independent review
confirmed the one-hunk functional scope and unchanged callback regression.
Additional negative checks reject CRLF normalization, missing callback
invalidation and an already-reversed baseline. Private artifacts, logs and the
route inventory are in the performance worktree's
`build/signature-ab-recipe-20260910-final/`. No emulator, gameplay or timing run
was performed for this preparation.

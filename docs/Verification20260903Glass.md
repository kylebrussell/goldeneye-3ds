# Dam tower glass and model-publication hashing

Baseline commit: `3db37018924107741c5f2241b992607dccb3459e` (already pushed).
No ROM or extracted assets are added to Git by this change.

## Visual repair

The tower's Pitem 104 window secondary display list omits parent material
state. The previous portable model path decoded it with blending disabled
and no inherited lighting, producing opaque black panes.

The adapter now passes big-endian parent setup lists through the same GBI
pipeline before each child list. The type-4 render-mode body and GlobalLight
initializer are generated directly from the decomp and checked against their
sources. The SDK's signed blender bit shift is made unsigned at the adapter
boundary without changing its command bits. Lighting and material commands
execute before vertex processing. Setup values participate in retained-cache
identity; disabled setup does not hash the payload.

Ordinary and tinted glass use the original object's calculated opacity.
PICA implements `texture alpha * shade alpha + primitive alpha` in one
multiply-add texture-environment operation. Live opacity publication changes
only the material byte in the retained overlay and combined scene, without
decoding lists, remapping UVs, allocating geometry, or uploading vertices.

Azahar visual A/B used the existing authored route checkpoint 61 / pad 133,
room 111, position `(17013.3546,496.006691,12527.8211)` and direction
`(-0.675254067,0,0.737585212)`. Baseline `8d633901...` visibly showed black
panes. Candidate `55f9b6b1...` showed the stairs, railings and exterior through
those panes, with the concrete columns retained. Screenshots were inspected
through the native UI. Private reproduction: `build/visual-probe/glass-pad133.geview`.

This fixes the black obstruction, not every glass fidelity detail. The generic
path still lacks the canonical camera-dependent LookAt/environment reflection
publication and full object fog/damage/fade parent state. Windowed-door
material support exists at the adapter boundary but is not yet wired through
the separate live door overlay. Distant exterior objects/room visibility also
need a separate pass. No gameplay/controller code was replaced.

## Optimization and replay evidence

The model cache already computes a per-input publication signature over the
room, placement, outer matrix and quantized joint bank. Its aggregate signature
now folds that value once instead of hashing all those fields a second time.
The ordered topology signature still participates; published geometry and
draw ordering are unchanged.

Fixed 750-frame input replay, no concurrent builds:

| Candidate | Signature phase ticks | Post-warm-up peak | Frames over 16 ms |
| --- | ---: | ---: | ---: |
| Glass only `55f9b6b1...` | 7,712,983 | 19 ms | 7 / 630 |
| Glass + signature reuse `67c951eb...` | 4,576,860 | 18 ms | 7 / 630 |

At 268,111,856 ticks/second, the measured signature phase decreased about
40.7%, or 11.7 ms across the entire run (about 0.016 ms per displayed frame).
This is a small CPU optimization, **not** evidence of a sustained 60-FPS or
whole-frame-tail improvement. The pre-existing warm-up maximum remains
56–57 ms. Both runs finish 750 original simulation/actor ticks at exactly
`(19900.337891,-39.704582,17499.558594)` with identical draw counts, game-state
records and 72 decoded sounds with zero decode failures. Evidence:
`build/visual-probe/glass-{55f9,67c9}-move750.result`.

## Verification

- ASan/UBSan model-publication tests cover authored window geometry,
  lighting, blending, depth compare/no-write, six opacity values, and material
  cache invalidation: `build/host-tests/glass-publication-final.log`.
- The actual GPU setup functions are exercised for all 256 opacity bytes and
  the subsequent ordinary-material state reset by `test_glass_gpu_alpha.py`.
- Generic model sanitizer tests retain byte-identical outputs through
  topology changes, changed-peer publication, invalidation, matrix sharing,
  exact-size rejection, room changes and empty inputs:
  `build/host-tests/glass-model-signature.log`.
- The earlier full host suite completed successfully in
  `build/host-tests/glass-full.log`. It exposed a previously uninitialized
  optional monitor-test callback; the fixture now zero-initializes that
  provider instead of depending on incidental stack contents.
- Final ARM build: `build/host-tests/arm-glass-signature.log`.
  Executable SHA256:
  `67c951eb56b7884c627d042aff966d795ab286f806d81a0710af7d26b77520b5`.
- Assets are unchanged:
  `938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.

Final full host suite: `build/host-tests/glass-final-full.log`, exit 0, no
ASan/UBSan/assertion failures. Build output, private candidate, hardware
staging and Azahar executable hashes match the final SHA above. Installed
input/tour overrides were moved back into the private evidence directory;
no stage override remains in Azahar. Hardware staging's existing `cradle`
selector, saves and DSP settings were preserved.

Sustained combat performance and mission completion remain broader project
goals, not claims made by this repair. The next useful work is the remaining
camera-dependent glass reflection/parent fog state and the much larger
movement/combat frame-time spikes, not further micro-optimizing this hash.

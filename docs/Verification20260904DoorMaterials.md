# Retained windowed-door material publication

Continues `23e1e49c`. This is a renderer/material integration, not a change
to original door movement, collision, portal state, AI or object opacity.

## Implemented boundary

The campaign door overlay previously omitted the type-4 parent render setup
already restored for static glass. Initial installation and moving-door
cache publications now invoke that same decomp-generated setup for native
`DOORFLAG_WINDOWED` records. Original z-buffer policy, GlobalLight and
primary/secondary blend/combiner commands reach the child display lists.
Other door types retain their previous path.

Door opacity is owned by the original `calculatedopacity` field. It is not
a geometry generation: a stationary door can change opacity while its
matrix and mesh remain unchanged. The renderer therefore keeps neutral
primitive alpha in its immutable topology template and publishes the live
byte into all three mutable material copies (door segment, overlay and
combined scene). Initial installation also applies live alpha before the
candidate scene commits, so the neutral template is never the displayed
first-frame material. Opaque frame batches are not overwritten.

Per-part door ownership/ranges are allocated during cold scene installation
and committed with the scene. Their offsets are door-segment-local. Ordinary
prop growth/shrink can rebase the door segment without changing its material
owners. Empty scenes, replacement, failure and teardown release the metadata.
Material-only updates perform no allocation, geometry decode, UV mapping or
vertex upload.

## Verification

- `scripts/test_stage_model_publication.sh`: ASan/UBSan with authored
  PwindowZ geometry and a native windowed DoorRecord fixture. All 256 original
  opacity bytes produce the expected PICA constant/blend state while the
  retained cache performs one topology build and 255 unchanged builds.
  Non-windowed doors, opaque frame combiners and disabled-Z policy are covered.
  Prior static-glass / original LookAt tests remain enabled.
- `test_windowed_door_material_publication.py` executes the actual runtime
  publisher and alpha helper with host buffers. It checks 512 opacity/rebase
  transitions, both mutable scene copies plus the retained door segment,
  untouched room/neighbor/opaque-frame data and invalid-range rejection.
  Entry lookup and native opacity use test seams; native field semantics are
  covered separately by the test above. This is not an emulator test.
- Full host suite before the first-frame alpha guard:
  `build/host-tests/door-material-full.log`, exit 0. Final rerun uses
  `build/host-tests/door-material-final.log`, exit 0, with no sanitizer or
  assertion failures.
- ARM/3DSX build: `build/host-tests/arm-door-material-final.log`, successful.

Executable SHA256:
`72666c5e7439ca82a14a499e235a160d2a8152aab7f3d5ae57af29cb6afa29f0`.
Asset pack is unchanged:
`ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`.

## Exact remaining work

macOS remained locked, so this change has no new Azahar visual, input or
performance result. The prior matching installed/hardware-staged
`ade136f1` / `ee769251` pair is preserved. The new candidate is kept separately
for unlocked validation; no test overrides or save changes were installed.
Candidate directory: `build/3ds-candidates/window-door-material-72666c5e`,
containing a hash-matched executable and unchanged asset pack.

This closes windowed-door parent material and dynamic-alpha publication,
**not** the remaining camera-dependent reflection boundary. Door normals need
their actual published, potentially articulated segment-3 matrix indices and
the canonical camera LookAt; the static-glass object matrix must not be
substituted. Full parent fog/damage/fade and RDP mip/detail blending remain
separate gaps. Sustained combat frame tails, 60 FPS on hardware and complete
mission/menu playability remain unproven by this pass.

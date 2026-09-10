# Renderer-only guard room visibility — 2026-09-10

The new room union must reach model publication as well as background draws.
Previously `update_stage_guard_visibility` supplied mono room membership to
`state.visible`, and the guard runtime used it to skip matrices, model scene
inputs, attachments and muzzle flashes. Widening background visibility alone
would leave guards missing at eye-only doorway edges.

`ge_original_stage_guard_runtime_set_renderer_visibility(runtime, membership,
room_count, horizontal_margin_ndc)` now copies a renderer-only room mask and
horizontal frustum margin. Any nonzero mask value marks a room eligible. The
runtime applies this decision to its matrix publication and both cached and
uncached model collectors; existing liveness, residency and attachment-parent
checks remain in force. The mask does not alter canonical `state.visible`,
`room_id`, `PropRecord` flags, active links or character state.

Pass `NULL, 0, 0` to disable the override and restore existing mono decisions.
The margin is an expansion of normalized horizontal clip width, matching the
world renderer's `max_shift_pixels / 160` convention for a 320-pixel viewport.
Positive infinity disables horizontal rejection; near and vertical rejection
still apply. Invalid masks, excessive room counts, negative margins or NaN
reject the update without changing the previous override. Storage is fixed
(256 room bytes); callers can reuse or free their input after the call.

The platform installs this override before guard matrix/scene publication.
Gameplay visibility publication continues to use the original mono room set.
Changing the slider without moving must also refresh guard matrices and
overlay topology; the room-mask setter alone does not regenerate geometry.
World guard vertices use eye coordinates because their model matrices already
contain the view transform. They still require world stereo settings; the
coordinate space is not a first-person-layer ownership declaration.

## Muzzle-flash random ownership

The existing muzzle-flash producer consumes two gameplay PRNG values for each
authored visible flash. Calling it for an extra stereo-visible guard would
otherwise change the later gameplay sequence. Existing mono-visible owners
retain the original two calls and ordering. Additional flashes use
`randomGetNextFrom` with a local seed derived from guard identity, weapon slot,
and the current original game tick. The local seed never writes the global
PRNG. This also covers guards in a canonical-visible room that were outside
the mono sphere frustum and are recovered by stereo expansion.

This local sampling is an explicit visual approximation for the additional
stereo-only effect, which has no corresponding mono reference draw. It keeps
the same effect sample for repeated submissions of a game tick and preserves
canonical gameplay randomness. The authored Gunfire RW visibility still owns
whether a flash exists at all.

## Verification

`python3 scripts/tests/test_renderer_eye_integration.py` compiles the real guard
runtime and original random implementation. Native and short-enum layouts
pass ASan/UBSan checks for:

- Both horizontal edges, unchanged near/vertical tests, infinite horizontal
  margin and sampled shader-visible points from near to far depths.
- Copied room membership, invalid-update atomicity, disabled mono restoration,
  and unchanged canonical flags/character records.
- Unchanged global PRNG for added room-only and sphere-only stereo flashes,
  stable repeated samples, and the exact original two PRNG values for a
  canonical-visible flash.
- Extracted actual main room-consumer helpers: gameplay sees mono rooms,
  renderer sees both eye additions, queries do not mutate state, and failure
  falls back to the mono result. Source routing checks keep gameplay, glass,
  resident protection, guard override and articulated publication separated.

The existing real-asset `test_ge_original_stage_guard_runtime.c` adds a guard
with canonical visibility off, then recovers its matrices through renderer
membership. Cached and uncached scene queries agree, and `PropRecord` plus
`ChrRecord` remain byte-identical. Disabling the override suppresses that guard
again. The full fixture passes its prior 665 authored character pairs across
21 stages, 300 attached hats, body/weapon arena-reuse, scene-cache and autoaim
checks. Required generated fixture files were copied into this task's local
build directory from the read-only shared asset evidence.

This is software ownership and geometry-publication evidence. It does not
prove full visual parity, physical GPU cost, stereo comfort, or that every
authored animation callback is equivalent when the original offscreen branch
did not publish a fresh matrix. Such guards use the existing renderer fallback
matrix calculation, with its existing limits.

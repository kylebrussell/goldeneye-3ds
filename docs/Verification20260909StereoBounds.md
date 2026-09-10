# Stereo culling correctness and uniform submission — 2026-09-09

This pass fixes a demonstrated renderer rejection bug and removes redundant
stereo uniform submissions. It does not establish physical New Nintendo 3DS
XL frame rate, visual parity, or comfort at maximum slider depth.

## Correctness change

The shader's perspective displacement is `P * (D / W - 1)` pixels per eye.
The prior CPU bound silently replaced near depths smaller than `D / 3` with
`D / 3`, assuming nearby geometry would always be large enough to survive
culling. A small nearby triangle disproves that assumption: with the default
world settings (`P=8`, `D=180`) and `W=15`, actual displacement is 88 pixels,
but the old bound retained only 16 pixels beyond the mono viewport. The shared
left-eye draw list could therefore omit geometry visible to the right eye.

`ge_3ds_stereo_max_shift_px` now uses the maximum of the true near endpoint
and the far limit. The renderer uses separate world and first-person bounds;
custom weapon depth settings no longer inherit an unrelated world margin.
The configurable conversion to normalized clip coordinates cannot fall below
`1/160` for the existing 320-pixel authored viewport. Invalid near distances
disable horizontal rejection through an infinite margin, while mono keeps
the original matrices unchanged.

This may retain more geometry near the viewport edges than the incorrect
cap did. That is necessary for the shared stereo draw list to be conservative.
The first-person layer's default margin is independently smaller (6 pixels at
depth 15), avoiding unnecessary use of the world's 88-pixel margin there.

## Submission change

`renderer_set_stereo_layer` remembers its last uploaded pair of dedicated
shader uniforms. Authored and runtime coordinate spaces share world stereo,
and every mono layer uses the same zero shift. Repeated values now skip the
two Citro3D uniform calls. Eye, slider, configuration, layer, and uniform-index
changes still upload the appropriate values. This is a reduction in measured
host-call count; no device timing improvement is claimed.
Renderer initialization explicitly invalidates the cache after shader uniform
lookup, including replacement shaders that reuse the same register index.

## Verification

- `scripts/test_3ds_frontend_diagnostics.sh`: ASan/UBSan checks for both eyes,
  slider values including zero and above one, near depths from 0.5 to 16,384,
  sampled depths through 100,000, independent weapon settings, invalid near
  inputs, configuration parsing, and existing frontend probe behavior.
- `scripts/tests/test_3ds_stereo_renderer.py`: compiles the actual renderer
  helpers against a fake uniform sink and the real batch visibility module.
  The nearby triangle fails the old 16-pixel bound and survives the corrected
  bound. Mono matrices and unaffected matrix columns remain unchanged.
  Repeated mono/world uniforms are skipped, and all relevant state changes
  produce correct uploads. ASan/UBSan passes.
- `scripts/tests/test_runtime_overlay_frustum.py` and
  `scripts/tests/test_renderer_room_frustum.py`: existing world, runtime,
  eye-space, room-bound, and visibility-cache checks pass.

## Remaining evidence and limitations

The upstream original portal/room visibility publication still uses the mono
camera. A conservative batch frustum cannot recover rooms that were excluded
before renderer submission. Stereo views at doorway edges and room transitions
need dedicated capture comparison; a renderer-only union of per-eye room
visibility may be required. The original gameplay/AI visibility must remain
separate from such a rendering extension.

The relevant ownership boundary is concrete: `ge_original_bg_visibility_run`
in `port/src/ge_original_bg_visibility.c` clears and republishes
`g_BgRoomInfo`, `dword_CODE_bss_8007FFA0`, the portal/global-vis scratch state,
and preload requests. `ge_original_bg_visibility_room_snapshot` exposes those
same room-rendered and neighbor flags to original gameplay consumers.
`ge_dam_visibility_runtime_run_with_providers` currently accepts a view matrix
and symmetric FOV only; it has no independent off-axis projection center.
`update_original_dam_camera` in the 3DS main source also ages dynamic room
residency from this result, while `refresh_stage_glass_shading` skips rooms
outside it. Calling the existing visibility entry point a second time with
another eye would overwrite the canonical gameplay publication. Simply
drawing all resident rooms would bypass original portal/global-vis policy.

A complete extension therefore requires an isolated visibility instance or
tested state save/restore, per-eye camera/projection support, and propagation
of renderer-only room membership to preloading and dependent shading. Tests
must establish byte-identical canonical mono room/gameplay state, preserve
closed-door and global-vis rules, and show newly visible rooms in each eye.

Device testing must cover maximum depth in busy scenes, slider changes,
nearby small props and weapon edges, and both eyes at portal boundaries.
Physical CPU/GPU frame timing, continuous 60 Hz presentation, full original
image parity, and subjective stereo comfort remain unverified here.

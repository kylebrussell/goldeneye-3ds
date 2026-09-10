# Renderer eye visibility and source fidelity — September 10, 2026

This pass implements a renderer room union and repairs demonstrated material
and stereo-layer errors. It does not establish matched N64 image parity,
physical stereo comfort, or sustained hardware frame rate.

## Isolated room interpretation

`ge_renderer_bg_visibility.c` compiles the same extracted `bg.c` function bodies
and bounded adapter into a separate translation unit. Its namespace header
renames every exported interpreter function, room flag, scratch/global-vis
array, camera service and `g_BgPortals`. Function-local static scratch is also
separate by construction. The canonical interpreter is never called a second
time to produce an eye, and there is no save/restore interval in which gameplay
can observe an eye publication. Both eyes execute sequentially using renderer
storage. This API is not reentrant or thread-safe.

The immutable relocated program can be borrowed by either interpreter. Each
run now rebinds portal polygon pointers to its own restored geometry before
ordering portals. This prevents a renderer run from writing through cached
pointers into canonical geometry. Door controls are copied from the current
authoritative provider; the original portal recursion, special portals, depth
limits and global visibility commands remain unchanged. No all-resident-room
fallback was added.

`ge_dam_renderer_visibility_eye_input` accepts the selected local player's
camera, position and room explicitly. For the shader displacement `C/W + K`,
it adds `C/focal_x` to the view's horizontal translation and `K` to the projected
center. It also moves the isolated eye position by the inverse view basis for
portal-side tests. Authored horizontal/vertical off-axis projection centers
are supported; unsupported skew/non-perspective matrices and invalid inputs
are rejected. The main view and projection are unchanged.

`ge_dam_renderer_visibility_run` produces copied membership bits (1 canonical,
2 left, 4 right), a deduplicated active-stereo draw list, and independent preload
membership. Both zero eye shifts copy the original result byte-for-byte and
execute no renderer interpreter. Failure leaves the caller's output untouched;
the platform records failure and retains canonical room membership. It does
not bypass authored rules to conceal a failure. The frame's one slider sample
owns both visibility and shader submission, including slider changes without a
simulation tick.

## Streaming and dependent geometry

Each renderer eye uses a copied preload queue rather than caller callbacks.
New requests are committed only after both eyes succeed. Dynamic room aging
protects the union's visible rooms and the current authored preload requests;
preload membership never becomes draw membership. `begin_visibility` starts a
new request publication before the canonical camera evaluation. Resident
requests are recorded too, so loading a later room cannot evict an earlier
room that the same authored command sequence still requests. A camera change
releases old request protection. Slider-only updates start from the last
canonical request set, avoiding permanent leases on old eye-only rooms.

The private Dam test reproduced repeated preload/evict churn with both the
checkpoint mono path and stereo path when residency aged on every install
frame. Merely draining all installs before aging concealed this behavior.
The request protection addresses that concrete lifetime defect; capacity and
allocation failures still use the existing transactional geometry/texture
commit and abort path.

Canonical AI room snapshots, active lists and `PROPFLAG_ONSCREEN` publication
continue using mono membership. Renderer-only guard eligibility instead copies
the union and uses conservative horizontal sphere bounds; matrix publication,
scene collection and retained attachments use that eligibility. Extra guard
muzzle flashes use a local visual random seed so recovering a stereo draw does
not advance gameplay RNG. This additional-eye visual random pattern is an
explicit approximation, not matched N64 evidence. Existing canonical-visible
draws retain their original random calls.

Glass and windowed-door camera shading use the renderer union. Articulated
props in stereo use the existing resident model input boundary, without
setting canonical visibility flags. Guard batches are stored in eye
coordinates, but belong to the world layer: both initial draw and right-eye
replay now use world disparity and world culling bounds. The independent
first-person weapon/hands layer remains on its configured depth. Coordinate
space alone no longer assigns guards to weapon stereo.

## Tests and evidence

- `bash scripts/test_renderer_eye_visibility.sh`: ASan/UBSan executes the
  original and isolated interpreters together. It verifies canonical storage
  byte equality, zero-shift identity, left/right doorway recovery, unchanged
  caller queue/callbacks, pending geometry requests, repeated closed-door
  controls, authored global room disable, projection algebra and failure
  atomicity. Canonical storage includes pointers, room/portal flags, geometry,
  traversal queues, global-vis commands, camera and service scratch.
- `python3 scripts/tests/test_renderer_eye_integration.py`: actual guard helper
  extraction verifies copied room masks, both horizontal edges, near/vertical
  rejection, mono restore and preservation of canonical flags and PRNG.
- `bash scripts/test_renderer_eye_visibility_assets.sh`: optional private
  original Dam background/pack and authored camera routes, with the unchanged
  Bond camera producer and independent original per-eye interpretation as an
  oracle. The script records concrete route sets, geometry installs, real
  capacity use, and canonical snapshots in the worktree's private build tree.
  These are original-source/asset host results, not original N64 raster captures.
  The recorded run covers 416 camera states, 6,995 canonical-publication/mono
  checks, 13,990 independent original eye comparisons, 2,496 stationary hold
  frames and 54 eye-only geometry installs. It releases preload protection
  after camera moves (66 preload-only eviction events). Peak use is 15,519 of
  65,536 vertices and 51 of 192 textures. Capacity and removed-protection
  negative controls fail as intended. The original `guMtxF2L` TU excludes only
  signed-shift sanitizer checks for its unchanged packed-matrix expression;
  other ASan/UBSan instrumentation remains enabled. Private results are under
  `build/host-tests/renderer-eye-visibility-assets/report.json`.
- Existing stereo, room/batch frustum, room membership, glass/windowed-door
  shading, transactional room streaming and guard ownership tests remain
  applicable. The material change and its original producer-to-PICA tests are
  documented in `Verification20260910ModelEnvironmentAlpha.md`.

The capture comparison tool in `MatchedReferenceCaptures.md` hashes input
artifacts and enforces comparable recorded state, camera, assets and timing.
A port-only pixel self-comparison correctly fails its strict reference gate.
No actual N64 reference images were available in the inspected local evidence,
and no original N64 execution or physical device run was performed here.

## Local multiplayer camera boundary

After canonical per-player simulation, the multiplayer owner selects its local
player slot and publishes that player's full original 320x240 camera. Pass its
camera and mono room result to `ge_dam_renderer_visibility_run`; the eye inputs
must not select a player, advance input, execute AI, or consume canonical RNG.
The platform stores renderer camera/union with `RuntimeDamPreview` and copies
guard membership using `ge_original_stage_guard_runtime_set_renderer_visibility`.
A change of local slot requires a fresh camera/union/guard-matrix publication
before drawing. No network, player allocation or match-rule implementation is
part of this API. Multiple simultaneous local views require separate preview
outputs and sequential renderer interpreter calls.

## Finite remaining acceptance

1. Produce instrumented original N64 and port matched-state capture pairs for
   campaign doorway edges, watch/frontend/results, fades, explosions and glass.
   The comparison tool cannot supply missing execution/state instrumentation.
2. Validate continuous maximum-slider motion and both-eye captures at real
   room/door transitions, including streaming failure/retry and moving props.
3. Resolve RDP compressed-depth/decal tolerance and coverage/blender behavior
   using those pairs. A strict equality decal experiment was previously
   rejected; no guessed global depth bias is introduced by this pass.
4. Close remaining multi-cycle material expressions and dynamic parent-state
   publication described in the material verification note. Per-eye glass
   specular/texture generation currently shares the canonical orientation;
   translating parallel eyes does not change that orientation, but exact
   original/PICA pixel equivalence is unverified.
5. Measure physical New 3DS XL CPU/GPU cost, frame pacing and stereo comfort.
   Host execution, ARM compilation and emulator captures cannot establish
   these physical acceptance checks or radio multiplayer behavior.

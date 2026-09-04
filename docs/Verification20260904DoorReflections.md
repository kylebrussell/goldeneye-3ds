# Retained moving-door reflection publication

Continues `0c008f77`. The previously staged material-only candidate and the
installed `ade136f1` / `ee769251` pair remain untouched.

## Change

The windowed-door renderer now uses the segment-3 matrix index captured when
each original N64 vertex was loaded. Cold scene installation captures these
indices alongside the existing real-matrix geometry traversal; the matrix
template API does not substitute identity matrices. Indices are owned by the
installed door segment (two bytes per flattened door vertex), and survive an
ordinary-prop prefix rebase. Replacement, failure, empty scene and teardown
release the corresponding storage.

After a moving-door cache publication, indices are copied from the exact
immutable topology component that produced its geometry. Per-part vertex and
batch counts must still match the installed ownership ranges. A changed layout
is rejected rather than allowing a part to use another door's indices.

Visible windowed-door batches compose their original published matrix with
the original camera view, quantize the normal matrix to s15.16 and reuse the
existing original GlobalLight/LookAt and portable GBI shading path. Articulated
children do not fall back to matrix zero. The static-glass API still rejects
doors, so accidentally passing a door's stationary object matrix is not an
alternative path.

Shade/ST updates keep the retained door segment and both scene publications
coherent. World positions and clip coordinates remain unchanged. The update
does not allocate or decode geometry; GPU color/UV uploads occur only for
changed batches. The original portal-visible room set gates shading, without
altering original door/gameplay ticks. Existing live calculated opacity,
blend/depth state and fractional-alpha textures are preserved.

## Verification

- `scripts/test_stage_model_publication.sh`: ASan/UBSan pass, recorded in
  `build/host-tests/door-reflection-focused.log`. Real model display lists with
  native multi-matrix instances produce byte-identical geometry/materials
  with and without index capture. Captured indices also match the subsequent
  cached topology. PwindowZ normals, original LookAt generation and original
  matrix composition are tested over 32 frames and all 16 supported door
  matrix slots. These rotating matrices are explicit test fixtures, not an
  authored campaign door placement or an emulator result.
- `scripts/tests/test_windowed_door_shading.py`: ASan/UBSan pass. Executes the
  actual runtime shading loop with explicit snapshot/shading/upload seams;
  256 rebases cover per-vertex index selection, all three vertex publications,
  unchanged-view upload suppression, visibility gating and invalid ranges.
  The original shading/math boundary is covered separately above. Added to
  the full host suite.
- Full host suite: `build/host-tests/door-reflection-full.log`, exit 0.
  The focused tests were rerun after the final defensive segment-range check.
- Final ARM/3DSX build: `build/host-tests/arm-door-reflection-final.log`, exit 0.
- `git diff --check` clean.

Candidate executable SHA256:
`3490fbf064ef2120db9646f67ccce6933c1d34309d2708ed87183744f1a95675`.

Unchanged asset pack SHA256:
`ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`.

Candidate directory: `build/3ds-candidates/door-reflection-3490fbf0`.
No assets or ROM content are committed. No installed test overrides or saves
were changed.

## Remaining / next validation

Two Azahar access attempts found macOS locked. No new visual, frame-rate or
interactive-playability claim follows from these host tests. The installed
executable still hashes to
`ade136f124c47c7f62fd8de245bdf85440d18d47237a5b2246253d3883b3c83b`.

Once unlocked, deploy the matching candidate pair through the existing safe
staging workflow; recheck both Dam tower views documented in
`Verification20260904Reflections.md`, then a genuine authored windowed door
while opening/closing. Repeat the identical movement probe without concurrent
builds, then profile sustained combat rather than treating the opening-room
750-frame probe as evidence of locked 60 FPS. Preserve saves and remove only
the temporary test overrides after recording results.

Full parent fog/damage/fade and exact RDP mip/detail blending remain separate
renderer gaps. Sustained combat frame tails, hardware 60 FPS and complete
mission/menu end-to-end playability remain unproven. This checkpoint closes
the previously missing moving-door matrix/LookAt publication boundary, not
the overall fidelity/performance goal.

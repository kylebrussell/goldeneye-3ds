# Dam original movement/collision slice

This note records the decompiled call graph that should be ported for Dam
movement. A replacement circle sweep or axis-slide controller is deliberately
out of scope: GoldenEye already contains its own line traversal, radius locus,
fractional movement, edge slide, and endpoint-hop behavior.

## Spawn evidence and coordinate space

The normal-play `INTROTYPE_SPAWN` record in `assets/obseg/setup/UsetupdamZ.c`
selects setup pad 33. Its authored values are:

- position `(4719, -18, 3949)`
- look `(-1, 0, -0.000643)`
- STAN name `p6g1`

`stanPackId("p6g1")` produces high word `0x0006` and low byte `0x31`.
That uniquely resolves to Dam STAN source tile 171, room 135. The tile's
vertices are `(4685,-25,4054)`, `(4747,-25,3914)`, and
`(4682,-25,3865)`, so the authored floor at the pad is exactly `-25`.

The original runtime does **not** leave the player in this authored coordinate
space. `proplvreset2` multiplies every setup pad position by
`get_room_data_float2()`, which is `1 / levelscale`. Dam's level scale from
`levelinfotable` is `0.23363999`. Therefore the original runtime spawn is
approximately `(20197.741, -77.042, 16902.072)`, with floor
`-107.002`. Rendering and camera code must either use this runtime space or
apply the inverse conversion at the renderer boundary.

## Original spawn chain

The relevant original chain is:

1. `load_bg_file` in `src/game/bg.c`
   - loads the complete STAN file
   - calls `stanDetermineEOF`, `stanLoadFile`, and
     `setLevelScale(0.23363999f)`
2. `proplvreset2` in `src/game/prop.c`
   - rebases setup pointers
   - scales pad positions by `1 / levelscale`
   - calls `init_pathtable_something`
3. `init_pathtable_something` in `src/game/initpathtablesomething.c`
   - calls `stanMatchTileName`
   - validates/falls back with
     `isPointInsideTriStandTileUnscaled_Maybe`, `sub_GAME_7F0AFB78`, and
     `walkTilesBetweenPoints_NoCallback`
4. intro parsing in `src/game/bondview_r.c`
   - stores pad 33 in `g_Startpad[0]`
   - obtains the floor through `bondviewYPositionRelated` ->
     `stanGetPositionYValue`
   - derives yaw from the pad look vector
   - initializes `field_488` through `change_player_pos_to_target`

## Original walking collision chain

The smallest useful entry point after input has been converted to a world
offset is `bondviewCalcUpdatePlayerCollision` in
`src/game/bondview2.c`. It preserves GoldenEye's exact fallback order:

1. `bondviewTrySimpleMovePlayerCollision`
2. `bondviewTryFractionMovePlayerCollision`
   - uses `calculateRayToSegmentIntersectionNormalized`
3. `bondviewTryEdgeMovePlayerCollision`
4. `bondviewTryEndHopPlayerCollision`

All four paths call `bondviewTryMoveToStan`, which performs:

- `stanTileDistanceRelated` for nearby special/force-crouch tiles
- `stanTestLineUnobstructed` for linked-tile traversal and dynamic props
- `stanTestVolume` for Bond's collision radius
- `stanTestLocusEdgeAboveY` for tall blocking edges

The STAN functions above live in `src/game/stan.c`. Their transitive geometry
slice includes:

- `sub_GAME_7F0B0914`, `sub_GAME_7F0B07BC`,
  `walkTilesBetweenPoints_NoCallback`, and `sub_GAME_7F0B0C24`
- `sub_GAME_7F0B1DDC`, `sub_GAME_7F0B21B0`, and their locus callbacks
- `getShortest2dDispToInfTileEdge`, `distToTilePnt2D`,
  `stanPointProjectsOntoTileEdge`, and the segment intersection helpers
- `stanResetHits`, `getCollisionEdge_maybe`, and `getTileEdgePoints`
- `stanTestPointWithinTileBoundsMaybe` and `stanGetPositionYValue`

`MoveBond` is the later, larger fidelity step. It calls
`bondviewProcessInput`, applies original acceleration/crouch/head-bob offsets,
calls `bondviewCalcUpdatePlayerCollision`, handles slope/step adjustment via
`stanGetMoveBondCollisionTiles`, and finally calls `bondviewUpdatePlayerY`.

## Required original globals/adapters

The isolated player collision slice reads or writes at least:

- `g_CurrentPlayer` and `g_playerPointers`
- `g_CurrentPlayer->field_488` (`collision_position`, `collision_radius`, and
  current STAN pointers), `bondprevpos`, `autocrouchpos`,
  `ducking_height_offset`, `stanHeight`, `field_70`, and `registeredroom`
- `obj_collision_flag`
- `g_WorldTankProp`, `g_PlayerTankProp`, `g_PlayerIsInTank`,
  `g_EnterTankAudioState`, `g_BondCanEnterTank`, and `g_PlayerTankYOffset`
- `g_GlobalTimerDelta`, `g_ClockTimer`, and the STAN saved-collision globals
- `level_scale`, `inv_level_scale`, `standTileStart`, `stan_prefix`,
  `firststaninroom`, and `g_StanRoomBounds`

For a first on-foot Dam slice, tank globals can remain in their original
inactive state. Dynamic object collision must remain behind the original
`cdtypes` path; it can initially receive an empty prop provider, then be wired
to doors, guards, and objects as those original systems come online.

The original side-effect calls needing narrow platform providers are
`sub_GAME_7F03D058` (temporarily enable/disable props), `roomGetProps`,
`propIsOfCdType`, `chraiGetCollisionBounds`, player room registration, and
`objectivestatusCheckRoomEntered`.

## Current adapter and native original-code slice

`scripts/extract_3ds_dam_collision.py` validates the decompiled Dam STAN hash
and emits all 2,755 tiles and 8,366 points into a byte-order-stable blob. It
preserves source tile indices, IDs, rooms, original `mid`/`tail` words, and
the original point links. `ge_stan_collision_open`, tile/point accessors, and
`ge_stan_collision_ground` provide a zero-copy validation/query boundary.
The spawn test proves tile 171 and floor `-25`; it is not a replacement player
controller.

`ge_stan_native_materialize` now rebuilds the complete contiguous native tile
arena. It preserves the original eight-byte tile header/point stride, keeps
all source links unchanged, validates every nonzero link against a tile start,
places the first tile 0x80 bytes after `standTileStart`, and appends the
original zero terminator. The complete Dam arena is 89,104 bytes; setup pad 33
materializes at base offset `0x1618`.

With `GE_PORT_STAN_GEOMETRY_SLICE`, the ARM11 toolchain and host tests compile
the actual implementations in `src/game/stan.c` for `stanMatchTileName`,
`stanGetPositionYValue`, `stanTestPointWithinTileBoundsMaybe`,
`walkTilesBetweenPoints_NoCallback`, and the original circle traversal pair
`sub_GAME_7F0B1DDC`/`sub_GAME_7F0B20D0`. The portable wrapper only binds
`stan_prefix`, `standTileStart`, and level scale to the materialized arena. A
little-endian comparison guard replaces `stanMatchTileName`'s original byte
alias, point-count reads use the numeric high tail nibble, and a pointer-width
adapter preserves the original link-base calculation on 64-bit hosts. Tests
resolve `p6g1`, reproduce the runtime floor `-25 / 0.23363999`, traverse an
authored linked edge, reject an authored zero-link edge, accept a radius in the
spawn-tile interior, traverse a radius touching a linked edge, and report a
collision for a radius touching the zero-link wall.

The ARM build also compiles bounded decompiled setup/intro slices directly
from `src/game/prop.c` and `src/game/bondview_r.c`. The deliberately explicit
symbols `proplvreset2PadSlice` and `bondviewLoadSetupIntroSpawnSlice` consume
the exact generated `UsetupdamZ` data. The first applies the original pad
scale-then-STAN-association loop to all Dam pads; the second walks the original
intro record stream, filters demo slots, selects the first single-player spawn,
samples its STAN floor, and derives the original look angle. In Azahar this
selects pad 33 / `p6g1` at runtime coordinates approximately
`(20197.7, 68.0, 16902.1)` with yaw `450.0` degrees (the original unnormalized
result). These are not the complete original functions: setup-bank pointer
rebasing, props/guards, inventory/ammo, intro cameras, watch state, player/prop
allocation, and room registration remain outside the bounded slices.

Remaining blockers are:

- the rest of the original loaded STAN/setup formats store 32-bit pointers and
  assume N64 byte order; pointer-bearing boundaries still need native
  materialization rather than direct casting
- `StandTile.id` is a 24-bit implementation-defined bitfield, while
  `stanMatchTileName` also reads its bytes through `StandTilePoint`
- many source routines cast pointers through `u32`/`s32`; valid on ARM11 but
  invalid in 64-bit host tests
- the locus layer's 64-byte clear and mismatched N64 soft-float callbacks are
  now contained by a typed compatibility record and exact ARM hard-float
  thunks; `stanTileDistanceRelated` and `stanTestLocusEdgeAboveY` run against
  authored normal, force-crouch, linked, and tall-edge cases
- the current `GE_PORT_BOND_MOVEMENT_SLICE` contains the exact collision
  family and normal root-motion consumer, while the full input body still
  reaches tank, audio, HUD, weapon, watch, and animation state that needs
  typed provider closure

The exact static-STAN portions of `stanTestLineUnobstructed` and
`stanTestVolume` now also run and reject authored Dam zero-link walls. Their
public wrappers deliberately fix `cdtypes` to zero, while direct nonzero calls
fail conservatively until the dynamic-prop collision provider is connected.
The exact `bondviewTryMoveToStan`, simple/fraction/edge/end-hop fallbacks, and
`bondviewCalcUpdatePlayerCollision` are now compiled from `src/game/bondview2.c` and operate on the real spawned
`struct player`. A diagnostic crosses an authored linked edge, rejects an authored zero-link wall, exercises the
fallback path, and verifies that the final position belongs to the final tile. The movement ABI also preserves the
original 64-byte `stanTileDistanceRelated` locus clear rather than exposing its misleading source-level element
type on the platform boundary.

The remaining movement milestone is not another controller. Original input must reach `MoveBond` through
`bondhead.c` and the model-animation root displacement that produces its normal world offset. Canonical
`bheadAdjustAnimation`, `modelSetAnimation`, the original animation loop/end/speed setters, and `modelTickAnim`
now compile as bounded host and ARM source slices. The authentic embedded four-node player gait
`ModelFileHeader`, skeleton, rwdata, and 16-joint group graph are materialized on the same `Model` overlay used by
the spawned original player. Exact `modelInit`, `animInit`, `modelTickAnim`, `subcalcpos`, and
`subcalcmatrices` consume the packaged walk/sprint frame records and write the authored final group transform to
`bondheadmatrices[0]`; sanitizer tests pin finite, deterministic walk and sprint matrices. The typed animation
lookup used by exact `bheadAdjustAnimation` resolves those same two native animation ABIs.

This path is initialized and checked at stage startup, but is not driven by the live gameplay tick yet. Exact
secondary-frame loading and canonical quaternion slerp now preserve the `anim2`/`unk84` 12-tick walk/sprint
crossfade, with sanitizer fixtures at ticks 0, 1, 6, and 12. The complete `bondviewProcessInput` body still needs
its normal single-player providers connected to the current player. Dynamic prop collision also remains a
typed provider currently returning `cdtypes=0`; it must be connected as doors, objects, and characters enter the
original prop lifecycle. Only after those exact producers are live should vertical smoothing and player/head
camera state be advanced in the runtime tick.

A bounded exact `bheadUpdatePos` slice and the normal on-foot `MoveBond` head-position-to-heading-to-collision
consumer are now compiled and host-tested. The first NTSC root sample correctly ramps to `0.07` through the
original `0.93` damper before entering the exact STAN collision family. The older isolated diagnostic requires a
typed `sample_head_root_velocity` provider and returns without movement when it is absent; it never substitutes
Circle Pad values for animation displacement. The current-player post-input adapter now supplies that displacement
directly from the original gait matrix and bhead scaling. It still must be invoked after `bondviewProcessInput`
publishes speed/turn state before the chain enters the live runtime tick. The exact external-state closure is catalogued in
`docs/DamOriginalInputAnimationClosure.md`; integer-frame selection in a callback is explicitly not an acceptable
substitute for the original model clock and matrix interpolation.

The post-input boundary is now closed and host-tested separately from controller sampling. Given only the original
clock/delta and `speedforwards`, `speedsideways`, and `speedtheta` already written on the current player, it retains
MoveBond's max-speed selection, exact `bheadAdjustAnimation`, the model clock/crossfade/matrix path, and bhead's
matrix-0 forward/sideways scaling and head amplitude. Its resulting velocity enters the existing exact
`bheadUpdatePos` damper and normal MoveBond/STAN collision consumer. A 12-tick sanitizer test moves the spawned
player against authored Dam STAN while completing the walk-to-sprint crossfade. The remaining runtime step is to
invoke this adapter immediately after the exact input body publishes those speed fields; it does not read controls
itself.

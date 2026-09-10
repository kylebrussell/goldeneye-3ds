# Native background collision and breakable lights

`ge_original_bg_geometry` owns all authored converted room point tables and
primary/secondary display lists for a stage. It does not borrow renderer
residency, visibility's fixed 0x50 room table, or caller input buffers.

The generated slice retains the original `bgBuildRoomVtxBounds`, room ray
intersection, bullet background hit, ray/bbox and double-precision triangle
intersection algorithms. It also retains the complete original light fixture
registration, darkening, proximity and shard-spawning closure. Its external
`glassCreateShard` call links to the canonical explosion/glass owner. The focused
host harness records that call boundary; it does not claim to test the complete
glass simulation or hardware rendering.

## Native representation

- `s_room_info` contains actual native pointers to persistent geometry and
  original per-VTX batch bounds. `ge_original_bg_geometry_native_rooms()` and
  `ge_original_bg_geometry_room_count()` supply the held-item bbox closure.
- Authored Gfx commands retain their big-endian eight-byte layout. All CPU
  accesses use explicit byte reads and eight-byte steps; host `sizeof(Gfx)` is
  never used to traverse these streams.
- Native `Vertex` includes pointer unions and is larger than GPU `Vtx` on a
  64-bit host. Separate native collision Vertex and 16-byte Vtx arrays prevent
  array-stride aliasing. Collision outputs retain real native vertex pointers.
- Original signed distance overflow is represented with explicit 32-bit
  wrapping arithmetic. Repeated-index zero-area TRI4 padding is rejected before
  forming invalid cache-offset pointers. The original Archives 0x4fd exclusion
  remains; an all-filtered hit returns false without publishing the original
  function's otherwise uninitialized output. Unselected maximum-score results
  likewise cannot publish uninitialized native pointers.
- The original light code's raw indices differ from the background code's
  `vtxoff` subtraction. Both exact access patterns are validated before any
  light callback can run, including the light loop's four TRI4 slots.

Room texture provenance comes from the authored Rare texture command. The
original `texHandleType0..4` load order leaves the base texture as the final
image. Final tile format/depth comes from the retained original
`texHandleType0..4` / `texTrySetTileState` emission closure. It emits native Gfx
commands and records the last actual G_SETTILE, including the palette-load tile
when a cached render-tile command is suppressed. Each room list resets the cache
at the same boundary as `texLoadFromGdl`; the two water texture IDs preserve its
additional cache reset. The optional original debug texture editor remains off
at this immutable authored-stage bootstrap boundary. The checked-in format table was generated
from all 2,698 original US image headers, using `texInflateZlib`'s eight-bit
format or `texInflateNonZlib`'s first four-bit format and the exact original
`g_TexFormatGbiMappings` / `g_TexFormatDepths` / `g_TexFormatLutModes`.
Original dimensions and explicit LODs come from the existing texture conversion
catalog, verified against every source SHA-256 and each original base image
header. Implicit paletted mip counts preserve the original 0x800-byte ceiling.
No pixel data is included. Opaque image addresses are used only inside the
metadata emitter and never dereferenced or submitted to a renderer.

The parser rejects unsupported list calls, matrices, expanded RDP image/tile
commands without this provenance, invalid Rare IDs/types, malformed GBI,
unloaded triangle cache indices and invalid native light accesses. All stage
descriptors tested below use the supported original authored streams. Resource
bounds are 139 rooms, 32 MiB owned buffers, 32,767 commands per list, the original
100 fixture slots and original 512-entry circular darkened-vertex table.

For each nondegenerate triangle, the persistent GBI cache's point identity must
equal the original collision algorithm's latest-VTX base plus cache slot minus
VTX offset. A partial cache reload that violates this is rejected atomically;
otherwise rendering and collision could use different triangles. Repeated-index
padding retains the original zero-area exception. Invalid ray arguments reject
only that query and leave the bound geometry usable.

## Integration contract

1. After stage asset resolution and before native object/player constructors,
   load with `ge_original_bg_geometry_load(pack, stage, world, room_bounds,
   &report)`. Require success for every gameplay stage. Log stage key,
   `report.status` and `report.failed_room` on failure.
2. Bind the owner and pass it with `ge_original_bg_geometry_light_break` to
   `ge_original_player_held_items_bind_lights`. The callback uses only a live
   primary-list triangle token returned by the bound geometry. Require the
   native owner before beginning a held-item tick.
3. After the local room scene is imported/refreshed, call
   `ge_original_bg_geometry_apply_scene` with the room batch prefix, excluding
   overlays. It validates immutable point provenance, copies persistent native
   colours through the existing exact GBI processing path, and reports changed
   vertices. Batch ranges must be increasing and nonoverlapping, as emitted by
   the room scene builder, and each world position must equal its source point
   plus this owner's room origin. Validation precedes all vertex writes, so
   aliased ranges and scenes retained from a different origin are rejected
   without partial publication. Upload that changed room prefix using the renderer's existing
   vertex conversion. Apply at the local scene boundary; canonical collision
   and light state never depend on eye draws or renderer loads.
4. Unbind the held-light callback and geometry, then close the owner after
   native stage object cleanup. Returned hit tokens expire at close.

The scheduler support extractor no longer defines the old background ray and
bullet functions or aliases the native `s_room_info` to visibility's pointerless
table. The native geometry wrapper supplies `bgTestBulletHitBackground`.
Main, held-item extraction and Makefile hooks are integrated by their owners.

## Verification

`bash scripts/test_bg_geometry.sh [complete-original-asset-pack.gepack]` runs
native and short-enum builds with AddressSanitizer and UndefinedBehaviorSanitizer.
Focused coverage includes native pointers above 4 GiB, original light distance
bias, TRI4 subtriangle identity, Archives filtering, exact vertex darkening,
original shard calls, repeat-hit idempotence, renderer reload persistence,
invalid hit tokens, malformed scene provenance, failed-construction rollback,
and the palette-tile/cache regression. Every nonzero-area primary triangle
centroid must produce a hit; only the original Archives 0x4fd filtered texture
is exempt. No other real-asset centroid exceptions were needed.

The complete coordinator pack exercised all 21 solo stages and all 11 MP arena
descriptors: all authored rooms load and publish bounds, centroid rays traverse
actual triangle commands, all primary light triangles call the original light
body, and every room's GBI scene passes provenance validation. Both ABI builds
pass. Peak fixture usage is Bunker2's 64/100. Peak owned buffer bytes are Silo's
3,412,296; MP peak is Facility's 1,996,632. These are host buffer measurements,
not device memory or frame-time measurements. Both runtime and generated slice
also compile with the actual devkitARM compiler and warnings treated as errors.

`bash scripts/test_dam_guard_bg_collision.sh` still passes its retained original
background/STAN focused harness and now verifies the scheduler yields collision
ownership to this module. Regenerating texture metadata from the original split
images and verified conversion catalog reproduces the checked-in table
byte-for-byte. `bash scripts/test_bg_geometry_room_coverage.sh PACK` proves that
a private mutation disabling all room2 collision fails the real-asset test in
both native and short-enum builds; production source is untouched.
## Native room-origin access

`ge_original_bg_geometry_room_origin(int32_t room)` returns a checked
`const struct coord3d *` into the currently bound owner's `origins` array.
It includes void room zero and rejects missing/invalid owners, out-of-range
room IDs and nonfinite coordinates. Values remain in the authored room-origin
units used by original `getRoomPositionByIndex`; no level-scale conversion is
applied. The pointer is borrowed, read-only, and must not survive rebind/close.
Invalid queries neither poison the owner nor fabricate a fallback room table.

The original bullet-impact/scorch/explosion/glass consumers read this origin
to convert world coordinates to room coordinates. Their retained legacy
`coord3d *getRoomPositionByIndex(s32)` signature can adapt to this accessor
under `GE_PORT_NATIVE_BG_GEOMETRY`, casting away const only for those existing
read-only consumers. Extractor/caller adaptation is separate runtime ownership.
Focused tests verify exact address/units, void/range/status checks, nonfinite
query isolation, binding changes and close. The real asset test checks every
authored room against its parsed original origin across all 32 stage descriptors.

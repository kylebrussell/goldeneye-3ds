# Facility exact-data pipeline

Facility is called `ark` by the original data. The next-level pipeline packages
that authored data without selecting the level in the 3DS runtime or adding a
parallel Facility implementation.

Run:

```sh
python3 scripts/extract_3ds_facility_level.py
python3 scripts/tests/test_extract_3ds_facility_level.py
```

The deterministic output is `build/3ds-levels/facility/`:

- `background.bin`: the exact `bg_ark_all_p.bin`, including its room, portal,
  and global visibility tables;
- `rooms/`: all inflated point and display-list streams plus per-stream hashes;
- `collision/collision.gestan`: all authored `Tbg_ark_all_p_stanZ` polygons in
  the existing validated portable STAN container;
- `collision/setup.bin`: the exact compiled `UsetuparkZ` payload;
- `collision/setup.json`: the setup identity, all four intro spawn choices, and
  the canonical normal spawn resolved to its STAN polygon and room;
- `room_bounds.gebounds`: room AABBs expanded by the authored portal polygons;
- `manifest.json`: hashes and cross-component counts for the complete bundle.

For the US data currently validated, Facility contains 78 indexed rooms, 109
portals, 101 global-visibility records, 2,599 STAN polygons, and 7,908 STAN
points. The normal intro command selects setup pad 167 (`p1682a1`) in room 13.
Those values are all derived from the decompiled setup/background/STAN data and
are asserted by the host test.

`scripts/build_3ds_assets.sh` includes this directory under
`converted/levels/facility/`. Existing Dam room files and manifests remain
byte-identical; the shared room extractor retains its Dam defaults and only
accepts stage metadata when called by the Facility pipeline.

## Runtime integration boundary

`ge_stage_assets.h` now provides exact Dam and Facility asset descriptors,
bounded room-stream path construction, and a resolver that opens the original
background and STAN through the existing parsers while verifying stage counts
and the canonical collision spawn. `GeDamDynamicScene` and
`GeDamVisibilityRuntime` have stage-aware entry points; their original entry
points remain Dam-selecting compatibility wrappers.

The platform loader selects a descriptor and uses its background, STAN,
room-stream, bounds/visibility, and dynamic-scene paths. An optional registered
runtime key such as `facility`, `runway`, or `egyptian` in
`sdmc:/3ds/goldeneye-3ds/stage.cfg` selects that exact descriptor and original
`LEVELID`; a missing or unrecognized file retains Dam. This temporary platform
bring-up selector does not replace the original game menu.

`ge_original_stage_setup.h` provides the next canonical boundary. It consumes
the packaged, byte-exact setup payload and materializes native versions of the
pointer-bearing pad, bound-pad, intro, waypoint, waypoint-group, patrol-path,
AI-list, and heterogeneous prop-definition tables. The native pad tables retain the original trailing
`plink == NULL` sentinel required by `proplvreset2PadSlice`, while their public
counts exclude it. AI records continue to point into the owned exact setup
blob. Prop definitions retain the exact 32-bit command ABI in native byte order
and expose a pointer-width-safe record graph for tags, switches/links, linked
doors, locks/safes/renames, and objective relationships. Authored model, pad,
guard, and AI IDs remain attached to each record. Host sanitizer coverage binds every
Facility pad and bound pad to the native STAN map and proves that normal intro
pad 167 resolves to `p1682a1`, room 13, STAN `0x69201`, and floor height 272.
The same loader also validates Dam so this is a stage-generic boundary rather
than a Facility-only replacement.

`ge_original_stage_prop_materializer.h` classifies that graph by the exact
canonical constructor it requires and dispatches only records for which both
the constructor service and authored model are available. It never silently
constructs a special or embedded branch. The host sanitizer audit covers all
20 packaged stages. Facility's 524 commands divide into 118 control/mission
records, 109 ordinary default objects, 46 doors, 65 guards, 118 weapon/item
records, and 68 special objects. With only the already-bounded ordinary
`domakedefaultobj` capability enabled, the test dispatches all 109 eligible
Facility prop/glass records in authored order and reports every other record
as an explicit service or special-branch dependency. A model-denial pass also
proves that all 14 Facility default objects using model 20 remain blocked and
are not sent to construction.

Facility now publishes that setup through the unchanged setup-pad and intro
spawn bodies, then enters the same original player movement, STAN, portal
visibility, room streaming, and camera path as Dam. The remaining live
boundary is:

1. Feed the now-relocated prop-definition stream/graph through the canonical
   object constructors using the shared classification/dispatch boundary,
   closing Facility's model dependencies as each object class becomes live.
   Runtime prop/model pointers begin null as they do in the original setup and
   belong to those constructors.
2. Enable Facility's exact AI lists only when the general original character,
   object, navigation, and service dependencies are available; the waypoint,
   group, patrol, and AI pointer tables themselves are already native.

The next activation step is therefore to materialize `UsetuparkZ` objects and
activate their existing AI records as the canonical object dependencies are
closed. Until then Facility is a live, navigable authored world, but not yet a
playable mission: its props, guards, objectives, weapons, and mission script
remain intentionally gated.

## Stage streaming capacity

`scripts/probe_stage_stream.sh` resolves either stage through the shared
descriptor, takes the first ten rooms from the original background portal BFS,
and feeds them through the exact dynamic room decoder. Extra room IDs can be
supplied to grow the same resident scene transactionally:

```sh
./scripts/probe_stage_stream.sh facility
./scripts/probe_stage_stream.sh facility --all-connected
./scripts/probe_stage_stream.sh dam
```

Facility's spawn-room BFS begins `13, 15, 14, 12, 9, 8, 5, 6, 7, 10`; its
full connected component contains 75 rooms. Installing all 75 succeeds within
the current shared limits and reaches 37,731 vertices, 3,566 batches, and 80
unique Rare textures. The initial ten-room Facility publication is 5,820
vertices, 547 batches, and 26 textures. Thus Facility itself does not require
a separate renderer capacity or an eviction exception before activation.

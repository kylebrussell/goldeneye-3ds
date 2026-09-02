# Nintendo 3DS native port

Status note: much of this document records early bring-up milestones and is
not a current missing-systems checklist. For the latest live integrations,
emulator evidence, audio setup, exact artifacts, and remaining blockers, see
[the September 2 verification checkpoint](Verification20260902.md).

This branch keeps the original byte-matching Nintendo 64 build intact and adds a separate ARM11 target under
`platform/3ds`. The current milestone is a bootable native `.3dsx`, not a playable game yet. It proves the 3DS
application lifecycle, PICA200 rendering, input, timing, and the boundary where decompiled game systems enter.
The current build now executes the original `frametiming.c` and `quaternion.c` on ARM11, plus a portable C version
of the original MIPS assembly random-number generator. C-stick input advances that diagnostic orientation state,
which is reported on the bottom screen. The level renderer separately consumes the exact decompiled
`bondviewUpdateCameraMatrices` output; original `bondhead` input/root motion is the missing connection between them.
The complete untouched `src/joy.c` is now linked into the ARM11 runtime as well. The native input bridge presents
the Circle Pad and C-stick as two virtual N64 controllers, feeds the original 20-sample playback ring once per
logical tick, and reads back GoldenEye's own button-edge, mask, clamp, and range-normalization results. Logical 3DS
actions are provisionally mapped to N64 buttons (`Z` fire, `B` use/reload, `A` next weapon, `START` pause); the
mapping will be finalized when the original player-control path is connected.
A source-compatible `boss.c` stage-state slice is now live as well: the original title request, deferred stage
selection, stage getter and debug/memory flag state execute on ARM11 behind a small snapshot adapter. A bounded
`lvlManageMpGame` stage-tick slice now runs once per portable tick. It retains the original clock, idle latch,
stage-time branches, title/world split, and world-subsystem order. The game subsystems named by that order are
still provider boundaries; the 3DS runtime has not yet linked player, mission, AI, prop, weapon, or music ticks.
The runtime can also mount a deterministic asset pack generated locally from the user's extracted ROM data.
The offline conversion path now decodes every extracted texture and emits one PICA200-native Tex3DS resource per
LOD, plus deterministic JSON and zero-copy binary catalogs. The running renderer loads `COPYICON` through a
bounded demand-loaded cache, uploads it to PICA200, and retains deterministic LRU/byte-budget accounting.

The scheduler compatibility layer now exposes the libultra queue, event, timer, VI, thread/task, cache, RSP, and
RDP calls used by the game over portable service primitives. The 3DS frontend pumps a periodic
`OSTimer`/`OSMesgQueue` at runtime. An isolated compatibility build compiles the untouched original `src/sched.c`
on host and ARM11. Sanitizer tests now execute a deterministic full scheduler cycle: VI retrace, command dequeue,
the original task selection and dispatch paths, explicit SP/DP completion, and the task-done message. The retrace
client notification and bounded cooperative scheduler-thread step are covered as well.
The audio boundary has an allocation-free interleaved stereo PCM ring, deterministic zero-filled underflow,
overflow accounting, a saturating Q15 stereo mixer, and libultra `osAi*` compatibility calls. A separate smoke
build compiles and runs the untouched original audio event-queue, sample-copy, and pitch-ratio sources. The 3DS
frontend now owns four fixed stereo PCM16 `ndspWaveBuf`s, refills only completed buffers from the AI ring, flushes
DSP-visible cache lines, follows runtime AI sample-rate changes, and shuts NDSP down before freeing linear memory.
The original libaudio main-bus, auxiliary-bus, and save filters now emit a tested 15-command ABI list whose
clear/load/save/move/mix/interleave subset executes in a bounded CPU interpreter and reaches the same PCM ring. The
interpreter now also implements ABI1 codebook loading, loop state, 4-bit ADPCM decode and fixed-point resampling,
including init/continuation history, segmented state addresses and atomic error handling. The ARM11 startup check
executes a deterministic ADPCM vector. `ENVMIXER`, `SETVOL`, and `POLEF` remain the explicit unsupported ABI1
frontier, so the live game path still supplies underrun-safe silence rather than complete game audio.
The graphics path parses classic Fast3D plus GoldenEye's Rare-specific `TRI4` and `SETTEX`, resolves bounded
segmented addresses, follows nested and branched display lists, fetches classic vertices, shadows RSP/RDP render
state, decodes N64 split signed 16.16 matrices, maintains independent bounded model-view/projection stacks, and
emits typed renderer actions. It now decodes live segment remapping, viewport, light, look-at, fog, perspective
normalization and vertex transforms into a parallel processed-vertex cache with clip flags, lighting and texgen.
An allocation-free homogeneous clipper now handles positive-W and all six frustum planes, interpolates position,
normal, UV and RGBA attributes, and fan-triangulates bounded output. It is host/sanitizer/ARM11 tested and is in
the live Dam draw path, consuming the original Bond view/projection handoff and preserving material batches.
A bounded material translator maps common one-cycle shade/texture/primitive combiners, culling, wrapping,
filtering, depth, alpha, blend, fog and lighting state into a platform-neutral PICA description. A Citro3D adapter
now applies that description to TexEnv, cull, depth, alpha and blend state for live draws. Every approximated N64
feature is exposed through granular fallback flags; the Rareware body uses an explicit vertex-shade visibility
override until its missing original texture binding is recovered.
The large pipeline and traversal workspaces are heap-backed, and the 3DS frontend reserves a 64 KiB app stack;
this fixed the early ARM11 `NoExecuteFault` caused by the former roughly 28 KiB nested GBI stack path.

The current crosshair is generated through that GBI-to-PICA path rather than from hand-authored PICA triangles.
The private Rareware-logo segment, rotating body, `PROP_BLOTTER1`, and the original room-1 probe remain useful
renderer and asset-loader diagnostics, but they are no longer composited as unrelated showcase objects on the
default top screen. The default view now draws a source-derived ten-room chunk reached from the real Dam mission
start through the original background connectivity routine.

## Dam mission-opening cluster

This cluster is selected from original mission data, not by choosing visually interesting assets. Intro spawn
record 7 for normal play selects setup pad 33, whose STAN name is `p6g1`. That STAN tile maps the spawn to room
135. The exact decompiled `bgGetConnectedRooms` breadth-first traversal of the complete background portal table
selects the current ten-room load budget:
`[135, 133, 134, 132, 136, 124, 125, 126, 127, 128]`. The first internal portal edges are:

- portal 0: rooms 134 and 133
- portal 1: rooms 135 and 133
- portal 2: rooms 132 and 133
- portals 3, 4, and 5: rooms 132 and 136
- portal 6: rooms 132 and 124

The parsed private background contains all 137 rooms and 194 portals. The current ten-room frontier continues
through portal 7 from room 124 to room 125. Tests verify the exact room order plus a combined ten-room scene of
17 display lists, 1,603 commands, 3,043 triangles, 9,129 vertices, and 28 referenced textures.
`scripts/analyze_dam_mission_start.py` writes deterministic evidence used to audit the spawn mapping.
The original-connectivity-driven ten-room set remains the initial asset preload budget. It is no longer used as
the visibility decision. The asset pack now contains bounds derived from all 136 authored room meshes and every
portal polygon. Each live camera update passes those bounds, all 194 portals, and the exact
`bondviewUpdateCameraMatrices` result into the decompiled `bgDetermineVisibleRooms` recursion; the renderer submits
only batches belonging to its result. At the Dam spawn this is
`[135, 133, 134, 132, 124, 125]` after nine descents.

The background's big-endian global-visibility stream is also materialized into its native typed ABI before the
original interpreter runs: all 389 command units are validated, including seven segmented portal operands mapped
to their exact portal indices. The stream's original preload commands and current portal `controlbytes1` have
typed provider boundaries. The live runtime currently retains authored initial portal controls and a no-op room
preloader; the original door subsystem and a dynamic room-cache provider must publish those mutable values before
opening doors can affect visibility and preload policy.

`scripts/extract_3ds_dam_rooms.py` generalizes the earlier room-1 probe. It can extract every indexed Dam room
record or a selected room/range, preserves each room's origin, and emits its point table, primary display list,
optional secondary display list, and a hash-bearing manifest. The asset build packs those files below
`converted/levels/dam/rooms/roomNNN/`. The portable `ge_dam_rooms_build()` loader accepts a descriptor array,
performs a capacity-query pass, traverses both display lists with bounded GBI limits, and emits world-space
triangles plus per-draw room/list/material metadata.

The same validated background-room format is now exercised by the next-level
Facility bundle without selecting it in the runtime. See
[`FacilityAssetPipeline.md`](FacilityAssetPipeline.md) for its exact
background, room, portal/visibility, STAN, compiled-setup and canonical-spawn
artifacts, plus the remaining stage-selection/setup-materialization boundary.

The 3DS frontend loads the ten rooms from the private pack and adds their original room origins before projection,
so they retain their authored relationship instead of being fitted independently. It resolves the rooms' original
image IDs through the converted texture catalog and submits authored material batches to PICA200. CPU clipping
uses the original 320x240 first-person viewport centered on the 3DS top screen with 40-pixel pillarboxes.
The crosshair remains a bring-up diagnostic, not a replacement weapon/HUD system.
The ARM11 runtime now calls the actual decompiled `bondviewUpdateCameraMatrices` body using generated pad 33's
position/look/up state and room 135. Its original matrixmath and libultra producers allocate the expected five
matrices and two look-at lights and publish the original view/projection handoff, which now drives the top-screen
projection and clipping path. The original movement/collision integration boundary and exact call chain are
recorded in `docs/DamOriginalMovementSlice.md`.

The complete 2,755-tile/8,366-point Dam STAN is materialized in the original eight-byte linked arena layout and
bound to the decompiled `stan.c` globals. The ARM runtime calls the original name lookup, point-in-tile, floor,
linked traversal, and radius traversal bodies. At pad 33 it resolves `p6g1`, obtains floor `-25`, accepts the spawn
interior and linked edge, and rejects the authored zero-link wall. The exact `bondviewTryMoveToStan`,
simple/fraction/edge/end-hop fallback bodies, and `bondviewCalcUpdatePlayerCollision` are now linked against the
real spawned player and exercise authored linked and zero-link edges under sanitizers and on ARM. This is still
collision-system bring-up, not player input: `MoveBond` depends on the original `bondhead.c`/model-animation
root-motion path that has not yet been closed.

The runtime additionally executes explicit decompiled spawn slices from the
original `prop.c` and `bondview_r.c` sources. They scale and STAN-bind every
generated Dam pad, walk the full intro record layout, and establish normal-play
pad 33 / `p6g1` with its original floor and yaw calculation. The spawn is committed through the exact original
`change_player_pos_to_target` body into a real `struct player`, including a native player prop and room 135
registration. The player viewer prop uses the same exact original pool/active/enabled/room state as the three
materialized world records. The bottom-screen label is intentionally `Decomp intro`, not “original mission loader”: guards,
inventory/ammo side effects, intro cameras, mission scripting, and the full prop constructor path are not linked.

## Local prerequisites

- Docker through Colima or Docker Desktop
- An unmodified NTSC-U ROM whose SHA-1 is `abe01e4aeb033b6c0836819f549c791b26cfde83`
- For hardware tests, a homebrew-capable Nintendo 3DS and a recent Homebrew Menu
- For network loading, the console and development Mac must be on the same network

ROMs and extracted assets are ignored and must never be committed or distributed.

## Reproduce the N64 build

Place or symlink the verified ROM at `baserom.u.z64`, then run:

```sh
./scripts/extract_baserom.u.sh
./scripts/build_decomp.sh -j4
```

The Docker build runs as `linux/amd64` because the original IDO and `qemu-irix` tooling is x86-64-only. On Apple
Silicon, use `IDO_RECOMP=NO` as the script does: compiling the recompiled IDO under x86 emulation can crash GCC.
The expected output is `build/u/ge007.u.z64: OK`.

## Build the 3DS target

The build uses devkitPro's official `devkitpro/devkitarm` image, so no host-wide devkitPro install is required:

```sh
make 3ds
```

Artifacts are written to:

- `platform/3ds/goldeneye-3ds.3dsx`
- `platform/3ds/goldeneye-3ds.smdh`
- `platform/3ds/goldeneye-3ds.elf`

Run `make 3ds-clean` to remove them.

Generate and stage the private runtime resources with:

```sh
make 3ds-assets
make 3ds-stage
```

`make 3ds-assets` verifies the source ROM SHA-1 before creating
`build/3ds-assets/goldeneye.u.gepack`. The asset build uses a reproducible container for the existing GoldenEye
texture decoder and devkitPro's `tex3ds` converter. Its generated catalogs are
`build/3ds-textures/catalog.json` and the zero-copy runtime index
`build/3ds-textures/catalog.gecat`; generated T3X files live under `build/3ds-textures/t3x/`. Conversion is sorted,
parallel, and reproducible, and stale generated textures are pruned atomically. The current build catalogs 4,532
texture LOD resources and packages the generalized Dam room extraction and manifest under
`converted/levels/dam/rooms/`. `make 3ds-stage` lays out a copy-ready SD-card tree under
`build/3ds-sd/3ds/goldeneye-3ds/`. The pack is ignored by Git and must not be distributed.

The platform-neutral bridge, asset pack, texture cache, scheduler services and original completion cycle, PCM/AI
audio boundary and original libaudio slice, Fast3D decoder/matrix stack, and asset conversion pipeline have host
tests:

```sh
make test-3ds-port
```

## Test

For an emulator, open `platform/3ds/goldeneye-3ds.3dsx` in Azahar or another maintained 3DS emulator. With the private
asset pack staged, the top screen shows an original-camera, authored-texture view of Dam rooms
`135, 133, 134, 132, 136, 124, 125, 126, 127, 128`, plus the PICA200 crosshair at spawn pad 33. The bottom screen reports the loaded
room/list, source-draw/GPU-group and geometry counts, generated setup and decomp-intro state, native STAN checks,
original Bond-camera allocations, asset-pack/cache status, ARM11 input, GoldenEye frame/RNG state, camera quaternion
data, fixed-step engine ticks, and memory availability. C-stick input advances the original quaternion state for
diagnostics; it does not yet move the level camera. Hold R to turn the spawn crosshair red. The input diagnostic
and crosshair are temporary frontend scaffolding and are not intended to become gameplay implementations.

On hardware, libctru's NDSP loader requires either the Homebrew Menu `hb:ndsp` handle or a console-derived
`sdmc:/3ds/dspfirm.cdc`. Do not distribute the console-derived DSP component.
For Azahar's **HLE** audio mode, an empty `3ds/dspfirm.cdc` in its virtual SD card is sufficient:
it satisfies libctru's file lookup while the emulator implements DSP mixing itself. This is the
[devkitPro-documented emulator setup](https://github.com/devkitPro/3ds-examples/blob/master/audio/README.md),
not usable firmware for hardware or LLE. Keep that empty file out of the hardware staging tree.
If neither the file nor handle exists, the build reports `NDSP audio: unavailable (d880a7fa)`.

For hardware, copy the contents of `build/3ds-sd/3ds/goldeneye-3ds/` to
`sd:/3ds/goldeneye-3ds/`, then launch it from Homebrew Menu. To use
3dslink instead, first copy the asset pack to the SD-card path above, open Homebrew Menu's netloader, and run:

```sh
./scripts/deploy_3ds.sh --ip YOUR_3DS_IP
# or: make 3ds-link N3DS_IP=YOUR_3DS_IP
```

Running `./scripts/deploy_3ds.sh --skip-build` without an address performs a
read-only staged-file/hash check and prints the exact directory to copy.

The `.3dsx` and private pack have been exercised in Azahar, but this milestone has not yet been run on physical
Nintendo 3DS hardware. The copy/netloader instructions above are the pending hardware-test path, not a claim of a
successful New Nintendo 3DS XL test. The port is not yet fully playable.

## Port architecture and next milestones

`port/include/ge_port.h` is the platform-neutral boundary. The 3DS frontend translates HID input into GoldenEye
actions and advances the engine at the original 60 Hz logical rate. `ge_port_init()` initializes the original
controller playback path; every tick inside `ge_port_advance()` calls `ge_original_input_tick()` before frame
timing, the isolated original `lvlManageMpGame` timer prefix, RNG, and quaternion work. The resulting
`original_move_*`, `original_look_*`, `original_buttons`, and
`original_buttons_pressed` fields are the source-compatible values to pass into the next player/game-loop slice.
Callers using the lower-level boundary directly must call `ge_original_input_init()` once, then exactly one
`ge_original_input_tick()` per logical 60 Hz tick. The renderer owns a citro3d render target, shader, and dynamic
vertex buffer.

`ge_original_level_tick()` compiles the bounded `src/game/lv.c` stage-tick slice. Pause, TLB reset, button edges,
and each ordered world/title subsystem are explicit providers; clock delta, global timer, stage timer, idle state,
and active-update count are observable in `GePortState`. Runtime providers for the world/title systems remain
empty. The slice therefore proves original timing and dispatch order, not an executing gameplay loop.

The Dam setup path now also compiles the decompiled `sizepropdef` body as a bounded source slice and has a
native setup-data materializer at the port boundary. The materializer walks the authored `UsetupdamZ` command
stream and creates real native `ObjectRecord` and `PropRecord` storage for its first glass, standard object,
and door records (commands 107, 122, and 267). Their authored normal/bound pads, resolved STAN, position, and room
are copied into the original record types; a typed sidecar preserves the door words which follow the inherited
object in the N64 setup ABI. Registration, activation, and enablement are dispatched through typed runtime
providers only after each constructor succeeds. The host test covers bound pads 10076 and 6 under AddressSanitizer
and UndefinedBehaviorSanitizer and verifies command 267's second-room membership. The 3DS runtime owns the definitions for
the duration of the stage and feeds their props through the exact decompiled `chrpropAllocate`, `chrpropActivate`,
`chrpropEnable`, and `chrpropRegisterRoom` bodies. They occupy the original 600-slot pool, active list, enabled
state, and room chunk lists; the bottom screen reports their authored command indices and active count.

This remains a bounded setup-data materializer rather than the complete `proplvreset2`: it bridges generated
N64-width command words to native pointer-sized structures. Bounded exact constructor slices then run in authored
order. The
ordinary-pad prefix of `domakedefaultobj` verifies command 122's model request, u8.8 scale `512 -> 2.0`, fixed-point
damage `0x00fa0000 -> 250.0`, multiplayer respawn branch, pad basis/position, and starting STAN. The exact
zero-radius `getposstan`, preallocated `objInit`, `sub_GAME_7F04088C` placement, tile shading, and collision-polygon
paths now follow it. Command 107 likewise uses its exact bound-pad scaling and on-screen placement path.

The asset build extracts model 62 (`PchrwppksilZ`) from its verified ROM resource and materializes its native
pointer-bearing ABI: four original nodes, six texture records, the weapon skeleton/switch table, exact bounding
box, 74 vertices, original display lists, and gunfire data. A sanitizer test drives that real model through the
command-122 constructor, preserving `PitemZ` scale `0.1` and the authored extra scale for a final model scale of
`0.2`. The asset build also extracts `PwindowZ` (model 104) and `PdamgatedoorZ` (model 178) into pointer-safe native
group/bbox/display-list-collision graphs. Command 267 executes the exact bound-pad centre, basis, bbox scaling,
STAN walk and two-sided room probes, fixed-point door motion conversion, initial `doorUpdateBbox` collision,
tile shading, portal-control seam, and original prop activation order. Its authored 0.95 open position correctly
retains collision because its perimeter threshold is 1.0. Dynamic door ticking, linked command 268, prop rendering,
and the live background portal-index provider remain to be connected before the gate is interactive and visible.

The renderer boundary now also traverses those three exact segment-5 model resources through the shared Fast3D
pipeline and flattens their original draw/material state into the same authored world-scene ABI as Dam rooms. It
preserves the constructors' matrices, positions, room membership, and Rare texture IDs: model 62 contributes 46
triangles in 13 batches, model 104 contributes 2 triangles in one batch, and model 178 contributes 22 triangles
in 8 batches. The sanitizer test covers capacity-query and committed-output passes for all three. This closes the
model-to-scene decoding boundary. The live 3DS stage now takes the matrices, positions, and rooms directly from
the constructed `ObjectRecord`/`PropRecord` pairs, installs all 70 triangles as an atomic dynamic-scene overlay,
loads their original textures through the bounded cache, and preserves that overlay across subsequent room-preload
transactions. Dynamic model/door ticks must still republish changed transforms; the initial authored instances are
now part of the camera-clipped scene rather than status-only prop records.

The asset build also extracts and hashes the exact US `bond_eye_walk` and `sprinting` animation records plus the
complete 0xe7e0-byte animation-data segment. A native adapter materializes their pointer-bearing
`ModelAnimation`/descriptor/root-joint ABI and calls the canonical `model.c` root decoders. Host tests feed exact
sprint frame 7 through model decoding, Bond's 0.1 scale, `bheadUpdatePos`, the normal MoveBond heading transform,
and the exact STAN collision path. On 3DS startup the packed segment is mounted, both roots are materialized, and
known walk/sprint vectors are decoded again; the bottom screen reports 35 walk and 19 sprint frames. Canonical
`bheadAdjustAnimation`, `modelSetAnimation`, its loop/end/speed setters, and `modelTickAnim` now compile as bounded
host and ARM source slices. The authentic embedded player gait header, skeleton, rwdata, and group graph are now
initialized on the spawned original player's actual `Model` overlay. Exact `modelTickAnim`, `subcalcpos`, and
`subcalcmatrices` consume the packaged frame records and write the final authored group transform to
`bondheadmatrices[0]`; host sanitizer tests cover both walk and sprint and the 3DS startup verifies the walk path.
The exact `bheadAdjustAnimation` lookup resolves those same materialized animation ABIs. Original walk/sprint
changes now retain their 12-tick `anim2` transition: both ROM frame pairs are loaded and canonical shortest-path
quaternion slerp applies `unk84`, with sanitizer fixtures at blend ticks 0, 1, 6, and 12. The path remains outside
the live tick until the complete `bondviewProcessInput` providers and bhead update chain are connected. There is
no invented animation clock or stick-to-position shortcut.

The remaining work is intentionally staged:

1. Replace the stage-tick slice's no-op world providers with the original player, difficulty, room-transition,
   sky, particle, character/prop, language, and music systems in decomp dependency order. Connect the proven
   cooperative original-scheduler cycle to that execution and framebuffer presentation. Controller sampling,
   base timing, stage selection, and dispatch order are no longer blockers.
2. Replace the setup-pad camera inputs with live original player/head camera state, then expand the material
   backend for the remaining depth/fog/lighting cases. The first-person projection/view matrices already feed the
   native clipper and PICA draw path; gameplay camera state and transforms remain owned by the decompiled game.
3. Turn the generalized room descriptors and preserved portal metadata into runtime streaming and visibility;
   bind the cluster's original textures/material batches and add its props and characters while preserving live
   matrices and Rare texture IDs through the bounded cache. Do not ship source-ROM assets.
4. Invoke the tested current-player post-input gait/movement boundary after original `bondviewProcessInput` every
   gameplay tick. It already drives `bheadAdjustAnimation`, the crossfaded model clock/matrices, and
   `bondheadmatrices[0]` through linked `bheadUpdatePos`/MoveBond/STAN using the speed fields written by input.
   Supply dynamic props through
   the typed provider, which currently returns `cdtypes=0`; keep inactive tank providers at the adapter boundary
   and do not introduce replacement player physics. Then bring up one complete level loop, followed by menus,
   missions, AI, weapons, and save data.
5. Implement ABI1 `SETVOL`, `ENVMIXER`, and `POLEF`, connect complete voice/music command production to AI/NDSP,
   then add producer/consumer synchronization before moving audio production off the frontend thread.
6. Continue profiling in Azahar, then perform the still-pending New Nintendo 3DS XL test; the New 3DS CPU speedup
   is enabled by the current shell.

# Dam visual regression harness

For authored tours of every solo mission, see `docs/StageVisualTours.md` and
`scripts/generate_stage_visual_tour.py`. The generator documented here retains
Dam-specific mission ordering and landmark evidence.

The harness has two complementary layers. Neither replaces GoldenEye's
gameplay camera, movement, or renderer in a normal run.

## Offline geometry/depth probe

`scripts/probe_dam_world.sh` decodes the same authored room display lists and
PICA material state as the 3DS build, then ray-casts a deterministic camera
image. It writes three PPM files:

- `*-nearest.ppm`: the geometrically nearest authored batch at each pixel.
- `*-pipeline.ppm`: the batch surviving the current depth-test/write order.
- `*-conflicts.ppm`: red pixels where those batch identities differ.

Example using authored Dam coordinates:

```sh
./scripts/probe_dam_world.sh \
  --origin 4719,16,3949 --forward 0.14,-0.09,0.99 \
  --fov 60 --size 320,180 \
  --output build/visual-probe/tunnel-fence
```

This layer intentionally identifies geometry/material ownership rather than
trying to reproduce texture filtering or blending. A zero-conflict report
rules out simple depth overwrite, but it does not rule out an alpha-test,
blend, UV, fog, or projection defect.

## Authored-camera emulator tour

`scripts/generate_dam_visual_tour.py` reads the exact decompiled Dam pad and
STAN tables. Its default tour follows 18 authored route checkpoints from the
normal spawn, down the road, through the tunnel, and onto the dam. Each
checkpoint is viewed forward, left, and right. Route tangents determine the
heading, while the exact STAN floor plane determines eye height and room
ownership. This avoids treating guard, prop, trigger, and volume pads as if
they were player viewpoints.

When `sdmc:/3ds/goldeneye-3ds/dam-visual-tour.geview` exists, the 3DS runtime
uses those records only for render-camera publication. Original simulation,
world construction, materials, portals, and drawing remain live; the tour does
not write collision position or invent gameplay state. A normal run is
unchanged when the file is absent.

On this development Mac, generate, stage, and launch the finite tour with:

```sh
./scripts/run_dam_visual_tour.sh
```

The runner builds and stages an exact executable/assets pair and installs the
temporary tour file in Azahar's SD card. Press `Ctrl+P` at a checkpoint to use
Azahar's screenshot capture. Quit Azahar when finished; the runner then removes
the installed tour and result files. A completed runtime writes an exact result
record before returning to Azahar's game list; the runner preserves it as
`build/visual-probe/dam-visual-tour.result` and fails if no completion record
was produced. The record includes the completed view count, peak resident room
and texture counts, stream successes/failures, and final camera status. The
source tour and JSON manifest remain under `build/visual-probe/`. Azahar 2126's
command-line video-dump option is not used because it did not create an output
file on this Mac.

The result also carries Dam-specific evidence from the live canonical runtime:
guard `propsTick` successes/rejections, guard firing and player-damage events,
door interaction ticks, overlay publications, mission-AI offset/ticks, and the
full-props activation state. The runner rejects a tour if that path never
activates, rejects a tick, or stops advancing. Displayed-frame average/peak,
simulation average, and GPU average times are recorded independently of the
periodically reset on-screen profiler so performance regressions can be
compared across exact staged artifacts.

Set `GE_VISUAL_TOUR_FRAMES` to hold views longer. The former room sweep and the
every-pad sweep remain explicit secondary modes for broader coverage:

```sh
./scripts/generate_dam_visual_tour.py --room-coverage --frames 30
./scripts/generate_dam_visual_tour.py --all-pads --frames 60
```

### Mission and branch routes

The generator also exposes finite, named routes for mission-area regression.
They are derived from the setup rather than from hand-entered world
coordinates:

```sh
./scripts/generate_dam_visual_tour.py --route modem --frames 30
./scripts/generate_dam_visual_tour.py --route alarms --frames 30
./scripts/generate_dam_visual_tour.py --route bungee --frames 30
./scripts/generate_dam_visual_tour.py --route objectives --frames 30
```

To generate, stage, and launch one of these directly, pass it through the
runner environment, for example:

```sh
GE_VISUAL_TOUR_ROUTE=objectives \
GE_VISUAL_TOUR_DIRECTIONS=forward \
./scripts/run_dam_visual_tour.sh
```

- `modem` follows the authored waypoint links from the insertion area to the
  nearest navigation marker for tagged setup props 290 and 292. Those are the
  modem monitor and connection box at exact bound pads 10057 and 10058.
- `alarms` visits the four exact alarm areas from setup props 310, 312, 314,
  and 316 (bound pads 10070, 10071, 10072, and 10074). The first/security-area
  leg and the road/tower legs remain separate route segments because the
  decompiled setup places them in disconnected waypoint components. Within a
  segment every checkpoint and edge comes from `pathwaypoints` and its
  `path_table_*` record.
- `bungee` follows the linked road/dam waypoints and finishes at regular pad
  330. This is the exact pad whose room `ai_24` checks before starting the
  authored jump/exit sequence.
- `objectives` combines modem, alarms, and the post-alarm bungee approach in
  player mission order. It intentionally preserves segment boundaries rather
  than drawing a false continuous tangent across disconnected navigation
  components.

All named routes use forward/left/right views by default. Add
`--directions forward` for a faster path-only pass, or include `back` when a
four-way material/occlusion sweep is needed. `--route main` is the explicit
spelling of the unchanged default spawn-road-tunnel-dam tour.

The JSON manifest is the stable index used to extract named frames and compare
future builds. It records source hashes, pad/STAN/room identity, both authored
and runtime coordinates, waypoint/waygroup identity where applicable, mission
target evidence, and exact frame spans.

## Route streaming-capacity audit

`scripts/test_dam_route_capacity.sh` turns the generated forward-only `main`
and `objectives` manifests into a host-side streaming regression. For each
authored checkpoint it runs the unchanged `bondviewUpdateCameraMatrices`
camera body and exact `bgDetermineVisibleRooms`/global-visibility interpreter,
feeds preload requests through `GeDamPreloadQueue`, and installs them through
`GeDamDynamicScene` until that view reaches a preload fixed point. It uses the
background's original portal control bytes and the same ten-room initial
component as the 3DS runtime.

The audit reads the texture, projected-vertex/batch, and initial-room limits
from `platform/3ds/source/main.c`; room capacity comes from the shared world
header. It fails on a room/vertex/batch install error or when unique authored
Rare texture IDs exceed the configured GPU slots. A deliberate one-texture
negative pass verifies that an insufficient configuration is rejected.

Run it with:

```sh
./scripts/test_dam_route_capacity.sh
```

The machine-readable report is written to
`build/visual-probe/dam-route-capacity.json`. With the current authored pack
and initial portal state, the measured peaks are:

| Route | Resident rooms | Rare textures | Vertices | Batches | Visibility preloads accepted |
| --- | ---: | ---: | ---: | ---: | ---: |
| Main | 57 / 137 | 59 / 128 | 22,977 / 65,536 | 2,230 / 65,536 | 33 |
| Objectives | 58 / 137 | 63 / 128 | 24,663 / 65,536 | 2,386 / 65,536 | 33 |

These are cumulative values because the current cache still has no canonical
eviction implementation. The report also records every route-requested and
resident room, total preload attempts, peak visible rooms, and explicit tour
room requests.

## Room-stream capacity probe

`scripts/probe_dam_stream.sh` replays the default player-route room sequence
through the same packed room assets, preload queue, Fast3D decoder, and dynamic
scene transaction used by the 3DS runtime. It reports cumulative room, vertex,
batch, generation, and failure counts after every installation:

```sh
./scripts/probe_dam_stream.sh
./scripts/probe_dam_stream.sh 113 121 117 116 53 103
```

This distinguishes a real asset/vertex/batch capacity failure from an emulator
tour that is merely waiting for visible neighboring rooms to finish loading.

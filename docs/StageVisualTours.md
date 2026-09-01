# Authored all-stage visual tours

`scripts/generate_stage_visual_tour.py` creates finite emulator camera tours for
every solo mission. It is the scalable companion to the specialized Dam route
generator; it does not replace the original gameplay camera or construct world
coordinates by hand.

For the selected stage the generator derives all inputs from decompiled and
checked assets:

- mission order, sparse `LEVELID`, setup/background/STAN keys, and level scale
  come from the original `boss.c`, `bondconstants.h`, `chraidata.c`, and `bg.c`
  tables;
- the first view is the unique normal intro spawn whose demo slot is zero;
- every positioned object record contributes its exact ordinary or bound pad as
  a route target;
- each target is mapped in full XYZ space to the closest authored waypoint;
- the route uses deterministic unweighted shortest paths over the reciprocal
  original `path_table_*` graph, always choosing the nearest remaining target
  and using waypoint index only as a stable tie-breaker;
- view position, room, and floor height come from the selected pad and its exact
  generated STAN tile. The per-stage level scale is used for runtime coordinates.

Original object records with negative pad sentinels, or dormant records whose
pad is outside both authored pad arrays, are listed under `unpositioned_props`
in the JSON manifest. They never receive invented positions. Disconnected
waypoint components remain separate `route_segment` values rather than gaining
fabricated links.

Generate individual tours with runtime-safe forward views:

```sh
scripts/generate_stage_visual_tour.py --stage runway
scripts/generate_stage_visual_tour.py --stage silo
scripts/generate_stage_visual_tour.py --stage cradle
scripts/generate_stage_visual_tour.py --stage surface1
scripts/generate_stage_visual_tour.py --stage surface2
```

The default development outputs are:

```text
build/visual-probe/<stage>-authored.geview
build/visual-probe/<stage>-authored.json
```

The manifest records the corresponding runtime files:

```text
sdmc:/3ds/goldeneye-3ds/stage.cfg                 # contains <stage>\n
sdmc:/3ds/goldeneye-3ds/<stage>-visual-tour.geview
sdmc:/3ds/goldeneye-3ds/<stage>-visual-tour.result
```

`scripts/run_stage_visual_tour.sh <stage>` generates the tour, stages the exact
executable/assets pair, installs the stage selection and tour, launches an
isolated Azahar process, and restores any pre-existing `stage.cfg` afterward.
For example:

```sh
scripts/run_stage_visual_tour.sh runway
scripts/run_stage_visual_tour.sh facility
GE_VISUAL_TOUR_FRAMES=60 scripts/run_stage_visual_tour.sh cradle
```

The runtime stores at most 384 views. A one-direction tour for every solo stage
fits that bound. Additional directions are allowed with `--directions`, but the
generator fails before writing output if the requested sweep exceeds capacity.

Run the focused representative regression with:

```sh
scripts/test_stage_visual_tours.sh
```

It covers outdoor Runway, indoor multi-component Silo, zero-portal Cradle, and
both setup variants of the shared Surface world. The Python suite also generates
all 20 solo-stage routes and the native C parser validates the representative
`GEVIEW1` files.

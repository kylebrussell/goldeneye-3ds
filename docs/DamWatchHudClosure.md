# Dam single-player watch/HUD closure

This map records the remaining native boundary for GoldenEye's canonical
single-player watch. It deliberately does not specify a replacement menu or a
parallel UI state machine.

## Live decompiled path

- `KEY_START` is mapped to the N64 `START_BUTTON` by `ge_port_n64_buttons`.
- The unchanged `MoveBond` body calls the unchanged `bondviewProcessInput`.
- Its rising-START branch calls the unchanged `trigger_solo_watch_menu(0)`.
- The trigger builds the authored health/armour gauge lists, selector
  rectangles, and static rectangle into the inline player buffers.
- The unchanged `MoveBond` body calls `bondviewWatchAnimationTick` while the
  watch state is nonzero. The watch trigger and animation tick now share the
  one canonical `watch_transition_time` variable.

The former 3DS main-loop START interception occurred before `MoveBond` and
made this entire path unreachable. It has been removed; mission-exit START
acknowledgement remains in the unchanged post-MoveBond
`ge_original_dam_mission_exit_process_input_exact` block.

## Closure 1: authored left hand/watch model (closed)

The first watch tick requests `ITEM_SUIT_LF_HAND` through the canonical
`draw_item_in_hand` and `used_to_load_1st_person_model_on_demand` chain. The
first-person asset provider now publishes that item from the exact verified
US-ROM resource. Its compressed and inflated hashes, serialized HEADER-root
ABI, canonical header/skeleton relation, and native scene capacity are covered
by focused tests.

Implemented without synthesizing geometry:

1. The verified US-ROM `Csuit_lf_handZ` resource is included in
   `extract_3ds_first_person_pp7.py` and the asset pack. Its canonical file
   record is `ITEM_SUIT_LF_HAND`; its decompiled header and skeleton are
   `suit_lf_hand_header` and `SKELETON(suit_lf_hand)`.
2. The exact `gitem_structs` slice includes the canonical
   `SUIT_LFRECORD` entry and `Csuit_lf_handz_stats`, retaining the `C` resource
   prefix.
3. The item/resource relation is bound through the first-person provider and
   validates the relocated header against the canonical header: 10 switches, 9
   matrices, 22 textures, and the authored skeleton.
4. Native tests prove a non-null relocated HEADER/group/switch graph and exact
   scene construction. Live `sub_GAME_7F07E7CC`/watch-animation progression is
   the next runtime gate before enabling the render path.

## Closure 2: unchanged watch model render

Retain `bondviewRenderWatch` rather than building a separate watch mesh. Its
direct native requirements are already represented elsewhere in the port:

- exact watch animation state and `ANIM_DATA_bond_watch`;
- `modelGetNodeRwData`, `subcalcmatrices`, cuff switch selection, and the
  authored suit model graph;
- the published Bond camera/world-to-screen matrix;
- dynamic matrix/Gfx allocation;
- model `subdraw` output.

The platform work is to replay the resulting model/Gfx commands through the
existing `ge_gbi_*` decode/traversal/state pipeline and submit the decoded
geometry to Citro3D. The adapter must consume the exact per-frame matrices,
switch visibility, environment colour, and model display lists produced by
the decompiled body. It must not recreate hand/watch pose or cuff logic.

## Closure 3: unchanged watch pages

`bondviewRenderWatch` calls the unchanged `draw_watch_current_page`, whose
canonical dispatcher owns all five pages:

- mission status;
- inventory;
- control options;
- game options;
- mission briefing.

The page bodies and navigation state are in `src/game/options.c`; the
navigation helpers and `sub_GAME_7F0A6A80` are already retained in the
non-tank slice. The remaining work is to retain the page draw bodies and bind
their exact services: language/text lookup, inventory and objective reads,
controller-option persistence, font/text rendering, and dynamic Gfx/vertex
allocation. Their generated Gfx should use the same replay boundary as the
watch model. Existing Zurich Bold and Bank Gothic native atlases can realize
the canonical text commands, but page layout, strings, colours, and ordering
must continue to come from the unchanged page bodies.

## Verification gate

Before treating the watch as live, a focused host/sanitizer test should drive
a rising START through `MoveBond`, tick until `WATCH_ANIMATION_0x5`, and assert:

- the suit resource was loaded and relocated into the left-hand buffer;
- the canonical model instance and animation are initialized;
- controls lock only at the canonical state;
- all five page indices emit terminating, pointer-safe Gfx streams;
- a second START runs the canonical close sequence back to state zero;
- ordinary gameplay HUD visibility resumes after closing.

The ARM audit must retain one definition each of `trigger_solo_watch_menu`,
`bondviewWatchAnimationTick`, `bondviewRenderWatch`,
`draw_watch_current_page`, and `watch_transition_time`.

# Canonical multiplayer rules integration

`ge_original_multiplayer_rules.h` supplies a native boundary around original
multiplayer rules. It is part of local simulation bring-up; it does not by
itself enable playable or wireless matches.

## Runtime contracts

1. Allocate and publish 2–4 original `struct player` instances and permanent
   records, then call `ge_original_multiplayer_configure`. The config validates
   the original frontend scenario player counts, all fourteen weapon sets,
   eleven authored arenas and their player limits. The frontend must check the
   returned `unlock_after` mission against the user's save. Configuration does
   not reset match statistics. It invalidates previous spawn publication.
2. Load the original multiplayer setup, never the solo setup. Canonical
   `prop.c:proplvreset2` inserts `mp_` after `U`: Facility uses
   `Ump_setuparkZ`, while Archives uses `Ump_setuparchZ`.
   Call `ge_original_multiplayer_arena` for exact case-sensitive keys rather
   than reconstructing them from display names. The catalogue is generated
   directly from `front.c` and `chraidata.c`.
3. Resolve every setup pad's native STAN pointer. Pass the bounded intro and
   pads to `ge_original_multiplayer_publish_spawns`, then publish that same
   intro pointer in `g_CurrentSetup.intro`. Spawn demo filtering and first
   starting weapon selection follow `bondview_r.c`. Up to sixteen spawn pads
   are accepted. Invalid/truncated intros leave previous publication intact.
4. The simulation owns the intro/pad lifetimes and must not mutate validated
   records. Each player's prop, inventory allocation, body and hand resources
   must already be valid. `ge_original_multiplayer_respawn` checks canonical
   death/blood/fade and YOLT gates, switches to the requested player, runs the
   original complete respawn body, then restores the previous player and
   permanent-data pointer. The actor/model allocation, collision floor query,
   inventory functions, hand equip and body loader are canonical dependencies.
5. Use `ge_original_multiplayer_weapon(slot)` for authored weapon IDs `0xF0`
   through `0xF7`. Preserve `lastmpweaponnum` while walking setup commands; the
   following multi-ammo crate uses that preceding weapon's ammo type/amount.
   The `ge_original_multiplayer_ammo` helper takes a pointer to that slot as its
   context; a larger runtime context needs a callback thunk.
6. Call `ge_original_multiplayer_end_tick` once before incrementing the match
   timer. Its extracted `lv.c` branches preserve point-limit death-animation
   waiting and YOLT elimination ordering; the time limit uses the original
   crossing condition. `FREEZE` stops player actions while death visuals finish;
   `RESULTS` requires the caller to stop play and open match results. Original
   score queries cover normal, LTK, golden-gun bonus, flag time, teams and YOLT.

## Source/build ownership

Compile `port/src/ge_original_multiplayer_rules.c`, canonical
`src/game/mp_weapon.c` and the output of:

```sh
python3 scripts/extract_multiplayer_rules_slice.py . build/generated/ge_original_multiplayer_rules_slice.c
```

Use the original native type flags (`GE_PORT_USE_ORIGINAL_TYPES`, `AIPARSE`,
`GE_PORT_SETUP_DATA`, `GE_PORT_MS_INHERITS`, `VERSION_US`, `PLAYERFLAG=int`).
The generated slice owns `g_Startpad`, `startpadcount`, `starting_weapon`,
`ZeroCoordSpawnPos`, original arena/scenario tables, and the extracted scoring,
spawn selector, BeginLife, respawn and character-fade functions. Player
allocation owns `init_player_BONDdata`; live input owns `scenario/get_scenario`
and inventory; the existing guard-damage closure owns damage and death.

The sole native-width edits to the original respawn body make its STAN local a
`StandTile *` and use `uintptr_t` when advancing intro pointers. They preserve
the 32-bit target behavior and avoid truncating native host pointers. Source
hash comments identify each extracted body.

## Verification and current checklist

- [x] `bash scripts/test_multiplayer_rules.sh`: ASan/UBSan two/four-player
  original damage through armour, fatal damage, duplicate fatal-hit rejection,
  kill/death/suicide bookkeeping, respawn, inventory clearing/regrant, original
  ammo limits, STAN/position/eyeheight updates, and current-player restoration.
- [x] Original scenario scoring, original point-limit waiting, time expiration,
  YOLT final-player gating, all weapon records, arena restrictions, short and
  malformed intro rejection.
- [x] `bash scripts/test_stage_pickup.sh`: real original proximity pickup chain
  for solo, two and four players; armour and permanent pickup statistics remain
  owned by the collecting slot. Distance and occlusion rejection are retained.
- [x] Native rules adapter compiles with `-Wall -Wextra -Werror`.
- [x] Verified-ROM conversion/registry and native world/setup loading for all
  eleven authored multiplayer arenas. `bash scripts/test_multiplayer_assets.sh
  /path/to/baserom.u.z64` runs ASan/UBSan through full setup relocation, native
  STAN binding and original respawn for every allowed 2–4 player count. Each
  map's authored spawn count is retained; all source STAN geometry and spawn
  records are checked against the verified ROM bytes. This is a headless
  asset/spawn fixture, not a rendered playable match.
- [ ] Integrated hand fire/hit detection across player bodies and actual
  dropped-weapon actor materialization, pickup regeneration and scenario items.
- [ ] Results frontend, original awards/music, and complete match flow.
- [ ] Coordinator ARM link and device/emulator acceptance.

The rules fixture uses resource doubles for floor resolution, existing bodies,
hand equip, audio/HUD and dropped-actor construction. Damage, death counters,
inventory operations, ammo writes, spawn selection, life reset and respawn
remain original source. The separate pickup fixture uses the original
`propsTickPlayer → objTickPlayer → propPickupByPlayer` chain with collision,
object-free and audiovisual providers. Neither fixture validates a full arena,
renderer, weapon firing, object regeneration, wireless radio or physical play.

## Multiplayer assets

`extract_3ds_multiplayer_stages.py` reads the canonical US ROM by SHA-1 and the
original source mapping tables, emits private bundles to `build/3ds-multiplayer`,
and generates `ge_multiplayer_stage_registry.inc`. It never modifies the ROM or
shared source assets. `build_3ds_assets.sh` includes all bundle files under
`converted/multiplayer/<arena>/` and verifies the packed background, setup,
collision and room-bound bytes against the generated inputs.

`ge_stage_asset_multiplayer(index)` follows the original frontend's eleven
non-Random entries. `ge_stage_asset_multiplayer_by_level_id(level)` explicitly
selects the multiplayer form of a shared solo level. `GE_STAGE_COUNT` remains
21; multiplayer descriptor IDs occupy `0x100..0x10A`. The generic descriptor
lookup accepts those IDs, and keys use `mp_temple`, `mp_facility`, etc. The
existing solo by-level lookup continues to return the solo descriptor.

| Arena | Max players | Authored spawns | Setup |
| --- | ---: | ---: | --- |
| Temple | 4 | 5 | Ump_setupdishZ |
| Complex | 4 | 7 | Ump_setuprefZ |
| Caves | 4 | 8 | Ump_setupoatZ |
| Library | 4 | 8 | Ump_setupameZ |
| Basement | 4 | 5 | Ump_setupashZ |
| Stack | 4 | 5 | Ump_setupimpZ |
| Facility | 4 | 8 | Ump_setuparkZ |
| Bunker 2 | 3 | 5 | Ump_setupsevbZ |
| Archives | 3 | 8 | Ump_setuparchZ |
| Caverns | 3 | 7 | Ump_setupcaveZ |
| Egyptian | 2 | 6 | Ump_setupcrypZ |

Library/Basement/Stack intentionally share background and STAN geometry while
retaining their distinct authored setup files. The fixture validates native
world/setup loading and player spawn positions; authored arena restrictions
also depend on the canonical background visibility commands and live object
construction in the integrated renderer/simulation.

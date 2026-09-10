# Multiplayer character ownership integration

This tranche restores the character-construction boundary for two through four
players. It does not enable a match loop or establish playable wireless matches.

## Integration contract

- Select each canonical menu character with
  `ge_original_player_body_set_character(slot, character)`. Slots are 0–3;
  characters are the unchanged `front.c` indices 0–63. `-1` selects the original
  default for that player slot. The adapter owns the original `player_char`
  symbol. Body, head, gender and perspective-height queries use this shared
  selection. `ge_original_player_body_reset` resets diagnostics, not selections.
- Publish the roster in `g_playerPointers` and switch the canonical current
  player before `solo_char_load`. Multiplayer retains both first-person hands,
  chooses its authored body/head and constructs the right-hand world weapon.
  Solo wardrobe selection, swirl starting weapon and hand removal remain.
- Create the stage guard/model provider even for a zero-guard multiplayer arena.
  Budget four character model instances beyond the models needed by guards and
  dynamic spawns. Four reserved `ChrRecord`s cannot consume authored guard slots.
  Distinct owners have distinct models, characters, world weapons and matrices.
- Before drawing, call
  `ge_original_stage_guard_runtime_set_local_player_view(runtime, player, 1)`
  for a local first-person view. Its own world body and both held attachments
  are excluded while opponents remain visible. Pass `NULL, 0` for cinematic
  publication. Rendering does not remove simulation bodies or weapons.
- Keep player storage alive until the stage provider is destroyed. Body
  allocation slots remain associated with the roster for the stage lifetime;
  a fifth owner is rejected. Character IDs remain fixed within that runtime;
  changing a choice requires stage/runtime reconstruction. Existing multiplayer
  bodies retain canonical animation/resume behavior through death and respawn.

## Held-item lifecycle

Runtime creation automatically binds a typed player-item factory in
`ge_original_gameplay_services`; destruction unbinds only its own factory.
The original `sub_GAME_7F09B368` lower-hand helper flags an equipped item for
removal; `sub_GAME_7F09B398` raises a hand through the original
`getCurrentPlayerWeaponId` / `getPropForHeldItem` mapping. Both are already in
the live gun/input extraction. No additional per-frame weapon synchronization
call is needed. The caller must run canonical gun and object ticks for every
player and publish the VIEWER roots through the active-prop composer.

Thirty stable native player-item records supply the canonical weapon-create
fresh-slot allocation boundary. Each successful construction uses the exact
blank collectable state, native `objInit`, and original `chrEquipWeapon`;
left-hand and explicit dual-weapon links follow that function. Recognized player
allocation failure returns NULL without falling through to the guard-grenade
factory. This bounded provider does not implement canonical cross-pool/offscreen
weapon eviction; thirty simultaneous live player items exhaust its pool.

`drop_inventory` creates concealed copies through the same factory. Original
`propobjSetDropped` / `objDrop` own projectile state, detachment, room placement,
and active-list insertion. Dropped items retain their model even if their former
owner dies or resumes a body. Their renderer uses canonical object matrix and
runtime position, independently of the owner's first-person visibility. Both
cached and uncached scene publication include them.

Only canonical `objFree` clearing the stable weapon record's `prop` permits
native model release and record reuse. Runtime teardown invokes canonical object
cleanup before releasing native records, clearing held links and dual links and
removing dropped roots from the active list. This closes the earlier release-on-
lost-hand lifetime bug. Player storage, canonical prop pool, projectile services,
and model providers must outlive runtime destruction.

`ge_original_stage_guard_runtime_chr_pool(runtime, &capacity)` exposes the
reserved guard allocator pool for zero-authored-guard arena composition. Its
capacity includes dynamic guard slots; four player records remain separate and
are reached through VIEWER props. Item snapshots scan slot indices 0–29 and
report generation, attachment, pending removal, and native object/model identity.

## Content

`scripts/extract_player_body_characters.py` mechanically extracts and checks
both original `mp_chr_setup` table variants; all 64 appearances agree. Character
packaging now includes Rosika (`CfattechwomanZ`) and the terrorist/biker heads
(`CheadbalaclavaZ`, `CheadbikeZ`). The complete solo/cast/multiplayer union is
74 models: 39 bodies and 35 heads, 1,155,216 payload bytes, identity
`fbb8414ba7fd88ce9c1b38eb854e6a47f461502e3c545ea158bc358cea038b8a`.
Rebuild the asset pack; an older 71-model pack cannot load those three choices.

The explosion extractor no longer publishes its own character table, selection
array or gender getter. Its unchanged damage functions resolve the shared
player-body appearance provider.

## Verified

- `scripts/test_player_body.sh`: ASan/UBSan solo wardrobe regression and two/four
  player selection, hand retention, separate ownership, reconstruction boundary,
  resume diagnostics and invalid selection checks.
- `test_ge_original_stage_guard_runtime`: ASan/UBSan two/four actual canonical
  body instances and held weapons, integrated heads, four local-camera views,
  cached and uncached geometry publication, existing-body resume and fifth-owner
  rejection. It also runs unchanged canonical hand helpers, actual factory
  dispatch, `drop_inventory`, `objDrop`, `objDetach`, `objFree`, dual linking and
  model clearing extracted by `extract_player_item_lifecycle_fixture.py`.
  Two/four players exercise change/removal/unarmed, both hands, concealed death
  drops, existing-body respawn, 34 reuses, capacity exhaustion, native model-byte
  reclamation, dropped scene publication, and final attached/dropped teardown.
  Projectile allocation, inventory membership, sound, lighting, and room services
  are explicit resource doubles; drop coverage uses the canonical offscreen
  branch. Existing 665 authored characters across 21 stages and 300 authored
  hats pass as well. This test is built by `scripts/test_stage_assets.sh`.
- `test_ge_original_character_models`: ASan/UBSan all 74 ROM-backed models,
  3,082 nodes and 1,021 display lists. Its optional timing loop now requires
  `GE_TEST_CHARACTER_TIMING`; timing is not part of multiplayer acceptance.
- `scripts/tests/test_stage_3ds_character_models.py`: canonical identity,
  dependency roles, atomic staging and round-trip manifest verification.
- `scripts/tests/test_bond_move_explosion_slice_exact.py`: all 109 remaining
  extracted bodies/data are token-identical to canonical source.
- `GE_TEST_MODEM_ONLY=1 scripts/test_covert_modem_fire.sh`: the explosion-provider
  ownership change links and passes the existing gun/projectile fixture.
- The integration agent reports a successful full ARM link of
  `goldeneye-3ds.3dsx` with the shared appearance provider and multiplayer modules.

## Remaining match acceptance

These fixtures establish body and held/dropped-item resource ownership,
canonical attachment/drop/free transitions, and rendering. They do not establish
full canonical shooting/damage, animated death, spawn-pad selection, inventory
respawn, onscreen drop collision, scoring, or results. Those systems must drive
these player records through the multiplayer simulation workstream. Actual emulator matches, physical radio matches and
hardware performance remain integration-coordinator acceptance.

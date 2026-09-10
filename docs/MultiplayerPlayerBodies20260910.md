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
  for a local first-person view. Its own world body and initial held attachment
  are excluded while opponents remain visible. Pass `NULL, 0` for cinematic
  publication. Rendering does not remove simulation bodies or weapons.
- Keep player storage alive until the stage provider is destroyed. Body
  allocation slots remain associated with the roster for the stage lifetime;
  a fifth owner is rejected. Canonical teardown must clear a retired character's
  model before its reserved record can be constructed again. Existing live
  bodies retain the canonical animation/resume behavior.

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
  rejection. Existing 665 authored characters across 21 stages and 300 authored
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

These fixtures establish initial body/weapon resource ownership and rendering,
not canonical shooting/damage, animated death, spawn-pad selection, inventory
respawn, dual-world-weapon updates, dropped-item lifetime, scoring, or results.
Those systems must still drive these player records through the multiplayer
simulation workstream. Actual emulator matches, physical radio matches and
hardware performance remain integration-coordinator acceptance.

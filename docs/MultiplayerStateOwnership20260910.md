# Multiplayer state ownership audit — 2026-09-10

This is a schema prerequisite, not a complete serializer or determinism claim.
The audit read integration source at `62b02603a2cb493b6adbeec6499aeac9c31ae65a`
and the multiplayer task's player/input work at
`fb06ff129bce91e0c5e79a3465998896e657fdb4`. The latter workstream may have local
changes and is identified separately from integrated source. No definition of
`GeMultiplayerSession` was present in either inspected `port` tree; its runtime
provider remains to be agreed with the multiplayer owner.

The later core/view integration retains the last canonical shuffled player's
selection, native frustum and background visibility at the tick boundary.
These execution cursors are typed state, not values to normalize to the
locally rendered player. Local presentation reads captured view fields.
Native core, inventory, prop-pool, hand, player and background subdomains now
exist, but their combined coverage still leaves explicit gaps and cannot
certify a complete live match hash. The tables below remain requirements for
all remaining owners, rather than a claim that declaration coverage alone
establishes semantic field classification.

## Existing protocol boundary

[`GeMpProtocol`](../port/include/ge_multiplayer_protocol.h) owns the immutable
ordered roster/configuration and one in-flight input frame. Its
`ge_mp_protocol_take_frame` / `ge_mp_protocol_finish_tick` boundary requires the
caller to execute one canonical tick and supply a full SHA256. The protocol
does not enumerate native state, construct manifests, serialize snapshots or
restore authoritative state. Current version 1 aborts on unequal hashes; a
snapshot/convergence protocol needs an explicit versioned contract.

`frame.tick`, roster, schema/version and complete rules/content/build identity
belong in the common state envelope. `local_slot`, transport ACK masks,
receive/send timestamps, retry counters and selected local render camera do
not: these legitimately differ between peers. Transport evidence and
`GeOriginalStageMissionRuntime.ai_offset_hash` are not replacement state hashes.

## Native owners and required coverage

The following are concrete starting regions, **not an exhaustive field
allowlist**. Every `field_*`/`unk*` member reachable from an enabled simulation
path still needs a read/write classification. Serialize fields by name and
type, never entire C structs or N64 byte-offset spans.

| Owner / exact source | Future-influencing state and encoding boundary |
| --- | --- |
| Roster: `g_playerPointers[4]`, `g_playerPlayerData[4]`, `g_CurrentPlayer`, `g_playerPerm`, `player_num`, `array_PLAYER_IDs[4]`, `random_byte` in [`player.h`](../src/game/player.h). Pending adapter: `GeMultiplayerStorage.players`, `.inventory`, `.count`, `.local` in `port/src/ge_original_multiplayer_players.c`. | Encode active slot count, stable player/prop IDs and shuffled simulation order. Preserve the canonical current-player and input-selector execution cursors at the hash boundary; validate all related bindings together. `.local` is device presentation selection; saved previous-session pointers are teardown state, not the match. |
| Movement/combat: `struct player` in [`bondview.h`](../src/game/bondview.h). | Include `pos`, `pos2`, `offset`, `pos3`, `current_model_pos`, `previous_model_pos`, `current_room_pos`, `stanHeight`, collision/vertical interpolation fields, `crouchpos`, `ducking_height_offset`, `collision_bounds`, `vv_theta`, `vv_verta`, angular/sideways/forward speeds, centering/autoaim state and target IDs, `prev_buttons_pressed`, `bondhealth`, `bondarmour`, death state, invincibility and input-lock/interaction state. Resolve `cameratile`, current/previous collision tiles and prop/target pointers through validated STAN/actor IDs. Derived-looking angles, aim/view transforms and old position values cannot be omitted while movement or firing reads them. |
| Weapons: each `player.hands[2]`, [`ge_original_gun_live_tick`](../port/src/ge_original_gun_live.c), original [`gun.c`](../src/game/gun.c). | Include weapon/current/pending selection; `weapon_firing_status`, hold/action/animation state and timers, magazine ammo, trigger history/hand selection, volley, spread/noise, recoil/sway/aim and throw transforms consumed by firing, rocket reference and ownership. Enumerate unresolved hand fields and the embedded native model region beginning near `field_B68`; integer fields such as `modeldatas` can carry addresses. Render handle/matrix storage must not accidentally become serialized integers. |
| Inventory: `InvItem`, `invitem_weap`, `invitem_prop`, `invitem_dual`, `player.p_itemcur`, `ptr_inventory_first_in_cycle`, `equipcuritem`, `equipallguns`, `equipmaxitems`, `ammoheldarr[30]`, `gunheldarr[10]` in [`bondview.h`](../src/game/bondview.h); original [`bondinv.c`](../src/game/bondinv.c). | Encode all occupied entries, discriminated payloads, weapon pairs, referenced prop IDs, cycle order and current selection. Fixed slot index plus generation identifies reused entries. The touchscreen `GeOriginalInventorySnapshot` pointer identities, display names and epoch are a UI freshness contract, not a serializable inventory. |
| Canonical input: [`joy.c`](../src/joy.c) `g_ContData`, `g_ContDataPtr`; pending adapter `g_pending_pads[4][2]`, `g_input_count`, `g_input_player`, `g_raw_pads` in `port/src/ge_original_input.c`. | At a post-consumption boundary retain the logical controller history needed by the next tick: pad state, ring indices (`curlast`, `curstart`, `nextlast`, `nextsecondlast`), button edges and playback count, or prove a smaller reconstructed history equivalent. Controller `p` and `p+count` belong to player `p`. Received quantized pads must bypass local sensitivity/rescaling. `GeOriginalBondInputProvider` control style, aim/inversion options, action locks and timers are gameplay inputs; agree them per slot. |
| Actor allocation/order: [`chrprop.c`](../src/game/chrprop.c) `g_Props`, `g_FreeProps`, `g_ActivePropsHead`, `g_ActivePropsTail`, `RoomPropListBlockIndices`, `RoomPropListBlocks`; [`PropRecord`](../src/bondtypes.h); [`GeOriginalStageActiveProps`](../port/include/ge_original_stage_active_props.h). | Include live/free slot membership, generations, active iteration order, parent/child/sibling links, type, gameplay flags, `timetoregen`, position, STAN reference and room lists. Room-query ordering can affect the selected interaction/collision result. `pre_tick_pending` gates a real AI tick and must be zero at the chosen boundary or encoded. Diagnostic tick/allocation counters are separate. Do not sort away original list order just because actor records use stable ID order. |
| Characters/background AI: [`ChrRecord`](../src/bondtypes.h), `g_ChrSlots`, `g_NumChrSlots`, `g_ActiveChrs`, `g_ActiveChrsCount`; [`ge_original_stage_mission_runtime.c`](../port/src/ge_original_stage_mission_runtime.c). | Include `actiontype` and its **matching** `act_*` union, movement/ground/fall state, health/damage, flags/hidden state, rates, sleep, fire counts, aim/flinch counters, hearing/last-seen positions and times, target/shooter references, `ailist` ID plus `aioffset`, `aireturnlist`, `timer60`, random byte, morale/alertness/presets, weapon/hat ownership. Include background actors even without visible bodies. AI globals include `objectiveregisters1`, `g_AiAccuracyModifier`, `g_AiDamageModifier`, `g_AiHealthModifier`, `g_AiReactionSpeed` and applicable seen-guard counters. |
| Animation with gameplay consumers: [`Model`](../src/bondtypes.h), typed `ModelRwData_HeaderRecord`, [`GeOriginalPlayerGait`](../port/src/ge_original_player_gait.c). | Encode animation asset IDs, root position/ground, scale, frame/blend/loop/rate/speed state, pending transitions, attachment IDs and gait `previous_root` / `have_previous_root`. Hit regions/root motion and attack timing may depend on these. Rebuild frame-cache indices, decoded animation slots and renderer matrices only after proving they are caches. Tag supported callbacks by stable function role, never address. |
| Objects/pickups/doors: [`ObjectRecord`, `WeaponObjRecord`, `AmmoCrateRecord`, `DoorRecord`](../src/bondtypes.h), [`ge_original_prop_state.c`](../port/src/ge_original_prop_state.c), [`ge_original_door_runtime_source.c`](../port/src/ge_original_door_runtime_source.c). | Encode authored command/model/pad IDs, lifecycle and activation/destruction/deposit flags, live ownership bits (`runtime_bitflags` owner), damage, transform/collision state, pickup/regeneration and ammo payloads. Doors add `frac`, `openPosition`, `speed`, `openstate`, lock keys, auto-close/last-event timers, paired-door IDs and authoritative portal controls. A renderer publication generation is not an actor lifetime generation. |
| Projectiles/explosions: [`chrprop.c`](../src/game/chrprop.c) `g_Projectiles`, `g_Embedments`, `g_WeaponSlots`, `proxy_mine_table`; [`Projectile`](../src/bondtypes.h); [`explosion.c`](../src/game/explosion.c) `g_ExplosionBuffer`, `g_NumExplosionEntries`. | Include occupied/free state, owner/object/embedded-actor IDs, velocity/position/rotation, age/fuse/drop type, collision history/rooms, mine activation and damage/attribution state. Enumerate unnamed projectile members by actual tick consumers. Establish which systems are enabled in the first playable arena; reject an unsupported live kind instead of silently hashing it as empty. Audio handles may be excluded, but a sound-associated timer requires a consumer audit first. |
| Rules/statistics: [`GeOriginalMultiplayerConfig`](../port/include/ge_original_multiplayer_rules.h), `scenario`, weapon-set selection, [`player_data`](../src/game/player.h), [`mpmenu.c`](../src/game/mpmenu.c) `get_points_for_mp_player`. | Agree arena/setup, scenario, weapon set, teams, limits, character/body/head choice, handicaps and gameplay options. Encode `kill_counts[4]`, `kill_count`, `killed_gg_owner_count`, `have_token_or_goldengun`, `flag_counter`, `order_out_in_yolt`; preserve other `player_data` metrics needed for final awards/results. Scores alone do not retain death attribution or scenario state. |
| Respawn: [`ge_original_multiplayer_rules.c`](../port/src/ge_original_multiplayer_rules.c) `g_Startpad[16]`, `startpadcount`, `starting_weapon[2]`, validated intro/pad bindings; original `mp_respawn_handler`. | Encode ordered spawn pad IDs, selected intro variant and replay/recording slot, per-player `field_29E0`, life start/kills, eligibility and pending request. **`deathanimfinished`, `redbloodfinished`, `colourfadetimemax60` gate respawn**: these must advance deterministically for every player even when only one local view is rendered. The spawn parser's retained pointers map to immutable setup IDs and bounded record counts. |
| Clocks/global gates: [`lv.c`](../src/game/lv.c), [`ge_original_bond_live.c`](../port/src/ge_original_bond_live.c), [`GeOriginalBondInputProvider`](../port/include/ge_original_bond_input_provider.h). | Include canonical `g_GlobalTimer`, `g_ClockTimer`, `g_GlobalTimerDelta`, match elapsed (`D_80048394` or its session-owned replacement), `g_MpTime`, `g_MpPoint`, pause/controls/game-over/stop-play gates and end decision. Define one advancement order for background AI, each player's input/movement/guns, props/damage/pickups, respawn and rules. Exclude wall-clock/retrace/suspend duration. |
| RNG: [`random_port.c`](../port/src/random_port.c) `g_randomSeed`; [`ge_original_chr_obj_random.c`](../port/src/ge_original_chr_obj_random.c) `g_chrObjRandomSeed`. | Capture **both full 64-bit states**, plus the stored `random_byte` and per-actor random fields. Pending `ge_original_multiplayer_players_create` takes a 32-bit seed; `GeMpConfig.rng_seed` is 64-bit. Specify expansion and initial draws explicitly and reset both streams for every match. No implicit truncation or previous-match carryover. |

Renderer-only room unions, GPU/texture caches, transient vertex arenas, NDSP
buffers, profiler counters and local UI state are excluded only when no next
simulation step reads them. Canonical `PROPFLAG_ONSCREEN`, autoaim inputs,
death/fade completion and animation root motion are **not** covered by that
blanket exclusion. Existing mono guard muzzle flashes still call
`randomGetNext`; extra eye-only flashes use a local seed. A multiplayer runtime
must isolate all presentation RNG or deterministically own those canonical
draw-time calls before local cameras/render rates can differ.

## Proposed stable references and pure encoder contract

Use the existing `ge_mp_actor_id(kind, owner, index, generation)` layout:
kind:8, creation namespace:8, index:16, generation:32. World allocation uses
owner 255; player namespaces use 0–3. **Creation namespace is immutable**;
current weapon carrier/killer/team is a separate field. Authored actor indices
come from setup command indices, player actors from roster slots, and dynamic
actors from deterministic registry slots. Reuse increments generation; zero,
wrap, duplicates and unresolved references reject a snapshot. Match/session
identity scopes IDs across restarts. Serialize free/active ordering alongside
the registry so the next allocation is reproducible.

Immutable references use a typed asset key plus bounded index/offset: STAN
tile index, pad index, AI list ID/instruction offset, animation ID/frame,
model/node index, setup command index. Do not serialize addresses, pointer
differences without validated ownership, sound handles, C enum widths, bitfield
layout, unions' inactive bytes or allocator padding.

Proposed APIs, **not implemented or agreed**:

```c
GeMpStateStatus ge_mp_canonical_snapshot(const GeMpSession *session,
    GeMpSnapshotStorage *storage, GeMpCoverageReport *coverage); /* native owner */
GeMpStateStatus ge_mp_state_encode(const GeMpCanonicalSnapshot *snapshot,
    GeMpByteSink *sink, GeMpCoverageReport *coverage);            /* pure writer */
GeMpStateStatus ge_mp_state_sha256(const GeMpCanonicalSnapshot *snapshot,
    uint8_t digest[32], GeMpCoverageReport *coverage);
```

The native provider owns field classification, stable reference resolution and
an atomic post-tick snapshot. The independent encoder owns versioned tagged
records, explicit big-endian integers, fixed-width booleans/enums, bounded
counts and SHA256 of the exact encoded bytes. Preserve IEEE-754 binary32 bits
(including signed zero) unless an agreed simulation rule normalizes them;
reject non-finite gameplay values. Avoid quantization that conceals drift.
Reference validation is two-pass so forward references are permitted but
dangling/recycled references are rejected. Restore/snapshot authority is a
separate transaction API with validation before any live-state publication.

`coverage` must identify schema, enabled systems, actor counts/kinds and
unclassified fields/references. Missing coverage returns an explicit
`INCOMPLETE` status and cannot reach `finish_tick` as certified gameplay state.
A pure writer can be tested first with fixtures without granting the native
provider that certification.

## Requirements for the multiplayer runtime owner

1. Freeze the session API, enabled arena/weapon/scenario systems and exact
   post-tick boundary. Enumerate all state owners above, including statics and
   pending lifecycle work; document every exclusion with its consumer proof.
2. Run two/four native players locally with distinct input/inventory/body
   ownership, real damage/pickup/death/respawn/results, deterministic ordering
   and reset of both PRNG streams. Preserve source-based rules, not a fixture's
   simplified scoring model.
3. Supply constructor/destructor/attach/detach registry hooks and manifest
   identity for all borrowed assets, including sidecars. Unknown live actor,
   action union or callback kind must fail visibly.
4. Prove encoded bytes ignore host addresses, padding, camera selection,
   stereo slider and render repetition, while mutations of every covered
   future-affecting field change the appropriate record/digest. Test local
   player cursor restoration and immediate slot reuse explicitly.
5. Compare next-tick behavior after snapshot restore, then sustained two/four
   independent native simulations driven by the same quantized inputs through
   spawn, pickup, fire, death, respawn and match end. Exercise drop/duplication,
   stale session/tick, corruption, disagreement and interrupted snapshot
   publication only after the full native coverage gate passes.

No code in this audit claims that a player-position/health/score digest,
transport model checksum or incomplete snapshot certifies the game simulation.

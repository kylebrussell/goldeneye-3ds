# Native inventory and permanent player-state export

`ge_mp_state_inventory.h/.c` exports a bounded native domain into the typed
`ge_mp_state.h` record format. It reads the actual multiplayer player owner,
all four `player_data` records, and all four arrays of 30 `InvItem` records.
It never serializes native struct memory, padding, addresses or union bytes
without a classified active member. It does not attest a complete game state.

## Identity and caller lifecycle

Kind `0xE1`, namespace player 0..3, slot 0..29 identifies **physical inventory
slot storage**, including free and never-used slots. Kind `0xE2`, namespace
player 0..3, slot 0 identifies a permanent player-data record and its inventory
owner fields. All 124 records are defined/acquired once in
`ge_mp_state_inventory_begin(&owner, &registry)` and released in
`ge_mp_state_inventory_end(&owner)` **before** native player destruction.
Zero-initialize `owner` before its first use. Reuse released registry keys to
increment generations for a subsequent native storage session. The registry
needs room for all 124 entries in addition to other native domain records.

Pickup, removal, inventory reset and respawn change physical contents, not
physical identities. They do not acquire/release an E1 ID. Inventory
next/previous/head/base references and active/free order sequences therefore
use `SLOT_REF` registry keys, not item-actor lifetime references. Free order is
the original allocator's ascending scan of `type == -1` within
`equipmaxitems`; active order preserves the original ring starting at its
actual head. Slots remain present even when they are outside the active
player count or not on either list.

The new narrow `ge_original_multiplayer_inventory_storage` getter returns
const inventory/owner pointers, fixed capacity, active-player classification
and a host lifetime guard. The lifetime increments on native storage creation
and is **not encoded**: it is solely a local ownership guard, like a borrowed
pointer lifetime, with no match simulation meaning. Export checks lifetime,
base, owner, capacity and player count, catching missed teardown even when an
allocator reuses the same address. Export does not create or repair lifetimes.
An owner with missed teardown can still release its registry entries before
being replaced; stale exports remain rejected.

## Typed coverage

E1 exports type, physical next/previous slot references, and exactly the
active union member: single weapon ID, dual right/left IDs, or registered prop
actor reference. Known `INV_ITEM_NONE` (zeroed, never-bound storage) and `-1`
(free storage) have no active union member. Their stale union payload is
omitted because original `bondinvAddInvItem`, `bondinvAddDoublesInvItem` and
`bondinvAddPropToInv` overwrite the selected member before it can be read.
Their type and stale slot links are retained. Unknown types, including the
unused `INV_ITEM_PICKUP` enum value, or unknown weapon IDs return INCOMPLETE.
No inactive pointer bits are interpreted as weapon IDs or vice versa.

E2 exports every named member of `struct player_data`:

- All seven shot counters, total kill count, Golden Gun owner kills and all
  four directed kill counters.
- Time other players were visible, distance traveled, backside damage,
  armour pickups, minimum/maximum kill intervals, longest/shortest innings,
  most killed in one life and most killed at one time.
- Handicap, token time counter, perspective height, YOLT elimination order,
  the original shared team/token/Golden Gun byte, autoaim, sight and civilian
  kills.

It also exports the inventory owner's active flag, storage capacity/base,
ring head, `equipmaxitems`, `equipallguns`, `equipcuritem`, all 30 held-ammo
values, all ten `gunheldarr` weapon-pair/time histories, death and suicide
counts, kills this life, life-start timer and four recent kill times. Active
players include the exact signed original multiplayer score as a derived
field; inactive players have no score value. A conservative signed-sum bound
prevents invoking the unchanged score function with overflowing native
counters. Finite binary32 bits, including negative zero, are preserved.

Native `player_data` and inventory union-member layout assertions require
review when those structures change. Native `textoverrides` is an explicitly
unclassified optional domain: a nonnull chain returns INCOMPLETE rather than
inventing object/text identities or dropping the state. Whole `struct player`
state, live hand/gun state, prop/object/projectile bodies, model/animation
state, rules configuration and session/input/RNG singletons remain separate
owners. Only the INVENTORY system bit is reported as enumerated on success;
E2's partial player/rules coverage does not attest either complete system.

## Export and integration

Call `ge_mp_state_inventory_export(&owner, &providers, &output)` while native
state and the registry are immutable. The required `prop_actor` callback maps
a native prop identity through its owning registry domain. Export verifies
the returned ID is live. The final aggregate snapshot must also contain that
prop's complete record; this exporter does not supply a placeholder prop.
Missing/unknown/stale prop references return INCOMPLETE with reference gaps.
Malformed or unclassified inventory state clears `record_count`, reports gaps
and cannot be used as a successful domain export.

The returned 124 records are sorted by stable ID and borrow typed arrays
inside `output`. Keep the output at the same address and immutable through
encoding. Merge the two schemas returned by
`ge_mp_state_inventory_schemas(&count)` in kind order into the aggregate
schema; retain the global record and all other registered domain records.
This module never sets `schema.complete`. Its successful domain status is
insufficient to enable transport finish-tick/state hashing for the full game.

The runtime only adds `ge_mp_state_inventory.c`; compile it with the same
original type/setup/AIPARSE/US ABI flags as multiplayer players/rules. No new
runtime source extraction or gameplay hooks are required beyond explicit
begin/end and the prop identity provider.

## Initialization and verification

Existing `players_create` allocates zeroed native storage, clears all four
permanent records, restores canonical default perspective/handicap, seeds
the agreed RNG streams and executes the original player initializers.
Existing `player_bind_inventory` clears a row, after which the original intro
inventory path calls `bondinvReinitInv` and inserts the agreed loadout.
Agreed participant permissions/options must still be applied by their owner
after creation. No history is erased by this exporter to force convergence.
The original BeginLife life-start timer also depends on the correctly reset
shared mission clock, already owned by stage/session initialization.

`scripts/test_mp_state_inventory.sh` links actual native multiplayer creation,
original inventory insert/sort/dual/prop/remove and original kill/death/suicide
counter functions. The fixture verifies 2/4 players, all 124 records including
unused rows/free slots, all permanent-field and inventory-owner array mutation
sensitivity, signed negative score bits, negative zero, ring/free links, prop
resolution, incomplete variants and text overrides, registry capacity and
generation exhaustion, ID stability on removal/reuse, and missed teardown.
It verifies typed initial payload convergence across 2 -> 4 -> 2 native
sessions with different menu/prior-match histories while retaining meaningful
agreed autoaim changes. Fixture-only typed payload fingerprints are explicitly
not complete native-state digests; a full-state hash request remains
INCOMPLETE with `schema.complete == 0` and preserves the output digest.

ASan/UBSan pass. Focused strict ARM compilation passes for the exporter and
modified player owner, and the original player allocation/input ownership
regression passes for 2, 3 and 4 players. No emulator or shared assets are used.

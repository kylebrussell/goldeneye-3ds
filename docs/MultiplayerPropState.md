# Native prop pool state export

`ge_mp_state_props.h/.c` exports the actual 600-slot original `PropRecord`
pool and its active, free, child and room lists into `ge_mp_state.h` typed
records. It never exports pointer addresses, padding or unclassified union
bytes. It does not attest a complete game state or enable network state hashes.

## Identity and lifecycle

Kind `0xE3`, namespace 255, slot 0..599 identifies a physical shared prop-pool
slot. Each successful original `chrpropAllocate` acquires a new generation;
each original free releases it. Allocation and reuse between snapshots are
observed individually. Player selection never changes creation namespace.
Current parent, body/player, object and other ownership are separate typed
references. Kind `0xE5`, namespace 255, slot 0 owns stage pool/list metadata.
`0xE4` remains reserved for the hand/gun domain.

Zero-initialize `GeMpStatePropsOwner`. Call
`ge_mp_state_props_begin(&owner, &registry)` immediately after successful
`ge_original_prop_state_reset`, before any player/object/effect allocation,
including allocations that subsequently fail and roll back. Begin requires
an empty active list and complete 600-slot free list. It defines 600 E3 keys
and one E5 key, acquires E5, and binds the optional observers. The registry
needs 601 entries in addition to every other domain's entries. E3 entries
remain defined while free; their generations persist across reuse.

Call `ge_mp_state_props_end(&owner)` after complete native object/player/effect
cleanup and before the next reset. End releases any residual IDs and unbinds
even on failure. Remaining live IDs, a missed lifecycle callback or a reset
while bound returns failure; callers must surface it. It cannot silently
repair history or turn an incomplete lifecycle into a successful snapshot.
A monotonic local allocation serial rejects late binding even after all prior
allocations have been freed. Local pool lifetime and serial guards are not
encoded simulation state; the registry generations carry allocation history.

The canonical allocator body remains unchanged behind its public wrapper.
The wrapper observes successful allocations after the native free-head pop.
The canonical free slice invokes an observer before changing free links.
Both native grenade and covert-modem constructor rollback helpers invoke the
same observer before their unchanged free bodies. Reset observes before
mutating storage. Registry failures invalidate the domain but do not cancel
or replace canonical gameplay operations. On ELF and Mach-O, weak no-op
observer definitions keep standalone/unbound solo slices linkable; linking
this domain replaces them with strong observers. The missing-serial fallback
reports unavailable, never a fabricated serial value.

## Fields and exact order

E3 emits every named native `PropRecord` member: type, flags, signed 16-bit
regeneration timer, selected union reference, position, STAN reference,
`zDepth`, parent, child, previous/next links, all four room bytes and signed
`unk30`. Child order follows original `parent->child` (newest child), then
`prev`; reverse `next` links and parent identities are checked. Disabled props
remain live records, including retained pickup regeneration timers. An
allocated prop need not be enabled or currently on the active list.

Known live tags select object, door, weapon, character/viewer, explosion or
smoke union members. Exactly the selected member has a field value; inactive
union fields have zero elements. Unknown tags, including unused `NUL` and
`PLAYER`, return INCOMPLETE. The unused scorch union alias has no classified
live prop tag. VIEWER also resolves its unique native `g_playerPointers` owner.
Finite float bits, including negative zero, and signed integer bits survive
unchanged. Rooms must have a terminator within the four-byte native array;
all four bytes, including the tail after the terminator, are exported.

E5 emits active head/tail and head-to-tail order, free head and the exact
`g_FreeProps -> prev` allocation order, room capacity, all 256 room heads,
all 256 block free flags and next links, each of the 15 entry holes in every
used block, each room's member count, and member order in original block/entry
scan order. Free-order and block-membership references use physical SLOT_REF
keys; live active/child/room members use generation-bearing ACTOR_REF IDs.
Lists are bounded and checked for invalid pointers, cycles, aliases and
references to free props. Used room blocks must belong to one reachable room
chain; inactive room heads must be empty.

Free props have no E3 actor record. Their allocator-order-aligned E5 arrays
conservatively retain stale typed type, position, z-depth, room bytes and
`unk30`, since allocation does not initialize all of these scalars. Free
union pointers are omitted: original `objInit`,
`init_GUARDdata_with_set_values`, `bondviewLoadSetupIntroSection`,
`explosionCreate` and `explosionCreateSmoke` overwrite the selected tag/payload before
normal use; failed construction returns the slot to the free list. Native
allocation resets links, flags, STAN, timer and room terminator before use.
Free-list `prev` is represented by free order. A free room block is marked by
first entry `-2`; its other stale words are omitted because original
`chrpropInitializeNewChunkForRoom` overwrites all 16 entries before reuse.

## Integration contract

`ge_mp_state_props_export(&owner, &providers, &snapshot)` requires immutable
native state and registry for the call. The required typed `reference`
callback maps object, door, weapon, character, explosion, smoke, STAN and
player pointers to live registered identities. Null references are explicitly
nullable. Nonnull unknown or stale references return INCOMPLETE; this module
never emits placeholder objects, players or effects. The aggregate encoder
must also receive complete records from each referenced owning domain.

`ge_mp_state_props_actor(&owner, native_prop, &actor_id)` is directly usable
as the inventory domain's `prop_actor` callback. It resolves only a currently
live physical pool slot. Output remains unchanged on failure.

The returned up-to-601 records are sorted by stable identity and borrow typed
arrays within the snapshot. Allocate the large snapshot in persistent static/heap storage, keep it at
the same address and immutable through encoding. Merge the two `ge_mp_state_props_schemas` entries with other domains
in kind order. Only PROPS is reported as enumerated. Whole player/body,
object/projectile, character/AI, model/animation, effects and immutable STAN
identity owners remain separate requirements. This module never sets
`schema.complete`; successful prop export alone cannot produce a full-state
digest.

Add `ge_mp_state_props.c` using the original type/setup/AIPARSE/US ABI flags.
Rebuild the existing prop-state source wrapper and regenerate the existing
bond move/explosion slice so the free observer is present. The optional guard
dynamic-spawn fixture slice is likewise instrumented. No main-loop, gameplay
mechanics, allocator decision or original source body is replaced.

## Verification

`scripts/test_mp_state_props.sh` links actual pool allocation, active-list,
room insertion/removal, child reparent/detach and instrumented canonical free
bodies. It also executes both real native constructor rollback free helpers.
ASan/UBSan covers all 600 allocations and exhaustion, free/reuse between
snapshots, disabled timer retention, block boundaries and holes, ordered
children, typed 2/4 player/viewer references, all classified union variants,
scalar/array mutation sensitivity, negative float/integer bits, stale free
union exclusion, missed free/reset/late-begin and registry exhaustion failures.
Fixture-only typed payload comparisons are not game-state checkpoints: the
fixture confirms a full-state hash remains INCOMPLETE with the output digest
unchanged.

Focused strict ARM compilation passes for the exporter, state getter/source
wrapper and both modified rollback owners. Host original multiplayer spawn
and inventory regressions pass; the explosion exactness test still verifies
109 original bodies/data after removing its one explicit optional free
observer. No emulator or shared assets are used.

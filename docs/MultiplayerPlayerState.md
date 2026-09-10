# Native player E6 state export

`ge_mp_state_players.h/.c` reads the actual native `struct player` owner.
The generated inventory classifies all 1,500 named US player members; the
E6 schema has 404 fields and 59 reference values. All owned scalar and array
elements are copied through explicit typed member expressions. Native struct
bytes, padding, pointer addresses and model/display-list alias words are
never serialized as integer payloads. This is a domain exporter, not a
complete-state attestation.

## Caller lifecycle and physical rows

The existing multiplayer allocation owns four physical player rows from
`players_create` through `players_destroy`, even in a two-player session.
Only active rows run the original initializers and enter `g_playerPointers`;
inactive rows remain unpublished calloc storage. E6 preserves their actual
owned fields and records `active_player`. It does not invent initialized
models or hands for those rows.

Zero-initialize `GeMpStatePlayersOwner`, then call
`ge_mp_state_players_begin(&owner, &registry)` after native player creation.
It defines and acquires four keys: kind E6, namespace player 0..3, slot 0.
Call `ge_mp_state_players_end(&owner)` before native player destruction.
Each new storage lifetime acquires a new registry generation. End releases
IDs even when returning STALE after a missed destroy or invalidated lifetime;
that failure must be surfaced. The existing read-only inventory storage
getter supplies all four base pointers, capacity, active-row classification
and host lifetime guard. E6 verifies those along with the published roster.
The host lifetime is not encoded; it detects stale borrows and allocator ABA.

`ge_mp_state_players_actor(&owner, native_player, &id)` resolves any of the
four physical rows. It is usable from the prop domain's VIEWER/player
reference provider. Output remains unchanged on failure. E6 registration
can precede E2/E4 capture; all referenced owners must exist before a successful
export or aggregate encoding.

## Owned typed state

E6 copies camera position/orientation, room and collision state, movement and
head control fields, life/health/armour display state, watch/menu controls,
weapon trigger/aim/sway controls, camera/viewport parameters, cheats/input
history and every remaining named scalar outside the explicitly owned
external ranges. Geometry aliases use their canonical float/short component
members. Viewports use `vp.vscale/vtrans`; colour structures use their named
byte arrays. These aliases have equivalent component meanings rather than
an unknown active union payload.

Player-level `hand_invisible`, `hand_item`, `field_2A44` and `lock_hand_model`
are E6 fields. Signed enum values such as ITEM_NOTHING are widened by value,
so native and short-enum builds have identical typed payloads. Copied
`ModelFileHeader` values include all four count fields, radius and `isLoaded`.
Root node, skeleton, switches and texture table are separate typed references.
Both hand-load buffer pointers and each texture pool's start, end, left and
right cursor are captured as owner identities plus stable byte offsets.
The global allocation-size tables are separate gun/resource configuration,
not members of this player domain.

E2 owns inventory base/head/equipment/ammo, held-weapon history, text overrides
and per-life/death/stat counters already covered there. E6 emits an E2 owner
reference instead of duplicating those fields. All two embedded hands in all
four physical rows are E4 references. E4 owns their hand fields, model prefix,
matrices and tagged RW data; it exports no player-level hand-control or copied
header/load fields. An inactive hand whose model state has not been classified
still leaves E4 and the aggregate incomplete.

## Reference and overlay boundaries

The required provider receives player index, field ID, reference kind, native
pointer and an optional storage extent. It returns a live registered owner ID
and stable byte offset after validating the pointer against the owning domain.
Content/model/matrix/texture owners must validate their own extent and offset;
texture-pool end may be a legitimate one-past boundary. E2/E4/E3 identity
references require zero offset and their reserved key/namespace conventions.
Null pointers are explicit nullable references and bypass the provider.
Neither allocator addresses nor the currently selected viewport choose IDs.

The final aggregate must include the referenced owner's typed record. A
resolver identity alone is not proof that its model/content/animation payload
has been exported. Unknown or stale references cannot produce a successful
E6 export. These six storage ranges require separate typed owners:

- `something_with_watch_object_instance` through the member before
  `buttons_pressed`: original watch Model and associated storage. Even named
  fields such as `watch_scale_destination` alias the model's fields.
- `model` through the member before `bondheadmatrices`: original gait Model
  and RW storage. Despite its declaration, `model` is the embedded Model
  starting address, not a pointer value. Original `initBondDATAdefaults` also
  passes `field_654` as its RW arena.
- Armour and health watch command buffers, each ending at the next buffer.
- Green backdrop and static watch command buffers, including their following
  nominal `field_*` integers, ending at the next buffer or `healthdamagetype`.

These owner references preserve storage identity; E6 never treats an alias
word as an ordinary I32. Complete model, RW, animation and typed command
classification remains work for those owning domains. The generated member
inventory names the owner for every field in each range.

Seven additional nominal s32 fields hold camera pointers: `field_5C/60`
(fixed matrices), `field_64/68` (float matrices), `field_10E0` (fixed projection
matrix), `field_10E4` (look-at lighting), and `field_10D0` (unclassified target).
They are never emitted as I32. The first six require typed reference resolution.
`field_10D0` has no classified use beyond its pointer accessor in the current
canonical source, so a nonzero value unconditionally reports UNKNOWN and
INCOMPLETE. The two nominal s32 multiplayer award text fields similarly require
text/content resolution. Legacy 32-bit pointer values are passed to resolvers
without dereferencing them; unrecoverable host pointer identity stays incomplete.

`players_cur_animation` is also a legacy 32-bit reference, with the dedicated
ANIMATION provider kind. Despite its s32 declaration, original `playerTick`
compares this member to animation-table addresses and stores the address passed
to `modelSetAnimation` (`bondview2.c`). Zero remains an explicit null sentinel.
Each nonzero token requires a validated animation owner plus stable byte offset;
unmapped or unrecoverably truncated host addresses leave a reference gap.
The exporter never guesses an animation index from an address word. E6 schema
version 2 changes field 591 from I32 to ACTOR_REF, adds its offset as field 592,
and advances subsequent generated field IDs by one. Prior E6 schema payloads
must not be interpreted with this layout.

## Export, gaps and source inventory

Call `ge_mp_state_players_export(&owner, &providers, &snapshot)` while native
storage and the registry are immutable. OK reports only PLAYERS as enumerated.
An unresolved field/reference returns INCOMPLETE and retains four diagnostic
records, with zero elements for unavailable fields. `gaps` identifies player,
field ID, reference kind, failing status and reason;
`ge_mp_state_players_field_path` returns its native path. `total_gaps` retains
the full count if the bounded detail array fills. Stale lifecycle and
unsupported-build failures do not publish successful domain records.

Finite binary32 bits, including negative zero, and signed integer bits are
preserved. Non-finite fields return actionable gaps. This schema targets the US ABI; other native variants require separate
classification. DEBUG state explicitly reports an incomplete build gap. Keep the snapshot in persistent
static/heap storage, at the same address and immutable through encoding.
Merge its schema in kind order with the global record and every other domain.
This exporter never sets `schema.complete`.

`scripts/generate_mp_state_players_fields.py . --check` verifies the committed
schema/accessors, per-element mutation list and complete member inventory
against the current declaration. The generator rejects unknown declarations
and types. Changes require renewed source-use review, especially nominal
integer fields that may actually be pointer or aggregate aliases. Reviewed
source boundaries are in `bondview.h`, `bondview2.c`, `bondview.c`,
`initBondDATAdefaults.c`, `bondhead.c`, `gun.c`, `mpmenu.c`, `bondtypes.h` and
`image.h`. No canonical gameplay body is altered.

## Verification

`scripts/test_mp_state_players.sh` builds actual native player creation and
initialization plus the existing inventory owner under ASan/UBSan. It runs
independent two- and four-player processes with different selected local slots
and compares typed payload bytes; it repeats under native and short-enum ABIs
and compares those payloads too. Each scenario mutates all 6,688 owned scalar
elements across the four physical rows, including inactive rows, model-header
metadata, hand-control values and display-vertex components. It also checks
cross-player prop/model/node references, texture cursor offset sensitivity,
negative-zero/signed enum bits, missing and stale references, unknown legacy
pointers, lifecycle retirement/generation advance and ABA rejection.
Animation reference checks write the actual allocated native player member,
reject unknown/stale/truncated identities, and verify that two legacy addresses
mapped to one stable animation owner and offset produce equal typed values.
Changing the canonical offset changes the exported values. External
fixture providers supply resource identities only, not fake model or effect
payloads. A complete-state hash remains INCOMPLETE and leaves its digest
unchanged.

Compile `ge_mp_state_players.c` with the existing original type/setup/AIPARSE/
US ABI flags. Focused strict ARM compilation passes. No main/Makefile changes,
emulator execution or shared asset writes are part of this domain change.

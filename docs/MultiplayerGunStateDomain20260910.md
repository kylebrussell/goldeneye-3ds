# Native gun/hand domain — field and reference inventory

Reserved identities: kind `E4`, creation namespaces 0–3 for canonical player
slots, slots 0/1 for the two persistent hands. Lifetimes belong to the canonical
player owner; switching weapons and resetting per-life values do not create new
hand identities. This exporter accepts caller-supplied generation-bearing IDs
and installs no allocation/reset hooks. Other reservations remain `E0` core,
`E1` inventory, `E2` permissions, `E3` prop lifetimes, and `E5` allocator/room
metadata (namespace 255, slot 0). `E6` is reserved for outside-hand native player
state; E4 exports none of those outside-hand fields.

`port/include/ge_mp_state_guns.h` defines the explicit field IDs, typed callbacks
and `ge_mp_state_guns_capture` contract. The native input is the actual canonical
`const struct player *`, with both `player->hands[]` captured without changing
current-player selection. Whole simulation coverage remains incomplete.

## Direct hand fields

`src/game/bondview.h:struct hand` declares 130 members. The direct capture
manifest contains 97 named primitive fields after expanding `weapon_beam` and
excluding references and the embedded model/RW overlay. These include weapon
selection/action/animation/ammo/timers, all firing/status bytes, all sway and
spring components, blend histories and cursor, offsets, noise, rocket-fired
state, all four inline 4×4 matrices, coordinates, volley and every nominal
unknown scalar outside the overlay. Unknown names do not justify skipping their
values. Scalar `ITEM_IDS` values use I32, signed bytes use I8, native F32 values
retain exact finite bits, and plain `char` beam bytes preserve octet values.

`BeamRecord` has no pointer or discriminated union: its four byte fields,
position/delta and four scalar floats are individually named. Coordinate unions
provide alternate x/y/z and `f[3]` names for the same three float components;
matrix unions likewise expose the same 16 floats. Array order is preserved.

## Implicit overlays and required interpretations

The declarations conceal an important aliasing boundary. `gunfire.c` constructs
`(Model *)&hand->field_B68`, calls `modelInit` and points its RW base at
`&hand->modeldatas`. Thus `field_B68..field_B84`, including `mtxlist`, are the
first **0x20 bytes** of the original 32-bit Model prefix. They must be exported
as named prefix fields: two I16 values, character/header/matrix/RW references,
scale, attached-model and attached-node references. Exporting the nominal s32
members would serialize addresses or misclassified pointer words.

`modeldatas..field_C04` are 32 original RW words. Reading the full `Model`
animation tail would reinterpret those words as animation/script pointers and
also exceed the embedded model's actual prefix. `modelInit` initializes the
prefix and RW nodes; `animInit` is a different operation. Host sanitizer builds
use `ge_original_gun_frame_arena.c`'s separate per-player Model/RW owners; the
exporter's explicit model-view resolver must identify the actual owner for the
ABI. It must never call the resetting `ge_original_gun_host_model()` or RW-data
allocator just to inspect state.

`modelGetNodeRwData` selects interpretation using authored node opcode and RW
word index, with special traversal through head-placeholder RW bases. The
explicit view contract preserves node/content identity, opcode and original word
coordinate, then exports selected named members for:

| Tag/opcode | RW interpretation |
| --- | --- |
| 1 | `ModelRwData_HeaderRecord`: three signed bytes, ground, positions, angles and interpolation floats. |
| 101 (explicit alias tag) | `modeldata_root`: U16/byte prefix and U32 `unk1c`, with named float/coordinate members. Its first word and `unk1c` differ from the Header C view; an owner must classify this alias rather than treating all words as floats. |
| 4 | DisplayList `unk00` I32 (the original declaration remains only partially understood). |
| 7 | Op07 index I32. |
| 8, 9, 18 | LOD/BSP/Switch visibility, preserving the native signed 32-bit representation. |
| 11 | Two I16 values. |
| 12 | Gunfire I16 visibility and U16 secondary value. |
| 23 | Head-placeholder model-header and RW-base references. |
| 24 | Collision-display-list vertex and display-list references. |

Unknown tags, malformed spans, overlaps and unclassified RW words remain explicit
omissions. An owner may classify unused words only after checking the actual
native model/content layout. No raw word array, union byte dump or padding is
hashed. Model animation/script state beyond the prefix belongs to a separate
validated owner; it is never inferred from the hand's RW storage.

## References and resolver obligations

The actor resolver receives the actual non-null pointer plus an explicit kind:
audio state, pending rocket object, matrix arena, model character, model RW
arena, attached model or nested RW arena. It must validate current membership and
lifetime, returning a live generation-bearing actor ID or `STALE`/`INCOMPLETE`.
The content resolver handles model headers/nodes, vertex assets and display
lists, returning manifest-qualified stable content identity. Neither may return
an address cast to an integer. Rocket pointers are `AttachedObj *`/weapon-object
identities; a prop resolver must validate the owning object's association before
mapping them to `E3`, rather than casting an object address to a `PropRecord *`.

The model-view resolver validates the actual model prefix, matrix allocation
and bounded RW views before the exporter dereferences them. This contract avoids
speculative traversal through arbitrary model/content pointers. Null references
are encoded explicitly; unresolved non-null references retain presence and an
omission. They cannot support a complete hash. Audio handles and global gun
animation/script tables remain obligations even when their current effects
appear audiovisual.

## Implemented capture and lifetime contract

The 164 fields in `ge_mp_state_guns_schema()` consist of 97 direct hand fields,
19 model/reference/diagnostic fields, and 48 ordered RW columns. Each direct hand
contains 198 primitive elements. The public header gives every field a fixed ID;
no offsets, padding, pointers, whole structs, or unclassified RW words appear in
the wire representation. The three native `ITEM_IDS` members are assigned to
signed I32 values, including `ITEM_NOTHING == -1`; this works with both the host's
normal enum ABI and devkitARM's signed short-enum ABI.

`ge_mp_state_guns_capture(state, player, player_slot, hand_actor_ids, resolvers)`
requires a registry and two currently live E4 IDs with the specified namespace
and physical hand slots. The player owner confirmed that `hands[2]` persist from
`ge_original_multiplayer_players_create` through `players_destroy`.
The API accepts all four physical player rows, including the inactive allocated
rows returned by the storage getter; aggregators should supply all eight hand
IDs/records. The 2-player inactive-row test checks this behavior without
normalizing those rows. `init_player_BONDdata`/BeginLife and weapon switches overwrite their contents
without replacing storage. The aggregator must validate the player storage and
its lifetime token from `ge_original_multiplayer_inventory_storage` before
capture, and release/reacquire hand IDs at destroy/create to prevent ABA reuse.
The exporter does not register owners or change their lifetimes.

Call only while native owners and the registry are quiescent. Model-view and
reference callbacks must be read-only and validate membership, readable spans,
content identity, and allocation lifetime. A model view supplies the actual
prefix pointer and its readable size, a matrix pointer/count/readable-byte span,
and up to 32 typed RW views. Matrix pointers must agree with both the hand and
model prefix. The exporter bounds matrices to 64, gates all reads by supplied
spans, and checks original 32-bit RW word boundaries and overlaps. Provider
claims are necessary: a byte count alone cannot establish that an arbitrary
pointer belongs to a live allocation. A null model view remains unclassified.

Each RW sequence preserves provider order, word coordinate and stable authored
node identity. Active variant members are copied by name; columns belonging to
other variants are derived zero values, distinguished by the explicit tag.
Those zeros do not normalize inactive hand fields or unknown raw storage. The
unused-word mask must exactly complement the classified active spans; it is an
owner attestation about unused storage, not a mechanism for ignoring unknown
active fields. Mutable node links, modified authored headers, and nested RW
allocations still require their separate owner records. A content ID identifies
an asset; it does not substitute for mutable runtime header/node state.

Non-null references carry a presence bit even when resolution fails. Dynamic
IDs are checked against the supplied live registry; missing, stale, or zero
resolver results produce reference omissions. Static IDs must be supplied by a
validated manifest/content map, never derived from native addresses. Matrix/RW
allocation generations and current validity remain distinct from the persistent
hand identity. Keep the captured result in place until encoding: its record and
field pointers refer into caller-owned storage.

The capture always returns `INCOMPLETE`, including fully classified hand views.
`coverage.unclassified_fields` includes the cross-domain obligation, and the
per-hand omission bitfield also identifies missing views, bad RW classification,
missing content IDs, and unresolved/stale references. It never sets
`schema.complete`, installs a `finish_tick` hook, or provides restore behavior.

## Verification and precise limits

Run `scripts/test_mp_state_guns.sh`. By default it rebuilds and runs both normal
and short-enum ABI variants under AddressSanitizer and UndefinedBehaviorSanitizer.
Set `GE_MP_GUN_EVIDENCE_DIR` to retain generated source, extracted-body SHA-256
receipts, binaries, logs, and diagnostic byte/hash sequences. No objects are
shared between the two ABI builds.

Each ABI passes 2,965 native-owner element mutations:

- All 198 direct elements in each of eight actual canonical hands, all three
  numeric model-prefix fields per hand, and all 32 components of each initial
  two-matrix allocation: 1,864 mutations.
- Every one of the 60 numeric elements across the 11 explicitly selected RW
  interpretations, in the retained host RW owner storage.
- Thirteen actual native pointer-field changes, resolved through fixture
  identity maps, plus 1,024 components of a full 64-matrix native allocation.
- Four ammo-field mutations in actual inactive physical hand rows of a
  two-player allocation; their model views remain explicitly unclassified.

Tests call the actual multiplayer allocation/provider and original extracted
`initBONDdataforPlayer`, `init_player_BONDdata`, `modelInit`, `modelInitRwData`,
`modelGetNodeRwData`, and `dynAllocate` bodies. Original `draw_item_in_hand`,
`gunRequestHandWeaponChange`, and `sub_GAME_7F05E808` changes are observed through
capture. Native mutation and capture are reset between sensitivity cases.
Read-only capture, matrix-alias equality, signed zero, non-finite rejection,
excluded padding/animation-tail bytes, RW order, wrong hand namespaces, stale
hand/reference generations, unknown opcodes/content, missing views, overlapping
RW ranges, and malformed/truncated capacities also have focused checks.

Counts, tags, presence bits, masks, and omission metadata are tested via provider
view/reference changes and focused assertions, rather than counted as native
scalar mutations. Pointer targets/content and their identity registry are test
fixtures, not production asset, audio, projectile, or allocation registrations.
The test model nodes are fixture content operated on by real model functions.
A future aggregate still needs validated native owner implementations for all
resolver callbacks and all referenced records.

Two fresh processes each execute the same 32-step sequence of the real weapon
functions for two players, and another two do so for four players. Every
canonical diagnostic byte and SHA-256 matches; Python's `hashlib` independently
checks every digest. Normal and short-enum output also matches. These are gun
function sequences using actual player/hand owners, **not full committed GePort
gameplay transcripts**. They do not prove gameplay synchronization, restart
convergence, or immunity to different prior model/RW histories. The test-only
small fixture schema allows diagnostic encoding; both missing aggregate
attestation and real capture omission counts are checked to prevent a native
state hash from being certified. The existing pure codec regression also passes
its independent Python wire oracle, SHA vectors, and 20,000 registry transitions.

## Native build handoff and remaining owners

The platform's C source discovery will find `ge_mp_state_guns.c`. The MP-owned
Makefile must add `ge_mp_state_guns.o` to the same original ABI flag group as
`ge_mp_state_inventory.o`/`ge_mp_state_core.o`:
`-DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE -DVERSION_US
-DPLAYERFLAG=int`, alongside the global `-D_LANGUAGE_C -DGE_PORT_MS_INHERITS
-fms-extensions` and original include paths. Do not impose a different enum ABI
on this one translation unit. Keep it under the canonical non-fast-math flags.
For a native integration proof, force-retain
`-Wl,-u,ge_mp_state_guns_capture` in a fresh frozen link. This change does not edit
the Makefile or claim that the required aggregate link has already occurred.

Strict ARM compilation passes with devkitARM image
`devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5`,
`-std=gnu11 -O2 -Wall -Wextra -Werror`, the ABI flags above, and ARMv6K/VFP
options. Compile-time assertions verify the 0x20-byte embedded prefix and the
32-word RW run on the real ARM ABI. Its only undefined symbols are
`ge_mp_registry_key`, `ge_mp_registry_resolve`, and `memset`; capture has no hidden
native globals or getter dependencies. The current native codec object provides
the two registry functions; the parent performs final forced-link verification.

Outside this domain, future-influencing gun owners include player
`hand_invisible[2]`, `hand_item[2]`, `field_2A44[2]`, `lock_hand_model[2]`, per-hand
`copy_of_body_obj_header`, texture-pool/model-buffer state, weapon statistics and
animation/script tables, mutable model node/header relations, producer validity
and frame-allocation state, projectile/object/prop records, audio lifetime state,
and other player/session/rule/respawn owners. They remain explicit obligations.
No emulator, timing run, gameplay producer change, or physical acceptance claim
is included here.

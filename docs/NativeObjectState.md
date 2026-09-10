# Native multiplayer supply state (EA)

`ge_mp_state_objects` describes native `WeaponObjRecord` (type 8),
`AmmoCrateRecord` (type 7), `MultiAmmoCrateRecord` (type 20), and
`BodyArmourRecord` (type 21) fields. These are the enabled multiplayer supply
families and the held/dropped weapon containers. It is a bounded typed owner,
not an assertion that the object domain or global simulation is complete.
Keep the aggregate `GeMpStateSchema.complete` zero.

## Lifetime and identities

Begin E3 after `ge_original_prop_state_reset` and before any native allocation.
After supply definition storage exists, call `ge_mp_state_objects_begin` with
the E3 owner and a strictly key-sorted list of `GeMpObjectDefinition` entries.
The caller supplies zeroed owner storage and a valid registry. Each entry
contains the exact native definition pointer, native `sizeof`, expected type,
creation namespace and physical slot. Wrong size, alignment, type, duplicate
pointer, duplicate/out-of-order key, or insufficient registry capacity fails
before any ID acquisition. Up to 1024 physical definitions are supported.
Owner/snapshot storage is caller-owned; avoid putting the large fixed-capacity
snapshot on the 3DS stack.

Kind `0xEA` identifies **physical definition containers for a stage lifetime**.
Use distinct, agreed namespaces for authored setup definitions (slot is the
original command index), authored guard weapon containers, the 30 player-item
containers, and the 20 dynamic ammo containers. Namespace numbers are caller
policy, not allocation-order-derived identities. Enumerate every supported
container, including free dynamic containers. `begin` copies descriptors;
their borrowed definitions must remain readable at the same addresses until
`end`. Do not use a resident/rendered subset as the supply population.

The read-only helpers expose the native owners without changing allocation:

- `ge_mp_state_objects_native_ammo_definitions` describes all actual
  `g_AmmoCrates[20]` slots, including free ones.
- `ge_original_stage_guard_runtime_player_item_definition` describes each
  physical slot 0–29, including free ones.
- `ge_original_stage_guard_runtime_weapon_definition` describes slots below
  the runtime's `weapon_count`, including the original authored weapon record.

EA survives a native prop free/reuse within the stage. `objFree` leaves the
definition's numeric fields and old `model` pointer, but clears `prop`.
`ammocrateAllocate` selects such physical containers for reuse. Body-item
resource retirement can clear its physical container without reallocating it.
EA therefore always exports numeric fields, even while `prop == NULL`.
The nullable `prop` reference uses the **actual E3 allocation generation**.
A free/reallocate between snapshots produces a new E3 identity while keeping
the same EA identity. Regeneration retains the allocated prop and its E3 ID.
Neither exporter manufactures native allocations, frees or respawn events.

After the final consumers/snapshot, call `ge_mp_state_objects_end` **before**
destroying any definition-containing stage or guard runtime or replacing/resetting
the registry. Then perform the
existing native cleanup/arena-end and E3 end. EA checks E3's captured pool ID,
native stage epoch, allocation serial and observation fault before access.
It rejects an ended/replaced pool or an E3 owner rebound for another stage.
An unannounced native pool reset preserves E3's `INCOMPLETE` observation
failure. Ending an invalid EA owner still releases its IDs and reports that
failure. The exporter cannot detect a caller freeing borrowed definition
storage before ending EA; obey the containing-owner lifetime contract.
Before cleanup releases anything, it validates the full owner count, registry
shape and every exact EA/namespace/slot identity. A malformed owner returns
`INVALID` without changing either owner or registry. Expired generations never
release a newer actor at the same key; legitimate native lifetime expiration
still permits release of this owner's remaining live identities.
Registry history belongs to the caller: retain generations within a match,
and use the agreed fresh registry for a fresh protocol session.

## Field map

The source layouts are declared in `src/bondtypes.h`; no native struct bytes,
padding, enum widths or pointer values are hashed. Matrices append native
`mtx.m[row][column]` in row-major order. Coordinates append x, y, z. All F32
values must be finite; finite bit patterns, including negative zero, survive.

| Field | Type | Native source |
|---|---|---|
| 256 | U8 | current `type`, including zero in a free blank container |
| 257 | U16 | `extrascale` |
| 258 | U8 | `state` |
| 259 | I16 × 2 | `obj`, `pad` |
| 260 | U32 × 3 | `flags`, `flags2`, `runtime_bitflags` |
| 261 | F32 × 21 | `mtx` × 16, `runtime_pos` × 3, `maxdamage`, `damage` |
| 262 | U8 × 8 | `shadecol` RGBA, `nextcol` RGBA |
| 263–267 | nullable actor refs | `prop`, `model`, collision block, projectile, embedment |
| 268 | I8 × 2, weapon only | `weaponnum`, `LinkedWeaponType` |
| 269 | I16, weapon only | `timer` |
| 270 | nullable actor ref, weapon only | `dualweapon` |
| 271 | I32, magazine only | `ammoType`, explicitly promoted from native enum |
| 272 | U16 × 13, multi-ammo only | each slot's `modelnum`, in source order |
| 273 | U16 × 13, multi-ammo only | each slot's `quantity`, in source order |
| 274 | F32 × 2, armour only | `initialamount`, `amount` |
| 275–276 | U8 each | immutable descriptor namespace and expected variant |

Variant-specific fields have count zero for other variants. Export rejects
an unexpected native header type, except the zero header of a free blank
container. For live props, E3 must resolve `obj->prop`; its native reverse
object link and prop type must also match. The exporter checks E3 lifetime,
but the aggregate must still perform E3/E5's full list/field validation.

`model` is consulted only when `prop` exists. Native `objFree` clears
`model->obj` and leaves the old model address; subsequent supply construction
replaces it before use. Exporting that obsolete address as a live model would
invent a dependency on a released resource. Every non-null collision pointer
is represented: `sub_GAME_7F04F244` follows pointer + flag 0x100 + !state 0x20,
without testing `PROPSTATE_EXT_COLISION_BLOCK`; `chrobjCollisionRelated`
follows any non-null collision pointer. `objInit` allocates these blocks in
`MEMPOOL_STAGE`, and `objFree` neither frees nor clears them. They remain
stage-owned references even after prop free, until construction replaces the
pointer or the containing stage ends. The union uses the original branch order:
`RUNTIMEBITFLAG_EMBEDDED` selects embedment, otherwise
`RUNTIMEBITFLAG_HASPROJECTILE` selects projectile. Unselected old union bits
have no typed pointer interpretation. `dualweapon` remains represented even
when the container is free; actual weapon free clears both paired links.

## Reference and coverage work still required

`ge_mp_state_objects_actor` resolves an exact registered definition address.
It can serve E3's object/weapon provider and E8's projectile/object references.
An interior/unlisted address fails; no address is serialized. An internal
`dualweapon` resolves through EA first. Other non-null references require a
`GeMpStateObjectsProviders.reference` mapping to a live typed registry ID;
the aggregate must also contain the referenced record. Missing or stale
mappings return `INCOMPLETE`, with `unresolved_kinds` identifying the class.

The following owners need actual typed records/bindings before completeness:

- Mutable `Model` instances and their model-node/content relationships. The
  numeric object/model slot fields are retained here but do not replace a
  content-identity or mutable-model owner.
- Native collision blocks and `Embedment` records, including their nested
  vertex/model/projectile relationships. E8 owns projectile slots; it does
  not own the embedment pool.
- Linked objects outside these four supply families. Resolve them with their
  real typed owner; do not cast an arbitrary pointer into an EA identity.
- Player-item allocation metadata outside `WeaponObjRecord`:
  `player_item_next` and `player_item_generation`. The narrow read-only
  `ge_original_stage_guard_runtime_player_item_allocator_state` exposes them
  for a future aggregate owner. Per-container body/model resource generation
  remains part of that resource owner's state.
- Mutable setup relationships used by native reparent/regeneration paths,
  and enabled nonsupply object families. This exporter does not enumerate
  doors, glass, monitors, vehicles or arbitrary base objects.

These four native variants have no direct audio-handle member. Their live
projectile audio belongs to E8; other object's audio fields require their
respective typed variant owners. A successful EA export sets the bounded
`GE_MP_SYSTEM_OBJECTS` coverage bit but cannot attest all these owners exist.

## Verification and build integration

Run `bash scripts/test_mp_state_objects.sh` and the same command with
`-fshort-enums`. Both use ASan/UBSan and the same source layouts as production.
The fixture promotes actual `Ump_setupdishZ` command 0/2/25 words, retains
the original armor setup conversion, and exercises source armor pickup and
the entire original object-regeneration conditional. Every slot in two- and
four-player proximity input rosters independently delays regeneration; these
rosters are explicit proximity inputs, not a full multiplayer world claim.
The source free/drop/projectile allocation and ammo-pool allocator bodies run
against the real observed native Prop pool. Tests verify impact clearing,
armor pickup accounting, E3 generations, pair unlinking, free inactive fields,
finite/signed-zero typing, unresolved refs, stage end and missed-end reset.
The exact `sub_GAME_7F04F244` regression proves that null-to-live collision
pointer changes with EXT clear affect both native edges/top/bottom and EA's
typed reference, including collision-query gates and native permanent free.
The enclosing full scheduler, nonarmor pickup branches, room geometry,
audio device and model assets are outside this fixture; resource seams are
explicit and unexercised object/model branches abort. It is not a match or
transport acceptance test.

Production adds only `ge_mp_state_objects.c` alongside `ge_mp_state.c` and
`ge_mp_state_props.c`, using the existing native source flags:
`GE_PORT_USE_ORIGINAL_TYPES`, `GE_PORT_MS_INHERITS`, `AIPARSE`, `VERSION_US`,
`_LANGUAGE_C`, and `-fms-extensions`. The test-only generator is
`scripts/extract_mp_state_objects_fixture.py`; its prerequisites are the
original `propobj.c`, `chrprop.c`, `model.c`, `bondview2.c`, `explosion.c`,
`prop.c` and the authored setup source. Nothing enables an aggregate emitter.

# E7 native background and light state

`ge_mp_state_bg` exports one typed E7 record in creation namespace 255, slot 0.
It captures the complete mutable state of `GeOriginalBgGeometry` used by later
background hits and breakable-light calls. It does not serialize native
addresses, structure padding, or renderer scenes.

The caller acquires the singleton registry identity after successful geometry
load/bind and releases it before geometry close. A new stage requires a new
registry generation. `ge_mp_state_bg_init` reserves snapshot storage once;
`ge_mp_state_bg_capture` performs no allocation and requires a quiescent
canonical boundary and a live matching registry ID. Add its record and
`ge_mp_state_bg_schema()` to the aggregate codec. Keep the snapshot in place
through encoding, then call `ge_mp_state_bg_close` after its last use.

All values are copied into snapshot-owned memory. Capture failure clears the
published record and record count. Stale binding/identity, invalid fixture or
darkened-point references, insufficient storage, inconsistent colour mirrors,
and unclassified owner status prevent publication.

## Field inventory

IDs are fixed in `enum GeMpBgField`, from 256 through 276.

| Native state | Typed representation | Reason |
| --- | --- | --- |
| Owner status, room/point counts | U32 scalars | Identifies a complete bounded geometry owner |
| Level scale and reciprocal | Two F32 values | Affect hit and shard coordinates |
| Current fixture allocation slot and room | I16 scalars | Preserve the original fixture registration state |
| Darkened cursor | I32 scalar | Selects the next circular-history replacement |
| All 100 fixture entries in physical order | I16 rooms, U8 lists, U32 start/end command indices | Preserves first-fit order and exact triangle range ownership; end may be one past the list |
| All 512 darkened entries in physical order | U16 rooms and point indices | Future containment/proximity and record replacement depend on this history |
| Room point offsets and primary/secondary command counts | Room-ordered U32 sequences | Defines stable room/list/point coordinates for the record |
| Room VTX batch counts, origins and bounds | U32 counts and F32 coordinates | Preserves the original native query inputs |
| Every authored point's current colour | One explicit U32 `0xRRGGBBAA` per point in room then point order | Live colours can outlast entries evicted from the circular darkened table; all four channels affect later rendering |

Unused fixture slots have room 0 and null range sentinels (list 255,
start/end `UINT32_MAX`). An active range must resolve into its declared room's
owned primary or secondary command buffer. Unknown or inconsistent references
produce `INCOMPLETE`; they never become a guessed room or point identity.

The exporter verifies the three colour representations agree: original native
Vtx bytes, owned authored point-table bytes, and native collision Vertex
colours. It records one semantic colour sequence after this check. It neither
omits alpha nor treats a light mutation as renderer-only data.

## Immutable and separately owned state

The complete content identity must cover authored point geometry, room streams,
room bounds, texture/LOD metadata and the exact build. Point coordinates,
texture provenance, generated batch bounds, command point mappings and list
buffers are immutable after bootstrap apart from their explicitly captured
colour bytes. The duplicated native collision vertices and point-table bytes
are derived from those inputs. The original texture-emission cache is used
only during bootstrap and resets before every room list; it has no later tick
reader. Unwritten room flags, fixture reserved words, allocator accounting,
addresses and C padding have no canonical writer or gameplay read in this
owner's retained closure.

E7 uses `GE_MP_SYSTEM_OBJECTS` for this BG/light subdomain and never sets
`schema.complete` or attests complete object/effect coverage. E0 owns RNG.
Glass shards, sparks, explosions, smoke and other consequences require the
separate E8 effects domain; E6 owns player state. The registry lifetime is
owned by the caller, as in the core snapshot's singleton capture contract.

## Verification

`bash scripts/test_mp_state_bg.sh [complete-original-asset-pack.gepack]` uses
AddressSanitizer and UndefinedBehaviorSanitizer in native and short-enum builds.
The focused digest is identical between ABIs. Equal worlds at different native
addresses and reversed input descriptor order hash equally. Tests prove
snapshot durability, sensitivity to each RGBA channel independently of the
darkened table, darkened record/cursor order, fixture physical order, and stage
identity generation. Invalid references, inconsistent colour mirrors, stale
bindings/IDs and insufficient snapshot capacity cannot publish a record.

The complete original pack passes capture/encoding for all 21 solo stages and
all 11 multiplayer descriptors, both before and after their primary light
triangles are passed through the original break function. The exporter also
compiles with the actual devkitARM compiler with warnings treated as errors.
These are native ownership/codec checks, not hardware or whole-session
determinism acceptance.

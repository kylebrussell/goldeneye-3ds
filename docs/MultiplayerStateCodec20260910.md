# Canonical state codec contract — 2026-09-10

`port/include/ge_mp_state.h` and `port/src/ge_mp_state.c` provide a pure,
pointer-free byte codec, stable-ID registry and SHA-256. They do not collect
native state, advance a tick, modify gameplay, restore a snapshot or change
protocol v1. A successful codec call validates the supplied schema and coverage
attestation. It does **not** establish that the native simulation is completely
represented. The native ownership audit is
[MultiplayerStateOwnership20260910.md](MultiplayerStateOwnership20260910.md).

## Native adapter obligations

The adapter owns `GeMpCanonicalSnapshot` and all its borrowed storage. It must:

1. Supply a fresh nonzero session ID, canonical tick, enabled-system mask and
   nonzero build/content/rules SHA-256 identities. Enabled systems include
   `GE_MP_SYSTEM_SESSION`. Enumerate every enabled system even when it has no
   live actors. The schema's kind declarations must collectively cover those
   systems. Additional system bits require explicit schema registration.
2. Keep `schema.complete = 0` until the native owners approve the entire coverage
   manifest. Set `coverage.unclassified_fields` and `unresolved_references` for
   any remaining gap; never silently omit an unknown native field or pointer.
   Every future-influencing field needs an explicit stable field ID, type and
   multiplicity. IDs 1–5 are common ordering/ownership fields; native IDs start
   at 256. A native schema version must change when meanings or interpretation
   change. The codec hashes the full supplied schema, not just its version.
3. Include exactly one global record (kind 0, actor ID 0). Sort actor records by
   full ID, kind declarations by kind and fields by field ID. Supply every field
   in that record's schema, including explicit empty sequences. Include one
   actor record for every live registry entry, including actors outside visible
   rooms and any inactive actor that still affects future behavior.
4. Define stable registry keys for the canonical allocation pools, including
   free slots. Every defined key needs a kind declaration in the schema, even
   when that slot is free or unreferenced; an unknown free kind returns
   `INCOMPLETE` just as an unknown live kind does. Call acquire/release at native
   lifetime transitions. Preserve the registry throughout the session;
   reinitializing it mid-session would destroy
   stale-reference protection. The codec cannot infer skipped native lifetime
   events. If an actor is transferred, record the current owner separately;
   never rewrite its creation namespace. A namespace is not a heap address,
   renderer residency index, current player selector or current carrier.
5. Represent active/free/room/child traversal order with ordered reference
   sequences. Do not sort these values to make hashes agree. Set
   `GE_MP_FIELD_UNIQUE` for membership lists that prohibit repeated IDs; this is
   checked per sequence. Shared references in different fields are allowed.
   `ACTOR_REF` points to a live actor and requires its record; `SLOT_REF` points
   to a defined slot and may identify a free slot. `NULLABLE` allows zero as a
   null reference; otherwise zero is invalid. Nullable nulls do not count as
   duplicate memberships.
6. Export raw finite IEEE-754 binary32 values, both exact 64-bit RNG states,
   timers, pending input/edge state, stable script/asset identifiers and every
   other required native owner identified in the audit. Do not truncate the
   seeds or replace them with output samples. Pointer fields must be classified
   as actor/slot references, versioned content identifiers, or documented
   derived/local-only state. There is no generic pointer or opaque native
   struct type in this codec.
7. Keep schema, registry, records and value buffers immutable for the entire
   call. Each typed value array provides `count * type-width` readable bytes;
   unaligned storage is supported. Output buffers must not overlap inputs.
   Native allocation, linked-list invariants, content offsets and field meaning
   remain the adapter's validation responsibility.

The enabled-domain declarations and `complete` flag are explicit claims by the
adapter; no generic codec can discover unreported native globals. Until that
coverage contract is approved, even successful fixture hashes are unsuitable as
evidence of simulation convergence. A deliberately smaller enabled-system mask
cannot certify a running game whose other systems still influence future ticks.

## API and failure behavior

`ge_mp_state_measure` validates and returns the required byte count.
`ge_mp_state_encode` validates before writing, then emits the canonical bytes.
`ge_mp_state_sha256` validates and streams the same bytes into SHA-256 without
allocating a snapshot-sized buffer. `ge_mp_sha256` is the ordinary byte-digest
helper; it makes no state-coverage claim.

Validation first checks all schema/record shapes and scalar values, then resolves
references. Forward references are therefore accepted. It rejects non-finite
floats by inspecting their exponent bits, including signaling NaNs without
floating-point arithmetic. Signed zero and finite subnormals retain their bits.
Booleans must be 0 or 1. Signed integers use their fixed-width two's-complement
encoding. Native padding, addresses, registry storage capacity and host byte
order never appear in the wire representation.

| Status | Meaning |
| --- | --- |
| `OK` | Supplied data satisfies the declared schema and coverage contract. |
| `INCOMPLETE` | Incomplete schema attestation, missing identity/system/field/kind/record, unclassified state, or unknown reference. |
| `INVALID` | Malformed metadata, duplicate/unsorted keys or records/fields, forbidden duplicate membership, invalid bool/float/type/count/null, invalid ID layout. |
| `STALE` | A known slot is free or its generation does not match the actor ID. |
| `CAPACITY` | Registry/output/record/value-byte bounds are exceeded. |
| `EXHAUSTED` | A slot's 32-bit lifetime generation cannot advance without reuse. |

On failure, digest, encoded destination and caller's byte-count output remain
unchanged. `GeMpCoverageReport` is diagnostic output; it gives missing system
bits, supplied/detected unclassified-field and unresolved-reference counts, and
the first applicable record/field index. `SIZE_MAX` means no applicable index.
It does not inventory every failure after the first one. `encoded_size` is
available after successful validation, including insufficient destination space.

Limits are 256 kind declarations, 1,024 fields per kind, 4,096 total records
(including globals), 4,096 registry slots, 1,048,576 values per field and 16 MiB
of encoded bytes including framing. Empty arrays may have null value pointers.
No serialization of a partially validated prefix occurs.

## Registry identity and lifetime

The key is `kind:8 | creation_namespace:8 | slot:16`; kind zero is reserved.
All 256 creation namespaces are representable. Actor IDs are
`key:32 | generation:32`, with generations starting at 1. `define` inserts a
previously undefined key into sorted caller-owned storage; duplicate definitions
are invalid. `acquire` only accepts a free defined slot and increments its
generation. `release` requires the exact current live ID. Released entries stay
in the registry with their generation history, and are serialized. The next
acquire cannot alias the released ID. Generation `UINT32_MAX` cannot be acquired
again after release. Neither an unknown slot nor an old generation is implicitly
created or repaired during reference resolution.

Returned registry entry indices are lookup results, not identities: defining a
new lower key can shift them. The caller must retain keys/actor IDs, not cached
array indices. Registry storage belongs to the caller; these routines allocate
and free no memory. Native free-list order is an explicit field, independent of
the registry's canonical key order.

## Wire version 1

All integers and counts below are unsigned big-endian unless the value's schema
type specifies a signed representation. There is no alignment or padding.

| Sequence | Bytes |
| --- | --- |
| Magic `GEMS`, wire version, native schema version | 4, 2, 4 |
| Session, tick, enabled systems | 4, 8, 8 |
| Build, content, rules SHA-256 | 32 each |
| Registered systems, number of kind schemas | 8, 4 |
| Each kind: kind, systems, field count | 1, 8, 4 |
| Each field schema: ID, type, flags, min count, max count | 2, 1, 1, 4, 4 |
| Number of registry slots | 4 |
| Each slot: key, generation, live | 4, 4, 1 |
| Number of records | 4 |
| Each record: actor ID, kind, field count | 8, 1, 4 |
| Each field: ID, type, count, values | 2, 1, 4, count × type width |

`GeMpStateType` has explicit wire values 1–12 in header order: U8/U16/U32/U64,
I8/I16/I32/I64, F32, BOOL, ACTOR_REF, SLOT_REF. Reference widths are 8 and 4
respectively. Schema fields and ordered value arrays are length delimited; IDs,
types and multiplicities cannot be exchanged without changing canonical bytes.
Attestation diagnostics and the `complete` flag are not serialized: successful
serialization already requires complete attestation and zero reported gaps.

## Regression evidence

Run `sh scripts/test_mp_state.sh`. The ASan/UBSan C suite checks four standard
SHA-256 vectors (empty, `abc`, 56-byte padding boundary, million `a`), all scalar
types, forward/null/stale/missing/duplicate references, 20,000 modeled registry
transitions, generation exhaustion, poisoned struct padding, differing source
addresses, unaligned values, signed zero, subnormals, invalid floats/bools,
coverage failures (including undeclared free pool kinds), actual encoding and
streaming/buffered digest agreement at the exact 16 MiB limit, framing-only
overflow with maximum record/field counts, maximum record count, ownership
transfer, retained free-slot generation history, sequence order and atomic output
failure. An independent Python `struct.pack` oracle constructs the complete
732-byte fixture and compares the streaming digest against `hashlib.sha256`.
The production C file also compiles with strict warnings under devkitARM for
ARMv6K/VFP. These are codec regressions, not native gameplay/restore/network
acceptance tests.

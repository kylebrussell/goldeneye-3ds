# E9 canonical background visibility state

`ge_mp_state_visibility_capture` owns a fixed-capacity value snapshot of the
canonical native visibility interpreter and publication. It allocates nothing,
uses record kind `0xe9`, namespace `255`, slot `0`, and declares only the
visibility subdomain of `GE_MP_SYSTEM_OBJECTS`. Its 51 fields are explicitly
numbered 256–306 in `ge_mp_state_visibility.h`. It never sets `schema.complete`.

## Lifecycle and reference map

Acquire the caller's E9 registry entry after the stage visibility runtime is
loaded; capture after the last successful canonical player pass, with an empty
native traversal queue; release before runtime close. The snapshot borrows no
native values after capture, but its field pointers refer into its own storage:
do not move it before encoding. Failed validation clears record publication
and reports either an unresolved reference or an unclassified field. Invalid
or stale actor IDs also fail. Generation advances for new lifetimes within one
registry; fresh match sessions use fresh registries scoped by protocol session
identity. This provider neither acquires nor resets gameplay owners.

The adapter's small state bridge tracks successful canonical publication and
retirement. A failed run makes capture unavailable. Runs without a materialized
program do not qualify for this runtime exporter. The bridge validates the
specific live program, background identity/size, canonical portal base, compact
projection-player pointer, command pointer/count/hash, and each polygon's exact
owned address and 3–9 point capacity. It verifies relocated commands still
match the immutable program before exposing their typed opcodes, lengths and
arguments. Portal arguments are stable portal indices, not native pointers.
Points are copied as finite coordinate triples in portal/point order.

Runtime/publication duplicate floats are compared by exact binary32 bits,
including the sign of zero. Canonical runtime evaluation invalidates capture
before any argument/camera early return and marks its last-original status as
invalid until traversal succeeds. Low-level canonical restore also invalidates
before its preload-callback rejection. These changes affect export freshness,
not retained gameplay room values or the room-cycle counter. Failed private
eye run/restore operations leave the canonical publication available.

The input authoritative door-control array must have the exact portal count
and match each published portal control byte. The capture retains both this
future input and the current native portal's two control bytes and ordered
adjacency. `g_CurrentPlayer` in this interpreter is its own compact projection
storage, not a native gameplay player actor. Native selected-player identity
belongs to the player/session owners. Immutable background/bounds/program
content still requires the session's complete content identity.

## Source and field audit

| Source owner | Writers/readers and captured values |
| --- | --- |
| `g_BgRoomInfo[139]` | Adapter resets the exact 0x50 publication table; `sub_GAME_7F0B39BC`, traversal and global-vis instructions write rendered/neighbor/loaded-mask/visit fields. Guard on-screen, active-room and hit queries consume this publication. Capture all authored flags and min/max bounds; bounds must agree with the retained runtime input. Nonzero geometry/storage/reserved fields fail as unclassified: actual geometry belongs to E7's different owner. |
| `g_BgNumberOfRoomsDrawn`, `dword_CODE_bss_8007FFA0` | Preserve active room order, signed 32-bit depth and special flag plus screen boxes. No sorting, deduplication, byte narrowing or device-local restore. Effects/prop room passes consume these fields. |
| Dam `sub_GAME_7F0B8D78` prefix | Original native renderer changes lake room 0x23 depth to zero before prop traversal. E9 records whatever native depth actually exists and detects this writer. The then-current effect adapter lacks that prefix; integration reproduced the omission. All eleven multiplayer arenas exclude Dam. Capturing Dam values does not attest the missing solo/general runtime writer. |
| Global visible-room list/count | Preserve active authored room-ID bytes and physical order. These are U8 room codes; draw depths remain I32. No character signedness or array padding enters encoding. |
| Current room/max rooms/level/scales | Retain current room, authored room count, level index, forward/inverse geometry scale and visibility scale. Runtime future inputs must match the current canonical publication. |
| `D_80044858` | Retain the actual modulo-ten room-load cycle. `bgDetermineVisibleRooms` increments it; canonical restore saves/reinstates it. Original room load/unload policy consumes it. It is not a renderer frame count. |
| `bgViewRelated` and projection services | Retain four clipping controls, native screen box, viewport, six compact-player projection values, position, world-to-screen matrix, clip range and projection offsets. Full finite F32 bits, including signed zero, go through the codec. |
| Portal array/geometry/control source | Exact owned pointer validation; ordered connected rooms/control bytes, authored point-offset prefix and XYZ arrays, and caller's live door controls become typed values. Door setters and original line queries read this owner between visibility passes. |
| Global commands | Immutable bounded program is identified and checked; typed opcode/length/argument arrays and stream hash retain relocated portal/room-index provenance. No command pointer is serialized. |
| Parser `dword_CODE_bss_80081600` | The original function-static bbox can survive across passes and be read without a preceding FORCE_VISIBLE. Its bbox and recursive stop flag are captured. Only its storage name moves to file scope; parser reads/writes and lifetime remain unchanged. Unused `unk14` has no original writer/reader and must remain zero. |
| Stack/current flag/descent/depth controls | Preserve all 20 stack values, cursor, current visibility, descent count, maximum depth, queue cursors, portal-depth bytes and active cache data as typed current-publication diagnostics. These values reset before subsequent traversal; keeping them does not grant further system coverage. |
| Portal cache | Original traversal resets each cache count to -1 before use. Preserve counts in portal order; preserve bbox only for count >=0, including count zero. Emit explicit valid-portal indices with boxes in that order. |

## Named exclusions and integration gaps

The entire `ge_renderer_private_*` interpreter, including its separately named
parser state, is excluded. It cannot begin, publish or retire the canonical
bridge. Eye results, stereo membership, local scene residency, GPU buffers and
`GeDamPreloadQueue` are renderer ownership. The actual queue's room states,
physical request ring, visibility-request membership and load/eviction counters
depend on local player, stereo and asynchronous scene commits. Its callback
only changes per-invocation `preload_ok` and adapter request diagnostics; it does
not change canonical visible-room decisions. The focused test demonstrates
equal E9 digests with one versus two native preload requests.

The adapter's `ge_preload_request_count` is reset at each pass and read only
into its result; the borrowed provider/context pointer is never read after a
pass before replacement. Neither is encoded. No callback is invoked during
capture. An actual gameplay-facing asynchronous room loader would need its own
typed owner; this exclusion does not attest such an unimplemented owner.

An empty traversal queue is mandatory. Consumed queue slots and cache boxes
whose count is -1 are dead scratch, reset or overwritten before any future
reader, and are excluded. Inactive draw/list tails are also excluded. The
portal metric `D_80044900` is written during a successful line test immediately
before its caller reads it; it is not a future-pass input. Debug count and
metric-padding globals have no future gameplay reader in the linked closure.
Materialization address scratch, allocation sizes, padding and immutable
program/background pointers are excluded as addresses; semantic geometry,
commands and live bindings are checked explicitly.

Program load/close do **not** reset the native room-cycle or retained parser
bbox. E9 intentionally reports the actual values, so different earlier local
histories can differ even with fresh registry/session identity. Root must own
an explicit source-backed session-start initialization policy before any
complete state claim. Capture does not silently normalize those values.
Native fog (including alternate-state storage), E7 geometry/lights, effects
and all other object owners remain separate coverage obligations.

## Validation

`scripts/test_mp_state_visibility.sh PACK` passes native and short-enum
ASan/UBSan and requires identical codec digests. The combined pack exercises
1,889 room-camera publications across all 21 solo and 11 multiplayer stages.
Focused cases cover pointer/address independence, durable copies, actual
cycle/bbox/portal-coordinate/control mutations, negative draw depth and
publication order, alpha-room order, live door-control mismatches, private-eye
isolation, preload independence, command/player/polygon/background identity,
active traversal rejection, unknown room metadata, nonfinite retained state,
retirement/failed publication and stale IDs.

Boundary regressions additionally cover both clip-range signed-zero mismatch
directions and successful matching negative-zero encoding; all eight early
argument/camera rejection cases for canonical run and restore wrappers; and
preload/null-input/null-result low-level restore failures. They check both the
bridge and wrapper status, successful recovery, unchanged cycle on rejection,
and canonical digest preservation after equivalent private-eye failures.

`scripts/test_mp_state_visibility_mutations.py` requires semantic assertion
failures when cycle or retained bbox is omitted, signed depth is truncated,
door-control matching is removed, or an active queue is accepted. Four further
mutations require failures for numeric float comparison, omitted runtime
invalidation/status, and omitted early restore invalidation (nine total).
The existing
multiplayer visibility suite still passes. Exporter and adapter compile with
actual devkitARM. These checks make no hardware, timing or visual acceptance
claim, and an isolated fixture's codec schema is not a full-runtime attestation.

Build the exporter with the normal visibility/codec includes and flags. The
canonical adapter includes its new state header and `.inc`; extracted and
private interpreters include `ge_mp_state_visibility_interpreter.inc`. Retain
capture/schema entrypoints in the complete runtime build and track these new
include dependencies. Main, registry assembly and the 3DS Makefile remain root
ownership.

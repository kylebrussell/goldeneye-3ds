# Native multiplayer core state domain — 2026-09-10

`ge_mp_state_core_capture` reads actual native owners into 75 typed codec fields.
It always returns `GE_MP_STATE_INCOMPLETE`: this domain does not attest complete
native simulation coverage. It provides no restore operation, protocol hook,
tick advancement, input mutation or `finish_tick` integration. The codec contract
is [MultiplayerStateCodec20260910.md](MultiplayerStateCodec20260910.md); the wider
ownership audit is [MultiplayerStateOwnership20260910.md](MultiplayerStateOwnership20260910.md).

## Adapter contract and IDs

`port/include/ge_mp_state_core.h` defines the public field IDs, typed captured
values and `GeMpCoreState`. Call:

```c
ge_mp_state_core_capture(&core, &actual_port_state, singleton_actor_id,
    GE_MP_CORE_BOUNDARY_COMMITTED_RAW);
```

The singleton key is kind `0xE0`, immutable creation namespace `255`, slot `0`.
The adapter defines/acquires it in the shared registry once per native session
lifetime and passes that actor ID, including its nonzero generation. The codec
subsequently resolves the ID against the complete shared registry. This module
does not create or reset registry lifetimes. The inventory exporter separately
owns kinds `0xE1` (inventory slots) and `0xE2` (player data/permissions), namespaces
0–3; this module does not occupy those IDs.

`ge_mp_state_core_schema()` supplies the `GeMpRecordSchema`. Its system mask
identifies contributing domains, not completed domains. Merge its record and
schema declaration in sorted kind/actor order alongside other native owners.
Preserve all omission diagnostics in the adapter's incomplete-coverage report;
do not set `schema.complete` or mark an entire system enumerated merely because
this record was captured. `GE_MP_CORE_SCHEMA_VERSION` versions this field
contract; the full native adapter must also version its combined schema.

The caller owns the actual `GePortState`, singleton lifetime and quiescent tick
boundary. `GeMpCoreState.record.fields` and its value pointers refer into that
same caller-owned object: keep it in place until encoding ends. Invalid pointers,
wrong singleton key/zero generation or invalid boundary enum return `INVALID`
before changing output. Capturing a valid domain returns `INCOMPLETE` with usable
diagnostic fields. No native pointers or padding become codec values.

## Exact field ownership

Every scalar and array has an explicit ID in `GE_MP_CORE_FIELD_LIST`. IDs below
are hexadecimal. Sequence order is physical storage order, without sorting or
compaction; signed values and finite F32 bits are preserved by the codec.

| IDs | Actual owner and exported state |
| --- | --- |
| `100–102` | `random_port.c:g_randomSeed` U64; `ge_original_chr_obj_random.c:g_chrObjRandomSeed` U64; original `player.c:random_byte` I32. The misleading byte name does not justify narrowing its native 32-bit value. Neither stream is sampled or advanced by capture. |
| `110` | Caller-owned `GePortState.simulation_ticks` U64. `ge_port_advance_multiplayer_tick` rejects the next tick when it equals `UINT64_MAX`; global frame counters alone cannot represent this guard. |
| `120–127` | Original `frametiming.c:lastFrameCounter`, `currentFrameCounter`, `speedgraphframes`, `previousFrameCounter`, `halfFrameCounter`, `isFrameCounterOdd`, `halfMinusPreviousCounter`, `frameDelay`, all I32. |
| `140–151` | All 18 retained `lv.c` timer globals: `g_ControlsLockedFlag`, `g_ClockTimer`, `g_GlobalTimerDelta`, `g_GlobalTimer`, `D_80048380`, `g_CurrentStageToLoad`, `D_80048388`, `D_8004838C`, `D_80048390`, `D_80048394`, `g_MpTime`, `g_CurrentMultiPlayerSec`, `D_800483A8`, `g_CurrentMultiPlayerMin`, `D_800483B0`, `g_StageTimeSec`, `D_800483B8`, `g_PowerOnTimeSec`. Preserve original names/units, including the minute-named value computed by the retained body; no derived timer is substituted. |
| `160–167` | Original `boss.c:g_StageNum`, `g_MainStageNum`, `g_BossIsDebugMenuOpen`, `g_DebugAndUpdateStageFlag`, both memory-display flags (I32), and `g_CurentMMallocValue`/`g_CurentMaMallocValue` (U32). |
| `180–183` | Original `player.c:player_num` I32 and all four `array_PLAYER_IDs` in order; `bondview.c:g_PlayerTickCount` I32; saved `ge_original_bond_input_provider.c:g_control_style` through its existing read-only accessor. Shuffle traversal loop indices are local variables, not a persistent cursor. Capture between passes/ticks; it cannot checkpoint a C call stack halfway through traversal. |
| `200–211` | Private `ge_original_input.c` publication/configuration owner: four pending float samples by component; both button arrays; every 4×2 raw pad's buttons/X/Y; exact raw/dual flags, player count/selection, stick limit, RAMROM controller count, owner-validated playback identity and replay/block presence. |
| `240–24e` | Original `joy.c`: both physical banks of 20×8 pad buttons/X/Y/error bytes; `curlast`, `curstart`, `nextlast`, `nextsecondlast`, playback count and pressed-button arrays per bank; selected bank, connected/controller masks and playback/record presence. |

Only read-only getters were appended to `ge_original_input.c` and `joy.c`, with
declarations in their headers and `ge_original_input_state.h`. The joy getter
checks its physical dimensions at compile time. The input owner classifies
playback by comparing the *actual registered* function pointer with its own
pending-input and RAMROM callbacks (or null). Unknown functions receive the
explicit unknown value and an omission. A non-null record callback is unclassified
and remains an omission. An unknown `g_ContDataPtr` is reported as bank `-1`
without pointer subtraction or fabricated identity.

Both joy banks, inactive pending/raw slots and all four shuffle entries are
serialized verbatim. Equal fresh-process transcripts do **not** certify
convergence after different earlier histories. Native restart/bootstrap owns
resetting history or explicitly proving fields irrelevant to future behavior;
the exporter never clears or normalizes inactive storage to force agreement.

## Explicit exclusions and omissions

`copy_of_osgetcount_value_0/1` are copied exactly into diagnostic observations,
outside canonical fields. Native `osGetCount` reads wall clock; the original
`updateFrameCounters` only copies/updates those stamps. In the current committed
raw path, `ge_port_advance_multiplayer_tick` calls `ge_port_tick`, which calls
`updateFrameCounters`; none of their decisions uses either stamp. Conversely,
`waitForNextFrame` reads the current stamp to choose elapsed frames. The source
contract regression checks this distinction. Any use of that pacing path falls
outside `COMMITTED_RAW` and adds `GE_MP_CORE_OMIT_NONCOMMITTED_CLOCK`; callers must
not label a real-time/wait-driven boundary committed merely to omit its clock.
The stamps are neither zeroed nor replaced with synthetic time.

Always-reported obligations are:

- Other native owners: player bodies/rosters, guns/inventories, props/AI/actions,
  objects/projectiles/explosions/animation, rules/respawn/session metadata and
  any fields not covered by this finite manifest.
- `GeOriginalLevelProviders` callbacks/context, including pause decisions and
  future behavior of dispatched subsystems. These cannot be encoded as addresses
  or guessed provider enums.
- Complete `GeOriginalBondInputProvider` default and per-player roster/options,
  pointer-to-roster bindings and permission identity. The exported saved control
  style and selector do not replace these next-tick guard inputs.
- `GeOriginalMultiplayerDeathState` pre/post guards and related wrapper lifetime
  state, owned by the multiplayer adapter.

Additional omissions identify unknown playback/record functions, unknown joy
bank pointers, queued/bound RAMROM content, noncommitted pacing or unsupported
build variants. RAMROM presence/controller count is captured; replay cursor,
header, queued sample bytes and content identity require their own complete
adapter and are not silently accepted. This contract is for the retained US
timer ABI; non-US/`BUGFIX_R1` builds report the variant omission.

Physical controller polling/rumble queues, PFS state, native scheduler task and
framebuffer pointers, and diagnostic bad-read counters are outside this committed
raw domain. `GePortState.accumulator_seconds` and dropped-tick accounting belong
to real-time pacing; its derived camera/UI copies and latched logical actions are
also outside this export. Its committed entry point reads `simulation_ticks` for
acceptance, publishes raw pads directly and bypasses solo input publication.
Changing those boundaries requires renewed classification, not copying an
incomplete record's digest into protocol v1.

## Tests and retained evidence

Run `bash scripts/test_mp_state_core.sh`; optionally set
`GE_MP_CORE_EVIDENCE_DIR` to retain exact per-process bytes, logs and a source-hash
receipt. The committed fixtures are `port/tests/fixtures/mp_core_raw_2p.txt` and
`mp_core_raw_4p.txt` (64 ticks each). Each case starts two independent processes,
uses actual `ge_port_advance_multiplayer_tick` and actual input, joy, boss, level,
frame, shuffle and RNG providers, and compares every resulting byte. The harness
binds real provider rosters to allocated test players; it does not run MoveBond,
gun/world simulation, full player bootstrap or networking. Explicit per-tick
shuffle/character-RNG calls exercise those real algorithms without claiming an
entire gameplay transcript. Python `hashlib` independently verifies all tick
digests.

All 75 fields pass sensitivity through 1,418 **actual-owner** mutations per
process, including every ring element, inactive input/shuffle slots, high RNG
and tick bits, and native signed-zero values. The test-only translation unit
includes the actual input/joy source to reach private state, mutates one owner,
recaptures through its getter, checks the expected field plus canonical bytes
and digest, then restores it. No field relies only on mutating exported buffers.
Unknown callbacks, RAMROM, unknown bank, noncommitted pacing, read-only repeated
capture and atomic invalid arguments are checked. The test schema is explicitly
diagnostic; with native completeness false, codec hashing still returns
`INCOMPLETE`.

Retained local evidence is under `build/integration/mp-state-core/`:
`host-receipt.json`, four process logs/byte tapes, `native-owner-audit.json` and
`native-owner-symbols.txt`. The final diagnostic transcript hashes are:

- 2p: `e510e67edcf5ec99cbb39bd652be0fb32c584ea01d244846711f7bc14637652c`
- 4p: `c8563bd64c148f77ab92729302581bb46db5baa05f7de8b5c1d663e583456419`

The existing multiplayer port-tick, player allocation/input and 14 authored
RAMROM parse regressions also pass. No emulator, timing benchmark or aggregate
test wiring was used or changed.

## Native integration handoff

The platform's `CFILES` discovery finds `ge_mp_state_core.c`; the Makefile owner
must add `ge_mp_state_core.o` to the original-ABI flag group used by multiplayer
players: `GE_PORT_USE_ORIGINAL_TYPES`, `GE_PORT_SETUP_DATA`, `AIPARSE`,
`VERSION_US`, `PLAYERFLAG=int`, with inherited `_LANGUAGE_C`,
`GE_PORT_MS_INHERITS` and `-fms-extensions`. Use the ordinary port/original include
paths and ARMv6K/VFP flags. The core source compiles at `-O2 -Wall -Wextra -Werror`
with that ABI. Fresh input/joy objects also compile; the unchanged joy source's
existing parenthesis/empty-body warnings require its existing suppression policy.

Read-only symbol auditing found all 45 non-libc/codec references in actual frozen
native owner objects at `build/integration/dam-aim-source/platform/3ds/build`,
whose pinned build receipt identifies `04cc53e7a6c0de1490b7f4c15520766207f7bb8c`,
plus freshly compiled input/joy getter objects. In particular, `random_byte` is
in `ge_original_multiplayer_players_slice.o`; `array_PLAYER_IDs` and
`g_PlayerTickCount` are in the retained **full-props** scheduler support slice;
`player_num` is in the retained movement-state slice. RNG/frame/lv/boss owners
and the control-style accessor were verified in their actual native objects.
No replacement globals were used for this native ownership audit.

This demonstrates symbol availability, not a freshly linked application. The
integration owner still must rebuild changed input/joy/port objects, add the ABI
flags and force-retain `ge_mp_state_core_capture` in a fresh pinned ARM link.
The frozen source/build was not modified, and main/Makefile ownership remains
with the multiplayer/integration owners.

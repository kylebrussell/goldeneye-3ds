# Native session, rules and respawn global record

`ge_mp_state_session_capture` produces the codec's mandatory kind **0**, actor
**0** record. It does not register or acquire an actor. The caller's nonzero
protocol session scopes this record and every referenced identity. A new match
starts a fresh registry after full teardown; allocation/reuse generations remain
monotonic within that match. No process address, native padding or local display
selection is encoded.

Schema version 4 is a real typed export with 83 fields, **not a complete game checkpoint**.
It currently returns `GE_MP_STATE_INCOMPLETE` even when every exported field has
a value, because the additional native/wrapper owners listed below remain open.
It never changes `GeMpStateSchema.complete` or manufactures missing identities.

## Caller boundary

After player creation, configuration, validated spawn publication and session
reset, supply a `GeMpSessionSchedule` with the actual death owner and each active
player's `GeOriginalBondLiveState`, plus the actual active-prop and HUD owners.
At the initial boundary, `next_tick=0`,
`elapsed60=0` and the death masks are reset. After a successful shared runtime
pass, `next_tick` is the caller's incremented `multiplayer.tick`; all active
death pre/post guards must equal `next_tick-1` and both masks must contain the
entire active roster. The exporter copies the actual four physical guard arrays,
including reset storage for inactive slots.

The terminal-before-world boundary represents the original order where
`session_tick` reaches results and elapsed time advances before another world
pass. It requires the native results decision and the guards of the preceding
completed world tick, or reset guards if the match ended before any world tick.
An unfinished player pass is an explicit scheduling gap. The capture does not
advance the world, select a player, call the unbounded original intro parser, or
republish mutable spawn records. Hold all native storage and the output fixed
through aggregate encoding.

`GeMpSessionProviders.reference` resolves a native pointer and readable extent
to a live typed owner and a **stable logical offset**, not the host ABI byte
offset. For example, the fixture encodes a pad's original 44-byte record offset,
even when the host's native PadRecord is larger. Player refs must be E6,
namespace equal to the array index, slot zero and offset zero; callers should
use `ge_mp_state_players_actor`, which checks the physical storage lifetime.
Content resolvers must establish the actual range and lifetime and provide
separate typed records for mutable intro/pad content and nested STAN/name refs.
The generic codec subsequently requires each live referenced owner record.

Null references are explicit. A nonnull unresolved, stale or incorrectly typed
reference adds a field/index gap and leaves that reference field (and its offset
field) with zero elements. This is a diagnostic omission, never a fabricated
null success. External demo storage is read only after its resolver has
validated the full native header extent; an unknown pointer is never dereferenced.

## Native field ownership

| Fields | Native owner and interpretation |
| --- | --- |
| 256–265 | Private session active count, decision, first end reason, results/freeze cause masks, actual `g_MpPoint`, `g_stopPlayFlag`, `g_gameOverFlag`, `g_pausedFlag`, and ordered active E6 roster refs. Signed flags remain signed words: `g_gameOverFlag >= 2` is a countdown, not a Boolean. |
| 266–271 | Actual scenario and weapon set, start-pad count, two starting weapons and all 16 ordered start-pad owner refs/offsets. No sorting or synthetic spawn distribution. |
| 272–282 | Owner-retained validated intro and word bound, pads and count, selected demo slot, and the current setup's intro/pads refs. A mismatched or missing publication remains a gap. |
| 283–288 | Actual playback/recording flags; demo binding discriminant (0 absent, 1 the rules owner's header, 2 external), actual selected header slot, and external header owner ref/offset. |
| 289–298 | Caller next committed tick/elapsed/boundary, actual death owner count, E6 roster refs, pre/post tick arrays and masks, and each active live wrapper's `initialized` admission flag. |
| 299–321 | Actual active-prop counters, count, Chr count, bound/pending/player count/mask, last mismatch, ordered-allocation presence, E3 player refs, setup owner and Chr pool refs, and current native Chr/propdef/AI publication. |
| 322–328 | Actual HUD tick, player count, initialized/prepared/captured/failed and intro-camera mask. No display snapshot or device-local selection. |
| 329–336 | Actual watch pause owner, glow/alternate state and timers, frontend mode/count and all four physical favorite-weapon pairs. |
| 337–338 | Actual player allocation owner's explicit launch-control flag and all four physical control-style words. Active values feed later native respawn initialization; inactive storage retains its actual allocation initialization. |

The new getters in `ge_original_multiplayer_session` and
`ge_original_multiplayer_rules` borrow the real owners. They maintain no duplicate
simulation state. The session getter validates the bound active roster before
reading it. Its inactive saved pointers are excluded because every native read
is bounded by `session_count`, and reset overwrites newly active entries.
Consequently a 4-player match followed by a 2-player match does not export stale
inactive addresses. When the validated intro is null, its old demo selector is
also inactive: reconfiguration blocks respawn and the next successful publication
overwrites it before use. Missing publication still prevents successful coverage.

The rules-owned synthetic demo header is classified by its owned binding and
slot, the only field read by retained `check_ramrom_flags`/intro/respawn selection.
Its other fields are not represented as invented zeros. Playback or recording
adds a separate RAMROM stream/header-owner gap; completing general replay needs
its actual header fields, stream cursors and record callback ownership. An
external demo always needs its own typed content reference, even when its flags
are currently zero.

## Other owners and remaining gaps

E0 owns the native timers, `g_MpTime`, control lock, selected/current player,
shuffle, RNG and input/replay transport state. E1/E2 own inventory, permissions,
teams, kills/deaths, flag counters and YOLT ranking. E6 owns native player death,
fade, spawn cursor and per-player menu state. These are not duplicated here.

Three explicit gap paths remain even when the active-prop and HUD owners are
supplied and valid:

- `runtime.guards.matrix_muzzleflash_tick_guards`: wrapper guards that admit
  canonical model/muzzle updates require the real guard owner.
- `runtime.player_floor_gun_model_loader_providers`: current selected provider
  bindings, their model/content lifetimes and canonical floor/spawn dependencies.
- `mp_music.mission_state_and_transition_rng_owner`: original music transitions
  can choose a random track; these cannot simply be called cosmetic and erased
  from a future-state audit.

These gaps cannot be cleared by passing zero values or a generic success callback.
Whole-game completion also still requires all typed actor/object/character,
model/animation, effects and BG owners; content identities are not their payloads.
E0's existing death-guard omission can only be retired by an aggregate owner that
actually supplies this capture and validates the shared boundary.

`GeOriginalBondLiveState` initialization/input/move counters are write-only
diagnostics. Its loadout summary is read while binding initial hand models; the
post-setup model and inventory state belong to E4/E6 and E1/E2. Only its live
`initialized` flag admits the next movement pass and is included here. The
runtime's `interaction_failed` is reset before each use and is zero at a
successful whole-world boundary. Failed partial frames must abort, not be
captured as a commit. View-local snapshots, current display/eye, renderer buffers,
wall-clock accumulator, pending device buttons, control routing, frontend request
sequence and diagnostic status/counters are not canonical fields in this record.
Chevron glow and alternate game-over message state select native menu
presentation and are retained verbatim with that owner. The mission music state
is separately identified above.

## Active-prop and HUD owner closure

`GeMpSessionSchedule.active_props` must be the actual published owner verified
by `ge_original_stage_active_props_native_view`, not a copied struct with matching
fields. Successful compose publishes the private owner and its actual allocated
`ordered` pointer together. Closing that current owner clears both; closing a
replaced owner leaves the replacement intact. The getter adds no native prop
allocation/free operations or replacement scheduling. An altered ordered pointer,
unbound/replaced owner, pending pre-tick, wrong player roster, mismatched native
Chr pool/count or unresolved typed reference retains the active-prop gap.

The wrapper's `ordered` entry payloads and `player_prop` compatibility alias are
consumed during compose and never read by later tick/admission code. After compose,
`ordered` is only checked for nonnull and freed by close. Capture therefore checks
its allocation ownership and serializes its actual presence, without reading or
inventing live identities for those old entry pointers. Legitimate pickup frees
can leave such old entries behind; the actual changing prop order, free list,
rooms and generation history belong to E3/E5. Numeric counters and the stored
count are copied verbatim, even where they are diagnostic. Setup-wrapper and
Chr-pool refs require their real typed resource owners; nested setup payloads are
not covered merely by identifying their owner. The getter's allocation identity
is a host ownership check and never enters the wire record.

HUD admission is read from the actual public owner. At a successful world boundary
its tick must be `next_tick-1`, its player count must match, both prepared/captured
masks must cover the complete roster and it must not be failed. Initial state has
no admitted HUD frame. Missing or incomplete HUD state retains the named HUD
scheduling gap. Its durable display snapshots are excluded because they are
only read by presentation; the next begin-frame clears them before production.
Native HUD producers and their queue/content dependencies are separate owners.

### Retained watch transition owner

`ge_original_multiplayer_watch_snapshot` now reads the actual retained menu
owner. It has no duplicate zero state or weak-symbol discovery. See
[MultiplayerWatch.md](MultiplayerWatch.md) for the exact source closure, native
awards RNG/audio transitions, input ordering and result exit boundary.

The original first shuffled player owns global timers. `who_paused` changes
when a player pauses; `mpwatchUnpauseGame` does not clear it. Glow/message state
changes only in menu ticks and awards. Favorites are overwritten only for active
players during awards. These actual fields are exported verbatim, including
prior-match history. Full kind0 initial bytes therefore need not converge after
different previous menu histories. Tests preserve that history; the historical
cross-history comparison explicitly projects fields 256–328 only. It is not a
whole-native-state digest or permission to zero these fields. Agreeing a native
starting watch state across network participants remains a protocol boundary.

## Validation

Run `bash scripts/test_mp_state_session.sh`. It extracts original Caves
`Ump_setupcrypZ` intro/pads, canonical player allocation/inventory/counter bodies,
original death/fade/blood and exact match end ordering. Only resource storage,
content identities, animation end-frame input and sound/text sinks are provided
by the fixture; no replacement scoring, blood or death algorithm is used.

ASan/UBSan run native and short-enum binaries in separate 2/4-player processes.
They compare the prior-domain typed payload projection across different local
selections and prior-match histories, including a repeated 2→4→2 ownership
sequence. Watch fields also receive native mutation checks. Tests
exercise native scalar/array mutations, authored pad order and offsets, all 14
weapon sets, bounded publication changes, owned/external/missing demo bindings,
missing/stale/wrong player/content identities, partial death guards and roster
loss, and original freeze → completed death → results, time/point priority and
abort transitions. Read-only checks retain current player, RNG and death metadata.
Launch-control tests also change the explicit flag and every active style through
the actual owner, reject invalid styles atomically, and verify destroy/recreate
returns to the diagnostic default selection without altering native menu options.
The public codec rejects an aggregate digest with the sentinel output unchanged.
The fixture fingerprint is explicitly a typed test payload, never a state hash
claim. The exporter and both getter owners also compile with strict ARM
`-fshort-enums` flags.

The added owner fixture performs 26/28 additional native field mutations for
2/4 players. It exercises actual compose/close/replacement lifetimes, native
zero-clock admission, and the real HUD begin-frame rejection path. It does not
replace gameplay dispatch: either dispatcher being reached in this focused
fixture aborts the test. A separate existing active-prop scheduler test covers
the dispatched phase order, and the existing HUD test covers the complete
canonical producers. The new fixture also checks consumed-entry/display-payload
exclusion, stale/missing resource refs, allocation-pointer replacement, partial
HUD capture, independent-process/ABI equality, and repeated match convergence.
All captures remain incomplete and the public digest remains untouched.

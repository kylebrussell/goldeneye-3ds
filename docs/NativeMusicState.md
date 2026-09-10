# Native music transition control (EF)

`ge_mp_state_music_capture` reads the native `mp_music.c` transition owner and
the actual retained `music_0D2720.c` tables. Kind EF, namespace 255, slot 0 has
one match-scoped registry identity. Main acquires it with the stage identities
and retires it at stage end. The underlying native globals have program
lifetime; capture never initializes or normalizes them.

| Field | Type/count | Native storage |
| --- | --- | --- |
| 256 | I32 | `stageMusicID` |
| 257 | I32 | `mission_state` |
| 258 | I32 | saved `dword_CODE_bss_8008C604` |
| 259 | I32 × 4 | `music_slot_active_0` |
| 260 | I32 × 4 | `music_slot_minutes_0` |
| 261 | I32 × 4 | `music_slot_seconds_0` |
| 262 | I16 × 96 | 24 native setup rows in stage/main/background/xtrack order |
| 263 | I16 × 44 | ordered `random_tracks`, including `M_NONE` |

Every physical slot is retained, including inactive timers. Both table
sentinels are included. These are explicitly typed field values, not native
struct bytes or addresses. No runtime table writers were found in the source
audit, but their actual retained contents are captured rather than assuming a
separate generated lookup has equivalent order. Build/content manifests still
cover the interpretation and the rest of the game's content. E0 owns the full
shared gameplay RNG stream used by native random-track selection.

Capture validates the exact registry key and live generation before reading.
Invalid/stale captures publish no record. Successful snapshots own their values
and must remain at their address while the record's field pointers are used.
Capture does not call music, timers, audio or RNG and does not mutate registry
ownership. It does not claim complete aggregate simulation coverage.

The native music stage initializer `sub_GAME_7F0C11FC` resets only stage and
mission state. It preserves saved pre-watch mission state and slot history.
The distinct `lvlStageLoad` call at `lv.c:400`, `sub_GAME_7F0C1364`, clears all
three slot arrays. Main now invokes that exact reset through
`ge_original_multiplayer_music_stage_reset` immediately before the existing
music stage initializer, after the native stage timer initialization and before
live guard/body construction. It does not clear the saved pre-watch state.
The native slot countdown runs in the common non-title world tail after
`chrpropTick` and before `langTick`, outside the multiplayer limit branch.
Its production world-loop call remains to be restored in a separate change;
it must not be inserted into the pre-limit session decisions.

The session source slice also retains the original pre-limit death music
recovery. If mission state is 6, at least one player is dead, and every dead
player has completed the blood effect, it calls the original transition to
state 1 before time/point/YOLT limit processing. The source does not additionally
require animation/fade completion or a nonzero gameplay clock. Both the getter
and predicate are retained exact source. This restores a missing production
transition independently of the EF exporter.

The source audit distinguishes this transition owner from audio replay. No
feedback from the 18 native track/volume/fade request scalars into mission,
slot eligibility or shared RNG decisions was found. The current port's null
guard bypasses the original sequence-player wait path; the port synth has
separate players. Consequently EF does not hash DSP buffers, device queues,
wall-clock audio completion or those audio-request scalars. A change to that
backend boundary requires this classification to be reviewed. Full native
audio-state replay remains a separate scope.

`bash scripts/test_mp_state_music.sh` runs both enum ABIs with ASan/UBSan and
independent peer processes. It executes actual native stage/track selection,
slot activation/countdown/reset, watch save/restore, death recovery transitions,
and stage history. Audio calls terminate at an explicit test resource sink.
Tests cover all 15 signed fields, every table element, immutable capture,
durable values, stale generations and refusal to digest an incomplete schema.
Native table extents are checked in the translation unit containing the actual
definitions. `test_multiplayer_session.sh` separately executes all dead/blood
predicate combinations for two/four players and actual stage-slot reset, plus
its existing native damage, clock, limit, awards and results checks in both ABIs.

The preceding Caves combat gate omitted main's stage music initializer and fade
service and placed native stage timer initialization earlier. Its CPU
shared-runner completion evidence remains bounded by that startup difference.
Corrected tapes and repeat-session acceptance must use main's actual startup
sequence and preserve the random-track RNG advance; they cannot seed over it.
EF and these fixes do not establish full state agreement, native alarm/SFX
coverage, radio behavior, GPU output or hardware timing.

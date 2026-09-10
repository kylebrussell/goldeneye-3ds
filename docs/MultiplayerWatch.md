# Retained native multiplayer watch and awards

The shared runtime now calls the original `mpwatchMenuTick` at the original
`lvlViewMoveTick` boundary: after `MoveBond` and `watch_time_0`, before camera
and props. `ge_original_multiplayer_watch_bind` publishes the already-created
roster's actual frontend MULTI mode/count. Session reset calls it automatically.
It preserves player initialization, inventory, statistics, RNG and prior-menu
state. The runtime requires the canonical shuffle to be a valid permutation.

`scripts/extract_multiplayer_watch_slice.py ROOT OUTPUT` retains the exact
menu input/navigation/pause helpers, full `mpCalculateAwards`, native award
names, favorite-weapon selection/storage, shot-register getter, and real watch
and frontend globals. Compile the generated source and adapter with the same
native types/ABI/setup/US flags as the other original multiplayer slices.
Regenerate the session slice too: watch now exclusively owns glow/message state.

The full awards path includes native SFX flag shutdown, mission/music transition,
all accuracy/kill/death/armor/movement/life metrics, random tie selection, random
award selection, weapon favorites and localized text publication. The existing
production music owner remains responsible for its actual output backend. The
SFX resource owner marks returned root states with native flag 1 and implements
flag-filtered deactivation over its actual sound slots, clearing event flag 0x10.
There is no successful no-SFX substitute in production.

Two native C/ABI repairs are explicit: inactive scratch metrics are initialized
before otherwise-unused third/fourth call arguments are evaluated; the decompiled
ignored `bondinvGetWeaponOfChoice` return gets an explicit return. Active metrics,
selection and RNG order are unchanged. Legacy 32-bit award text words retain
native pointers on ARM. Wider hosts use typed per-field tokens and a sidecar
text reference guarded by the physical player lifetime. Call
`ge_original_multiplayer_watch_award_text(slot,award,&text)` to resolve them;
never cast a host token to a readable string. Missing text is an explicit
resource failure. E6 text references still require stable content identities.

`ge_original_multiplayer_session_tick` executes complete awards for each terminal
cause in source order. Simultaneous time/point/YOLT conditions can consume awards
RNG more than once in the same frame. Later session ticks are idempotent.
Confirmed native menu quit publishes the private session ABORT cause and invokes
this same complete awards path. Failed partial transitions require caller failure
and teardown, not retrying the session as a successfully committed frame.

The caller must continue committed input and all shuffled player passes after
RESULTS. Control lock makes the next game clock zero; `speedgraphframes` still
advances watch timers. The first shuffled player alone drives `g_gameOverFlag`
from values >=2 toward 1. Source calls pass FALSE (immediate results); the exact
TRUE branch's 300 NTSC ticks are tested separately. Every player's native menu
must reach `MENU_FINISHED` before `bossSetLoadedStage(LEVELID_TITLE)`. The existing
boss snapshot exposes that request. Exiting the outer loop at END_RESULTS would
skip native acknowledgement and is an integration error. Read-only native menu presentation is provided by the separate
[watch renderer](MultiplayerWatchRender.md); the runtime tick remains the sole
menu/input/awards owner. UI upload and main-loop continuation are integration
boundaries and require separate full-match validation.

Kind0 schema v3 reads actual who/glow/alternate/mode/count/favorites. No reset
normalizes their previous-match history. Other native ownership gaps keep the
aggregate incomplete; no diagnostic payload is a compatibility digest.

`bash scripts/test_multiplayer_watch.sh` runs ASan/UBSan for 2/4 players in native
and short-enum processes. It uses actual native player allocation, inventory,
raw input/joy ring, menu bodies, awards and music-state/table code. Audio rendering
and localized strings are explicit resource observations. It verifies initiating
player pause ownership, unauthorized unpause, death-menu closure, native life
statistics, first-shuffled timers, exact-source awards/RNG/text/favorite equality,
menu-driven quit, actual SFX flag filtering, 300-tick countdown, all-player title
exit, B-back navigation, and repeated player lifetimes. Different local selections
and ABIs produce identical final award/RNG bytes. Session damage/death/limit tests,
kind0 mutations/history projection and shared-runtime phase observation remain
separate fixtures; none replace full native match acceptance.

Award language resolution is explicitly bound to `ge_original_language_text`
and the real `LmpmenuE` pointer table. The packed original `langGet`/`g_LangBanks`
loader is not invoked by this slice. Tests link the actual language provider and
all its native table dependencies, including authored Quad Kill text.

Awards preflight also checks the original float-helper conversion boundary.
`mpFindMaxFloat`/`mpFindMinFloat` deliberately truncate selected metric values
through signed `s32 aux`; preserving this native quirk requires validating the
cast-reachable first three slots before side effects. Armor, average speed,
kill/shot ratio and kill/death ratio use the exact source f32 expressions and
must lie in `[-2147483648,2147483648)`. The fourth value is only compared and is
not given an invented conversion. Invalid metric fixtures snapshot all player,
permanent/watch, RNG, music and SFX state and assert transactional rejection.

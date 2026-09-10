# Canonical multiplayer session state

`ge_original_multiplayer_session.h` supplies the opaque native stage boundary
for round limits and stop/play state. It retains source from `lv.c` and
`mpmenu.c`, with the original shared player roster, scoring functions, death
completion fields and watch flags. The API does not reset players or create
replacement scoring, death, pause or respawn mechanics.

## Runtime order

1. Create/configure the multiplayer roster, initialize the existing level
   timer owner, and call `ge_original_multiplayer_session_reset(time_limit60,
   point_limit)`. Reset validates the roster and applies original
   `lvlSetControlsLockedFlag(0)`, `mpwatchUnpauseGame`, `lvlSetMpTime` and
   `lvlSetMpPoint`. It preserves stats, inventory, player selection and RNG.
2. Obtain the current canonical simulation clock, then call
   `ge_original_multiplayer_session_tick(tick60, elapsed60, &state)` before
   adding this tick to elapsed time. This is before the original
   `chrlvAllChrTick`, regeneration and shuffled viewport/player passes.
3. `FREEZE` calls original `mpwatchSetStopPlayFlag`: `g_stopPlayFlag=TRUE`.
   It does **not** set `g_pausedFlag`, lock controls, or zero time. The
   original input/action gates consume the stop flag while death animation,
   red blood, colour fade and world state continue advancing. Point limits
   wait for every dying player's three death gates. YOLT waits until all but
   one player have exhausted two lives and finished those gates.
4. `RESULTS` applies the exact state-only entry of
   `mpCalculateAwards(FALSE)`: mission state zero, `pauseAndLockControls`,
   `g_gameOverFlag=1`, original message/glow timers and each player's
   `mpmenuon=TRUE`, `MENU_SCORES`, and cleared award-label pointers. Selection
   is restored after that per-player publication. The already-produced
   `g_ClockTimer` remains unchanged; the **next** original level timer pass
   sees the control lock and produces zero clock/delta.
5. Snapshot results with the existing canonical result snapshot adapter,
   using `state.end_reason`. Explicit cancellation calls
   `ge_original_multiplayer_session_abort(&state)`, which uses the same
   immediate result state as the original multiplayer quit-confirmation
   path. Repeated terminal calls retain the first published result state.

All actual `mpCalculateAwards` call sites pass `FALSE`: time limit, point
limit, YOLT and multiplayer quit. The `TRUE` branch can assign 300 NTSC ticks
(250 PAL), but no original caller uses it. The adapter introduces no delay.

The original source stores no end-reason field. Its time, point and YOLT
checks are independent and can call the result function more than once in
one tick. `results_causes` preserves all those conditions; `end_reason`
identifies the **first source call** (time, then points, then elimination) as
an explicit reporting convention. `freeze_causes` records independent point
or elimination stop requests. All three original action blocks are retained
by the extractor; only terminal side effects are observed as cause bits and
YOLT's nonnegative death sum is widened to avoid native signed overflow.

## Initial watch state and ownership

The original `initBONDdataforPlayer` sets each allocated player's
`mpmenuon=FALSE`. Other menu fields are assigned when opening the menu; a
stage-start adapter must not force every player through solo watch setup.
`mpwatchUnpauseGame` resets exactly stop-play, game-over and paused flags. It
does not clear per-player menus or `who_paused`; fresh player initialization
owns the former, and the latter is assigned when a player pauses. Native
`checkGamePaused` already returns the same canonical `g_pausedFlag`.

`init_watch_at_start_of_stage` in `options.c` is a separate solo-watch UI and
selected-folder settings initialization path. It modifies the current
player's controller settings and calls `fileLoadSaveSettingsForSelectedFolder`.
It is not a replacement for publishing each multiplayer participant's agreed
controls/options and is not invoked by this adapter.

Cosmetic award calculation and music/SFX presentation remain separate. The
full `mpCalculateAwards` changes music and selects award labels using extra
accuracy, movement, weapon and RNG metrics. This bounded state adapter does
not fabricate those awards or consume their RNG. It preserves the original
state entry needed to stop the match and publish the existing canonical
score/kills/deaths/rank snapshot. Multiplayer menu navigation, per-player
pause/quit agreement and final title-stage exit remain frontend owners;
the adapter does not silently emulate their input state machines.

The existing `lv.c` timer slice owns `g_MpTime`, clock and control-lock
storage. The existing damage/input slices own stop-play, game-over and paused
storage. The new generated slice adds previously unretained `g_MpPoint`,
message/glow fields and exact accessors; there are no duplicate flag owners.

## Build and validation

Generate `ge_original_multiplayer_session_slice.c` with:

```
python3 scripts/extract_multiplayer_session_slice.py ROOT OUTPUT
```

Compile it and `ge_original_multiplayer_session.c` with the same original
ABI/type/setup/AIPARSE/US flags as the multiplayer rules adapter. The session
fixture links the existing exact rules and damage/inventory fixture closure,
original `lv.c` timer slice, original control-lock/action-gate functions, and
the new source extraction. Resource-only mission-state/audio boundaries are
observed without substituting health or death accounting.

`scripts/test_multiplayer_session.sh` passes under ASan/UBSan. It covers actual
2/4-player damage-to-kill transitions, each independent death gate, freeze
with advancing clocks, immediate result/menu flags, next-frame clock lock,
normal/time/point/YOLT/token/Golden Gun/team/LTK limits, negative suicide
scores, zero ticks, simultaneous cause ordering, explicit abort, repeat result
entry, state-preserving reset, invalid roster/current/timer/stat rejection,
and player selection/RNG preservation. Each decision and YOLT metadata is
compared to the existing canonical rules extraction from the same starting
state. Both native adapter and generated slice pass strict ARM compilation.

# Multiplayer HUD capture

`GeOriginalMultiplayerHud` retains the presentation side effects of
`bondview2.c:maybe_mp_interface` at each canonical shuffled shooter, and owns
four durable snapshots for later native display. It does not select players,
run guns, advance fade/death, call watch model rendering, or change visibility.

Call `begin_frame(state, simulation_tick, player_count)` once, then for every
selected canonical shooter:

1. Run the original gun and first-person/muzzle preparation.
2. Call `prepare_shooter(state, slot)`. This copies pre-death gauge geometry
   and watch/menu flags, then runs the original alternate MP gauge branch's
   `healthdisplaytime` decrement and zero clamp.
3. Run original death and eligible respawn handling.
4. Call `capture_shooter(state, slot)`. This runs the exact lower-message
   timer, the original bottom-render message-index writer, and the exact
   upper-message timer, capturing each queue immediately. It also copies
   post-death ammo, exact sight commands and decoded rectangle, and fade.
5. At the local slot, bake the native world/first-person scene before selecting
   a later shooter.

After the loop, `get(state, simulation_tick, local_slot, &snapshot)` is a
read-only value copy. The state must be zero-initialized at match creation;
it rejects stale ticks, incomplete previous frames, wrong selected players,
duplicates, and advancing to another player before completing capture. Failure
after a timer producer poisons the frame so a retry cannot tick it again.

All snapshot strings, gauge vertices, sight commands/material/vertices and
fade fields are owned values. Native text/icon builders may run later using
this snapshot and immutable font/texture metadata. Both eye passes use the
same result. Final draw must not call either message timer, gauge preparation,
or live-player snapshot getters for multiplayer.

The upper queue/timer remain globally shared, as in the original: they tick
once for each canonical viewport, subject to the selected player's menu
guard. Lower messages use the selected player's timer and message buffer;
multiplayer enqueue never increments the solo `display_statusbar` counter.
The generated snapshot now handles that original MP path independently of
the solo queue and retains its message-index write at capture. MP anchors
use original US offsets within the native 320x240 viewport at (40, 0); the
existing solo native anchors are unchanged.

Do not add a general `watch_open` suppression around MP HUD builders. The
original tail runs even with the watch open; canonical HUD/sight flags and
the exact menu guards determine visibility. `watch_open` records any nonzero
animation state; `watch_objectives_visible` records the separate fully open
objective-page condition. This adapter captures watch flags, not watch model
geometry or mutable objective text. MP menu/results/radar rendering is owned
by their respective providers and is not implemented here.

Fade capture retains the read-only prefix of `currentPlayerDrawFade`,
including its black override when either camera frame counter is active.
The movement/death owner advances fade time. `cameraFrameCounter1` and `2`
must be backed by their canonical native state, never weak fallbacks.

`scripts/test_multiplayer_hud.sh` compiles the extracted original timer,
enqueue, gauge, sight and fade logic with native and short-enum ABI modes
under ASan/UBSan. Each runs 216 shuffled 2/3/4-player frames covering all local
slots, menu/watch/intro/game-over guards, timer underflow, pre-death gauge vs
post-death ammo capture, stale/duplicate/failed calls, overwritten live
strings/fade, and byte-identical read-only eye snapshots. The existing
`scripts/test_dam_mission_exit.sh` regression passes, including solo HUD,
native ROM-font HUD builders, and exact source contracts. These are portable
provider tests; no emulator or hardware presentation claim is made.

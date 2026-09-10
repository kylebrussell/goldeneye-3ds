# Multiplayer death and fade progression

Death completion affects respawn eligibility and must advance for every
simulation player regardless of which view a machine displays. The adapter
owns scheduling metadata; original `struct player` fields own gameplay state.

Initialize `GeOriginalMultiplayerDeathState` with
`ge_original_multiplayer_death_reset` after creating the canonical roster and
player props. For each tick, select every canonical player in the original
shuffled order and call `ge_original_multiplayer_death_pre_move(state, tick)`
immediately before `MoveBond`. It runs the original screen-fade update followed
by character-fade update, matching `bondviewMovePlayerUpdateViewport`.

After each player's original gun update, call
`ge_original_multiplayer_death_post_gun(state, tick)` with the shared dynamic
frame arena still open. This is the state portion of `maybe_mp_interface`:
start death, decode the original blood stream, test original animation frame
against its end frame, and start the original 60-tick screen/120-tick character
fades. Animation-frame production remains owned by the original movement/gait
pipeline. No completion flag is forced and no view is drawn. The existing rules
owner checks original scenario/button conditions and performs respawn after
this boundary. The original first-person render calls precede this death block;
any gameplay RNG they consume must likewise run for every simulation viewport
before this call, rather than during local drawing.

Both calls return 1 on success and 0 for invalid roster/timers, duplicate phases,
missing phases or skipped/reversed tick numbers. Post-gun also rejects a closed
or insufficient arena before decoding blood. Each active decoder allocates two
80-by-96 buffers through original `dynAllocate`; all four players fit the blood
portion in the existing shared arena, whose remaining capacity must also cover
gun/body/effect allocations. Drawing a second eye must consume publications
without invoking these transitions. The solo mission-exit service must not
also tick multiplayer fades or death state.

`scripts/extract_multiplayer_death_slice.py` retains original fade functions and
the death block up to the respawn policy, specializes the existing multiplayer
music-state branch, and removes only the blood display-list sink. It records
source hashes and checks call order. One explicit native ABI conversion is
required: the original `(s8)(frac * 255)` character alpha is represented as
`(u8)(s32)(frac * 255)`, preserving MIPS word conversion followed by a low-byte
store without undefined float-to-signed-byte conversion at alpha 128–255.

`scripts/test_multiplayer_death.sh` runs ASan/UBSan fixtures for two/four players
with different selected display slots, using the real authored blood stream,
decoder, original `dynAllocate` and original animation frame/end accessors.
It verifies blood completion, delayed animation completion, exact fade lengths,
pause behavior, alpha results, duplicate/phase rejection, insufficient-arena
recovery, and equal outcomes independent of display selection. Animation frames
are fixture inputs; this does not prove the full movement/body runtime.

The related allocation API
`ge_original_effect_buffers_reset_players(count, stage_id)` retains the original
effect capacity branches: particles are `MAX_FLYING_PARTICLES / count`, halved
again for Streets/Depot; scorch storage is absent in multiplayer. The existing
solo reset remains the Dam-compatible convenience entry point.

## Render RNG audit

Canonical `gunUpdateAndFire` consumes gameplay RNG for recoil/muzzle geometry
(`gunfire.c`, including flash scale/rotation and additional muzzle nodes).
Calling this once for every simulation player preserves its ownership; running
the older `ge_original_first_person_pose_tick` as well would duplicate recoil
and flash RNG. At review time the main loop no longer calls that older tick.

The legacy `ge_original_stage_guard_runtime_muzzle_flash` path consumes two
gameplay random numbers for a canonically visible weapon from the local draw
collector; its private RNG fallback only covers extra stereo visibility.
Different local viewers can therefore consume different global RNG counts.
The companion guard owner now provides
`ge_original_stage_guard_runtime_prepare_muzzle_flashes` for every simulation
viewport and `ge_original_stage_guard_runtime_muzzle_flash_for_view` to consume
its retained publication. Multiplayer must opt into that preparation before
local/stereo drawing. This adapter does not alter that separate owner.

The canonical beam display-list builder `sub_GAME_7F061E18` also consumes gameplay
RNG for watch-laser alpha/colour. Any future retained beam draw path must account
for this per-viewport consumption. These render-RNG concerns remain distinct
from the death adapter, which consumes no random numbers.

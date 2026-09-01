# Dam mixed `propsTick` activation boundary

The host sanitizer in `scripts/test_dam_mixed_props_tick.sh` mechanically
extracts the unchanged US `propsTick` body from `src/game/chrprop.c`. It drives
one mixed active list in the same tail-to-head order used by the game. It does
not install that dispatcher in the 3DS runtime.

## Fixture and branch map

| Active prop | Authored/native state | Full `objTick`/`playerTick` entry |
| --- | --- | --- |
| Viewer | `PROP_TYPE_VIEWER`, no multiplayer `ChrRecord`, onscreen | Single-player `playerTick` takes `clear_and_return`, clears `PROPFLAG_ONSCREEN`, returns `TICKOP_NONE`. It does not run `MoveBond`. |
| Guards 0-3 | Setup commands 23-26, body 37 (`greatguard2`), authored character and AI IDs retained | `propsTick` selects `chrTick`. The mixed harness traps outside the four expected guard records; exact `chrTick` has separate sanitizer coverage. |
| Glass | Setup command 107, `PROP_TYPE_OBJ` / `PROPDEF_GLASS` | Static-object `objTick` path. The materializer alone does not supply the model instance that `objTick` dereferences at entry, so exact dispatch must wait for the existing native constructor. |
| First prop | Setup command 122, `PROP_TYPE_OBJ` / `PROPDEF_PROP` | Static-object `objTick` path, with the same model-instance prerequisite. |
| First gate | Setup command 267, `PROP_TYPE_DOOR` / `PROPDEF_DOOR` | Door simulation branch, after common object processing. This owns the work currently reached through the bounded exact door runtime. |
| Linked gate | Setup command 268, `PROP_TYPE_DOOR` / `PROPDEF_DOOR` | Same door simulation branch. Both native door/model constructors and authored link must be complete first. |
| Covert modem | Exact `ITEM_BUG`/`PROP_CHRBUG` fresh-slot construction and prepare-throw state; `PROP_TYPE_WEAPON` / `PROPDEF_COLLECTABLE` | A just-launched object with `HASPROJECTILE` first returns `TICKOP_RETICK` and sets `ISRETICK`. Its retick clears `ISRETICK`, advances projectile physics, then reaches collectable-specific processing. The focused mixed pass begins at this second-pass state; activating full `objTick` also requires exact delist/reactivate-this-frame list mutation. |

Explosion, smoke, unexpected prop records, unexpected tick operations, and
retick list mutation are fail-fast traps in this focused harness. Expected
`TICKOP_NONE` commits and the normal first-player alarm/gas/sound/defrag tail
are counted explicitly.

## Exact production switch-over

The current 3DS loop must not call `ge_original_door_runtime_tick` in a frame
where full `objTick` is owned by canonical `propsTick`; both execute the same
door simulation and would double-advance both gates. The dormant guard wrapper
must likewise be replaced, not run alongside a new mixed `propsTick`, because
it already calls that exact dispatcher.

The current separate `MoveBond` tick remains necessary. The exact
single-player `playerTick` branch for the viewer does not perform player
movement; with no multiplayer body `ChrRecord`, it only clears the onscreen
flag. Gun update must precede mixed `propsTick` in the shared arena generation,
then guard and object consumers may append allocations, followed by one arena
finalize.

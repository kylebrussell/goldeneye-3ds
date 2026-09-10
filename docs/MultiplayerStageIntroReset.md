# Multiplayer stage-intro reset subset

Call `ge_original_multiplayer_stage_intro_shared_reset_exact()` once after
multiplayer configuration and setup preparation, before the first initial
spawn. `runtime_multiplayer_place_players` uses that order. The first spawn
calls `ge_original_multiplayer_publish_spawns` before selecting a pad; that
publication rebuilds `startpadcount`, `g_Startpad` and `starting_weapon` from
the prepared intro. Calling this reset after publication would erase the
count and starting weapons.

`extract_multiplayer_stage_reset_slice.py` copies the following assignments
from the pre-record prefix of
`bondview_r.c::bondviewLoadSetupIntroSection`, preserving their exact text and
relative order. It emits external declarations derived from the original
native declarations, including the regional `watch_time_0` declaration. It
defines no game state or aliases. The existing native source slices own all
these symbols; the shared runtime slice owns `watch_time_0`.

| Retained native assignments, in source order | Original reset |
| --- | --- |
| `g_isBondKIA`, `g_bondviewForceDisarm`, `cameraBufferToggle`, `cameraFrameCounter1`, `cameraFrameCounter2` | Zero |
| `camera_80036438`, `credits_state`, `credits_pointer` | Zero / null |
| `g_ForceBondMoveOffset.f[0..2]`, `g_SurroundBondWithExplosionsFlag`, `startpadcount` | Zero |
| `g_PlayerIsInTank`, `g_PlayerTankProp`, `g_TankSfxState[0..1]` | Zero / null |
| `g_TankTurnSpeed`, `g_TankOrientationAngle`, `g_TankTurretVerticalAngle`, `g_TankTurretVerticalAngleRelated`, `g_TankTurretOrientationAngleRad`, `g_TankTurretOrientationAngleDeg`, `tank_turret_turn_speed`, `g_TankTurretAngle`, `g_TankTurretTurn`, `g_ExplodeTankOnDeathFlag` | Zero |
| `is_timer_active`, `g_PlayerInvincible`, `g_CameraMode`, `g_CameraAfterCinema`, `stop_time_flag` | One, false, zero, zero, zero |
| `mission_timer`, `watch_time_0`, `watch_transition_time` | Zero, zero, `0.9090909f` |
| `starting_weapon[GUNLEFT]`, `starting_weapon[GUNRIGHT]` | `ITEM_UNARMED` |

Per-player allocation and `bondviewPlayerBeginLife` do not reset this shared
state. A previous death/end camera mode prevents the original collision
publication from updating a bodied player's view position. Pending solo
force-disarm advances across the next roster's `MoveBond` calls and unequips
whichever player reaches its threshold. `AI_BondDisableDamageAndPickups`
leaves native `g_PlayerInvincible` true, which suppresses both actual player
damage and the original `propsTickPlayer` pickup pass. Tank mode, pointers,
audio handles and movement offsets are also read by retained movement code.
The mission timer supplies begin-life `lifestarttime60`; watch transition
state is read by the original watch progression.

This is an explicit statement subset. The following original prefix names
have no strong native storage in the linked source closure and are omitted:
`resolution`, `g_WorldTankProp`, `g_PlayerTankYOffset`,
`tank_turret_unused_angle`, `g_BondCanEnterTank`, `camera_fade_active`,
`camera_transition_timer`, `intro_camera_index`, `g_IntroSwirl`,
`ptr_random06cam_entry`, `g_CurrentSetupIntroCamera`,
`g_SetupIntroCameraCount`, and `g_IntroAnimationIndex`. No new owners are
created for them. The original Cuba-only framebuffer allocation branch is
excluded as a whole, including its conditional `cameraFrameCounter1 = 1`;
multiplayer arena setup never selects Cuba. Local intro parsing/spawn
variables and all post-prefix intro processing remain outside this adapter.

The viewport owner separately advances `mission_timer` immediately before
`MoveBond` for canonical slot 0 and `watch_time_0` immediately afterward for
every viewport, using the original region-specific statements. A stage reset
does not substitute for that progression.

Generate `ge_original_multiplayer_stage_reset_slice.c` with:

```sh
python3 scripts/extract_multiplayer_stage_reset_slice.py . OUTPUT.c
```

The production build links its object alongside
`ge_original_multiplayer_stage_reset.o`. Generator prerequisites are
`scripts/extract_multiplayer_stage_reset_slice.py`,
`scripts/extract_guard_bullet_hit_slice.py`,
`scripts/extract_bond_move_state_slice.py`, and `src/game/{bondview_r,bondview,bondview2,front}.c`.
Use the normal original-source include paths and flags:
`-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DBUGFIX_R0 -fms-extensions`.

`bash scripts/test_multiplayer_stage_reset.sh` runs ASan/UBSan fixtures for
two- and four-player rosters. Negative controls first demonstrate the stale
camera, force-disarm, invincibility damage and pickup gates, then execute the
production reset and the same original functions. Coverage includes original
ray/BBOX intersection through player damage, collision/view/portal
publication, begin-life timer reads, the original force-disarm branch, and
the original `propsTickPlayer` loop. Every retained scalar, pointer and tank
field is tainted and checked. The script also checks actual main/spawn reset
and publication order. Flat STAN, model/inventory resources and pickup
handlers are explicit fixture boundaries; pickup inventory mechanics and a
complete rendered/networked match are not claimed. Generated reset and
wrapper objects also compile for ARM at `-O2 -Werror`.

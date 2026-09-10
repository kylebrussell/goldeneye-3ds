# Multiplayer stage-intro reset subset

Call `ge_original_multiplayer_stage_intro_shared_reset_exact()` once before
the first multiplayer spawn. It retains these assignments from
`bondview_r.c::bondviewLoadSetupIntroSection`, in their original relative order:

```c
g_bondviewForceDisarm = 0;
g_CameraMode = 0;
mission_timer = 0;
watch_time_0 = 0;
```

The original assigns these before processing intro records or selecting a
spawn. Per-player allocation and `bondviewPlayerBeginLife` do not reset them.
A previous solo death/end camera mode otherwise prevents the real
`bondviewUpdatePlayerCollisionPositionFields` from updating a bodied player's
view position. A pending solo force-disarm otherwise advances across the next
roster's `MoveBond` calls and unequips whichever player reaches its threshold.
The mission timer supplies begin-life `lifestarttime60`; watch time is also a
shared native scalar. These are not new game-state definitions or a full intro
reset.

The source audit of the rest of the pre-record block found:

| Original statements, in source order | Current ownership |
| --- | --- |
| `g_isBondKIA`, resolution/camera-buffer counters, credits state | Camera/credits bookkeeping is partly reset by mission-exit services; the complete original block is not retained here. |
| Forced movement offset and surrounding-explosion flag | Original intro assignments remain outside this subset. No new values are inferred. |
| Start pads, tank pointers/audio/angles, tank death flag | Multiplayer configuration and spawn publication reset/rebuild start pads. The original tank reset block is not retained by this adapter. |
| Timer-active flag, invincibility, camera/transition state | Mission-exit services reset timer-active and part of transition state. This adapter resets the proven native camera scalar; it does not claim the other fields are reset. |
| Intro camera pointers/count, mission/watch timers, animation/watch-transition state | This adapter resets the two timers. The remaining cinematic/animation assignments are outside its scope. |
| Starting weapons | Validated multiplayer intro publication rebuilds the shared `starting_weapon` pair before begin-life; the loadout slice also equips the selected player's corresponding items. |

The viewport owner separately advances `mission_timer` immediately before
`MoveBond` for canonical slot 0 and `watch_time_0` immediately afterward for
every viewport, using the original region-specific statements. A stage reset
does not substitute for that progression.

`bash scripts/test_multiplayer_stage_reset.sh` runs the original collision/view
publication and begin-life bodies for two- and four-player rosters. It also
retains the exact force-disarm branch to observe its unequip calls. Flat STAN
and inventory/model-loading boundaries are explicit fixture resources; the
fixture does not claim complete movement, rendering, or a world match.

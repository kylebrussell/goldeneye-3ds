# Single-player parity checkpoint — 2026-09-04

Follow-ups: [audio CPU optimization](Verification20260904AudioOptimization.md),
[world submission / actor publication](Verification20260904VisibleRuns.md), and
[Caverns frame budget and presentation pacing](Verification20260904CavernsBudget.md)
record subsequent performance passes; the Caverns checkpoint supersedes the
staged executable below.

The target is complete original single-player content, with 60 FPS prioritized
on New Nintendo 3DS / New 3DS XL and compatibility retained for original 3DS.
The existing frontend enables the faster CPU only after `APT_CheckNew3DS`.
Neither hardware family has been certified by this checkpoint.

## Fixes in this pass

- **Campaign objective messages:** the live call to
  `display_objective_status_text_on_status_change` was gated to Dam even when
  another stage's objective runtime was bound. Every bound campaign stage now
  reaches the shared status/HUD adapter. Objective criteria and difficulty
  rules are unchanged. The campaign integration check pins this dispatch;
  the existing sanitizer fixture checks completion/failure HUD transitions.
- **Menu overlays:** selection rectangles, sliders and the erase panel were
  submitted before the wallet model, allowing opaque paper to overwrite them.
  They now follow the model and precede text, matching the original frontend
  constructor order. Placeholder backgrounds retain their separate early pass.
- **Menu line initialization:** the live snapshot-to-renderer bridge filled
  only part of each stack-allocated line. Optional `tab`, color and value
  placement fields could retain previous stack bytes. The array is now cleared
  each frame. A live mode-menu comparison showed SELECT MISSION missing before
  initialization and visible with its selection rectangle afterward. This is
  evidence for that fix, not a pixel comparison of every original menu.
- **Probe music initialization:** diagnostic boots skipped the frontend but
  still deferred music initialization to it. Startup now makes one decision
  before audio setup and uses it for both music ownership and frontend dispatch.
  Input/visual probes therefore exercise the gameplay synthesizer.
- **Native music volume return:** `sub_GAME_7F0C0BF0` calls `get_mTrack2Vol`
  without a C return statement in the decompilation. The generated native slice
  now explicitly returns the value. The original source/N64 target is untouched.
  This is a documented native ABI correction, not token-identical C. Its source
  hash remains recorded; the other 140 extracted bodies/data retain their token
  checks. An optimized UBSan fixture checks all 32,768 valid volume settings.
- **Audio resampler cost:** unsigned pitch advances monotonically from the
  four history samples. The final persisted sample is therefore the largest
  input touched. One endpoint check replaces the previous walk of every tap,
  still before any output/history writes. Filtering, phase, sample counts and
  PCM arithmetic are unchanged.

## Verification and measurement limits

`make test-3ds-port` passes in `build/host-tests/parity-full-20260904.log`.
The final line-array initialization additionally passes the frontend source
integration check and ARM build in `parity-arm-20260904.log`. The frontend
behavior and rendering fixtures passed separately. Build outputs and private
ROM-derived resources remain outside source control.

The resampler test sweeps all 65,536 pitch values, continued/initial state,
fractional phases, byte rounding, zero counts, overlapping buffers and invalid
input boundaries. An independent per-tap walk predicts acceptance; rejected
commands must leave all DMEM/history bytes untouched. Both the pre-change
scalar implementation and candidate produced the same complete-output digest
`abfbe11a`, with 31,898 accepted and 33,638 rejected cases, under ASan/UBSan.
Existing independent PCM golden vectors also pass. Private comparison logs:
`parity-audio-{baseline,candidate}-20260904.log`.

The earlier `parity-audio-{dam,caverns}750.result` probes establish that music
synthesis runs after fixing diagnostic startup. They still have zero layer
volume, which exposed the separate missing-return bug. **Do not use them as
audible/full-gameplay audio certification.** Likewise, old traces with zero
`simulation_wait_audio_ticks` music calls omit this CPU workload entirely.

The intermediate Dam resampler comparison reduced measured music work from
1,039,462,852 to 810,780,271 system ticks, but encounters, health and movement
endpoints diverged as the original timing consumed different retrace deltas.
It is not a controlled whole-game speedup. Its muted-volume baseline is also
superseded by the final validation below.

## Final candidate

Executable SHA-256:
`babce586ae48dacd4505cff2096ddd96b21ba2b0e7ad560e6d8f3097a7dfc7a1`.
Private copy: `build/3ds-candidates/parity-babce586/goldeneye-3ds.3dsx`.
The asset pack is unchanged (`ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`).

These Azahar runs use the same 750-frame move/look/fire configuration and
have no concurrent builds. Each completes 750 original movement/actor ticks
and 750 music service calls. The main music layers now have volume 26,212
(authored track gain applied to the default setting), instead of zero. Actor
status remains healthy, scene installation succeeds, and no sound decode
failure is reported.

| Stage | Total measured work | Music CPU ticks | Post-warmup >16 ms | Post-warmup peak |
| --- | ---: | ---: | ---: | ---: |
| Dam | 10,271 ms | 811,727,375 | 147 / 630 | 28 ms |
| Caverns | 7,868 ms | 676,461,339 | 78 / 630 | 22 ms |

Evidence: `build/visual-probe/parity-final-{dam,caverns}750.result`.
The routes remain near their starting rooms; neither is a mission completion.
The counters measure instrumented emulator work, not physical hardware FPS.
The restored audio workload makes comparisons against old silent/no-synthesis
benchmarks misleading. Sustained 60 FPS is still unmet.

The final extended Dam route (`parity-final-dam-combat.result`) ends in Bond's
death at target 11/160: 5,084 presentations, 5,085 original movement/actor ticks
and music calls. It is a failed mission, not a crash or completed route.
There are 516 sound starts/decodes with zero decode failures, healthy actor
status, and zero door/guard/monitor overlay-refresh failures. The original
music state switches to death track 27 with nonzero volume. Of 4,964
post-warmup samples, 1,732 exceed 16 ms, 122 exceed 25 ms and two exceed 33 ms;
peak is 38 ms. A live capture during the run showed 52 FPS. This establishes
the remaining performance gap with the restored audio workload.

The final executable is installed in Azahar and copied to
`build/3ds-sd/3ds/goldeneye-3ds/`; `deploy_3ds.sh --skip-build` validates the
staged hashes. The prior staged executable is preserved in
`build/3ds-candidates/hardware-before-parity`. Temporary emulator frontend,
input and stage overrides were moved into `build/visual-probe/`, restoring
normal startup. Saves and DSP files were not manually changed. This is SD
staging, not physical hardware validation.

## Remaining parity requirements

Passing subsystem tests or a finite input trace does not establish mission
completion. The following still require evidence and, where failures are
found, fixes:

1. Complete all 20 solo missions at each applicable difficulty, including
   objective failure, gadgets, escort/hostage scripts, boss encounters, exits,
   death/retry, unlocking, save/reload, ending and bonus missions.
2. Compare every frontend/watch/result screen against the original at matching
   state: layout, fonts, colors, textures, animation, cursor and transitions.
3. Traverse full stages and verify doors, glass, depth/occlusion, actors,
   effects, sky/fog and streaming. The Caverns guards-through-closed-door depth defect is fixed;
   runtime door clipping now reaches the shared renderer, while residual
   edge/seam artifacts remain open. See
   [Door clipping verification](Verification20260904DoorClipping.md).
4. Profile sustained combat with audible original music and SFX, then reduce
   remaining actor/prop, scene-publication and world-submission spikes while
   retaining original simulation and content.
5. Measure on physical New 3DS XL and original 3DS separately. Emulator CPU
   work counters and a 60 FPS menu screenshot do not prove either hardware
   target or sustained 60 FPS across missions.

The authored visual tours in [StageVisualTours.md](StageVisualTours.md) and
the existing Dam input routes are useful coverage tools. Their `complete`
result means the probe finished; mission outcome must be checked separately.

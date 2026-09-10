# GoldenEye 007 on New Nintendo 3DS XL: completion audit

This is the current acceptance checklist, replacing the early bring-up status
in `Porting3DS.md`. The requested endpoint is original-game fidelity, sustained
60 fps, comfortable dual-stick controls, slider-controlled stereoscopic depth,
useful touchscreen features, and local wireless matches. **That endpoint has
not been reached.** A successful build or isolated emulator route cannot
establish it.

## Implemented in the September 9 pass

- Fixed the user-reported repeating audio screech: generated reverb filter
  coefficients now reach the audio interpreter in its required byte order.
  The previous producer generated a sustained approximately 5.5 kHz oscillation
  from one impulse; the corrected filter decays to silence. This is a native
  producer-boundary correction, with the N64 code path preserved. See
  [the audio reproduction and fix](Verification20260909ReverbByteOrder.md).
- Controls now support configurable deadzones, continuous analog response,
  sensitivity, inverted pitch, shoulder swapping, and D-pad fallback. The
  bottom screen has touch actions and settings, with captured gestures and
  physical/touch input edge merging, plus a live read-only objectives page.
  Changes persist at orderly mission/app
  cleanup. See [controls and touchscreen](ControlsAndTouchscreen.md).
- Stereo culling now covers the actual near-plane disparity, including
  independent first-person settings. Repeated identical stereo uniform
  uploads are skipped. See [stereo evidence](Verification20260909StereoBounds.md).
- An opt-in local wireless lobby provides UDS discovery, hosting, joining,
  peer readiness, timeouts, compatibility checks and disconnect cleanup.
  Hold SELECT when launching to enter it. This is a connection test; it
  deliberately cannot start a match. See [wireless](LocalWireless3DS.md).
- The frame analyzer can enforce a strict captured-route 60 Hz gate. It
  requires complete timing and presentation records, enough samples, no
  over-budget work, no repeated/skipped VBlanks, and no multi-retrace ticks.
  Failed gates return a failing process status. Frontend probes also fail
  when their requested gameplay segment fails, and restore existing diagnostic
  configurations and captures when they finish.

## September 10 campaign and controls evidence

- The [campaign acceptance matrix](CampaignAcceptance.md) records all twenty
  missions and four difficulties: 80 passing objective/result/save/restart
  fixtures and 703 criterion perturbations. This exposed and repaired invalid
  result difficulty/timer handling and the unlocked-007 save-time query.
  Complete player/AI routes remain unverified in each matrix cell.
- [Controls](ControlsAndTouchscreen.md) now reach the original manual-aim pan
  threshold and use the canonical previous-weapon chord. A third bottom-screen
  tab provides captured-release inventory selection through the
  [tested watch boundary](CanonicalInventory.md), preserving ammo and previous
  weapon bookkeeping and rejecting stale/blocked requests. The adapter also
  provides a held-input release gate for resume and mission entry.
- [Complete native music sequence validation](Verification20260910MusicSequences.md)
  now covers all 63 authored tracks, repeated output and layered transitions.
  It exposed native ADPCM address masking and undefined negative envelope
  scaling, now repaired. Offline corrected PCM has no clipping or sustained
  alternating stop tail in these cases; full-game speaker listening remains
  an explicit acceptance check.

## September 10 renderer follow-up

The renderer now has isolated per-eye portal interpretation, conservative room
union, request-protected streaming residency, renderer-only guard/prop shading
eligibility, and corrected world disparity for eye-coordinate guards. Authored
model environment-alpha expressions and strict matched-state capture comparison
tooling are also implemented. See [renderer verification](Verification20260910RendererEyes.md)
and [material verification](Verification20260910ModelEnvironmentAlpha.md).
Original N64 matched captures and physical stereo/GPU acceptance remain open.

## Required work, in dependency order

| Area | Concrete remaining work | Acceptance evidence |
| --- | --- | --- |
| Original fidelity | Matched N64 reference captures for campaign, menus, watch, results, effects, audio and transitions; resolve depth/decal tolerance and remaining material/coverage differences | Same assets, camera, game state and timing; explain unavoidable N64 RDP/PICA differences instead of calling approximations exact |
| Campaign completion | Exercise every mission at every difficulty, including objectives, failure, death, save/unlock, exit and restart; repair failures exposed by these runs | Recorded complete routes, objective/mission results and save roundtrips; room tours are insufficient |
| Stereo room visibility | Isolated renderer union and dependent residency/shading implemented; complete live doorway/streaming capture and failure/retry acceptance | Doorway-edge and streaming captures of both eyes; unchanged canonical gameplay/AI visibility; slider-zero equivalence |
| Sustained 60 fps | Profile CPU and GPU on physical New 3DS XL with audible music/SFX and maximum stereo; reduce demonstrated costs, including loading transitions and busy combat | At least 3,600 measured frames per representative route after declared warmup, cold/repeat runs, complete frame/presentation records; loading hitches reported separately |
| Control ergonomics | Test C-Stick range, sustained aiming, crouch, scoped zoom, remote-mine gestures, handedness, touch capture and saved settings on the device | Playable combat/stealth routes and observed touch accuracy, plus reboot/save checks |
| Touchscreen | Test readability and gesture comfort; validate the implemented canonical inventory selection through live gadget/model routes | Read-only information matches the original watch; touch actions preserve authored inventory/weapon semantics |
| Wireless game state | Replace single-player assumptions with canonical multiplayer player slots, per-player input/camera/weapon/body ownership, spawn/respawn, authored maps/rules and results | Two then four local simulated players complete matches without a network before transport is connected |
| Wireless synchronization | Choose and implement game-state authority, ordered tick inputs/events, RNG ownership, stable actor IDs, snapshots/state hashes, packet-loss recovery, join/leave and host-loss handling | Deterministic transport simulation with delay/drop/duplication/reordering, then two/four physical devices; player damage, pickups, kills and scores agree |
| Release | Package a reproducible candidate with asset conversion version, runtime settings and evidence; test sleep/resume, low memory, shutdown and malformed data | Rebuild identity, clean SD install, explicit tested-hardware and feature limitations |

The multiplayer boundary is particularly substantial: `main.c` configures one
player and `ge_original_player_body.c` explicitly reports multiplayer
unavailable. Sending packets or removing that check alone does not implement
the missing ownership and simulation. The lobby's compatibility tag and pack
lineage fingerprint are not exact executable/asset hashes and must not be
reused as proof of deterministic match compatibility.

## Reproducible checks

```sh
make test-3ds-port
make 3ds
python3 scripts/analyze_3ds_frame_timing.py capture.result \
  --warmup 120 --require-60hz --min-samples 3600
python3 scripts/analyze_3ds_frame_timing.py before.result after.result \
  --compare --warmup 120 --require-60hz --min-samples 3600
```

The comparison form additionally rejects mismatched recorded gameplay and
asset/input provenance. Passing either command applies only to the measured
route and environment. An Azahar capture remains emulator evidence even if
all timing gates pass. New 3DS XL physical frame pacing, radio behavior and
stereo comfort require physical consoles.

Private ROM-derived assets, executables and capture outputs stay outside
source control. No claim of perfect fidelity, complete campaign parity, locked
hardware 60 fps or working wireless matches is supported by this pass.

# Campaign preflight — 11 September 2026

The preflight found and corrected two shared setup defects before further
physical testing. The new build retains the earlier aiming, primary button
history and switch-panel fixes.

## Production changes

- Register all 27 authored padlock-to-door relations and bind the original lock
  health check. Doors now respect surviving padlocks; destroying every lock on
  a door releases it. These relations occur in Dam, Surface 1, Surface 2 and
  Train. Stage cleanup releases the links before their endpoints.
- Restore eight skipped backward links that close nonreciprocal door groups:
  Bunker 1 (1), Surface 2 (1), Bunker 2 (1), Aztec (2), Egyptian (3). Reciprocal
  two-door pairs still publish once. Tests exercise native group activation and
  displacement, not just pointer equality.

## Fresh preflight coverage

All 15 checks passed against the exact installed asset pack:
`5fc14fae6e53a975c0b09162ee2a52e2355828b47ba58a4cbf807ce73df0221c`.
The runner confirms source inputs and the supplied pack did not change during
execution. Evidence: `build/host-tests/campaign-preflight-20260911/final/`.

| Area | Coverage and limit |
| --- | --- |
| Setup relations | 20 missions plus credits: 10 switches, 166 door links, 27 padlock relations and 6 safe links. Native dispatch, door travel, lock destruction and safe pickup behavior; missing endpoints, cleanup and rebind. |
| Setup references | 398 tags, 111 objective references, 92 patrol paths and 17 vehicle records. Structural checks; vehicle navigation and bullet damage routing are not simulated. |
| Objectives/results | 20 missions × 4 difficulties: 80 fixtures and 703 criterion perturbations per enum layout, including result/restart/save behavior. No complete mission playthrough claimed. |
| Room publication | 21 stages, 1,214 rooms, 1,173 evictions and 6,007 canonical publication checks per enum layout, including retained-room rebasing and overlay changes. Not camera-route or hardware pixel acceptance. |
| Lifecycle/input | Fresh APT resume, watch abort/pause/death/quit/results, player rebind, prop retirement, held-button history, controls and sight alignment fixtures. |
| Rendering contracts | Material/submission reuse and same-image sampler publication. Actual hardware depth behavior remains open. |

Campaign setup, objective and room-publication tests run native and short enums
with ASan/UBSan. The short-enum test is useful target coverage but does not turn a
64-bit host process into the 32-bit 3DS runtime. The forced ARM build is separate.

Additional agent runs against that same pack passed 183,991 original triangle-hit
checks per enum layout, 416 Dam stereo viewpoints, and 6,995 visibility/publication
checks. See `build/host-tests/campaign-render-20260911/report.json`.

The movement smoke harness was repaired to compile current input-provider, HUD
and explosion owners. It passes its Dam movement/STAN/camera fixture, but still
reuses other aggregate objects. It is excluded from the fresh preflight and is
not evidence of a fresh full-runtime campaign simulation.

## Repeat the checks

```sh
python3 scripts/test_campaign_preflight.py \
  --pack /absolute/path/to/goldeneye.u.gepack \
  --output build/host-tests/campaign-preflight-next \
  --jobs 3
```

Use the pack actually installed on the console. The old staged pack under
`build/3ds-sd` is smaller and predates later character and multiplayer additions;
its hash differs. No asset pack was regenerated or replaced by this audit.

## Remaining physical checks

The missing-wall defect still matches a depth-disabled replay; its hardware cause
is not yet established. SELECT retains the completed color, depth, vertex and
material diagnostics in the new build. Retest Dam aiming, panel activation and
padlocks, then linked doors and watch/restart behavior in subsequent missions.
Actual pixels, collision progression, audio, stereo comfort and sustained frame
rate require hardware validation. These host results do not establish perfect
fidelity or 60 fps throughout the campaign.

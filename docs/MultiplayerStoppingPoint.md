# Multiplayer stopping point — 2026-09-10

Work stopped at the user's request after usage limits were reached. No further
features, investigations or broad reruns are authorized by the earlier work
plan. All three subagents were notified. No multiplayer compiler or replay
process remained running at handoff. The checkout was clean at the tested
source commit `a7c59ff5c036069f09b1faff581f39b00b3f33dd`; this document is the
only subsequent change.

Branch: `codex/multiplayer-simulation-uds`.
Worktree: `/Users/kyle/.codex/worktrees/ddeb/goldeneye-3ds`.

## Ready commits and dependencies

Integration already imported the preceding watch presentation, EC main binding,
BG-reader and callback repairs. The latest coherent sequence is:

| Commit | Scope | Dependency/status |
| --- | --- | --- |
| `e2799c1a` | ED physical Model header/node topology and borrowed getters | Integrated; malformed-owner finding below remains open |
| `93095089` | Native seven-page setup input and copied launch application; kind0 v4 controls | Integrated; visible setup/main integration incomplete |
| `16cb4ac6` | EF native music transition/table state, stage-slot reset, death-music recovery; ED/setup build plumbing | Integrated; requires the ED/setup cohort |
| `0ad30683` | Exact native stage player-pointer/shuffle reset | Integrated; preserves caller context on roster destruction |
| `fde2ed19` | Actual shuffle storage in focused fixture owners | Integrated; accompanies `0ad30683` |
| `774028e8` | Native countdown after background AI and regeneration, before shuffle; explicit title rejection | Integrated; uses `get_mission_state` retained by `16cb4ac6` |
| `a7c59ff5` | Frozen raw controller tapes, corrected bootstrap, dependency provenance, independent peer/enum/restart driver | Ready to import after the preceding sequence |

Integration reported its combined HEAD as
`4c57e81defd98ed36b824d4984d8c93048f03c5c`, including through `774028e8`.
There are no uncommitted or WIP implementation files to preserve. Planned
renderer, owner and reference work below was not started.

## Existing validation

The final replay receipts completed at the exact clean `a7c59ff5` source in
both native and short enum layouts, each with 370 production compilation units.
The recorded fixture and native archive hashes were checked against the files
at handoff. No replay was restarted for this stopping point.

Receipts live under
`/Users/kyle/.codex/worktrees/ddeb/goldeneye-3ds/build/host-tests/multiplayer-match-controls/`:

- `controls-identity.json`: SHA256 `96f557e228d6acc3c19ad35ee2b7407b3cd096176154180a1219a6b21a6c8fa3`.
- `comparisons.json`: SHA256 `4f5b4ab21fb1a7ea1cf907960efff33b3e916eaf0d311063072c35bd05bd15e7`.
- Separate native/short, two/four-player and 2→4→2 peer JSONL witnesses,
  invocation records and transcripts. All six independent-peer comparisons
  and all three cross-enum comparisons passed.
- Every cold-versus-chain source-history audit reports zero unexplained
  differences. Raw program player-tick history, the first warm two-player body
  animation publication and monotonic allocation/lifetime history remain
  explicitly different; they are not normalized into state equality.

Both tapes are committed under `port/tests/fixtures/multiplayer-controls/`.
The corrected Caves runs pick up the PP7 at tick 84, first damage at 592,
death/score at 754, respawn at 1072 (two players) or 1154 (four), timed results
at 1799 and all-player FINISHED→TITLE at 1808. These are bounded CPU witnesses,
not a complete canonical-state digest or hardware acceptance.

Actual forced devkitARM ELF and 3DSX rebuild passed after the production
corrections in `774028e8`: `/tmp/ge-mp-music-countdown-arm.log`. The later
`a7c59ff5` changes are host fixture/tape/provenance/documentation changes.

Existing focused evidence also passed: EF actual native transition and source
table captures for independent two/four-player peers in both enum layouts;
session death/blood predicates, watch transitions, 741-identity main lifecycle;
native player reset and removed-reset negative control; setup, inventory,
player-state and all 11 authored arena initial-spawn fixtures. Countdown tests
execute actual shared begin-world, native pre-tick and music bodies in both
enum layouts, including same-pass AI, pending, clock zero, delta zero, expiry,
failure ordering and title exclusion. Missing/before-AI/before-regeneration
countdown mutants are rejected. Relevant logs are
`/tmp/ge-mp-music-countdown-{world,exact,state}.log` and
`/tmp/ge-mp-music-{state,session,watch,main-lifecycle}.log`.

## Explicit unresolved work

- ED common shape validation omits the unique-native-address invariant.
  Integration reproduced a malformed descriptor array publishing one physical
  node under two IDs and allowing teardown to mutate ownership. Begin/add do
  enforce uniqueness. No repair was started before the stop request.
- EC has the analogous missing common-shape uniqueness check on read-only
  inspection; a separate reproduction/repair remains necessary. Prior valid
  state evidence does not close this malformed-owner boundary.
- ED main collection/lifetime binding, EE actual RW allocation/free state,
  mutable model RO data, matrices and animation caches remain incomplete.
  F0 was reserved for native model distance, merging and joint-callback controls;
  no F0 module was written. Kind0's direct EF reference is still pending.
- Native setup input and copied controls are implemented, but the visible
  source setup renderer, wallet/portrait/stage texture bindings and replacement
  of main's diagnostic row editor are incomplete. Remote participants need
  actual per-seat setup input; no automatic readiness is supplied.
- Campaign owns the separate solo countdown hook after the agreed common-tail
  correction. The multiplayer commit does not implement that solo main hook.
- Complete aggregate state coverage, actual UDS lobby/match integration,
  remaining native alarm/SFX prefix, broader arenas/scenarios and hardware
  GPU/audio/radio/timing acceptance remain open. The protocol must not be
  certified with an incomplete aggregate digest.

The earlier sealed integration package and its receipts remain unchanged.
Independent EF review is unfinished: fidelity preserved exact `16cb4ac6`
source at `build/host-tests/music-state-review/snapshot-16cb4ac6` in its
`5c80` worktree, with status in `REVIEW-PAUSED.md`. Only source/diff/test-code
inspection ran; no independent EF runtime or ARM check completed, and the
later countdown is excluded. The ED review receipt belongs to integration.
This handoff does not assert completion of multiplayer.

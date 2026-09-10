# Integration stopping point — 2026-09-10

Work stopped at the user's request after usage limits were reached. Earlier
plans do not authorize further feature work or investigations. Ready work is
committed; unfinished work and evidence limits are recorded below. No push,
deployment, parent-checkout edit or new emulator run occurred during this closure.

Branch: `codex/integration-acceptance-20260910`.
Worktree: `/Users/kyle/.codex/worktrees/1134/goldeneye-3ds`.
Final imported checkpoint before this handoff-only commit:
`3d0484fd` (full identity is its Git commit). The final branch HEAD is the commit
containing this file; the coordinator sends that full hash separately so this
document does not contain a self-referential commit hash. The production runtime
last changed at `4c57e81d`; `036b481a` adds host fixtures/tapes/provenance and
`3d0484fd` imports the multiplayer stopping-point document.

## Included work

All previous integrated campaign, renderer, asset, audio, input, multiplayer and
performance work remains in ancestry. Do not reapply the peer branches wholesale.
[Integration acceptance](docs/IntegrationAcceptance20260910.md) records the earlier
checkpoints. The final additions after frozen `d7e6b7d5` are:

| Integration commit | Upstream commit | Scope |
| --- | --- | --- |
| `d141790b` | coordinator | Bounded d7 host/ARM closure and music countdown source correction |
| `648be97f` | `e2799c1a` | ED physical Model header/node topology; open malformed-owner finding |
| `c607e7a3` | `93095089` | Native setup input, copied launch settings and kind0 v4 controls |
| `d64b6e38` | `16cb4ac6` | EF music fields/tables, stage-slot reset and death-music recovery |
| `3b8d905c` | coordinator | One ED full-pack hook and one setup hook in the aggregate |
| `e6b23d66` | `0ad30683` | Native stage player/shuffle reset and caller-context restoration |
| `8f80c01f` | `fde2ed19` | Real shuffle storage in focused fixture owners |
| `4c57e81d` | `774028e8` | MP countdown after pre-tick and regeneration, before shuffle; TITLE guard |
| `036b481a` | `a7c59ff5` | Corrected host bootstrap, frozen raw controller tapes and peer/restart audit |
| `3d0484fd` | `850187a7` | Multiplayer owner's final ready/open handoff |

Merge resolution preserved the combined linker retention list and existing
aggregate layout, without duplicate transport or exporter test groups. Existing
main input, stereo, renderer readiness and cleanup changes were retained.

## Validation and immutable evidence

All paths in this section are relative to this worktree. Private inputs and
evidence remain ignored files; Git commits alone do not contain them.

- **Latest common full host and forced ARM checkpoint: `d7e6b7d5`.**
  `build/integration/aggregate-host-evidence-d7e6b7d5/` records all 9,181 copied
  inputs, a fresh 366-unit native graph, 1,670 dependency identities and 52
  generated sources. Closure SHA256:
  `2135553bc3554c0ca6dae889470e745e82e7416c0e63c976df9370e599720f29`.
  The coordinator reverified all 18 closure and 431 native execution artifact
  hashes. `candidate-build-d7e6b7d5/` retains the forced ARM receipt, ELF,
  generated sources and 63-symbol audit. Its 3DSX SHA256 is
  `3836b23363415a0ed1bbe1528c78ededd28f1acaf2e6cb698ae6954ce4667f87`.
  This frozen fixture omitted main's music initializer/fade service and used
  different startup ordering; its Caves results do not establish exact main
  initialization or RNG-history agreement. Later corrections are excluded.
- `build/integration/music-countdown-scope-correction-d7e6b7d5.json` corrects
  the prior solo-only statement while preserving affected sealed files. Native
  `lv.c:1354` selects TITLE; the common non-title tail at 1375–1377 calls
  `chrpropTick`, countdown, then `langTick`. Solo and MP need their separate
  actual-world hooks. Correction SHA256:
  `31e7e42b55a7bb01488b4fd22183fe7235dfdbb49b809d1b43de3ba5e1376423`.
- **Merged focused checkpoint `3b8d905c`:**
  `build/integration/mp-ed-setup-music-3b8d905c/receipt.json` records passing
  setup, EF, session, main lifetime, kind0 and ED fixtures. Independent setup
  checks cover both enum ABIs, 2/3/4 participants, every local slot and 468
  malformed copied-launch cases with unchanged player/permanent/control/RNG
  state. Receipt SHA256:
  `1707fee7a2822456cd0284e571e0bc2e5b475e7a085e180e2547bdcc6bff8012`.
- **Merged focused checkpoint `4c57e81d`:**
  `build/integration/mp-countdown-reset-4c57e81d/receipt.json` preserves complete
  passing runtime/countdown, player reset, setup, inventory and kind0 transcripts.
  Both countdown ABIs reject omitted/before-AI/before-regeneration mutations.
  Original PTY handles became unavailable before final polling; final inventory
  and kind0 exit codes were not recovered, and no matching processes remained.
  This limitation is explicit. The native exactness command first missed its
  required repository argument; that failed invocation is retained. The corrected
  invocation exited 0. No broad rerun was made. Receipt SHA256:
  `0d9c52e4aee75ad8f50dba8a48c02360bba0b5cc998b21547431ce7c29d55cda`.
- **Upstream corrected replay checkpoint `a7c59ff5`:**
  `build/integration/mp-controls-replay-a7c59ff5/` copies and verifies 44 existing
  evidence files (87,736,177 bytes). Six independent-peer and three cross-enum
  comparisons pass using native/short 370-unit graphs. Raw cold/restart state
  differs: player-tick history, first warm body-animation publication and
  monotonic lifetimes remain visible and explicitly classified. This is bounded
  CPU evidence, not complete state equality. Copy receipt SHA256:
  `c328a9a1ab5fb731284a23b54ec0d52ccf16a2b184ca7596f09219dda61e9c5e`.
  No replay or full suite ran on the final merged source after this import.
  See [the multiplayer handoff](docs/MultiplayerStoppingPoint.md) for upstream
  build/fixture hashes and forced ARM scope at `774028e8`.
- **Latest packaged checkpoint remains `3760b2dd`:**
  `build/integration/candidate-3760b2dd/` has 672 inventoried files and 286 scoped
  references. Manifest SHA256:
  `d933211141bcadb35d4e9c4a37b69d14b787471ca8c125a06465c5346344a7c2`.
  It verifies normally and fails the completeness gate; all nine areas are
  partial. It contains earlier known limitations, compiled defaults and explicit
  external DSP prerequisites. The older `candidate-31fce6e5/` remains unchanged.
  No new package was made. `acceptance-next-d7e6b7d5-draft.json` is only a draft
  and does not describe final-HEAD acceptance.

## Excluded WIP and open findings

- **Solo countdown WIP, not imported:**
  `4360d85ce2a7f1b839c4daac9cf7ca54d069c1db` on
  `codex/solo-music-slot-tick`, worktree
  `/Users/kyle/.codex/worktrees/1ce8/goldeneye-3ds`.
  `docs/WIPSoloMusicCountdown.md` on that branch records its main hook and
  uncompiled/unwired C regression draft. It needs base `3b8d905c` plus released
  `774028e8` (already integrated here as `4c57e81d`). Do not also import its
  duplicate helper cherry-pick `6a0c7e9d`. No solo runtime/ARM acceptance exists
  for this WIP; only its source-order check and whitespace check passed.
- **ED duplicate-native-address finding remains unfixed.** Independent report:
  `build/integration/mp-header-topology-independent-review-e2799c1a/review.md`.
  In both ABIs a malformed owner can publish one physical node under two IDs;
  teardown accepts and changes it. Valid begin/add do not construct that state.
  Review receipt SHA256:
  `a15d9d51d894961fc5b2027be48d5684312ce2bb79d96a3ba9c08710f70903b6`.
  MP reports an analogous EC omission from source inspection only; its separate
  reproduction and repair were not started. Earlier valid-owner EC evidence
  does not establish this malformed-owner boundary.
- **Independent EF review is unfinished.** Source inspection only; no actionable
  defect or independent runtime/ARM closure. Copied status:
  `build/integration/ef-independent-review-paused-16cb4ac6/REVIEW-PAUSED.md`.
  Exact snapshot remains in the fidelity worktree at
  `/Users/kyle/.codex/worktrees/5c80/goldeneye-3ds/build/host-tests/music-state-review/snapshot-16cb4ac6`.
- Independent awards numerical repair review remains incomplete after an
  automatic reviewer flag. Neither that review nor the other flagged review
  was retried, reassigned or treated as passing.
- ED main binding, EE RW/RO/matrix/cache ownership, reserved F0 model controls,
  kind0 EF reference, visible native setup and its resource/main integration,
  actual UDS lobby/match and complete aggregate coverage remain incomplete.
  No additional renderer/EE/F0 WIP was begun by the MP owner.
- Campaign completion/reload, matched reference visuals, heard audio, physical
  New 3DS XL controls/stereo/radio and sustained 60fps remain unaccepted.
  The Mac-locked emulator route was not retried or bypassed. Existing A/B tools
  and asset receipts are integrated; final-source measurements remain pending.

## Coordination and resumption

All peer roots received the stop instruction and notified their subagents.
Integration's remaining ED reviewer was told to seal existing findings and idle;
the two previously flagged reviewers were not restarted. No test, compiler,
replay or emulator process is intentionally left running by this coordinator.
The parent task owns bringing this committed branch into its checkout while
preserving the parent's existing local work. No further feature follow-up is
scheduled. Resume only on a new user instruction, starting with this handoff,
the exact evidence scopes and the open findings above.

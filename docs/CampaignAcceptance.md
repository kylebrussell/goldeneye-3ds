# Campaign acceptance — 2026-09-10

**Full campaign completion remains unverified.** This matrix distinguishes
executable boundary fixtures from player routes. `F(n)` means the lifecycle
fixture passes with `n` enabled authored objectives; it does not mean the
mission was played or completed. Every row still needs a recorded route.

| Mission | Agent | Secret Agent | 00 Agent | 007 custom | Complete player route |
| --- | --- | --- | --- | --- | --- |
| Dam | F(1) | F(2) | F(4) | F(4) | Unverified at every difficulty |
| Facility | F(4) | F(5) | F(5) | F(5) | Unverified at every difficulty |
| Runway | F(2) | F(3) | F(4) | F(4) | Unverified at every difficulty |
| Surface 1 | F(2) | F(4) | F(4) | F(4) | Unverified at every difficulty |
| Bunker 1 | F(2) | F(3) | F(5) | F(5) | Unverified at every difficulty |
| Silo | F(2) | F(4) | F(5) | F(5) | Unverified at every difficulty |
| Frigate | F(2) | F(4) | F(4) | F(4) | Unverified at every difficulty |
| Surface 2 | F(2) | F(3) | F(4) | F(4) | Unverified at every difficulty |
| Bunker 2 | F(2) | F(4) | F(5) | F(5) | Unverified at every difficulty |
| Statue | F(5) | F(5) | F(5) | F(5) | Unverified at every difficulty |
| Archives | F(3) | F(4) | F(4) | F(4) | Unverified at every difficulty |
| Streets | F(2) | F(3) | F(3) | F(3) | Unverified at every difficulty |
| Depot | F(1) | F(4) | F(5) | F(5) | Unverified at every difficulty |
| Train | F(3) | F(4) | F(5) | F(5) | Unverified at every difficulty |
| Jungle | F(3) | F(4) | F(4) | F(4) | Unverified at every difficulty |
| Control | F(3) | F(3) | F(3) | F(3) | Unverified at every difficulty |
| Caverns | F(1) | F(4) | F(5) | F(5) | Unverified at every difficulty |
| Cradle | F(2) | F(2) | F(2) | F(2) | Unverified at every difficulty |
| Aztec | F(2) | F(2) | F(2) | F(2) | Unverified at every difficulty |
| Egyptian | F(2) | F(2) | F(2) | F(2) | Unverified at every difficulty |

The 007 column tests the authored 00 Agent objective set and the canonical
no-completion-save rule. It does not cover every possible custom AI setting.
Aztec Agent and Egyptian Agent/Secret Agent fixtures intentionally evaluate
the setup and result boundary even though a fresh legitimate progression
cannot select those bonus mission/difficulty pairs. Their fixtures cannot
establish frontend accessibility. The separate unlock ladder tests require
Secret Agent completion through Cradle for Aztec and 00 Agent completion
through Aztec for Egyptian, then all twenty 00 Agent missions for 007.
Cuba is a credits stage, not a selectable campaign mission, and is excluded
from the 80 combinations. Its terminal AI remains in the existing 21-stage
source contract audit.

## Executable evidence

```sh
# Supply a generated stage pack. It is read-only; no Azahar is launched.
bash scripts/test_campaign_acceptance.sh /path/to/stages.gepack > campaign-fixtures.tsv
bash scripts/test_dam_mission_exit.sh
```

`test_stage_assets.sh` also runs the campaign fixture once its local pack is
ready, recording `campaign-acceptance.tsv` in its host-test output directory.
The checked-in [80-row result](generated/campaign_acceptance_fixtures.tsv)
contains test summaries only. No private asset pack or game capture is committed.

This recorded run used Apple Clang 21.0.0 on arm64 macOS with AddressSanitizer
and UndefinedBehaviorSanitizer. The copied input pack's SHA-256 was
`fb704e861ced74cfa9e8476622d253c343055deed7ffb45fe7d7316880b87052`.
It came from the preexisting generated host-test stage pack; the canonical
setup loader also checks each descriptor's expected size and FNV hash.

The fixture loads each actual serialized setup through
`ge_original_stage_setup_load`, constructs its objective/tag registry, and runs:

1. The original difficulty-filtered initial result, with real authored text
   IDs and objective rows; the mission is initially incomplete.
2. Wrong-room, wrong-item, and unsuccessful-photo events, followed by
   provider-driven completion using authored flags, tags and event criteria.
3. A separate perturbation of every enabled non-null criterion: missing
   flags/events/items remain incomplete; fail flags or destroyed required
   pickups/photos report failure. All **703 perturbations** pass.
4. The shared `AI_EndLevel` service, actual live `objectiveIsAllComplete`,
   original result body and durable save provider. An incomplete exit requests
   the title without a save; framebuffer draining delays the successful exit;
   the complete exit writes the selected mission and difficulty. This starts
   at the terminal service, not at the authored AI conditions that reach it.
5. The exact frontend result decision for successful, aborted, and dead
   snapshots. KIA wins over abort; abort wins over otherwise complete
   objectives. These test the result boundary, not combat/death animation or
   watch-input routing.
6. Time truncation, all twenty cheat targets, exact-threshold unlocks, slower
   repeat preservation, 007 save suppression, and CRC-validated file reload
   after each fixture. The asset-free save test separately exercises the
   bonus-mission/007 unlock ladder and reloads the final slot.
7. Objective registry teardown, fresh setup loading and restart: the entire
   objective result snapshot returns to its initial state without stale tags,
   events, inventory or flags. This does not test the complete in-game
   restart/menu transition.

Props, inventory membership, stage flags, key copying, photo projection and
STAN-room ownership are controlled fixture providers. They are not generated
by movement/combat, pickup code, actor AI or room streaming. Fixture room 7
is deliberately synthetic. The test never writes a result status directly
to pretend an objective completed: it changes the criterion's provider inputs
or invokes its enter/deposit/photo event service. Negative event-state tests
also clear a criterion sidecar to model an event that has not occurred.

## Remaining acceptance for each matrix cell

A complete route must begin at the original frontend with the selected save
folder/mission/difficulty and retain input, build and asset provenance. It must
reach objectives through canonical gameplay and authored AI, record ordered
objective transitions, enter the authored exit sequence, render the original
result, reload the persisted slot and verify the corresponding unlock/best
time. A second run must verify restart, a failed objective, abort and death
without gaining completion. A room tour or injected objective flag does not
satisfy these requirements.

No new complete gameplay route was recorded in this workstream. The preexisting
Dam route generator/verifier and Facility AI flow harness remain useful narrow
checks; they do not establish the other stages or every Dam/Facility difficulty.
Direct controller diagnostics default to the existing canonical difficulty
(Agent on a fresh launch). In an isolated probe SD directory,
`probe-difficulty.cfg` can explicitly select `0` Agent, `1` Secret Agent,
`2` 00 Agent or `3` 007. Only one digit plus optional surrounding ASCII
whitespace is accepted. A valid controller probe must also be present; a
difficulty file alone never bypasses or changes the normal frontend. Visual
tours take priority over controller probes and ignore this override. Invalid
difficulty input stops the controller diagnostic before setup. The original
`lvlSetSelectedDifficulty` body applies a valid override before stage setup,
so authored difficulty filters and AI initialization see the same selection.
Results record `setup_difficulty`, final `difficulty`, and
`probe_difficulty_override`. The all-objective Dam verifier now requires both
difficulty values to be `2`; older results without provenance cannot certify
that route. The integration coordinator must stage the explicit difficulty
file with its private route inputs and remove it after the run. Mode 007 uses
the existing custom slider values; selecting it does not certify any specific
slider configuration.

The unresolved routes must exercise live NPC timing and survival, object
interaction/destruction, pickups and gadgets, moving vehicles, special room
triggers, and their actual terminal AI branches. The fixture only proves that
those producers have a consistent tested objective/result/save consumer.

Hardware controls, radio multiplayer, audible full-game audio, stereo fidelity
and sustained physical frame timing are separate acceptance requirements.
See [the completion roadmap](CompletionRoadmap.md).

## Repairs exposed by this pass

- The native mission-result adapter accepted difficulty `-1` and indexed a
  three-column target-time table out of bounds. The new regression reproduced
  this under UBSan. Out-of-range difficulty and negative mission ticks now
  fail before any table access, result counter or persistence mutation.
  Valid Agent/Secret/00/007 behavior retains the original result body.
- After all twenty 00 Agent save entries were complete, the durable provider
  reported 007 unlocked but its time query returned zero because the private
  original file slice had no folder binding for that read. The wrapper now
  returns the canonical `0x3ff` availability sentinel for unlocked 007 and
  zero for locked or invalid mission queries. The all-stage fixture failed
  before this repair; both the all-stage and asset-free save tests pass after.

The asset registry orders Cradle before Caverns, while the canonical mission
sequence orders Caverns before Cradle. The fixture uses an explicit stage map;
casting an asset index to a mission index would test the wrong save entry.
The live frontend already publishes its canonical mission selection, so this
was a fixture-design constraint, not a demonstrated live-stage regression.

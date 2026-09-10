# Native multiplayer acceptance gates

`test_ge_original_multiplayer_match.c` runs the production shared multiplayer
runner against canonical arena assets. It does not define replacement movement,
prop ticks, gun producers, held-item dispatch, damage, death, respawn or session
rules. `build_multiplayer_match_host.py` regenerates slices from the current 3DS
Makefile recipes and compiles the native production graph with ASan/UBSan. Its
compile ledger records source recipes, compiler arguments and dependencies. The
runner script audits strong linked gameplay entrypoints before execution.

## Reproduce

The existing staged asset packs and animation files must be present; the script
does not modify ROM assets, shared staged packs, SD cards or emulator state.

```sh
bash scripts/test_multiplayer_match_bootstrap.sh
```

This runs two complete shared frames for both two and four players in Temple.
Temple's authored ordinary objects are deliberately outside this bootstrap gate.

To add the populated Caves gate, pass the supplies pack as the fifth argument:

```sh
bash scripts/test_multiplayer_match_bootstrap.sh \
  build/host-tests/multiplayer-assets/multiplayer.gepack \
  build/host-tests/multiplayer-bodies/multiplayer.gepack \
  build/multiplayer-port-tests/original-dam-setup/first-person-pp7.gepack \
  build/3ds-animations/bond \
  build/host-tests/multiplayer-supplies/supplies.gepack
```

The resulting `build/host-tests/multiplayer-match/bootstrap-transcript.txt` and
`bootstrap-link-symbols.txt` belong to that build. They are evidence artifacts,
not source fixtures to copy into another build.

## Populated Caves gate

The original Normal scenario and Pistols weapon set produce 26 live Caves
supplies and one canonical omission. The fixture rejects every missing model,
missing service, unsupported branch or failed constructor. It uses the existing
setup selection/materialization, ordinary item and supply constructors, native
600-slot prop pool, room publication, activation and enabling. The resource
callbacks allocate owned model/collision storage and query actual STAN/BG.

Both rosters run 480 shared frames. A fixed original Honey controller stream
walks player zero from the canonical spawn to the authored PP7, taps inventory,
and pulses the fire trigger. The assertions require:

- The real weapon pickup disables its prop and begins a decreasing native
  regeneration timer.
- The selected hand equips the PP7, fires ten shots, reloads once, consumes its
  seven-round magazine and three-round reserve, and returns to fists.
- Ten native background BulletImpact records exist in valid authored rooms.
  The retained impact accessor borrows the bound BG owner's exact origins.
- Native first-person model, RW storage and float matrix publications are
  present when visible; both hand publication slots are read after gun update.
- Every player runs all shared frames with actual canonical body/animation
  ownership, and stage resources and the prop pool close after their consumers.

The callbacks provide asset ownership, native selection, camera/visibility and
read-only local presentation capture. No fake gameplay producer is linked.
GPU command submission, platform audio output, display residency and hardware
execution remain resource/platform boundaries. This gate does not establish
player-hit, death, respawn, score/results, dual-wield rendering, complete state
hash agreement, radio behavior or performance. Those require further gates;
retaining their entrypoints in the link is not execution evidence.

## Controller-driven match gate

Append `--combat` to the populated Caves command above. This writes
`combat-transcript.txt` and uses the same freshly built production archive and
strong-entrypoint audit. No instrumented or replacement gameplay functions are
linked. Both two- and four-player rosters run the production shared runner until
all players request the title screen through their original results menus.

Player zero collects and equips the authored PP7. A test controller computes a
route for player one from the loaded arena's actual STAN adjacency, then emits
ordinary Honey stick/buttons. Original MoveBond owns every position, floor and
collision result. Player zero aims using its published camera and ordinary pad
input; no position, health, inventory, ammo, hand state, score or death flag is
assigned by the fixture.

The frozen cold-process Caves tapes, including the corrected native stage music
initializer, produce this transcript (ticks are zero-based):

| Transition | Two players | Four players |
| --- | ---: | ---: |
| Authored PP7 pickup | 84 | 84 |
| First native player damage | 592 | 592 |
| Victim death and killer score 1 | 754 | 754 |
| Controller-triggered native respawn | 1072 | 1154 |
| Original timed results/awards | 1799 | 1799 |
| All menus FINISHED, original TITLE request | 1808 | 1808 |

Both victims die from the PP7 after eight shots, including a native reload.
The two-player victim reaches `bonddead=2` during the death frame; the
four-player victim reaches it on tick 755, following its next canonical pass. The test
requires exactly one observed death, one respawn with full health, one recorded
kill, timed results with readable native award text ownership, and the original
all-player menu completion condition. It verifies reuse of the same native
player, prop and inventory owners across respawn. A neutral interval followed by
a Start edge on every pad closes the original results menus.

Every canonical viewport also compares the actual retained room visibility and
room-bbox readers with the current native publication. These checks exposed two
host layout errors: the fixed room and draw-room tables were read through
pointer-expanded declarations. Both unchanged source readers now live beside
their publisher. The match additionally executes the typed host death-animation
offset conversion; source animation choice and RNG calls remain unchanged.

This gate covers CPU gameplay through the title request. GPU submission, audio
hardware, dual-wield presentation, complete exported-state agreement, networking,
radio behavior and performance remain separate acceptance work. The routing and
aiming code is test input generation; it does not replace a game subsystem.

## Frozen controller and restart controls

The raw-pad replay driver is:

```sh
python3 scripts/test_multiplayer_match_replay.py
```

It builds both native enum widths with explicit compiler flags, then runs two
independent processes for each cold roster and each complete 2/4/2 stage chain.
The chain closes actual consumers, destroys the native roster, retires the prop
pool and reconstructs stage resources between matches. Every reconstruction
uses the same two RNG seeds and the same roster-specific input tape. During
replay the adaptive route/aim generator is bypassed; only the tape publishes
pads. `--record --abis native` is an explicit tape-authoring operation, separate
from replay acceptance.

The bootstrap retains the production stage music boundary after initial spawn
and ordinary supply construction, before player body/life initialization:
`ge_port_start_stage`, the exact music-slot reset, then `sub_GAME_7F0C11FC`.
The latter performs its original random track selection. Every frame retains
music fade service after the shared pre-world/shuffle boundary. Audio device
and sequence output remain platform/resource boundaries.

`controls-identity.json` records source and generated-source hashes, exact
compiler commands, compiler binary/version/target, compiler header dependencies,
extractor recipes and local Python imports, archive/fixture identities, asset
hashes and raw-tape hashes. The builder rejects a dependency or recipe that
changes during compilation. The driver rejects an asset or replay tape changing
during execution. These are local evidence receipts, not a negotiated network
manifest or a complete host-system image identity.

The JSONL witness records an explicit bounded set of native values: both RNG
streams, mission/watch/player-tick counters, selected slot, music transition
state/slots, session flags, player position/health/death/menu/hand/ammo/statistics,
resolved animation offsets, readable awards, authored prop state and active-list
pool indices. It also records raw pool and player lifetime counters separately.
There are no raw pointers or padded structure dumps. Independent peers and enum
widths compare the entire witness. Restart comparisons retain every native value
and list differences; only the test run label is excluded. Source-backed
assertions reject any unexplained difference rather than rewriting witnesses
for equality. These comparisons
are not a complete exported-state digest.

The restart control exposed a missing stage reset: the original boss calls
`reset_play_data_ptrs` before constructing viewports. The native roster adapter
now retains that exact reset, so the initial shuffle is `[0,1,2,3]` on every
stage; the previous caller context is restored when the roster is destroyed.
The fixture seeds real prior order with the original shuffle function, and a
removed-reset negative control fails.

The remaining cold/warm differences are source history. `g_PlayerTickCount`
continues from 0 to 1800, then 7200, then 9000 in the 2/4/2 chain. Its original
`>=2` gate leaves both cold two-player body animation references null on tick 0;
a warm two-player match publishes animation offsets 30588 and 33172 on that
tick. The cold match publishes those same offsets on tick 1. Raw pool lifetimes
are 1/2/3, with allocation boundaries 0/29/60 and final counts 29/60/89; player
storage lifetimes likewise advance 1/2/3. These values remain in the receipts
and are not rewritten to claim state equality. The gate rejects any other
witness difference and checks continuity across actual teardown/reconstruction.

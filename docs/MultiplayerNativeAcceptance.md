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

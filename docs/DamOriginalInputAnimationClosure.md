# Dam original input-to-animation closure

The port currently retains exact, bounded decompiled bodies for these stages:

1. `modelAnimReadRootMotionValue`, `sub_GAME_7F06D2E4`, and
   `sub_GAME_7F06D3F4` decode the ROM-authored root channels.
2. `bheadAdjustAnimation` selects `bond_eye_walk` or `sprinting` using
   `g_BondMoveAnimationSetup`. The port substitution is limited to resolving
   the native pointer-bearing `ModelAnimation` from the packaged ROM data.
3. `modelSetAnimation`, its loop/end/speed setters, and `modelTickAnim` are
   retained verbatim from `model.c` behind
   `GE_PORT_MODEL_ANIMATION_CLOCK_SLICE`.
4. `bheadUpdatePos` and the normal MoveBond root consumer feed the original
   STAN collision/fallback path.

## Exact remaining dependency closure

`bondviewProcessInput` is still intentionally not a runtime port boundary.
Retaining that whole function pulls in these state families before its normal
on-foot speed result can be trusted:

- control type, aim toggle, vertical inversion, controls-lock and pause state;
- watch/menu state and single/multiplayer state;
- current weapon flags, trigger processing, inventory cycling and remote-mine
  detonation;
- crouch, lean, auto-aim, zoom and gun gameplay updates;
- tank entry/exit and tank turret state;
- look-ahead STAN line tests, pitch/turn integration and camera FOV state.

The shortest exact route is to retain the existing function verbatim and
replace only those external reads/calls with typed providers, initially fixing
the providers to a normal Dam state: one player, controls enabled, watch
closed, alive, not aiming, not in a tank, and a concrete original controller
configuration. Its resulting `speedforwards`, `speedsideways`, and
`speedtheta` then enter the existing exact MoveBond max-speed block,
`bheadAdjustAnimation`, and `modelTickAnim`.

The authentic player gait model is now materialized: its embedded skeleton,
four-node header, rwdata, and 16-joint group graph are initialized on the
spawned player's actual `Model` overlay. Exact `modelTickAnim`, `subcalcpos`,
and `subcalcmatrices` read packaged ROM frame records and produce
`bondheadmatrices[0]`. Host sanitizer coverage pins finite walk and sprint
matrices, and the ARM build initializes and verifies walk frame 9 at stage
startup. This is the original header/rwdata interpolation path, not an
integer-frame callback.

Secondary-animation decoding and merge bookkeeping are also retained.
`bheadAdjustAnimation` resolves the materialized walk/sprint ABIs through a
typed lookup, and the original 12-tick transition loads primary and `anim2`
ROM frames before canonical shortest-path quaternion slerp applies `unk84`.
The only substitutions are typed providers for two frame pointers that the
N64 source stores through `s32`; deterministic sanitizer fixtures pin matrix
0 at blend ticks 0, 1, 6, and 12. The exact `sub_GAME_7F0062C0` root-sum body
also initializes the original walk/sprint speed multipliers (5.314286 and
18.842106), allowing `bheadAdjustAnimation` to select and crossfade sprint on
the current-player model. A current-player post-input adapter now consumes the
original clock/delta and already-written speed fields, derives the canonical
matrix-0 head velocity, and feeds exact `bheadUpdatePos` plus the normal
MoveBond/STAN consumer. It is covered for a complete 12-tick transition
against authored Dam collision. Live integration must call this boundary
after `bondviewProcessInput`; it performs no controller sampling.

## Mechanical linker frontier

`docs/generated/bondview_process_input_dependencies.json` is generated from
the relocations inside the US-version MIPS `bondviewProcessInput` symbol, not
from a handwritten dependency list. It records the object hash, exact symbol
offset/size, and separates calls/data already defined in `bondview2.o` from
the external provider/linker frontier. Regenerate it with:

```sh
python3 scripts/extract_bondview_input_dependencies.py \
  --object build/u/src/game/bondview2.o \
  --output docs/generated/bondview_process_input_dependencies.json
```

`port/tests/data/bondview_input_normal_dam.json` pins only the external
state needed to enter the canonical normal single-player Dam branches. It is
not an input algorithm: the complete original function body and its branch
order remain the authority.

## First typed provider tranche

`GE_PORT_BOND_INPUT_FULL_SLICE` now compiles only the complete canonical
function. Its 50,310 source bytes have SHA-256
`24530978a8783de7a243f2212ef79a3c5f2b3a866d38c8c8beaa5f8b9ebd8705`;
the test suite pins both values. The only conditional inside that body is a
typed `PropDefHeaderRecord` view for the dormant tank object's inherited
header, because the port compiler does not expose the decomp's anonymous
inheritance extension. Both branches read the same byte and preserve order.

The first provider tranche replaces external reads only: the current player,
single-player/pause/control options, multiplayer stop/game-over state, tank
presence, and original level-clock values. The provider reset deliberately
leaves clock values at zero: runtime must bind the original level clock and
must not invent animation or movement timing. The provider's single-player
pointer map is bound to the same live player/prop object, allowing the exact
player lookup, duck-height, and collision-radius bodies to run without a
second player representation.

After relocatable-linking that provider into the exact body and the exact
speed/turn, watch, state, generated live-player, original `joy.c`, and
table-driven `math_atan2f`/`math_asinfacosf`/`math_asinacos` objects, the direct
undefined-symbol frontier is closed. This matters beyond link bookkeeping:
GoldenEye wraps the negative Y axis to `3π/2`, while libc `atan2f` normally
returns `-π/2`. The ARM executable now uses the original angle tables rather
than silently falling through to libm for headings. The underlying MIPS
provider frontier alone contains 65 symbols; six of those are now satisfied by
the canonical helper object (`bondviewUpdateSpeedSideways`,
`bondviewUpdateSpeedForwards`, both pitch/turn dampers, and their two limit
functions). The helper fixture uses the original 60-degree Dam FOV and explicit
level-clock deltas, and checks exact acceleration, clamping, damping, and turn
sign outputs before the functions can be considered for live binding.

The exact watch zoom family is also retained. Its provider publishes both
original FOV writes: the player `fovy` field and the platform-facing current
FOV. Tests validate the original 15-tick interpolation target, the normal
`0.90909088` transition multiplier, and the watch-open one-tick branch. This is
a real state transition, not a no-op watch or camera stub.

The canonical single/multiplayer auto-aim query family is linked as well.
Normal Honey tests read the authored player flags; a multiplayer vector proves
the original permission fallback remains present instead of being folded into
a single-player constant.

One hundred and four mutable live-state helpers are generated verbatim from `gun.c`,
`gunfire.c`, `bondview.c`, `bondview2.c`, `bondinv.c`, `player.c`, `propobj.c`,
`image.c`, and the character sources. They cover weapon identity,
firing/noise reads, aim/sight state, crouch/sway state, damage visibility, and
the original visible-to-guards global, the original STAN floor-height query,
and the collision-edge intersection used by look-ahead. The canonical
`stan.c` geometry slice supplies line traversal, hit reset, collision-edge,
and floor-height implementations. Canonical single-player scenario and tank
globals retain their original zero/normal initialization, so dormant branches
are link-complete authentic state rather than provider constants or missing
symbols. The generator keeps those decompiled
files as the only source of behavior; its test compares the emitted bodies to
the canonical definitions before compiling them.

The original weapon-stat access, bitflag/ammo queries, circular inventory
lookup/mutation, forward/back cycle order, ammo-depletion auto-advance, and
queued hand-weapon transition bodies are also in the generated closure.
Canonical `default_weaponstats`, the complete 30-entry ammo limit table, and
the initial Dam weapon records through silenced PP7 are materialized from the
decomp-owned tables. The PP7 and silenced-PP7 records now retain their exact
`GwppkZ`/`GwppksilZ` names, standard-gun skeleton and model-header metadata;
they are no longer null model placeholders. The verified US-ROM extractor
also packages both exact first-person model blobs (18,512 and 19,536 bytes)
for the file/model relocation bridge. The first-person asset provider binds
the two original 0x14820-byte hand-buffer regions, requires a pack derived
from the verified US ROM, copies each exact resource only into the original
0x7530-byte model region, and validates the serialized switch/texture/root
layout before exposing it. It deliberately stops before endian and pointer
relocation; raw N64 structs are not safe native ARM structs. This intentionally bounded
record table is not a license to select later weapons before their records
are added. No per-weapon flags, ammo limits, or inventory behavior are
substituted by the platform layer.

The platform input bridge now exposes the exact raw `MoveBond` argument frame:
`joyGetStickX(0)`, `joyGetStickY(0)`, and `joyGetButtons(0, ANY_BUTTON)` after
the existing playback ring consumes the 3DS sample. The prior button mask is
supplied from the original player's `buttons_pressed` field, matching the
canonical caller rather than maintaining a second frontend-owned history.

The full canonical bodies for collision radius, guard hearing,
`gunTickGameplay`, `gunTickHandState`, first-person model demand, remote-mine
detonation, and the solo-watch transition are now retained. All canonical
gun-hand timing constants, throw/melee/taser transform keyframes and local
sound tables are generated from their original definitions. Remote detonation
mutates the canonical owner bitfield and keeps the real watch-detonate sound
request; its fixture spies on the sound call rather than deleting it. The
linker test derives both the direct frontier and the complete transitive
undefined set mechanically. A live full-body tick is still gated on the real
asset-pack-backed `load_object_fill_header`/texture relocation path, canonical
hand initialization and buffers, tile shading, watch gauge/display-list
construction, and `sndPlaySfx`/sound-state services. Those remain visible in
the transitive report; none are satisfied by no-op providers.

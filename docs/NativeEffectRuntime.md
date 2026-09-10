# Native effects in canonical simulation

The effect runtime separates original effect state mutations from GPU command
emission. `scripts/extract_effect_runtime_slice.py` retains the verified US
source bodies and the narrowly identified render-retirement statements.
It does not replace effect construction, damage, movement, randomness or
render eligibility with new formulas.

`ge_original_effect_runtime_reset(count, stage)` retains the relative original
stage reset order: stationary/moving spark reset; casing pool reset; the
existing explosion/smoke/particle/impact/scorch allocation boundary; then
`alloc_shattered_window_pieces` through a static 200-slot allocation seam.
The exact shard formula is 200/player count, halved on Streets/Depot, with
`active = 0` and `g_NextShardNum = 0`. The casing sound handle is cleared from
the later `init_ejected_cartridges` prefix. Invalid counts leave state alone.
The missing shard allocation previously allowed retained `glassCreateShard`
to dereference a null pool on a real glass hit.

`ge_original_effect_runtime_pre_world()` must run once per committed world
tick after original clocks/difficulty/sky and before `chrpropTick`. Its calls
retain `lv.c` order: `bullet_sparks_update_all`, `update_bullet_casings`,
`update_broken_windows`, `explosionUpdateFlyingParticles`. Native elapsed
time, movement, gravity, expiry and RNG calls remain in those bodies. The
selected player must still be the canonical prior viewport owner because
casing audio eligibility reads that player's hand action state. This is not
a local display callback.

`ge_original_effect_runtime_render_view(context, render_prop)` runs after
`propsTickPlayer` and before first-person `gunUpdateAndFireBothHands`, for
every canonical viewport. This is `bgLevelRender`'s semantic position in
`lvlRender`. `ge_original_bg_visibility_render_rooms` copies current native
room publication order and signed `s_bound_info.unk1`; the existing public
8-bit portal depth is insufficient for this order.

The runtime visits signed depths ascending for each room's opaque pass 0
then post-BG pass 2, followed by descending depth alpha pass 1. Equal depths
retain native publication order. Sorting the bounded room list skips only
empty depth buckets from the original loops. The extracted
`chrpropsRenderPass` retains on-screen traversal direction, prop flag gates,
first-rendered-room selection, Cuba pass adjustment, and alpha dispatch.
The non-debug original object-draw gate is 1; native `D_800483C0` retains its
original draw-enable gate and reset value. Call before any local renderer
restores or replaces the selected viewport publication.

At each dispatched prop the caller handles native character/object render
semantics through the callback, returning zero on ownership failure. The
effect closure then applies original EXPLOSION/SMOKE alpha retirement.
Callback failure suppresses later callbacks and effect mutation and causes
the runtime to return zero. A null callback supports effect-only diagnostics;
it is not complete world render semantics.

`explosionRenderPropExplosion` computes the integer threshold from
`flareanimspeed * 15.0f` and clears frames strictly above it.
`explosionRenderPropSmoke` normalizes parts that do not satisfy `size > 0`
to positive zero. Those fields control future part-slot reuse, so eligible
retirement belongs to every canonical viewport regardless of display.
`ge_original_effect_impact_semantic_exact(prop, alpha)` separately retains
the original matching impact scan and `Scorch.unk02` bit clear used by
character/object render paths. Here `prop->scorch` is the original union
alias; it must not be treated as a member of the solo scorch buffer. Only
GPU/texture/room-matrix emission is removed from that function.

The generated slice adds only previously absent original strong storage:
the 50 moving sparks, casing sound handle, original casing gravity constant
and `D_800483C0`. Stationary sparks, casings, shard pointer/count/cursor,
explosion arrays and image tables retain their existing owners. Models,
audio, content references, third-person render semantics and actual GPU
drawing remain their owning adapters' responsibilities. No whole-state
digest or complete renderer claim follows from these hooks.

Build `ge_original_effect_runtime.c` and generated
`ge_original_effect_runtime_slice.c` with the normal US native type/setup
flags (`_LANGUAGE_C`, `GE_PORT_USE_ORIGINAL_TYPES`, `AIPARSE`,
`GE_PORT_SETUP_DATA`, `GE_PORT_MS_INHERITS`, `VERSION_US`, `BUGFIX_R0`,
`-fms-extensions`). Generator prerequisites are the generator and its imported
extractors plus `glass2.c`, `glass.c`, `explosion.c`, `gunfire.c`, `gun.c`,
`objective_status2.c`, `initactorpropstuff.c`, `alloc_window_pieces.c`,
`chrprop.c`, `lv.c` and `ejectedcartridges.c`. `init_ejected_cartridges` supplies the documented
one-statement sound reset subset. Other regions are explicitly rejected by
this currently verified slice.

`scripts/test_effect_runtime.sh` runs real original shard/spark/particle
construction and update bodies under ASan/UBSan for two/four players, casing
motion, repeated stage initialization, canonical signed-depth pass ordering,
visible/hidden retirement, exact threshold equality, smoke normalization,
draw-disable gating, and object impact bit clearing. Room membership,
audio and loaded model/image resources are explicit fixture seams. The
visibility getter also passes the existing native/short-enum multiplayer
visibility tests. The generated source compiles for ARM at `-O2 -Werror`.

`ge_original_effect_runtime_storage` exposes read-only typed pools and a stage
observation epoch for E8. Native effect-buffer resets are observed separately
so a direct same-address allocation reset also invalidates the view. Close
consumers and exporters before `ge_original_effect_runtime_end`; pre-world and
render hooks reject an absent or ended owner. This bookkeeping does not alter
original effect fields. See NativeEffectsState.md for E8 identities and
remaining reference obligations.

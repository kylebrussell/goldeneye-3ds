# Stage prop lifetime boundary

`int ge_original_prop_state_end(GeOriginalPropState *state)` is the port's
whole-stage pool retirement boundary. Call it once after every native
consumer and resource owner has closed, before exporter end and before the
next prop reset. It is not an extracted gameplay cleanup function and does
not substitute for resource cleanup. It clears published active/free/room
roots and invalidates read-only pool views without changing any `PropRecord`
or pretending each residual prop passed through `chrpropFree`.

The original sequence is `lvlUnloadStageTextData` in `lv.c`, followed by the
next iteration of `boss.c` resetting `MEMPOOL_STAGE` and player storage.
`initobjects.c::init_load_objpos_table` rebuilds the complete static prop free
list and fresh stage room tables. Original guard, object and effect cleanup
does perform individual removals where required: `cleanupGuardData` cleans
characters then delists/disables/frees their props; `cleanupObjects` dispatches
`objFreePermanently` by authored type; `cleanupExplosions` retires effect props.
Those functions also own resource side effects. A blanket per-prop free loop
would not reproduce those operations.

The current frontend has different explicit resource owners:

| Close function | Residual pool ownership |
| --- | --- |
| `close_dam_world_objects` | Frees owned definitions/model providers and clears its wrapper; most canonical props remain allocated. |
| `close_stage_ordinary_objects` | Performs the typed autogun cleanup, closes scheduler/resource owners, frees ordinary definitions and providers; most ordinary root props remain allocated. |
| `ge_original_stage_guard_runtime_destroy` | Individually frees live player-item objects, delists guard roots, detaches viewer chr/body ownership and closes model owners; guard and viewer props can remain allocated. |
| `ge_original_multiplayer_players_destroy` | Restores prior player bindings and frees player storage, without freeing player props. |
| Early constructor failure | An allocated but unactivated prop can survive until stage retirement. |

Accordingly, the prop pool has a final arena lifetime in addition to its
individual allocation/free lifetimes. E3/E5 observes the distinct final event.
It validates pool address/lifetime, room ownership, allocation serial, and
free-list membership before retiring residual actor IDs. A missing individual
free leaves an ID on the physical free list and fails validation. A missing
allocation leaves an allocated physical slot without an ID and also fails.
Previously recorded lifecycle faults remain faults. Exporter end still
unbinds after errors so teardown can finish, but must report that failure.

For main integration, all success, restart and post-pool-reset load failures
converge on `cleanup_runtime`. Call the pool end after ordinary/guard, player,
gait, first-person/model, setup, collision and renderer consumers have closed
(immediately after `renderer_exit` is a suitable point), then end E3/E5.
Guard/model consumers must close before player storage is destroyed. Stop
simulation and snapshot/export callbacks before resource teardown starts;
do not run gameplay during the interval where native unions reference closed
resources. Do not move arena end ahead of typed native cleanup.

Track whether prop reset succeeded: failures before reset have no pool to
end, and exporter begin must succeed immediately after reset and before the
first allocation. `ge_original_prop_state_end` returns zero for null/wrong
owners and repeated closure, without mutation. `close_dam_world_objects`
clears its statistics wrapper before other consumers close, so the pool
owner caches validated room capacity at reset; final end accepts that same
owner address without reading overwritten statistics. The caller's wrapper
address must remain valid through final end.

The 3DS main path now follows this order on stage completion, restarts and
post-reset load failures. Its registry retains released physical keys within
one match so the next allocation advances its generation. After clean teardown
checks every identity has ended, a new match starts a fresh registry, making
initial IDs independent of each peer's prior match history. The protocol session
identity scopes numerically reused actor IDs across separate matches. E3/E5
begins immediately after successful pool reset. After native player creation,
E1/E2, all eight E4 hand identities and all four E6 player identities begin;
these end before player destruction. E7 capture storage and identity begin
after BG geometry load, and end before that native geometry closes. The E0 core identity ends after the
prop arena and its exporter. Any observer fault stops the match, and any
lifecycle or leftover-live-identity failure prevents a stage restart. No
complete schema or match digest is produced by these ownership bindings.

`scripts/test_multiplayer_main_state_lifecycle.sh` compiles the actual main
ownership helpers with native player/inventory allocation and the real prop
pool under ASan/UBSan in both enum ABIs. It checks 2/4/2 storage reuse, partial
loads, residual props, identical initial IDs across different prior histories,
within-match generation changes, stale IDs after teardown and rejection of a new stage
after a deliberately missed native free callback.

The next stage must reset the prop pool and reconstruct all consumers before
resuming gameplay. In particular, the current effect owner resets native
effect buffer prop references before the next stage's semantic ticks. The
arena boundary does not traverse or repair separate effect, on-screen,
inventory, model or player pointer owners. Their teardown/reset obligations
remain explicit.

`scripts/test_original_prop_stage_end.sh` uses real canonical pool/list/room
and free bodies under ASan/UBSan. Its resource objects are fixture allocations;
it tests the pool contract and actual registry lifetime behavior, without
claiming a complete stage/world cleanup or networked match.

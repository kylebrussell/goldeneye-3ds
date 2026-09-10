# Native effects state (E8)

`ge_mp_state_effects` exports named native fields from the US effect owners.
It neither hashes native struct bytes nor substitutes a new effect model.
Successful export classifies this bounded effects closure under
`GE_MP_SYSTEM_EXPLOSIONS | GE_MP_SYSTEM_PROJECTILES`. It does **not** attest
that those entire world domains, or the global state schema, are complete.
Keep the aggregate `GeMpStateSchema.complete` zero until every enabled owner
and reference has been independently closed.

## Ownership and lifecycle

Call the existing `ge_original_covert_modem_projectile_reset()` and
`ge_original_effect_runtime_reset(count, stage)` during native stage setup,
then `ge_mp_state_effects_begin(owner, registry)`. The caller supplies zeroed
owner storage and a valid registry. The exporter does not own protocol-session lifetime or reset the registry.
Generation history is retained within a match; the caller may start a fresh
registry for a fresh protocol session, as the main integration does.

E8 IDs describe **physical buffer slots for one native stage lifetime**,
including inactive slots. Kind is `0xE8`, creation namespace is the component
number below, and slot is the physical array index. Reusing a slot within the
stage preserves its identity; this is not an effect-spawn generation claim.
The exporter covers every location, including free retained state, so
inter-snapshot reuse cannot erase that state from enumeration. Capacity
preflight rejects a begin without acquiring a partial set of IDs.

`ge_mp_state_effects_actor(owner, component, native_pointer, &id)` validates
exact element boundaries and returns that live physical identity. In
particular, E3 can resolve `prop->explosion` and `prop->smoke` this way.
It does not accept an interior address, an address outside the active native
capacity, or an owner from an earlier stage.

After all native consumers and the final snapshot have finished, call
`ge_mp_state_effects_end`, then `ge_original_effect_runtime_end` before the
next stage reset. End releases the registered physical IDs; it does not
fabricate individual native frees. Native runtime resets, direct calls to
the existing effect-buffer reset boundary, and projectile-pool resets advance
observation epochs. An old exporter is stale even when static addresses and
capacities are unchanged. Ending that stale exporter still releases its IDs
and returns `STALE`. Epoch exhaustion prevents another valid owner; no epoch
wrap is accepted. The getters are read-only and do not advance simulation.

| Component | Native owner | Count |
|---|---|---:|
| 0 global | counters, capacities, colors, casing audio handle | 1 |
| 1 explosion | `g_ExplosionBuffer` | 6 |
| 2 smoke | `g_SmokeBuffer` | 20 |
| 3 particle | `g_FlyingParticlesBuffer` | 200/count; halve Streets/Depot |
| 4 impact | `g_BulletImpactBuffer` | 100 |
| 5 scorch | `g_ScorchBuffer` | 20 solo, 0 multiplayer |
| 6 shard | `ptr_shattered_window_pieces` | 200/count; halve Streets/Depot |
| 7 stationary spark | `g_BulletSparkArray` | 20 |
| 8 moving spark | `g_MovingBulletSparkArray` | 50 |
| 9 projectile | actual strong `g_Projectiles` | 20 |
| 10 casing | shared `g_Casings` | 20 |

The largest population is 657 records (solo, ordinary stage). Every E8
record has the same schema: fields 256 component `U8`, 257 physical slot
`U16`, 258 active `BOOL`, then fields 259–266 typed sequences `I32`, `I16`,
`I8`, `U32`, `U16`, `U8`, `F32`, and nullable `ACTOR_REF`. Each sequence keeps
the component-specific append order below. Coords use x/y/z; matrices use
row-major native `m[i][j]` for i,j=0..3. Native vertices append, per vertex,
ob[0..2], tc[0..1] to I16; flag to U16; cn[0..3] to U8. No sequence is sorted.

## Retained fields and conditional payload

Names below are the native declarations, not guessed interpretations of
unknown fields. Integer signedness follows those declarations; the
`DROPTYPE` enum is explicitly converted to I32 so enum width is not a wire
property. All exported F32 values must be finite; finite bits including
negative zero are preserved.

- Global: I32 stage, `g_NumExplosionEntries`, `g_NumSmokeEntries`,
  `g_NumParticleEntries`, `g_NumImpactEntries`, `g_NumScorchEntries`,
  `g_NextShardNum`, `D_800483C0`; U32 player count followed by component
  counts 0..10; F32 `g_SpExplosionDamageMult`; U8 all eight
  `g_BulletSparkColors` entries in r/g/b/a order; audio reference
  `g_CasingSfxState`. Particle, impact, scorch and shard ring cursors are
  checked against native capacities. Explosion/smoke counters also serve
  shake logic and are not falsely constrained as allocation counts.
- Explosion: active means `prop != NULL`. When active: I16 age, unk3CA;
  I8 explosion_type, unk3CD, player; prop references prop, source. If unk3CD
  is nonzero, append pos to F32 and room to I16. Then, **even when free**,
  append all 40 part frames to I16 and pos, size, rot to F32 for each frame
  greater than zero. `explosionCreate` replaces part 0 but does not clear
  the entire previous part pool. Zero/negative-frame payload is not read
  before its replacement. The scorch position is consumed only with unk3CD.
- Smoke: active means `prop != NULL`. When active: I16 duration, smoke_type,
  then prop reference. Always append all 10 part sizes to F32. For each
  **nonzero** size append pos, rot, deltarot, offset1, offset2, alpha to F32
  and count to I16, even when the smoke is free. `explosionSmokeTick` uses
  `size != 0`, while render retirement uses `size > 0`; negative-size state
  must not be discarded before canonical retirement.
- Flying particle: always I32 unk00 (active when positive); when active:
  F32 position, rotation, position_drift, rotation_drift, then four native
  vertices. Shard: always I32 active (active when positive); when active:
  F32 pos, rot, velocity, angvel; each of three vertices supplies I16
  x,y,z,s,t,flag and U8 r,g,b,a in that order.
- Impact: always I16 room (active when nonnegative); when active append
  I16 impact_type, I8 model_render_pos_index, room_clear_flag, prop reference,
  and four native vertices. Scorch: always I16 roomid (active when
  nonnegative); when active append F32 pos, explosion_size and four vertices.
- Stationary spark: always I16 lifetime; when positive append I32 age,
  I16 unk06; F32 unk08, unk10, unk14, unk18, unk1c, unk20, unk24;
  U8 unk28, unk29, unk2A, unk2B; image-table reference unk0C. Moving spark
  appends the identical embedded spark fields, then, when active, F32
  velocity and unk38.
- Projectile: active when `PROJECTILEFLAG_FREE` is clear. Always retain
  U32 flags; F32 speed, unk10, unk1C, mtx, unk60, unk64, unk68[0..3],
  unk78[0..3], unk8C; U32 unk90; F32 unk94; U32 lastSfxTimer, soundSlot,
  unkA8, unkAC; F32 unkB0, unkB4; I32 droptype, refreshrate; F32 unkC0,
  unkC4, unkC8; U8 unkCC[0..7]; F32 fallback target x/y/z at
  unkd4/unkD8/unkDC; U32 age;
  I32 unkE8. References are ownerprop and obj when active (otherwise null),
  then sounds[0..1] regardless of the free flag. The exact `projectileReset`
  leaves matrices, several scalars, room tail and sound handles unchanged;
  the exporter does not invent inactive defaults. The original gun producer writes and collision code reads the entire
  fallback target through a coord3d alias. Despite the declaration spelling
  of unkD8/DC as U32, their bits are decoded and validated as F32 y/z. No
  integer conversion, native pointer, or enum representation is copied.
- Casing: active means header nonnull; when active append F32 floor_y_pos,
  pos, vel, rot_mtx, rot_velocity_mtx and the model-header reference.
  This shared 20-record pool is distinct from E4's embedded hand state.

Source audit: `explosion.c`, `glass.c`, `glass2.c`, `gunfire.c`, and
`propobj.c`, with declarations in explosion.h, glass.h, gun.h and bondtypes.h.
No reads were found for ExplosionPart.bb, Explosion.unk3CF/unk3DE,
FlyingParticles.unk34, shard.field_0x34 or BulletImpact.unk04/unk4E; these
unused members and struct padding are omitted. Solo scorch model/unk02/unk03
are not used by the solo pool constructor/render path. The `prop->scorch`
used by impact rendering is an **object union alias**, whose model and
impact bits belong to the object/character owner; it is not this solo pool.
Inactive payload omitted above is replaced before consumption or remains
unread while its activity marker is clear. The deliberately conservative
free explosion/smoke/projectile retention is the exception described above.

## References and remaining integration obligations

`GeMpStateEffectsProviders.reference` must map each nonnull native reference
to the correct live stable registry ID for its typed category: PROP, OBJECT,
AUDIO, IMAGE_TABLE or MODEL_HEADER. The provider must not use pointer bits,
allocation order or a fabricated fallback ID. Each live referenced ID still
requires its own record in the final aggregate snapshot. An active spark
with a null image table is explicitly unresolved. Unknown or stale mappings
return `INCOMPLETE`, leave record_count zero, and set `unresolved_kinds` bits
using `1U << GeMpEffectReferenceKind` plus the unresolved-reference count.

Actionable gaps outside this bounded exporter:

- Supply real E3/object owners for explosion source, impact carrier and
  projectile owner/object references. Object impact flags, embedment,
  proximity mines, native object/character state and ordered model-hit
  entries are not made complete by E8.
- Supply stable content-table/model-header identities for all three spark
  image tables and casing model headers. The compiled US g_ExplosionTypes,
  g_SmokeTypes, g_ImpactTypes and gravity constants are immutable within the
  audited source; their exact version belongs to agreed build/content
  SHA-256 manifests. E8 does not calculate or claim those manifests exist.
- Supply logical audio identity and state for casing/projectile handles.
  Audio handle addresses are not a deterministic identity. Impact sound
  pools, other native audio globals and resource readiness require their
  own audit; null handles in a resource fixture are not proof of runtime
  closure.
- Keep the canonical source reset/update/render-semantic hooks described in
  NativeEffectRuntime.md, including every viewport's eligible explosion,
  smoke and object-impact retirement. Local rendering alone cannot own
  those future-state mutations. Character/object render semantics and
  their RNG ordering are a separate native owner.

Build `ge_mp_state_effects.c` with the same native US flags as the effect
runtime. Public headers expose only opaque native pointers and fixed-width
types. Static assertions verify original 32-bit US struct sizes/offsets;
host tests use both ordinary and short-enum layouts. Run
`scripts/test_mp_state_effects.sh` and the same command with `-fshort-enums`.
The ASan/UBSan fixture executes original shard/spark/particle constructors,
original updates and projectileReset; checks two/four-player mutations,
negative smoke/free parts, all named projectile scalar groups, the fully
occupied 657-slot case, invalid references/floats/cursors, and generation
changes/stale owners across both kinds of stage reset. Audio, room membership
and loaded image/model resources are explicit test boundaries. Its payload
fingerprint is a test comparison only; the real canonical SHA API is tested
to refuse the intentionally incomplete global schema without changing its
output digest. These are not full-match or UDS acceptance results.

Projectile fallback-target regression retains the actual original gun producer
statements and fallback gate. All three components export as F32, preserve
signed zero and finite values, and reject NaN or either infinity on each axis
(including retained free slots). Both host ABIs cover this alias; native
offset assertions check that the three source members remain contiguous.

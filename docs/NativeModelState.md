# Native Model state (EC)

`ge_mp_state_models` reads mutable native `Model` instances from
`src/bondtypes.h`. Kind `0xEC` is a bounded instance owner. It does not close
the animation system, immutable model content, instance RW data, or the global
simulation. Keep aggregate `GeMpStateSchema.complete` zero until every enabled
owner and reference has independent coverage.

## Physical storage and lifetime

The original `modelmgrInstantiateModel` in `src/game/model.c` allocates a
prefix ending immediately before `Model.anim`; the animated allocator owns
the full Model. Descriptors therefore require **exact native**
`offsetof(Model, anim)` for `GE_MP_MODEL_PREFIX` or `sizeof(Model)` for
`GE_MP_MODEL_FULL`. `ge_mp_state_models_native_size` supplies these values
without exposing native headers. N64/ARM values are 0x20/0xbc; host pointer
width changes the extents. A full descriptor must never name a prefix-only
allocation. The reader does not touch animation or callback fields of a prefix.

Begin E3 after native prop reset. Once the containing model storage exists,
call `ge_mp_state_models_begin` with a zeroed owner, the E3 owner and key-sorted
`GeMpModelDefinition` descriptors. Each supplies the borrowed native pointer,
exact extent, agreed creation namespace and physical slot. Each storage address
appears once. Up to 1024 instances are supported. Owner and snapshot are large;
allocate them outside the 3DS stack.

IDs identify physical Model storage for the stage lifetime, independent of
whether `obj` currently names a live resource. Namespace and physical slot
come from the containing owner, never pointer values or exporter registration
order. `clear_model_obj` only clears `obj`; `datas` and `rwdatalen` survive and
`modelmgrCanSlotFitRwdata` uses them for the next allocation. EC consequently
retains every field and nonnull reference of the supplied extent even while
`obj == NULL`. No native allocation/free is manufactured by export or end.

The read-only Pitem and character `*_model_physical_capacity` and
`*_model_physical_instance` getters enumerate every native full-Model slot,
including inactive capacity. Returned `active` is the provider's current
allocation state, not an EC identity. Pitem release clears its whole instance
and later allocation reuses its address. Character allocation advances through
its fixed array; all addresses remain valid until provider destruction.
Player-owned full Models can use player slot namespaces. E4 already describes
embedded hand prefixes: avoid describing the same physical prefix twice.

After the last native consumer/snapshot, end EC **before** destroying player,
Pitem, character, or other containing storage or replacing the registry.
Then perform native cleanup/arena end and E3 end. EC captures E3's actual
pool identity and stage epoch; checks also preserve E3 allocation observation
faults. Export fails before model reads if that lease expired. This lease
cannot detect an early free of separate borrowed Model storage during the
same stage; keeping those allocations readable until EC end is the caller's
explicit ownership obligation.

End validates the complete count, descriptors, registry shape and exact
EC/namespace/slot identity of every owned ID before any release. Malformed
owners leave registry and owner unchanged. An expired native lifetime still
permits retirement of owned IDs and reports the lifetime failure; a stale
generation cannot retire its replacement. Registry history belongs to the
caller: preserve generations within a match and use the agreed fresh registry
for a new protocol session.

## Typed fields

No native bytes, padding, pointers, enum widths, frame handles or callback
tokens are serialized. All F32 fields must be finite; finite bits including
negative zero survive. Prefix-only animation fields have count zero.

| Field | Type | Native order |
|---|---|---|
| 256 | U8 | descriptor extent: prefix 0, full 1 |
| 257 | I16 × 2 | `unk00`, `rwdatalen` |
| 258 | F32 × 1/28 | `scale`, followed by full animation scalars below |
| 259 | nullable actor refs × 6/12 | six prefix refs, then six animation/frame refs below |
| 260 | I8 × 0/4 | `gunhand`, `unk25`, `animlooping`, `unk27` |
| 261 | I16 × 0/4 | `framea`, `frameb`, `frame2a`, `frame2b` |
| 262 | I32 × 0/1 | `unk9c` |
| 263 | U32 × 0/2 | root callback role, animation-flip callback role |
| 264 | U8 | descriptor creation namespace |
| 265 | U16 | descriptor physical slot |

The 27 animation F32s follow native declaration order:
`animframe1, unk2c, endframe, speed, newspeed, oldspeed, timespeed,
elapsespeed, animframe2, unk5c, unk6c, speed2, unk74, unk78, unk7c,
unk80, unk84, unk88, unk8c, animloopframe, animloopmerge, playspeed,
animrate, unkac, unkb0, unkb4, anim_translation_scale`.

`modelTickAnim` consumes these clocks/targets for interpolation, merge expiry,
secondary animation, frame wrapping and callbacks. Exact `animInit` resets
only its authored subset: residual targets/frames remain represented. EC
reads `unk9c` as declared I32 even though the currently retained source only
writes it. No unclassified scalar is silently discarded.

Reference order is `chr, obj, render_pos, datas, attachedto,
attachedto_objinst, anim, anim2, unk34, unk38, unk64, unk68`.
`attachedto` resolves against EC's physical table first. Other nonnull values
require the typed caller provider and a live registry ID. Missing, stale,
unmapped or zero resolutions return `INCOMPLETE`, no records, and an
`unresolved_kinds` bitmask; external referents still require records in the
aggregate snapshot.

`subcalcmatrices` writes the four frame words with `loadAnimationFrame`;
joint consumers decode the addressed frame data. N64 uses 32-bit addresses,
while the host gait uses cache handles. `frame_reference` receives the exact
owning Model, frame role and signed native word and must resolve the actual
frame/cache/content identity. Zero is null; negative words are not rejected
merely for their sign. Numeric handle values are never treated as scalar state.

`unka0` controls the original root-motion callback in `sub_GAME_7F06D490`.
The read-only `ge_original_model_root_callback_read` resolves its actual
function from the checked host table or native ARM word. The corresponding
flip getter is `ge_original_player_gait_anim_flip_callback`; the original
`modelTickAnim` loop boundary invokes that function. Cleared callbacks map
to role zero. The caller's `root_role`/`flip_role` must map exact typed
functions to agreed nonzero semantic role enums; unknown functions fail.
Known retained functions include `sub_GAME_7F01FC10` for character root motion
and `bheadFlipAnimation` for Bond's animation loop. Function addresses,
per-process handle numbers and registration order are not valid roles.

## Remaining owner contracts

These are actionable reference/ownership gaps, not a claim that a Model-only
hash can agree on the full simulation:

- Native ModelFileHeader and skeleton/animation content need verified content
  identities and mutable relocated-header/node topology ownership. E8's
  model-header references are not Model-instance references.
- RW arrays and all native node-specific RW variants need typed physical
  owners, including linked Models, attached head topology and their lifetimes.
- Matrix arrays need typed owners and producer-phase identities; they may be
  frame-arena allocations, not stage-lived model storage.
- Animation frame/cache state, cache ordering and actual decoded content need
  separate ownership behind `frame_reference`.
- Pitem/character provider allocation cursors, active flags, resources and
  native resource topology are outside the Model struct. The getters expose
  physical Models but do not export that allocator metadata.
- Legacy `ModelSlot`/`AnimModelSlot` layouts in `objecthandler.h` use N64 s32
  pointer fields. Host callers cannot cast those raw legacy arrays into native
  pointer-width Model descriptors without an independently checked ABI bridge.

## Validation and build

Run `bash scripts/test_mp_state_models.sh [staged-model-pack]`. By default it
reads the existing `build/host-tests/multiplayer-bodies/stages.gepack`; it does
not regenerate or modify shared ROM/model assets. Both enum ABIs run under
ASan/UBSan. The fixture uses actual modelInit/animInit, animation setters,
modelTickAnim/loop callback, clear_model_obj and native reuse eligibility.
It covers two/four models, short-allocation bounds, all F32s, missing/stale
references, frame handles, process-order-independent callback roles, invalid
tokens, malformed teardown, generation replacement and native stage end.
Actual packaged Pitem/character constructors exercise all-slot enumeration,
release/reuse and incomplete resource-reference reporting. Explicit empty
model resources in the clock fixture isolate Model semantics; this is not a
world/combat or complete-state acceptance test.

`ge_mp_state_models.c` requires the same native type/setup/region flags as EA:
`_LANGUAGE_C`, `GE_PORT_USE_ORIGINAL_TYPES`, `GE_PORT_SETUP_DATA`, `AIPARSE`,
and the selected region/enum ABI. Link E3, registry state,
`ge_original_model_callbacks.c` and `ge_original_player_gait.c` for the two
read-only callback getters. There is no new generated production slice or
main hook; integration owns descriptor gathering, role/reference providers,
schema registration and lifecycle ordering.

# Door occlusion and publication optimization — 2026-09-04

This follows the [Caverns frame-budget checkpoint](Verification20260904CavernsBudget.md).
The new work fixes the visible guards-through-elevator-door defect and removes
unnecessary matrix construction from unchanged-door polling. Measurements use
Azahar; New 3DS XL remains the primary physical target, with original 3DS
compatibility retained. Neither hardware FPS target is certified here.

## Rendering correction

`chrobjRenderProp` supplies `zbufferenabled = (obj->flags2 & 0x10000) == 0`
before rendering every world object. The ROM child display lists do not need
to repeat that parent state. Native guards already supplied it, but manual
ordinary-door scene inputs omitted it unless they were windowed type-4 parts.
As a result, opaque elevator doors could draw without writing depth, allowing
later guard triangles to overwrite them.

The shared material/template adapter now restores the original depth flag for
all world objects before applying any additional glass-specific setup. Both
initial door installation and subsequent door-cache updates use that adapter.
The older standalone Dam door-scene adapter now supplies the same flag.
Authored depth-disable flags remain honored. Primary model lists compare and
write depth; secondary translucent lists compare without writing, through the
existing model renderer contract.

Live Caverns inspection confirms that the closed elevator doors now occlude
the guards. Their authored placement, active state and gameplay visibility
are not modified. The scene still submits guard geometry for normal GPU depth
testing. Patterned artifacts along the door seams remain visible and need a
separate diagnosis; this is not a complete Caverns fidelity claim.

The material fixture exercises actual model IDs 144, 176 and 178, verifies
missing inherited depth in the raw child-list path, and checks both material
and retained-template setup with the authored depth-disable flag. Existing
glass lighting, alpha, opacity and cache tests still pass.

## Optimization

The unchanged-door gate formerly called the full door snapshot, reconstructing
base/eye/iris matrices and copying collision metadata only to read its generation.
`ge_original_door_runtime_generation` now uses the same validated native source
and generation update logic without building those matrices. Native door lookup
is also shared in a single search. Changed doors still receive full publication.

The generation continues to track exact open-position bits and clipped-vertex
publication, including the original first-publication increment. Tests compare
generation queries before and after full snapshots across the existing campaign
opening/closing, paired, flexi, eye and iris fixtures. Canonical door movement,
collision, portal and sound operations remain unchanged.

## Performance

The same 3,000-dispatch Caverns trace ended at
`5353.534180,-2665.283691,-764.409119` in both builds. The first 120 submissions
are excluded; explicit display-pacing waits are excluded from CPU work.

| Metric | Previous `fef1a3df` | Depth + generation `1405bda8` |
| --- | ---: | ---: |
| Warm CPU mean | 8.8575 ms | 8.8479 ms |
| Warm CPU p95 | 14.3414 ms | 14.3084 ms |
| Warm CPU p99 | 16.7654 ms | 16.7219 ms |
| Warm CPU peak | 21.6251 ms | 21.5793 ms |
| CPU frames over 16.67 ms | 30 / 2,880 | 29 / 2,880 |
| Repeated / skipped submission intervals | 0 / 0 | 0 / 0 |
| Warm multi-retrace dispatches | 7 | 7 |

All 2,879 adjacent warm submissions remained one top-screen VBlank apart.
The moving closed-door view showed 60 FPS at 100% emulator speed. The gain is
modest in this context, not evidence of a hardware 60 FPS lock. CPU spikes
remain above budget. No builds or host regression tests ran during measurement.

Private evidence: `build/visual-probe/caverns-render-depth-trial750.result`,
`caverns-render-depth-generation3000.result`; analysis and build/test logs are
under `build/host-tests/caverns-render-pass/`.

## Door activation check and remaining rendering work

A separate controller-only trace approached the elevator, stepped back for 20
frames, pressed Use, and waited. Both leaves (commands 283/284, model 144)
reached the authored maximum `0.949997`, with two completed opens and all 182
movement collision tests clear. Live inspection showed the open doorway and
guards correctly revealed behind it. Pressing Use while Bond remained against
the doors instead produced 510 blocked checks and no movement; stepping back
resolves that obstruction without changing the original collision code.

This check also exposed the next geometry-publication gap: the campaign door
renderer consumes canonical matrices but does not yet consume the runtime's
clipped vertex array. The standalone Dam model-178 adapter already does.
The Caverns leaves publish 32 clipped vertices; the generic retained model cache
still reads their immutable ROM vertices. The open view has visible door-edge
geometry outside the doorway. Connecting dynamic clipped positions and texture
coordinates to the cache requires explicit invalidation and owned lifetime;
that work remains open, as do the closed-door seam patterns.

The full `scripts/test_port.sh` suite passed for the depth and generation
changes. The final ARM rebuild adds probe-only resident-door and collision
counters, with a null-preview guard. These counters are written after a finite
probe, outside the measured frame loop. The `door_runtime` tick/timer fields
count calls through the public wrapper; the live extracted prop tick can call
the canonical slice directly, so zero wrapper ticks do not mean doors were
unticked. Collision and completion counters confirm their movement.

Final executable SHA-256:
`9137d52d5a65fae285d0138b580a67a321b713535a3ac21cc657d806243fac96`.
The 3,000-frame comparison above used the same gameplay/rendering changes before
these extra result fields. Diagnostic traces are preserved as
`caverns-door-counters750.result`, `caverns-door-backoff750.result`, and
`caverns-door-backoff2250.result` in `build/visual-probe/`.

The longer 2,250-frame trace also exercised closing and reopening: four completed
leaf opens, two completed closes, and 540/540 clear collision checks. The final
build is installed in Azahar and staged under
`build/3ds-sd/3ds/goldeneye-3ds/`, with a preserved copy under
`build/3ds-candidates/caverns-door-9137d52d/`. `deploy_3ds.sh --skip-build`
validated the executable, metadata and unchanged asset-pack hashes. Temporary
stage/input overrides were moved into `build/visual-probe/`.
Normal startup was then verified at the Rare intro, and emulation was stopped through the UI.

Follow-up: [Shared door vertex publication](Verification20260904DoorClipping.md) connects clipped XYZ/ST to the campaign cache and GPU upload path. Remaining edge/seam artifacts still need diagnosis.

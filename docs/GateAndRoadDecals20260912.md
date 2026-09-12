# Gate and road-marking hardware pass — 2026-09-12

## Preserved evidence

Installed commit `0a3981dc40ad339a07eef956e0f9e403b72e1b63`, executable SHA-256
`bae88f62e8118c0d4d1cb4c73e42033bc6cf3b72e026d4b60433d52ca7a296d9`.
Backup: `build/hardware-captures/gate-decals-20260912-122714/`, including matching
ELF, file hashes, logs and all three existing Luma dumps. None of the dumps changed.
The two new captures are ticks `51065897702` (gate button) and `57251717148`
(truck side and road arrow); each has complete color, vertex and depth files.

## Gate

The new bounded witness identifies truck321/prop137 as the blocker for gate267.
Its actual front hull Z spans 8735.71289–8738.36426, overlapping the gate edge
at Z8739.94238. Six new accepted Use interactions target panel269, associated
with the first gate. There is no recorded activation of the second gate's panels.

After the first capture, gate267 reports fully open and its published geometry
moves from X17075.416 to X17664.9961. The truck later moves past it to Z8257.64355.
Gate268 stays closed. This evidence must not be confused with the user's observed
progression failure: the initial first-gate collision lock remains real, and the
second gate never receives an activation in this log.

The cause is a missing primary-room registration in the port's door-construction
bridge. It assigned `prop->rooms[0]` but did not register the door in that room's
lookup list. Canonical `setupdoor` performs both operations. The truck therefore
could not discover the closed gate through its room query, while the gate could
find and be blocked by the truck. Primary-room registration is now restored;
there is no altered collision shape, special gate exemption or truck movement rule.
The actual hardware hull was independently reproduced with the real truck model,
Dam STAN tile1253 and canonical terrain alignment; results are preserved at
`build/host-tests/gate-collision-20260912/results.txt`.

## Diagnostic clock

Saving the first capture blocks for roughly 11.5 seconds. The old live scheduler
counts that SD stall as simulation time on its next iteration. Gate lastcalc jumps
from 8663 to 9354 across five render iterations, coinciding with the gate opening.
SELECT's capture interval is now excluded from the next elapsed-time delta while
ordinary frame work is retained. No cap or other change to normal simulation time
was introduced. The source-extracted regression covers zero/short/11.5s/60s stalls,
no capture, deferred capture and preserved normal frame time; removing the clock
adjustment fails the regression. ASan/UBSan pass.

## Road markings

The yellow arrow uses texture56, run6, room111 in the second capture. Its four
triangles share authored Y=-25 with the asphalt in run2. It is correctly marked
ZMODE_DECAL with depth test enabled and depth writes disabled. GPU register0x107
is0xf71 (GEQUAL and color-only writes), so changing the material's enum or simply
allowing equal depth would not address the observed issue.

The four arrow triangles cover 4,837 road pixels. Their ideal depths agree with
the road plane, but the actual stored road depth is 1.4e-6–4.0e-6 nearer than the
CPU-projected arrow depth. Float24 diagnostic models reproduce the sensitivity;
they are not claimed to emulate every PICA arithmetic detail.

The RDP's decal comparison uses a depth window derived from primitive/stored dz,
with a minimum of eight expanded 18-bit depth units. GoldenEye's 511 scale and
511 translation produce a 1022-unit viewport depth span; multiplying by 256 gives
261632 expanded units. The minimum window maps to 8/261632 in normalized PICA
depth. See the primary implementation's `z_compare` and `dz_decompress` in
[angrylion-rdp-plus zbuffer.c](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/zbuffer.c).

The 3DS material adapter now applies that minimum tolerance only to depth-tested
DECAL materials with depth writes disabled, and restores zero offset on other
material states and direct draw passes. This is a conservative partial emulation:
slope-dependent dz, compressed stored-depth precision and the two-sided receiver
window remain unimplemented and the existing depth-mode fallback flag remains.
The capture log records the intended decal offset for each GPU run.


The captured-depth regression accepts 4,834 of 4,837 arrow samples with the
minimum window. Three actual foreground samples remain rejected, as do all
4,837 synthetic foreground samples placed beyond that window. This is a depth
comparison regression, not a claim of exact final PICA color output. Native and
short-enum sanitizer fragment tests cover tolerance eligibility, stored-depth
preservation, state reuse/reset and all 65,536 alpha-threshold pairs. Renderer
submission-cache tests and 512 windowed-door material publications also pass.

Focused commands:

```sh
python3 scripts/tests/test_3ds_capture_clock.py
python3 scripts/tests/test_renderer_fragment_contract.py
python3 scripts/tests/test_road_decal_capture.py
```

The capture regression requires the preserved private hardware evidence at its
default path, or the capture directory supplied as its first argument.


The real-pack door regression passes native and short-enum ASan/UBSan. It
constructs Dam gate267, checks exactly one registration in each authored room
111/113, performs native room lookup and polygon collision in both directions
using the witnessed truck hull, and verifies the native gate can open with that
hull stopped clear. It then checks passage through the open gate, primary-only
doors and removal from room lists on teardown. Removing the primary-registration
call makes the regression fail. This is a focused collision/gate fixture, not a
full replay of the truck AI or a physical mission playthrough.

```sh
bash scripts/test_door_room_registration.sh \
  build/sd-deployment/aim-gate-depth-20260911-180125/before/3ds/goldeneye-3ds/goldeneye.u.gepack
```

The test runner requires existing stage-assets fixture dependencies. It does not
replace or regenerate the installed asset pack. The full aggregate port suite
was not rerun for these focused changes. Hardware retesting remains necessary.

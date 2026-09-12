# Hardware retest after 25038c9d — 2026-09-11

Verified installed executable SHA-256:
`90f6bbea2080c931c441d5be84195b119088e724ab227a39b923c052c53a8d62`.
Read-only backup, matching ELF, dumps and captures:
`build/hardware-captures/watch-gate-retest-20260911-201404/`.

## New Watch failure

The new file is `luma/dumps/arm11/crash_dump_00000002.dmp`, SHA-256
`9cec03449cddcc891ba2feb4ffe984af0b069161d32071e88ab3ce1b92481a6b`.
The Watch now reaches state 5, passing the previous model-lifetime fault.
PC `0x001f459c` resolves to `call_sndGetSfxSlotFirstNaturalVolume`; LR
`0x001a06ec` is the settings read inside `deleteCurrentSelectedFolder`.
The faulting halfword load reads address zero from the uninitialized native
SFX natural-volume table. This is distinct from the previous flash-node write.

Canonical `sndNew` allocates seven natural/effective volume entries and fills
them with 0x7fff. The replacement PCM mixer does not run that allocator, while
retained Watch/settings code still uses the tables. The port now supplies owned
storage with those same defaults before entering the frontend or gameplay.
Repeated initialization retains volume changes and existing external owners.

The focused regression extracts the actual native getters and settings-save
functions, links the production table owner, and verifies all seven slots,
packed saved volumes/options, repeated initialization and existing owners.
Omitting initialization reproduces the null read. Native and short-enum layouts
pass ASan/UBSan. This validates the recorded fault path; another complete
physical Watch open/close cycle remains required.

## Gate evidence

The gate remains closed on hardware. The automatic trace confirms accepted
activation changes gate 267 to OPENING and native lastcalc ticks advance, but
collision rollback repeatedly returns displacement and speed to zero. Missing
object/character metadata counters remain zero. This is not a lost Use edge.
Tests with the actual Dam model/setup data exclude nearby panel and crate bounds
as the overlapping blocker. The truck's actual captured position, combined with
an assumed flat negative-Z heading, reproduces a 5.14-unit overlap with gate 267
and no overlap with gate 268. Actual yaw/pitch were not captured, so this is a
candidate blocker, not proof of its identity or a justification to bypass collision.
Reproduction sources, command, exact pack hash and results are preserved in
`build/host-tests/gate-collision-repro/README.md`.

A bounded observer now records the exact rejected door/blocker indices, positions,
heights, polygon vertices and cylinder radius before native rollback. It runs only
at the existing rejection exits and does not alter collision decisions. Output
uses the existing bounded post-Use trace cadence. The exactness test passes for
all 19 native collision bodies; the sanitizer fixture verifies both hull types,
copy ownership and unchanged collision inputs/counters. Another hardware Use
attempt is needed to establish which real candidate blocks this gate.

## Depth evidence

All 160 recorded draws have matching context/emitted depth state and authored
write settings. All five depth buffers closely match the software replay; this
set does not reproduce the previously observed missing initial depth writes.
The audit is in `build/host-tests/depth-evidence-20260911-retest/report.json`.
This does not imply every visible geometry defect is fixed.


## Captured vehicle geometry

The captures at ticks 22394724478 and 51759729308 contain truck wheel geometry
far from the live actor. The body can already be at the current position while
wheel matrices still describe an earlier location/view. Native offscreen vehicle
ticks do not rebuild the complete render bank; custom publication refreshed only
the root and exposed stale children. The transient bank can also belong to a
reused frame arena, so rebasing or copying the old children is unsafe.

Vehicle publication now reconstructs the root and all four wheel matrices from
current position, orientation and canonical wheel angles into storage owned by
that model instance. It uses the native render composition without ticking the
vehicle or advancing wheel angles. Initial scene installation, live articulated
refresh and visibility publication all refresh this bank before consuming it.
This addresses the evidenced vehicle path; other articulated types and the
intermittent initial depth-write problem still require separate investigation.


The stage-model-publication suite passes ASan/UBSan with native and short enums
against the exact installed asset pack. The new MILTRUCK and JEEP cases verify
root/four-wheel translation and scale, an independent roll/steering rotation
oracle, moving actor/camera, repeated publication, unchanged simulation state,
owned-bank release and an invalid previous arena pointer that must never be read.
All nine authored campaign vehicle records use these two five-matrix models.
The existing 12,000 articulated pose/static/force-copy cases also pass.

Repeat the real-model suite:

```sh
bash scripts/test_stage_model_publication.sh "$PWD" \
  build/sd-deployment/aim-gate-depth-20260911-180125/before/3ds/goldeneye-3ds/goldeneye.u.gepack
```

The short-enum runner is preserved at
`build/host-tests/depth-evidence-20260911-retest/test_vehicle_short.sh`.
The full aggregate port suite was not rerun for these focused changes.

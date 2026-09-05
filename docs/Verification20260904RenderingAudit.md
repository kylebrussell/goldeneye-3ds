# Independent rendering audit — 2026-09-04

This pass retains two bounded fixes: fractional texture alpha survives asset
conversion, and coverage-times-alpha materials reject fully transparent
fragments. It also adds material inventory and benchmark provenance checks.
It does **not** establish campaign visual parity or sustained hardware 60 FPS.

Follow-up: [sound-cache validation](Verification20260905SfxCachePerformance.md)
and [ARM11 music optimization](Verification20260905MusicSaturation.md) preserve
this exact asset pack and supersede the staged executable.

## Candidate and baseline

Baseline executable SHA-256:
`5ff43316fc50d6a5af450c82617aa0a2493c05c96540f2ded418d35b988afd3e`.
Baseline pack:
`ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`.
Both are preserved in `build/3ds-candidates/rendering-audit-before/`.

Final executable:
`8bacbec69080433735ac1c06c5f26f525af053a960c0fe6b8c94fa8779c4c1e0`.
Final pack:
`7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`.

Evidence below lives in `build/host-tests/rendering-audit/`. Private ROM-derived
assets and generated audit data stay local.

## Accepted repairs

`convert_3ds_textures.py` now selects RGBA8 for RGBA PNGs and indexed PNGs whose
tRNS palette contains fractional alpha. Gray-plus-alpha retains LA8. Binary
indexed textures retain RGBA5551. Explicit format overrides still work.

Of 4,532 source LOD images, 60 RGBA images and 43 indexed images actually use
fractional-alpha pixels. Another 253 gray-plus-alpha images already retained
their alpha through LA8. The new format policy changes 146 catalog LODs:
99 RGBA and 47 indexed images; four indexed palettes contain fractional entries
that their pixels do not use. Conversion preserves those entries conservatively.
The separate COPYICON T3X alias changes too. Pack comparison finds exactly 149
changed entries: 147 T3X entries and the two catalogs, with no added or removed
assets. The 13,062-entry pack grows by 126,592 bytes.

The calculated additional texel storage across all 146 catalog resources is
277,888 bytes, assuming power-of-two padding (minimum 8) and box mipmaps down
to a minimum dimension of 8. Loading the separate COPYICON alias as well adds
another estimated 2,688 bytes. These are aggregate allocation estimates, not
measured peak residency or a guarantee about original-3DS memory headroom.
See `asset-diff.json` and `texture-alpha.json`.

The shared material translator now rejects alpha zero when CVG_X_ALPHA is set
without explicit alpha compare. This prevents an invisible fragment from
writing color/depth in the supported endpoint case; fractional coverage remains
an approximation and receives its own fallback flag. No such modes appeared in
the raw background inventory, so this is a controlled material repair, **not**
proof that a particular in-level billboard defect is fixed.

The existing strict-greater alpha threshold is retained: it agrees with the
[original alpha-compare contract](https://ultra64.ca/files/documentation/online-manuals/man/n64man/gdp/gDPSetAlphaCompare.html).
Tests cover all 256 translated thresholds and all 65,536 alpha/threshold pairs.

## Rejected decal experiment

Strict PICA depth equality was tested as a coplanar decal approximation. A CPU
fragment model passed, but the real Facility room 66 view disproved its visual
adequacy: hazard-striped door trim appeared while most of the nearby green exit
sign disappeared. Restoring the previous depth comparison restored the sign.
The equality change was removed from the final candidate.

The [original RDP rendering modes](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro15/15-07.html)
use stored depth tolerances for decals; strict equality does not reproduce that
range. A global greater-or-equal change or guessed depth offset was not adopted.
The remaining trim/decal defect now has an authored reproducible camera:
`facility-route-098-pad-133-forward`, room 66, position
`8212.32014,-303.250779,-4931.70214`, look
`0.013898578,0,-0.99990341`. See `facility-fixed.geview` and the baseline,
rejected-equality, and corrected-candidate result files.

## Campaign material and parent-state audit

`ge_stage_stream_probe --materials` decodes every nonzero background room
independently and records room, texture, pass, depth, blend, alpha, fog and
combiner information. All 20 solo backgrounds decoded successfully:
1,178 rooms and 58,891 batches. This covers raw backgrounds, not every runtime
prop, effect, parent-list rewrite or possible camera.

| Background | Rooms | Batches | Decal batches | Translucent batches |
| --- | ---: | ---: | ---: | ---: |
| Facility | 77 | 3,988 | 176 | 76 |
| Caverns | 63 | 3,040 | 47 | 186 |
| Surface 1 | 39 | 2,281 | 104 | 413 |

All 2,156 missing-binding warnings use shade color and shade alpha combiners;
they do not sample texture data. Treating those flags as missing assets would
have misdirected the repair. See `missing-bindings.json`.

Fog is inherited: `bgfog.c:fogSetRenderFogColor` publishes geometry fog and
parameters, while `bgapply.c` applies runtime render-mode substitutions from
`bg.c`. The backend enables authored stage fog around world rendering and clears
it for first-person/HUD rendering. Raw child-list fog flags alone therefore
cannot justify disabling or adding fog. The billboard LUT also rewrites render
modes; the raw background audit does not cover all runtime applications.

One concrete remaining parent-state gap is `propobj.c:chrobjRenderProp`:
distance/regeneration fading selects a different render branch, and damage plus
shade color contributes dynamic fog-color state. The current generic model
publication helper supplies world depth and the unfaded glass branch, including
calculated glass opacity; it does not reproduce that entire dynamic prop path.
No speculative global fog adjustment was made in this pass.

## Validation boundaries

Fixed authored cameras in Facility room 66, Caverns room 59, and Surface 1 room
14 compare the old executable/pack pair against the final pair. These are port
inspections, not N64 reference captures, and screenshot timings do not synchronize
weapon/actor animation. Each final view is a finite 1,800-frame tour. The Facility
experiment above is the actionable visual finding; these views do not demonstrate
that every changed alpha texture appears on screen.

`test_renderer_fragment_contract.py` captures the real GPU-state publication
function and feeds a CPU fragment model. ASan/UBSan runs in native and short-enum
layouts verify cutout color/depth rejection, glass occlusion, blending, state
reset, and threshold boundaries. This is not a hardware pixel test. The full
regression suite passed before withdrawing the equality experiment; focused
fragment, glass and submission-cache checks passed afterward. The final ARM
build passed. Asset pipeline and timing analyzer tests passed.

`analyze_3ds_frame_timing.py --compare` now checks recorded script segments,
level, frame counts, start/end position and look, rooms, objectives, mission,
death and door state. The runtime writes parsed script segments after the probe.
The older Caverns depth-generation versus door-clipped comparison is rejected
for different final position/look and missing provenance. Matching these fields
is only a necessary check, not deterministic replay proof. See
`prior-comparison.json`; no new isolated FPS gain is claimed here.

The final candidate's 3,000-frame Caverns input probe completed successfully and
recorded all seven parsed input segments. After 120 warmup frames, work time was
8.8538 ms mean, 14.7558 ms p95, 16.7673 ms p99 and 20.9259 ms peak; 31/2,880
samples exceeded 16.667 ms. Its 2,879 recorded submission intervals had zero
repeated or skipped VBlank counters. Eight frames reported multiple retraces.
The final position was `5353.534180,-2665.283691,-764.409119`, which differs from
the previous door-clipping probe; this is a new bounded measurement, not a clean
optimization comparison. See `caverns-candidate3000-timing.json`.

The final pair is saved in `build/3ds-candidates/rendering-audit-8bacbec6/`,
staged under `build/3ds-sd/3ds/goldeneye-3ds/`, and installed in the local Azahar
SD tree with SHA-256 agreement verified. Temporary stage/input/tour overrides
were removed and the pre-existing configuration restored. No physical console
deployment was performed.

Remaining acceptance work includes faithful decal depth tolerance, dynamic prop
parent state, effects/coverage and wider camera coverage, exact N64 menu/scene
references, complete mission/objective playthroughs, and measured New/old 3DS
frame pacing. Emulator status-bar FPS is not hardware performance evidence.

Normal boot was also inspected through the animated cast sequence, then stopped.
The task-scoped caffeinate assertion kept the Mac awake during validation and
was released after completion; persistent power/security preferences were not changed.

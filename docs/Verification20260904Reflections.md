# Original glass reflection state and fractional-alpha assets

Continues `7c08e199` without changing canonical gameplay or ROM-authored data.
ROMs, extracted assets, private replays and packaged binaries remain untracked.

## Rendering repair

- Export the 32-byte LookAt payload allocated by the unchanged
  `bondviewUpdateCameraMatrices` / `guLookAtReflect` path. Never dereference
  its legacy signed-32-bit pointer on the 64-bit host.
- Retain the authored G_TEXTURE_GEN / G_TEXTURE_GEN_LINEAR state in model
  materials. For static ordinary/tinted glass, use original GlobalLight,
  original camera LookAt and the same object-to-eye matrix composition as
  the non-door `objTick` branch, with s15.16 normal-matrix quantization.
- Reuse the portable GBI vertex shader for normal, shade and generated ST;
  don't rebuild display-list topology, allocate geometry or rewrite XYZ.
  Update retained overlay and combined-scene attributes together. Unchanged
  attributes require no GPU publication. Only original portal-visible rooms
  need this camera-dependent work; residency alone is not visibility.
- Preserve generated coordinates through Tex3DS atlas mapping instead of
  overwriting them with raw zero ST. Use the authored texture scale, not a
  hard-coded whole-image scale. The N64 scale convention is documented in
  [Nintendo's reflection mapping manual](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro11/11-07.html):
  the generated unit interval spans `gSPTexture scale / 64` texels. PwindowZ
  supplies `0x0d80`, which covers its original 54-by-54 GLASS3 texture.

## Asset repair

The original GLASS3 PNG is gray+alpha, with luminance 0–112 and alpha exactly
96/255 throughout. The previous RGBA5551 conversion lost that fractional
alpha, making the pane invisible even after correct reflection UVs reached
the GPU. The converter's default `auto` policy now selects LA8 for original
PNG gray+alpha images and retains RGBA5551 for other images. Explicit format
overrides still work. LA8 preserves both eight-bit channels at the same
16-bit-per-texel footprint; this is not a wholesale RGBA8 memory increase.

The rebuilt 13,062-entry pack changes only 375 grayscale-alpha T3X resources
and the two texture catalogs. All other packed entries are byte-identical to
the previous pack. The remaining 4,157 converted LOD images keep RGBA5551.
Catalogs record each LOD's chosen format. These are authored asset conversion
changes, not replacement textures or guessed opacity values.

## Verification

- ASan/UBSan native model publication: original window geometry/material,
  six opacity values, 24 original LookAt directions, rotated/scaled object
  matrix, full-process versus retained-attribute equality, and unchanged
  position/projection/topology data. `build/host-tests/reflection-publication-final.log`.
- Original camera tests check owned payload lifetime, signed fractional
  endpoints, translation invariance and rotation-dependent axes.
- Texture tests check authored generated-ST scale, padded/inverted atlas
  mapping, raw-ST preservation, deterministic format selection and explicit
  format override. Existing exhaustive raw-UV comparisons still pass.
- Actual GPU upload function is exercised under ASan/UBSan: shade-only mode
  does not write XYZ, chooses generated rather than raw UVs, and marks only
  the requested vertex range dirty. The visibility gate precedes shading.
- Full host suite before the visible-room optimization passed in
  `build/host-tests/reflection-final.log`; final verification is recorded below.
- Full asset regeneration and 95 required-resource checks passed in
  `build/host-tests/reflection-assets.log`.

Azahar comparisons used existing authored route viewpoints in Dam room 111:
pad 133 / route 61 and pad 125 / route 60. With the old asset pack the panes
were invisible; with LA8 and the new renderer they are visibly translucent,
with scenery still visible through them. Diagnostic records confirm glass
attribute publication and successful camera/room installation. This is a
visual check of the native port, not an N64 pixel-difference certification.

## Remaining limits

This pass is limited to ordinary/tinted static glass. The separate live
windowed-door overlay still needs its inherited material/LookAt binding.
Full canonical parent fog/damage/fade state, exact RDP mip/detail blending,
and distant-object/room visibility remain separate fidelity gaps. Grayscale
alpha assets outside the two tower views were converted and packaged, not
individually visually certified. Sustained combat performance, hardware
60-FPS behavior and end-to-end mission completion are not established by
this focused repair or the short replay.

## Final replay and artifacts

Same 750-frame movement/look/fire replay, no concurrent builds:

| Build | Total profiled frame ms | Post-warm-up peak | Frames over 16 ms |
| --- | ---: | ---: | ---: |
| Prior `67c951eb` | 7,506 | 18 ms | 7 / 630 |
| Reflection, resident-only `31489f46` | 7,923 | 19 ms | 10 / 630 |
| Reflection, portal-visible `ade136f1` | 7,641 | 19 ms | 8 / 630 |

The visible-room gate reduces glass work from 7,500 batches / 45,000 vertices
to 2,570 / 15,420, and changed GPU ranges from 3,661 to 1,229. It recovers
282 ms across the replay relative to the unfiltered reflection candidate.
Compared with the prior no-reflection build, the measured residual cost is
135 ms across 750 frames (about 0.18 ms/frame). These are single-run
comparisons, not statistical proof of a frame-tail improvement. Warm-up peak
remains 59 ms. No post-warm-up frame exceeds 25 ms in this short route.

All three runs end at exactly
`(19900.337891,-39.704582,17499.558594)`, with 750 original MoveBond and actor
ticks, identical draw counts (`36649,207924,32396,130151`), and 72 decoded
sounds with zero decode failures. This replay stays in the opening room; it
does not exercise mission completion or sustained close combat. Private
evidence: `build/visual-probe/reflection-{3148,ade1}-move750.result` and the
prior `glass-67c9-move750.result`.

Final full host suite: `build/host-tests/reflection-visible-final.log`, exit 0.
Final ARM/3DSX: `build/host-tests/arm-reflection-final.log`, success.

- Executable SHA256:
  `ade136f124c47c7f62fd8de245bdf85440d18d47237a5b2246253d3883b3c83b`
- Asset-pack SHA256:
  `ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`

Both files are required to reproduce the visual repair; the previous pack
still erases glass alpha even with the new executable.

Build output, private candidate `build/3ds-candidates/reflection-ade136f1`,
hardware staging, and installed Azahar executable/assets match these hashes.
The preceding exact `67c951eb` / `938536d4` pair remains in its private
candidate directory. Saves and hardware staging's existing stage selector
were not changed.

macOS locked after the optimized replay and before the additional final
tower recheck. The two tower screenshots verified `31489f46` with the final
asset pack; `ade136f1` adds only the portal-visible-room work gate and
profiling/test comments. Its replay exercises live glass shading, but its
tower recheck remains pending. Installed test input, stage and visual-tour
overrides were moved back to private evidence storage. The emulator was
idle after the completed replay; next launch uses normal startup. No claim
is made that a normal-menu session was launched after the lock.

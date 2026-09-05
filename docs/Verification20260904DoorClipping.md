# Shared door vertex publication — 2026-09-04

This follows [door depth and generation polling](Verification20260904DoorOcclusion.md).
The campaign door path now consumes the original runtime's clipped vertices,
including the authored texture coordinates, instead of always displaying the
unmodified ROM vertex array.

## Implementation

The adapter binds only the exact `RootNode->Child->Child` DLCOLLISION node
modified by the original door runtime when `DOORFLAG_CLIP_TO_BBOX` is set.
Unrelated model parts retain their original vertices. Before the first clipped
publication, the same binding uses the native original array. It validates
vertex count and native stride, and converts native fields explicitly; host
pointer alignment makes native `Vertex` storage larger than its N64 encoding.

The model cache retains the original display lists, matrix indices, and an
owned source-address map. Current vertex content is part of the publication
signature, not a new cached topology per animation frame. A runtime array is
borrowed only during the build. XYZ and ST are republished using the original
segment-4 vertex addresses; position-only duplicate reuse is disabled for these
inputs because identical original positions can clip differently. Flag and
color/normal changes are rejected by this restricted clipping adapter. Generated
texture coordinates retain their separate shading path.

Dynamic UV changes mark output ranges as requiring static-attribute upload.
Door GPU updates now carry those ranges through the same UV-remapping mechanism
used for changed guard geometry, instead of uploading only positions. Unchanged
doors retain their existing generation gate and unchanged inputs retain their
published output. Failed cache builds invalidate publication reuse.

## Validation

ASan/UBSan fixtures exercise actual models 144, 176 and 178. Eight changing
vertex arrays per model match a fresh canonical display-list decode for source
vertices, world positions and texture coordinates. Tests include originally
coincident vertices with different deformations, changing ST with unchanged
matrices and array address, unchanged-frame reuse, removing/rebinding the array,
invalid counts, and bounded topology growth. Existing glass and inherited-depth
fixtures pass. The full `scripts/test_port.sh` suite and ARM build pass; logs are
under `build/host-tests/door-clipping/`.

The live Caverns 2,250-frame controller trace completes with four leaf opens,
two closes, all 528 movement collision checks clear, and no door/guard/monitor
overlay-refresh failures. Both elevator leaves finish at authored maximum
`0.949997`. The open view shows the changed clipped edges/UVs and correctly
reveals the guards. Some doorway edge/seam artifacts remain visible; this is
not a complete visual match to the original.

That longer trace averages 12.9428 ms CPU work after warmup, with 425/2,130
samples over 16.67 ms, a 29.1292 ms peak and 17 skipped submission intervals.
Its combat/movement endpoint differs from the prior run, so the lower average
must not be treated as an isolated optimization gain. The run is not a 60 FPS
lock or physical New 3DS/original 3DS validation.

Private evidence: `build/visual-probe/caverns-door-clipped2250.result` and
`build/host-tests/door-clipping/after-opening.json`.

The standard 3,000-frame closed-door controller script also completed. Its warm
CPU mean was 7.9055 ms, p95 12.5590 ms, p99 14.9748 ms and peak 16.8577 ms;
1/2,880 CPU samples exceeded 16.67 ms. All 2,879 adjacent warm submissions were
one VBlank apart, with no repeats or skips. Seven dispatches observed multiple
elapsed retraces. The endpoint was `5353.534180,-2665.283691,-814.765320`, versus
`5353.534180,-2665.283691,-764.409119` in the prior trace. Although the controller
script matches, this is not an identical gameplay trajectory and does not
establish an isolated CPU speedup. No builds or host tests ran during either
live measurement. Evidence: `caverns-door-clipped3000.result` and
`after-closed.json` in the corresponding private evidence directories.

Final executable SHA-256:
`5ff43316fc50d6a5af450c82617aa0a2493c05c96540f2ded418d35b988afd3e`.

The candidate is installed in Azahar, staged under `build/3ds-sd/3ds/goldeneye-3ds/`, and preserved under `build/3ds-candidates/door-clipping-5ff43316/`. Staged executable, metadata and asset-pack hashes pass `deploy_3ds.sh --skip-build`. Temporary stage/input settings were restored. This is local SD staging, not deployment to physical hardware.

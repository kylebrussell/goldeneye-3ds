# Authored model environment alpha — 2026-09-10

Two model alpha expressions now survive the GBI-to-PICA translation. This is
a source-command repair, not a matched N64 screenshot result or hardware pixel
comparison. The original `modelApplyRenderModeType3` and
`modelApplyRenderModeType4` bodies in `src/game/model.c` establish both cases.

| Original branch | Authored alpha | Previous translation | New PICA operation |
| --- | --- | --- | --- |
| Faded weapon, primary, non-inverted vertex alpha | `(1 * ENVIRONMENT) * SHADE` | `TEXTURE0 * SHADE` | `PRIMARY * CONSTANT` |
| Textured edge, primary, nonzero environment alpha | `clamp(1 * SHADE + ENVIRONMENT)` followed by `COMBINED` | `TEXTURE0` | `clamp(PRIMARY + CONSTANT)` |

For example, a fade alpha of 64, shade alpha of 128, and texture alpha of 255
previously produced approximately 128 instead of 32. An edge with shade alpha
64 and environment alpha 32 should produce 96 regardless of texture alpha;
the previous mapping instead used the texture alpha. These are expressions
over normalized alpha; actual RDP/PICA intermediate rounding is not claimed
identical.

`GePicaAlphaMode` now distinguishes the two nontexture expressions. The
translator folds the first fade cycle's multiplication by one, and the
platform maps the resulting expression into the existing single TEV stage.
Explicit texture-use checks preserve these expressions even when texturing is
disabled. No additional GPU stage, shader uniform, material-struct field,
runtime cache key, camera state, or gameplay state is required.

This fixes translation when these authored commands reach the renderer. It
does not add missing dynamic parent setup to every live world prop. The
generic world model publisher still implements the documented unfaded glass
parent branch, so a complete in-game distance/regeneration fade requires that
separate canonical parent-state work as well.

## Verification

`python3 scripts/tests/test_model_environment_alpha.py` extracts and compiles
both original model command producers and the actual Citro3D material
publication helpers. It emits original SDK GBI words, decodes them through the
real decoder/state interpreter, translates them, and captures the resulting
TEV operation, inputs and constant alpha. The integer arithmetic oracle checks:

- Every authored fading opacity (0–254) against every shade byte (0–255), for
  both model types, with texture alpha zero and 255.
- Every authored edge opacity (1–255) against every shade byte, for both model
  types, including saturated sums and the same texture-alpha endpoints.
- Fade translation with texture sampling disabled, and restoration of ordinary
  texture-times-shade alpha after an additive edge material.

Both native and `-fshort-enums` variants pass with ASan/UBSan. The same harness
linked against checkpoint `d0c3e8d2`'s material translator aborts at the first
authored fade assertion, demonstrating that the regression detects the prior
defect. It does not merely compare the new implementation against itself.

The existing material/apply tests, glass GPU alpha test, fragment-contract
test, and renderer submission-cache test also pass. The three changed runtime
C files compile individually with `-Wall -Wextra -Werror` against the installed
ARM/libctru/Citro3D SDK in image
`sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5`.
The producer test is included in `scripts/test_port.sh`; full integration and
final executable verification belong to the coordinator's combined build.

## Remaining fidelity limits from this audit

1. **Decal tolerance:** PICA's current greater-depth comparison does not model
   stored RDP depth tolerances. Facility room 66's exit sign disproved strict
   equality in the earlier audit. That [reproducible camera and rejected
   experiment](Verification20260904RenderingAudit.md) remain the concrete
   acceptance case; no guessed global depth bias is introduced here.
2. **Other model expressions:** Inverted fading alpha, secondary
   texture-times-environment-times-shade alpha, the textured-edge secondary
   expression, and nonblack environment color interpolation still exceed the
   present single-stage material mappings. General two-cycle approximation
   flags remain; this patch does not clear them.
3. **Dynamic parent state:** Complete distance/regeneration fading and dynamic
   damage/shade fog color are not supplied for every live world prop.
4. **RDP raster effects:** Fractional coverage, antialiasing/coverage history,
   dither, primitive-depth source, decal/interpenetrating depth behavior and
   exact RDP blender arithmetic remain approximations. Transparent-alpha
   rejection covers only a verified coverage endpoint.
5. **Sampling:** Converted texture alpha is retained by the earlier asset
   repair, but N64 three-point filtering, authored multi-texture LOD/detail,
   and intermediate precision are not established as identical to PICA.
6. **Acceptance:** This subtask performs no shared-emulator run, N64 reference
   capture, physical-device deployment, visual-quality judgment or GPU timing
   experiment. Matched scene evidence and physical stereo/GPU acceptance must
   be recorded separately.

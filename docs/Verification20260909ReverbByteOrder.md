# Shared music reverb screech: coefficient byte order

The reported high-pitched, repeating noise across both frontend menus and
gameplay exposed a producer boundary that prior DSP golden-vector and
optimization comparisons did not cover.

`src/libultrare/audio/drvrNew.c:init_lpfilter` computes the reverb low-pass
coefficients as CPU `s16` values. `reverb.c:_filterBuffer` passes their address
to the original `A_LOADADPCM` command. The port's ABI interpreter correctly
reads that command's coefficient payload as big-endian, like the original RSP.
Authored instrument ADPCM books and loop histories were already preserved in
big-endian form by the music bank loader. The **CPU-generated low-pass table
was not**. On the little-endian 3DS, its coefficients were byte-swapped when
the interpreter read them.

For the final custom GoldenEye reverb section (`fc=0x4500`), the intended first
feedback coefficient is `8832`, encoded as bytes `22 80`. The old native
producer stored `80 22`, which the interpreter read as **-32734**. The intended
stable decay therefore became oscillating feedback.

## Fix

After the original coefficient calculations, `init_lpfilter` now explicitly
encodes the generated table into big-endian bytes when
`GE_PORT_LIBAUDIO_NATIVE` is defined. The CPU-only cutoff and gain stay native
numeric values. The table has no subsequent CPU numeric readers; only the
RSP/ABI command consumes it. The original N64 preprocessor branch and original
calculation order remain unchanged. The repair does not mute music, remove
reverb, lower volume, add a filter to the final audio, or change NDSP/emulator
settings.

## Reproduction and verification

Run `bash scripts/test_original_reverb_filter.sh`. This compiles the actual
canonical `init_lpfilter` and `_filterBuffer`, then executes their commands
through the existing ABI interpreter under ASan and UBSan. It checks:

- Independent expected Q14 coefficient vectors for all five custom music
  cutoff values and the original default `0x5000`.
- Both positive and negative impulses through each filter for 22,056 samples.
  After the first 256 samples, the corrected output is at most one PCM unit
  (negative fixed-point rounding can leave a one-unit DC remainder).
- A separate compilation without `GE_PORT_LIBAUDIO_NATIVE` against the same
  numeric coefficient vectors, confirming the original N64 branch still
  produces its unchanged numeric table.

Optional arguments write mono signed 16-bit little-endian PCM for the corrected
filter and the previous byte-swapped producer behavior:

```sh
bash scripts/test_original_reverb_filter.sh correct.s16le swapped.s16le
```

For the `0x4500` filter and a single input impulse of 16,000, corrected output
begins `7375, 3975, 2142, 1154, 622, 334, 180, 96, 51, 27, 15, 8, 4, 2, 1, 0`
and remains zero. The prior encoding begins
`7375, -14736, -11862, 695, 11871, -2651, -12676, -4610` and grows to full-scale
saturation. All 21,800 samples after the first 256 remain nonzero, with peak
magnitude 32,768. Thus the defect demonstrably creates sustained sound from a
single transient instead of letting it decay. It occurs in the shared music
effect path used by frontend and gameplay.

A ten-second extension of the same producer/ABI experiment isolates a sustained
**5,513 Hz** peak in the prior encoding, with tail RMS 27,239.9 and 137,795
clipped samples out of 220,504. The corrected stream has no clipped samples
and an exactly zero tail. The raw PCM, WAVs, quieter audition copies, harness
snapshot and [numerical measurements](../build/audio-screech-20260910/metrics.json)
are preserved under `build/audio-screech-20260910/`. These are isolated filter
outputs, not a capture of the complete game's speaker output.

The original producer/ADPCM/resample/envelope/PCM-ring regression also passes.
Earlier before/after optimization PCM hashes only established consistency with
the same defective baseline; they did not verify this generated coefficient
boundary against the original byte-order contract.

This is a reproduced synthesis defect consistent with the user's report.
The impulse test does not itself constitute subjective confirmation of the
complete game audio, physical-console validation, or exhaustive parity for
every music sequence. Confirm the rebuilt application in both menus and
gameplay and obtain the user's listening confirmation.

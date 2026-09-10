# Complete native music path: sequence and state verification

The native music path now has offline execution coverage from authored CSeq
events through the original sequence player, instrument relocation, voice
producer, six-section custom reverb, ABI interpreter and stereo PCM ring.
This exposed and repaired two additional native boundaries. The verified
[reverb coefficient endian repair](Verification20260909ReverbByteOrder.md)
is retained and now also has a real-music reproduction.

## Repairs

`src/libultra/audio/load.c` used N64 `K0_TO_PHYS` masking on relocated ADPCM
books, decoder states and loop histories. The native interpreter explicitly
uses direct addresses. On a 32-bit ARM process the first real sequence render
therefore attempted to read `0x1c201298` after the producer stripped bits from
valid pointer `0x3c201298`. The native branch now calls `osVirtualToPhysical`,
like the other native audio producers. The original N64 masking branch stays
unchanged. This repairs the address contract; it is not evidence that current
3DS heap addresses caused the user's screech.

`src/libultrare/audio/env.c:_getVol` shifted negative signed envelope slopes
left by 16 bits. Real stage music exposed this as UBSan failures. The native
branch now multiplies the signed 16-bit value by 65,536: every result fits in
signed 32 bits, and the original float conversion and calculation order stay
unchanged. The N64 branch remains unchanged.

Focused host regressions cover full-width book/state/loop command addresses
(including repeated ADPCM loop commands), the unchanged N64 address branch,
and 2,359,296 envelope slope/fraction/sample combinations under ASan/UBSan.
Both changed producer files also compile with actual devkitARM and the 3DS
ARMv6K/VFP hard-float flags, warnings treated as errors.

## Boundary audit

| Payload | CPU representation | Interpreter representation / ownership |
| --- | --- | --- |
| Instrument ADPCM book | Native order/count | Authored coefficient bytes remain big-endian |
| ADPCM loop | Native start/end/count | Authored 32-byte history remains big-endian |
| Raw sample data | Byte storage | Authored sample bytes remain big-endian |
| Generated reverb filter | Native cutoff/gain | Generated coefficient table explicitly encoded big-endian |
| ADPCM, resample, pole and envelope saved state | CPU allocates storage; init flags establish state | Interpreter reads/writes big-endian state; the CPU does not reinterpret it numerically |
| CSeq | Explicit big-endian header/timing reads | Loop counters are individual bytes; replacement data is owned until synth close |
| ABI command pointers | Full native 32-bit addresses | Direct-address resolver; no native N64 segment masking |
| Final PCM | Interpreter's big-endian save buffer | Explicit conversion to native interleaved signed-16 stereo before the output ring |

The pre-existing worker handoff tests pass: synth mutation and teardown drain
the pending interpreter job first. The offline fixture uses the synchronous
runtime and does not test NDSP scheduling or speaker playback.

## Authored sequence evidence

The fixture derives all **63** track entries from the original music/stage
registry. That includes every distinct campaign main, action/X, background,
frontend, watch and death track, unused entries and four silence placeholders.
Each track renders 300 blocks of 736 stereo frames, then all three players
receive Stop and another 300 blocks render. Each segment is 10.0136 seconds
at 22,050 Hz. This is prefix coverage of every track, not every event or a
complete loop of every composition.

Additional fixtures recreate the runtime and bank for Folder and Dam repeats,
then execute and repeat an explicit 35-second schedule: logo/eye/folder/watch
changes, Dam plus X and wind layers, volume zero/restoration, layer stops,
same-track restart, Caverns, death and return to folders. These are executable
audio API routes, not captured gameplay or campaign completion routes.

| Completed evidence | Result |
| --- | --- |
| Cases | 67: 63 tracks, 2 fresh-runtime repeats, 2 transition schedules |
| Rendered output | 41,700 blocks; 30,691,200 stereo frames |
| Executed producer commands | 29,763,220 |
| ABI, ASan and UBSan errors | 0; UBSan configured to halt on error |
| Clipped output samples | 0; maximum absolute PCM value 32,587 |
| Final stopped-window peak | At most 13 PCM units |
| Silence placeholders | `Mno_music`, `Mnull1`, `Mnull2`, `Mnull3`: exactly zero |
| Ring overflows / underflows | 0 / 0 |
| Fresh runtime and transition replay PCM | Byte-identical |

The stop gate requires final-window peak at most 64, catching the demonstrated
large sustained oscillation while allowing the original multi-section reverb's
small fixed-point remainder. It does not mute, reduce volume, or filter output.
LeakSanitizer is disabled because QEMU user emulation cannot provide its ptrace
requirements; AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.
This is offline ARM32 Linux synthesis using GCC 12.2.0 and QEMU 7.2.22, with no
audio device and no wall-clock performance claims.

Private evidence is in this task worktree's
`build/audio-sequences-20260910/full/`. `report.json` records source/input/binary
hashes and every result; `stage-music-registry.inc` records the canonical table;
each case has stereo 22,050 Hz `.s16le`, stdout and stderr files. The completed
test executable SHA-256 is
`5c823ae9ec079b68f024970f338a3251ac8e64d8489c1272a034646f9e99ad00`;
the music-only pack SHA-256 is
`b69aeeba592a094bf92a1d8734116795a19a3fa86a2f90eb0aeab58ed9734d69`.
These private artifacts are not committed.

The separate original SFX regression also passes at `-O2` and `-O3`: all
261 authored sounds, 2,124,208 PCM samples, match the previous wide decoder
exactly (`c7efc44fa56614af`), alongside 6,000 synthetic decoder cases. The
canonical SFX owner lifecycle/sinkless clock, music bank relocation, CSeq header,
worker handoff, frontend switching, original producer and reverb tests pass.

## Real-music screech reproduction

A private diagnostic build disables only the generated reverb coefficient
encoding, preserving the rest of the corrected complete native music path.
It plays the real Folder CSeq for 300 blocks and renders 300 blocks after Stop.

| Folder sequence after Stop | Old coefficient encoding | Correct encoding |
| --- | ---: | ---: |
| Final second RMS | 7,701.30 | 11.00 |
| Final second peak | 20,314 | 11 |
| Final second AC RMS after removing DC | 7,701.29 | 0.00 |
| Numerical stop gate | Fails | Passes |

The corrected final remainder is a constant -11 in both channels. The complete
old reverb's spectrum differs from the single-filter impulse experiment; do not
substitute its earlier 5.5 kHz peak for this full-synth capture. The earlier
impulse and this real-sequence experiment independently demonstrate sustained
sound from the defective filter after input stops.

`build/audio-sequences-20260910/real-sequence-reverb-comparison.json` contains
the measurements and PCM hashes. `fixed-folder-sequence.wav` and
`old_reverb-folder-sequence.wav` contain the complete stereo offline outputs.
No listening or emulator settings were changed, and these files were not played
as part of the automated verification.

## Reproduction

Place read-only copies of the privately extracted music `.bin` files and
`instruments.ctl` / `instruments.tbl` under
`build/audio-sequence-inputs/music/`. Build the isolated ARM32 test environment:

```sh
docker build -f scripts/audio-sequence-tests.Dockerfile -t goldeneye-audio-tests .
docker run --rm --network none -v "$PWD:/work" -w /work goldeneye-audio-tests \
  bash -c 'GE_MUSIC_TEST_BUILD=build/audio-sequence-tests/objects \
    bash scripts/test_original_music_sequences.sh && \
    python3 scripts/render_original_music_sequences.py \
      --assets build/audio-sequence-inputs/music \
      --binary build/audio-sequence-tests/objects/test_original_music_sequences \
      --output build/audio-sequence-tests/evidence \
      --runner "qemu-arm -L /usr/arm-linux-gnueabihf"'
bash scripts/test_original_audio_loader.sh
python3 scripts/tests/test_original_audio_volume.py
```

The script's default runs every authored track; `--track Mfolders` limits the
track sweep but still runs the teardown/replay and transition fixtures. QEMU
guest core dumps are disabled, and each fixture has a bounded timeout. The
existing bank, CSeq, SFX exact and SFX lifecycle scripts also accept
`GE_AUDIO_TEST_ASSETS` for private inputs kept in an isolated build directory.

## Remaining acceptance

Listen to a rebuilt application's actual output through startup, folders,
watch, mission main/X/background changes, pause, death/results, exit/restart,
busy SFX mixes and shutdown. Check uninterrupted audible playback, channel
balance, clipping, transitions and the user's repeating screech report. Repeat
on New Nintendo 3DS XL, including sleep/resume and normal volume settings.
Longer track loops, every later sequence event, final mixed NDSP output,
subjective fidelity and physical listening remain unverified. Synthetic and
offline PCM tests cannot close those acceptance checks.

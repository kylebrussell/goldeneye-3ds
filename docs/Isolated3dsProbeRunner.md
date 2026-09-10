# Isolated Azahar probe runner

`scripts/run_isolated_3ds_probe.py` creates a new private XDG profile, cache,
NAND and SD installation. It refuses an existing output directory or a running
Azahar/Citra process and terminates only the process it launches. Scheduling
remains the integration coordinator's responsibility; do not overlap timing
runs with builds or other benchmarks.

## Exact benchmark requests

Use a request prepared by `scripts/prepare_3ds_benchmark.py`:

```sh
python3 scripts/run_isolated_3ds_probe.py \
  --emulator /path/to/azahar --profile /path/to/qt-config.ini \
  --binary /path/to/goldeneye-3ds.3dsx --pack /path/to/goldeneye.u.gepack \
  --request /path/to/prepared/request.json \
  --max-stereo --timeout 900 --output /path/to/new-private-run
```

`--request` is mutually exclusive with `--probe`, `--stage` and `--setting`.
The request must identify an emulator run. Its sibling `configs` directory
must contain exactly the hashed inventory, with no nested entries, symlinks
or unknown extra files. Allowed names are `stage.cfg`, `probe-detail.cfg`,
`world-profile.cfg`, `stereo.cfg`, `controls.cfg`, `dam-input-probe.cfg`, optional
`probe-retraces.cfg` and optional `probe-difficulty.cfg`.

The runner validates binary, pack, input, config and manifest hashes before
launch. Declared `converted/` sidecars are copied from the request's asset
directory after validating their hashes and rejecting path traversal/symlinks.
No mutable source build directory is aliased into the private installation.

The run directory retains the exact `request.json`, a `before.json` inventory
from `prepare_3ds_benchmark.snapshot` before launch, and an `after.json` from
the same function after a completed result. A failed/missing result does not
receive a completed after snapshot. Post-run inventory mismatch fails the run
and records `inventory_error`, retaining the result for diagnosis. The result
and snapshots can be passed to the preparation tool's `collect` command.

## Existing probe mode and difficulty

Direct `--probe` invocation remains available for correctness captures without
creating a benchmark request. `--stage dam` selects the stage for a gameplay
`GE_INPUT_PROBE`. Explicit settings may be `stereo.cfg`, `controls.cfg`, and
`probe-difficulty.cfg`; the latter is accepted only for gameplay probes and
must contain exactly one ASCII digit from 0 through 3 followed by a newline.
It cannot be inherited into a frontend-only probe. Every explicit setting's
hash is recorded in `run.json`.

## Explicit DSP environment inputs

`--dsp-component /path/to/dspfirm.cdc` copies explicitly supplied bytes into
the private SD root at `3ds/dspfirm.cdc`. It accepts a regular non-symlink file
from 1 byte through 1 MiB; this is a storage bound and identity check, not a
DSP-format, authenticity or hardware-compatibility certification. The source
is read once with a bounded read and stable-file checks. Staging uses those
captured bytes, so later source changes cannot change the staged receipt.
No component is downloaded or obtained from shared storage implicitly.

`--azahar-hle-dsp-marker` is a separate emulator-only option, mutually
exclusive with `--dsp-component`. It creates a private zero-byte marker only
when the prepared profile explicitly contains `[Audio] audio_emulation=0`.
Missing settings, LLE values and conflicting legacy LLE flags are rejected;
the runner never changes audio mode. Azahar 2126.0's
[HLE LoadComponent](https://github.com/azahar-emu/azahar/blob/2126.0/src/audio_core/hle/hle.cpp)
ignores DSP program bytes, while
[its Qt audio configuration](https://github.com/azahar-emu/azahar/blob/2126.0/src/citra_qt/configuration/config.cpp)
reads `audio_emulation`. This marker is explicitly labeled `not_firmware` and
must never be included in a hardware candidate or represented as a supplied
DSP component. It does not prove successful NDSP initialization or audible
output; inspect the actual run telemetry and output separately.

Both options record source/staged hashes as applicable under
`run.json`'s `environment_components`, separately from the game's asset and
config inventories. `environment-before.json` and `environment-after.json`
bind that identity to the request (when present), executable and respective
profile hash. A changed or missing staged component fails the run. Empty
files are always rejected by `--dsp-component`; marker creation requires the
explicit HLE option. The candidate packager has
[separate optional support for supplied nonempty components](PrivateCandidateDspPrerequisite.md);
it never accepts or bundles HLE markers.

## Profile provenance and stereo

The runner preserves caller-selected audio/emulation settings, removes shared
UI/network state, and normalizes both Data Storage group spellings into the
single QSettings `[Data%20Storage]` group with private SD/NAND paths. Other
absolute path overrides are rejected. Original profile bytes are not changed.

Without `--max-stereo`, stereo settings are retained. With the flag, only the
private profile receives `[Layout] factor_3d=100`, `render_3d=1` and
`[Renderer] disable_right_eye_render=false`, each with its default override
disabled. Azahar 2126.0 reads the first two keys in
[its Layout group](https://github.com/azahar-emu/azahar/blob/2126.0/src/citra_qt/configuration/config.cpp),
and defines `SideBySide` as value 1 in
[StereoRenderOption](https://github.com/azahar-emu/azahar/blob/2126.0/src/common/settings.h).
The source profile and prepared/final profile hashes are recorded in
`run.json`; the actual prepared/final bytes are retained as
`profile-before.ini` and `profile-after.ini`.

Tests use a mocked owned process; they do not launch an emulator:

```sh
python3 -m unittest scripts/tests/test_isolated_3ds_probe.py \
  scripts/tests/test_3ds_benchmark_preparation.py
```

Request inventories and profile settings do not establish audible output,
physical hardware performance or stereo quality. Those observations remain
separate acceptance evidence.

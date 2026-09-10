# Private candidate DSP prerequisite

The candidate packager accepts optional `--dsp-component /path/to/dspfirm.cdc`
on the staging command documented in
[the integration acceptance guide](IntegrationAcceptance20260910.md). This
copies an explicitly supplied component to the candidate's
`sd/3ds/dspfirm.cdc`, outside the game asset/config directory. It performs no
deployment or firmware acquisition.

Input must be a regular non-symlink file from 1 byte through 1 MiB. The
packager captures one bounded read, checks that the opened file did not change
during that read, and stages the captured bytes. Subsequent source changes do
not change the candidate or its receipt. These checks establish bounded file
storage and byte identity, not DSP-format validity, authenticity, audible
output or physical compatibility.

The manifest's `runtime_prerequisites.dsp_component` records `status`, the
fixed SD path, file size/hash and captured source hash. `supplied` means those
bytes were included. It does not mark the release acceptance row passed.
`--verify` recomputes the component identity and checks it against both that
entry and the complete package file inventory. Addition, removal, mutation,
an empty component, a symlink or contradictory metadata rejects verification.
`--dsp-component` cannot alter an existing candidate during verification.

With no component supplied, the manifest records `external-required` and
states: “External DSP component or hb:ndsp launcher support required; not
established by package.” [Libctru's component lookup](https://github.com/devkitPro/libctru/blob/master/libctru/source/ndsp/ndsp.c)
accepts the SD card or the launcher's `hb:ndsp` handle, so a bundled file is not
a universal release requirement. Physical release evidence can establish functioning sound through
either path. `completion_status` and `--require-complete` continue to depend
on all nine validated acceptance rows, including physical release evidence.
Supplying bytes cannot satisfy missing evidence or any unfinished roadmap
row. Source cleanliness, immutable toolchain, private input and build receipt
checks also remain in force; the external component is separately manifested
and does not change the builder receipt schema.

The zero-byte Azahar HLE marker is not firmware and is never accepted or
bundled by the candidate packager. Only the isolated emulator runner has an
explicit marker option. No usable nonempty component has been established in
the current integration inputs, so the default package declares the external
DSP prerequisite. Current unfinished acceptance rows keep it incomplete.

Run the host fixture tests with:

```sh
python3 -m unittest scripts/tests/test_package_3ds_candidate.py
```

They cover optional staging and offline roundtrip, source changes during
staging, component mutation/removal/addition, changes concealed by reindexing
the generic inventory, invalid marker/identity metadata, empty/oversized/
symlink/directory inputs, external/launcher support with complete acceptance,
and preservation of the physical acceptance gates. Test component bytes and
build subprocesses are fixtures; no emulator,
ARM build, real firmware download or device action is performed.

#!/usr/bin/env python3
"""Force-rebuild committed 3DS source and record a local packaging receipt."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
from package_3ds_candidate import (BINARIES, digest, file_record, files, git,
                                   immutable_toolchain, private_inputs, source_identity)


def dependency_inventory(root, inputs, output):
    """Retain compiler reports and audit repository dependencies outside Git."""
    build_dir = root / 'platform/3ds/build'
    tracked = set(git(root, 'ls-files', '-z').split('\0'))
    reports = sorted(build_dir.glob('*.d'))
    if not reports:
        raise ValueError('successful candidate build lacks compiler dependency reports')
    consumed = set()
    generated = {}
    output.mkdir()
    for report in reports:
        text = report.read_text().replace('\\\n', ' ')
        # GCC's first rule has the object dependencies; following rules are
        # empty -MP targets for headers. shlex handles escaped path spaces.
        first_rule = text.splitlines()[0].partition(':')[2]
        for name in shlex.split(first_rule):
            path = Path(name)
            if path.is_absolute():
                if path.parts[:2] != ('/', 'workspace'):
                    continue  # SDK headers are provided by the pinned image.
                path = root / path.relative_to('/workspace')
            else:
                path = build_dir / path
            path = Path(os.path.normpath(path))
            if root not in path.parents:
                raise ValueError(f'compiler dependency escapes source root: {name}')
            relative = path.relative_to(root).as_posix()
            if relative in inputs:
                consumed.add(relative)
            elif relative.startswith('platform/3ds/build/'):
                generated[relative] = file_record(path)
            elif relative not in tracked:
                raise ValueError(f'unrecorded private compiler input: {relative}')
        shutil.copyfile(report, output / report.name)
    return {'dependency_reports': files(output),
            'consumed_private_inputs': sorted(consumed),
            'generated_dependencies': generated}


def build(args):
    root = args.root.resolve()
    if not immutable_toolchain(args.toolchain) or not 1 <= args.jobs <= 32:
        raise ValueError('an immutable image digest and 1..32 build jobs are required')
    if args.output.exists():
        raise ValueError('output already exists; preserve prior receipts with a new directory')
    commit, tree = source_identity(root)
    inputs = private_inputs(root)
    args.output.mkdir(parents=True)
    command = ['bash', 'scripts/build_3ds.sh', f'-j{args.jobs}']
    log = args.output / 'build.log'
    with log.open('wb') as stream:
        result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                env=dict(os.environ, DEVKITPRO_IMAGE=args.toolchain))
    receipt = {'schema_version': 1, 'source_commit': commit, 'source_tree': tree,
               'toolchain_image': args.toolchain, 'command': command,
               'exit_code': result.returncode, 'log_sha256': digest(log), 'binaries': {},
               'private_inputs': inputs}
    if result.returncode == 0:
        try:
            if source_identity(root) != (commit, tree) or private_inputs(root) != inputs:
                raise ValueError('source or private inputs changed during build')
            receipt.update(dependency_inventory(root, inputs, args.output / 'dependencies'))
            binaries = args.output / 'binaries'
            binaries.mkdir()
            for name, magic in BINARIES.items():
                source = root / 'platform/3ds' / name
                with source.open('rb') as stream:
                    if stream.read(4) != magic:
                        raise ValueError(f'invalid executable/metadata magic: {name}')
                shutil.copyfile(source, binaries / name)
            receipt['binaries'] = {name: digest(binaries / name) for name in BINARIES}
            if source_identity(root) != (commit, tree) or private_inputs(root) != inputs:
                raise ValueError('source or private inputs changed while recording build')
        except (ValueError, OSError, subprocess.CalledProcessError) as error:
            receipt['exit_code'] = 1
            receipt['error'] = str(error)
            receipt['binaries'] = {}
    (args.output / 'build-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--toolchain', required=True)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    try:
        receipt = build(args)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'candidate build rejected: {error}\n')
    print(json.dumps(receipt))
    return receipt['exit_code']

if __name__ == '__main__':
    raise SystemExit(main())

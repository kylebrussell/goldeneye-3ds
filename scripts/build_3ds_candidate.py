#!/usr/bin/env python3
"""Force-rebuild committed 3DS source and record a local packaging receipt."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from package_3ds_candidate import BINARIES, digest, git


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--toolchain', required=True)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    root = args.root.resolve()
    if '@sha256:' not in args.toolchain or not 1 <= args.jobs <= 32:
        parser.error('an immutable image digest and 1..32 build jobs are required')
    if args.output.exists():
        parser.error('output already exists; preserve prior receipts with a new directory')
    if git(root, 'diff', '--name-only', 'HEAD'):
        parser.error('commit tracked changes before building a candidate')
    commit = git(root, 'rev-parse', 'HEAD')
    args.output.mkdir(parents=True)
    command = ['bash', 'scripts/build_3ds.sh', f'-j{args.jobs}']
    log = args.output / 'build.log'
    with log.open('wb') as stream:
        result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                env=dict(os.environ, DEVKITPRO_IMAGE=args.toolchain))
    receipt = {'schema_version': 1, 'source_commit': commit,
               'source_tree': git(root, 'rev-parse', 'HEAD^{tree}'),
               'toolchain_image': args.toolchain, 'command': command,
               'exit_code': result.returncode, 'log_sha256': digest(log), 'binaries': {}}
    if result.returncode == 0:
        if git(root, 'rev-parse', 'HEAD') != commit or git(root, 'diff', '--name-only', 'HEAD'):
            receipt['exit_code'] = 1
            receipt['error'] = 'source changed during build'
        else:
            receipt['binaries'] = {name: digest(root / 'platform/3ds' / name) for name in BINARIES}
    (args.output / 'build-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt))
    return receipt['exit_code']

if __name__ == '__main__':
    raise SystemExit(main())

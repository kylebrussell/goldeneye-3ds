#!/usr/bin/env python3
"""Stage and verify a private, traceable 3DS candidate; never deploy it."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from verify_3ds_asset_pack import pack_entries

AREAS = ('original_fidelity', 'campaign_completion', 'stereo_room_visibility',
         'sustained_60fps', 'control_ergonomics', 'touchscreen',
         'wireless_game_state', 'wireless_synchronization', 'release')
PHYSICAL = {'sustained_60fps', 'control_ergonomics', 'touchscreen',
            'wireless_synchronization', 'release'}
SETTINGS = {'controls.cfg', 'stereo.cfg'}
BINARIES = {'goldeneye-3ds.3dsx': b'3DSX', 'goldeneye-3ds.smdh': b'SMDH'}


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def files(root):
    result = {}
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError(f'symlink forbidden in candidate: {path}')
        if path.is_file():
            result[path.relative_to(root).as_posix()] = {
                'size': path.stat().st_size, 'sha256': digest(path)}
    return result


def validate_acceptance(data, evidence_root, require_complete=False):
    if data.get('schema_version') != 1 or set(data.get('areas', {})) != set(AREAS):
        raise ValueError('acceptance must cover exactly all nine roadmap areas')
    for area, row in data['areas'].items():
        if row.get('status') not in ('pending', 'partial', 'passed', 'failed'):
            raise ValueError(f'{area}: invalid status')
        if not row.get('remaining') and row['status'] != 'passed':
            raise ValueError(f'{area}: unfinished work needs an explicit remaining field')
        evidence = row.get('evidence', [])
        kinds = set()
        for item in evidence:
            rel = Path(item['artifact'])
            if rel.is_absolute() or '..' in rel.parts:
                raise ValueError(f'{area}: unsafe evidence path')
            artifact = evidence_root / rel
            if artifact.is_symlink() or not artifact.is_file() or artifact.stat().st_size == 0:
                raise ValueError(f'{area}: missing evidence: {rel}')
            if evidence_root.resolve() not in artifact.resolve().parents:
                raise ValueError(f'{area}: evidence escapes its root')
            kind = item.get('environment')
            if kind not in ('host', 'emulator', 'physical', 'reference'):
                raise ValueError(f'{area}: unknown evidence environment')
            if item.get('result') not in ('passed', 'failed', 'observation') or not item.get('scope'):
                raise ValueError(f'{area}: evidence requires a result and bounded scope')
            if kind == 'physical' and not item.get('device'):
                raise ValueError(f'{area}: physical evidence requires a device identity')
            if item['result'] == 'passed':
                kinds.add(kind)
        if row['status'] == 'passed':
            if not kinds or any(e['result'] == 'failed' for e in evidence):
                raise ValueError(f'{area}: passed area needs successful evidence without failures')
            if area in PHYSICAL and 'physical' not in kinds:
                raise ValueError(f'{area}: software evidence cannot satisfy physical acceptance')
            if area == 'original_fidelity' and 'reference' not in kinds:
                raise ValueError('original fidelity requires matched reference evidence')
        if require_complete and row['status'] != 'passed':
            raise ValueError(f'acceptance incomplete: {area}')


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def stage(args):
    root = args.root.resolve()
    if args.output.exists():
        raise ValueError('candidate output already exists; choose a new directory')
    if git(root, 'diff', '--name-only', 'HEAD'):
        raise ValueError('commit tracked changes before packaging')
    untracked = git(root, 'ls-files', '--others', '--exclude-standard').splitlines()
    if any(p != 'platform/3ds/source/main.c.orig' for p in untracked):
        raise ValueError('untracked source files must be committed or removed before packaging')
    acceptance = json.loads(args.acceptance.read_text())
    validate_acceptance(acceptance, args.evidence, args.require_complete)
    entries = pack_entries(args.pack)
    # A complete self-contained pack must carry textures and runtime content.
    for prefix in ('converted/textures/t3x/', 'converted/levels/', 'converted/runtime/'):
        if not any(name.startswith(prefix) for name in entries):
            raise ValueError(f'asset pack lacks required content family: {prefix}')
    if not args.toolchain or '@sha256:' not in args.toolchain:
        raise ValueError('record the immutable toolchain image digest')
    receipt = json.loads(args.build_receipt.read_text())
    if receipt.get('source_commit') != git(root, 'rev-parse', 'HEAD') \
            or receipt.get('toolchain_image') != args.toolchain \
            or receipt.get('exit_code') != 0:
        raise ValueError('build receipt does not match source/toolchain/success')
    for name in BINARIES:
        if receipt.get('binaries', {}).get(name) != digest(args.binaries / name):
            raise ValueError(f'build receipt binary mismatch: {name}')
    build_log = args.build_receipt.parent / 'build.log'
    if receipt.get('log_sha256') != digest(build_log):
        raise ValueError('build log does not match receipt')
    settings = args.settings
    if settings:
        if settings.is_symlink() or any(p.is_symlink() or not p.is_file() or p.name not in SETTINGS
                                      for p in settings.iterdir()):
            raise ValueError('settings directory may contain only regular controls.cfg and stereo.cfg')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.candidate-', dir=args.output.parent) as temporary:
        out = Path(temporary)
        install = out / 'sd/3ds/goldeneye-3ds'
        install.mkdir(parents=True)
        for name, magic in BINARIES.items():
            source = args.binaries / name
            with source.open('rb') as stream:
                if stream.read(4) != magic:
                    raise ValueError(f'invalid executable/metadata magic: {name}')
            shutil.copyfile(source, install / name)
        shutil.copyfile(args.pack, install / 'goldeneye.u.gepack')
        if settings:
            for source in settings.iterdir():
                shutil.copyfile(source, install / source.name)
        evidence_out = out / 'evidence'
        evidence_out.mkdir()
        shutil.copyfile(args.build_receipt, out / 'build-receipt.json')
        shutil.copyfile(build_log, out / 'build.log')
        copied = set()
        for row in acceptance['areas'].values():
            for item in row['evidence']:
                rel = item['artifact']
                if rel not in copied:
                    destination = evidence_out / rel
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(args.evidence / rel, destination)
                    copied.add(rel)
        (out / 'acceptance.json').write_text(json.dumps(acceptance, indent=2) + '\n')
        converters = [p for p in (root / 'scripts').glob('*.py')
                      if p.name.startswith(('extract_', 'convert_', 'generate_', 'stage_', 'pack_', 'verify_'))]
        converters += [root / 'scripts/build_3ds_assets.sh', root / 'Dockerfile.3ds-assets']
        manifest = {
            'schema_version': 1, 'candidate_only': True,
            'source_commit': git(root, 'rev-parse', 'HEAD'),
            'source_tree': git(root, 'rev-parse', 'HEAD^{tree}'),
            'toolchain_image': args.toolchain,
            'build_command': receipt['command'],
            'asset_source': {'pack_sha256': digest(args.pack), 'entry_count': len(entries),
                             'conversion': 'copied preconverted inputs; converter hashes identify candidate source, not proof of regeneration',
                             'candidate_converter_sha256': {p.relative_to(root).as_posix(): digest(p) for p in sorted(converters)}},
            'settings_policy': 'explicit files below; omitted settings use compiled defaults; no saves or diagnostic overrides',
            'files': files(out)}
        (out / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
        verify(out, args.require_complete)
        out.rename(args.output)
    return manifest


def verify(root, require_complete=False):
    manifest = json.loads((root / 'manifest.json').read_text())
    if manifest.get('schema_version') != 1 or manifest.get('candidate_only') is not True:
        raise ValueError('unsupported candidate manifest')
    actual = files(root)
    actual.pop('manifest.json', None)
    if actual != manifest['files']:
        raise ValueError('candidate inventory/hash mismatch')
    install = root / 'sd/3ds/goldeneye-3ds'
    allowed = set(BINARIES) | SETTINGS | {'goldeneye.u.gepack'}
    if {p.name for p in install.iterdir()} - allowed:
        raise ValueError('unexpected file in clean SD installation')
    for name, magic in BINARIES.items():
        with (install / name).open('rb') as stream:
            if stream.read(4) != magic:
                raise ValueError(f'invalid executable/metadata magic: {name}')
    pack_entries(install / 'goldeneye.u.gepack')
    validate_acceptance(json.loads((root / 'acceptance.json').read_text()),
                        root / 'evidence', require_complete)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify', type=Path)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--binaries', type=Path)
    parser.add_argument('--pack', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--acceptance', type=Path)
    parser.add_argument('--evidence', type=Path)
    parser.add_argument('--settings', type=Path)
    parser.add_argument('--toolchain')
    parser.add_argument('--build-receipt', type=Path)
    parser.add_argument('--require-complete', action='store_true')
    args = parser.parse_args()
    try:
        if args.verify:
            manifest = verify(args.verify, args.require_complete)
        else:
            for name in ('binaries', 'pack', 'output', 'acceptance', 'evidence', 'toolchain', 'build_receipt'):
                if not getattr(args, name):
                    parser.error(f'--{name.replace("_", "-")} required for staging')
            manifest = stage(args)
        print(json.dumps({'source_commit': manifest['source_commit'],
                          'candidate_only': True, 'verified': True}))
        return 0
    except (ValueError, OSError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'candidate rejected: {error}\n')

if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Stage and verify a private, traceable 3DS candidate; never deploy it."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
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
PRIVATE_ROOTS = ('assets', 'include', 'src', 'port', 'platform/3ds/source',
                 'platform/3ds/include', 'platform/3ds/data')
PRIVATE_REQUIRED = ('build/u/src/game/chraidata.o',)


def immutable_toolchain(value):
    return isinstance(value, str) and re.fullmatch(r'[^@\s]+@sha256:[0-9a-f]{64}', value) is not None


def file_record(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f'expected a regular input file: {path}')
    return {'size': path.stat().st_size, 'sha256': digest(path)}


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
            result[path.relative_to(root).as_posix()] = file_record(path)
    return result


def source_identity(root):
    if git(root, 'diff', '--name-only', 'HEAD'):
        raise ValueError('commit tracked changes before building or packaging')
    untracked = git(root, 'ls-files', '--others', '--exclude-standard', '-z').split('\0')
    if any(p and p != 'platform/3ds/source/main.c.orig' for p in untracked):
        raise ValueError('untracked source files must be committed or removed before building or packaging')
    return git(root, 'rev-parse', 'HEAD'), git(root, 'rev-parse', 'HEAD^{tree}')


def private_inputs(root):
    """Inventory private source families, excluding unrelated mutable build outputs."""
    ignored = git(root, 'ls-files', '--others', '--ignored', '--exclude-standard',
                  '-z', '--', *PRIVATE_ROOTS).split('\0')
    names = set(PRIVATE_REQUIRED)
    names.update(name for name in ignored if name and '__pycache__' not in Path(name).parts
                 and Path(name).suffix not in ('.pyc', '.pyo', '.o'))
    result = {}
    for name in sorted(names):
        path = root / name
        if root.resolve() not in path.resolve().parents:
            raise ValueError(f'private build input escapes source root: {name}')
        result[name] = file_record(path)
    return result


def validate_receipt(receipt, commit, tree, toolchain, binaries, log, dependencies):
    if receipt.get('schema_version') != 1 or receipt.get('source_commit') != commit \
            or receipt.get('source_tree') != tree or receipt.get('toolchain_image') != toolchain \
            or type(receipt.get('exit_code')) is not int or receipt['exit_code'] != 0:
        raise ValueError('build receipt does not match source/tree/toolchain/success')
    if not immutable_toolchain(toolchain) or not all(
            isinstance(value, str) and re.fullmatch(r'(?:[0-9a-f]{40}|[0-9a-f]{64})', value)
            for value in (commit, tree)):
        raise ValueError('invalid immutable source/toolchain identity')
    command = receipt.get('command')
    if not isinstance(command, list) or len(command) != 3 \
            or command[:2] != ['bash', 'scripts/build_3ds.sh'] \
            or not isinstance(command[2], str) \
            or re.fullmatch(r'-j(?:[1-9]|[12][0-9]|3[0-2])', command[2]) is None:
        raise ValueError('build receipt must identify the forced candidate build command')
    if set(receipt.get('binaries', {})) != set(BINARIES):
        raise ValueError('build receipt must identify exactly both candidate binaries')
    for name in BINARIES:
        if receipt['binaries'][name] != digest(binaries / name):
            raise ValueError(f'build receipt binary mismatch: {name}')
    if receipt.get('log_sha256') != digest(log):
        raise ValueError('build log does not match receipt')
    inputs = receipt.get('private_inputs', {})
    if not isinstance(inputs, dict) or not set(PRIVATE_REQUIRED).issubset(inputs):
        raise ValueError('build receipt lacks required private input provenance')
    for name, record in inputs.items():
        if Path(name).is_absolute() or '..' in Path(name).parts \
                or not isinstance(record, dict) or type(record.get('size')) is not int \
                or record['size'] < 0 or not isinstance(record.get('sha256'), str) \
                or re.fullmatch(r'[0-9a-f]{64}', record['sha256']) is None:
            raise ValueError('invalid private input hash record')
    if not receipt.get('dependency_reports') or files(dependencies) != receipt['dependency_reports']:
        raise ValueError('compiler dependency reports do not match receipt')


def validate_pack(path):
    entries = pack_entries(path)
    for prefix in ('converted/textures/t3x/', 'converted/levels/', 'converted/runtime/'):
        if not any(name.startswith(prefix) for name in entries):
            raise ValueError(f'asset pack lacks required content family: {prefix}')
    return entries


def validate_acceptance(data, evidence_root, require_complete=False):
    if not isinstance(data, dict) or data.get('schema_version') != 1 \
            or not isinstance(data.get('areas'), dict) or set(data['areas']) != set(AREAS):
        raise ValueError('acceptance must cover exactly all nine roadmap areas')
    for area, row in data['areas'].items():
        if not isinstance(row, dict) or row.get('status') not in ('pending', 'partial', 'passed', 'failed'):
            raise ValueError(f'{area}: invalid status')
        remaining = row.get('remaining', '')
        if not isinstance(remaining, str) or (not remaining.strip() and row['status'] != 'passed'):
            raise ValueError(f'{area}: unfinished work needs an explicit remaining field')
        if remaining.strip() and row['status'] == 'passed':
            raise ValueError(f'{area}: passed area cannot retain unfinished acceptance')
        evidence = row.get('evidence', [])
        if not isinstance(evidence, list):
            raise ValueError(f'{area}: evidence must be a list')
        kinds = set()
        for item in evidence:
            if not isinstance(item, dict) or not isinstance(item.get('artifact'), str):
                raise ValueError(f'{area}: evidence requires an artifact path')
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
            if item.get('result') not in ('passed', 'failed', 'observation') \
                    or not isinstance(item.get('scope'), str) or not item['scope'].strip():
                raise ValueError(f'{area}: evidence requires a result and bounded scope')
            if kind == 'physical' and (not isinstance(item.get('device'), str) or not item['device'].strip()):
                raise ValueError(f'{area}: physical evidence requires a device identity')
            if 'sha256' in item and item['sha256'] != digest(artifact):
                raise ValueError(f'{area}: evidence hash mismatch: {rel}')
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
    commit, tree = source_identity(root)
    acceptance = json.loads(args.acceptance.read_text())
    validate_acceptance(acceptance, args.evidence, args.require_complete)
    entries = validate_pack(args.pack)
    if not immutable_toolchain(args.toolchain):
        raise ValueError('record the immutable toolchain image digest')
    receipt = json.loads(args.build_receipt.read_text())
    build_log = args.build_receipt.parent / 'build.log'
    dependencies = args.build_receipt.parent / 'dependencies'
    validate_receipt(receipt, commit, tree, args.toolchain, args.binaries, build_log, dependencies)
    if receipt['private_inputs'] != private_inputs(root):
        raise ValueError('private build inputs changed since the recorded build')
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
        shutil.copytree(dependencies, out / 'build-dependencies')
        copied = set()
        for row in acceptance['areas'].values():
            for item in row.get('evidence', []):
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
            'source_commit': commit,
            'source_tree': tree,
            'toolchain_image': args.toolchain,
            'build_command': receipt['command'],
            'asset_source': {'pack_sha256': digest(args.pack), 'entry_count': len(entries),
                             'conversion': 'copied preconverted inputs; converter hashes identify candidate source, not proof of regeneration',
                             'candidate_converter_sha256': {p.relative_to(root).as_posix(): digest(p) for p in sorted(converters)}},
            'settings_policy': 'explicit files below; omitted settings use compiled defaults; no saves or diagnostic overrides',
            'files': files(out)}
        (out / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
        verify(out, args.require_complete)
        if source_identity(root) != (commit, tree) or private_inputs(root) != receipt['private_inputs']:
            raise ValueError('source or private inputs changed during packaging')
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
    allowed_paths = {'3ds/goldeneye-3ds/' + name for name in allowed}
    directories = {'3ds', '3ds/goldeneye-3ds'}
    if any((p.is_file() and p.relative_to(root / 'sd').as_posix() not in allowed_paths)
           or (p.is_dir() and p.relative_to(root / 'sd').as_posix() not in directories)
           for p in (root / 'sd').rglob('*')):
        raise ValueError('unexpected file in clean SD installation')
    for name, magic in BINARIES.items():
        with (install / name).open('rb') as stream:
            if stream.read(4) != magic:
                raise ValueError(f'invalid executable/metadata magic: {name}')
    entries = validate_pack(install / 'goldeneye.u.gepack')
    if manifest.get('asset_source', {}).get('pack_sha256') != digest(install / 'goldeneye.u.gepack') \
            or manifest['asset_source'].get('entry_count') != len(entries):
        raise ValueError('asset provenance does not match packaged content')
    receipt = json.loads((root / 'build-receipt.json').read_text())
    validate_receipt(receipt, manifest.get('source_commit'), manifest.get('source_tree'),
                     manifest.get('toolchain_image'), install, root / 'build.log',
                     root / 'build-dependencies')
    if manifest.get('build_command') != receipt['command']:
        raise ValueError('manifest build command does not match receipt')
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

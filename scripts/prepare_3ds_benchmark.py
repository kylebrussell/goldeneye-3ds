#!/usr/bin/env python3
"""Prepare private benchmark configs and verify read-only pre/post inventories.

This tool has no emulator or installation controls; it writes only explicitly
named request/evidence outputs. The coordinator stages files and runs captures.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = 'ge-3ds-benchmark-v1'
ALL_SAMPLES = 'all-after-warmup-except-final'
STEREO = ('enabled 1\nparallax_px 8\nconvergence 180\n'
          'first_person_parallax_px 3\nfirst_person_convergence 45\n'
          'hud_parallax_px 2\ncull_margin_per_px 0.00625\n')
CONTROLS = ('dual_analog 1\ninvert_look_y 0\nswap_shoulders 0\n'
            'move_deadzone 0.12\nlook_deadzone 0.12\nlook_sensitivity 1\n')


def digest(path):
    with path.open('rb') as stream:
        value = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
        return value.hexdigest()


def manifest_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def write_json(path, value):
    # Never overwrite an earlier evidence file or a coordinator's request.
    with path.open('x') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')


def source_identity(root):
    names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '--cached', '--others', '--exclude-standard', '-z']).split(b'\0')
    files = {name.decode(): digest(root / name.decode()) for name in names if name and (root / name.decode()).is_file()}
    return {'commit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
            'source_tree_sha256': manifest_digest(files),
            'status': subprocess.check_output(['git', '-C', str(root), 'status', '--short'], text=True)}


def asset_inventory(root):
    # The pack contains converted assets. Include any separately supplied
    # converted files as well; a same-size modified sidecar cannot pass.
    files = [root / 'goldeneye.u.gepack']
    files += sorted((root / 'converted').rglob('*')) if (root / 'converted').exists() else []
    return {str(path.relative_to(root)): digest(path) for path in files if path.is_file()}


def config_inventory(root):
    # Unknown launch configs are not silently inherited by a clean route.
    files = sorted(root.glob('*.cfg')) + sorted(root.glob('*-visual-tour.geview'))
    return {path.name: digest(path) for path in files if path.is_file()}


def input_frame_limit(text):
    lines = [line.split() for line in text.splitlines() if line.strip() and not line.lstrip().startswith('#')]
    if not lines or len(lines[0]) != 2 or lines[0][0] != 'GE_INPUT_PROBE':
        raise ValueError('invalid input probe header')
    version = int(lines[0][1])
    if version not in range(1, 8):
        raise ValueError('unsupported input probe version')
    if version == 2:
        return sum(int(line[1]) for line in lines if line[0] == 'segment')
    return next(int(line[1]) for line in lines if line[0] == 'frames')


def prepare(args):
    if args.warmup < 1 or (args.samples is not None and args.samples < 3600):
        raise ValueError('declare at least one warmup frame and 3600 measured frames')
    if not re.fullmatch(r'[a-z0-9_-]+', args.stage):
        raise ValueError('invalid stage name')
    limit = input_frame_limit(args.input.read_text())
    if not args.warmup + (args.samples or 3600) + 1 <= limit <= 60000:
        raise ValueError('input must cover warmup, measured frames and one trailing GPU guard frame (maximum 60000)')
    assets = asset_inventory(args.assets)
    if 'goldeneye.u.gepack' not in assets:
        raise ValueError('asset directory must contain goldeneye.u.gepack')
    args.output.mkdir(parents=True, exist_ok=False)
    configs = args.output / 'configs'
    configs.mkdir()
    shutil.copyfile(args.input, configs / 'dam-input-probe.cfg')
    (configs / 'stage.cfg').write_text(args.stage + '\n')
    (configs / 'probe-detail.cfg').write_text('1\n' if args.mode == 'pcm' else '0\n')
    (configs / 'world-profile.cfg').write_text('0\n')
    (configs / 'stereo.cfg').write_text(STEREO)
    (configs / 'controls.cfg').write_text(CONTROLS)
    if args.cadence:
        # Preserve recorded cadence bytes. Generation and semantic validation
        # belong to make_3ds_retrace_replay.py and the runtime parser.
        shutil.copyfile(args.cadence, configs / 'probe-retraces.cfg')
    config_hashes = config_inventory(configs)
    request = {
        'schema': SCHEMA, 'label': args.label, 'stage': args.stage,
        'mode': args.mode, 'warmup': args.warmup, 'samples': args.samples or ALL_SAMPLES,
        'configured_frame_limit': limit, 'gpu_guard_frames': 1,
        'environment': args.environment, 'environment_description': args.environment_description,
        'toolchain': args.toolchain, 'source': source_identity(ROOT),
        'binary_path': str(args.binary.resolve()), 'binary_sha256': digest(args.binary),
        'asset_path': str(args.assets.resolve()), 'assets': assets,
        'asset_sha256': assets['goldeneye.u.gepack'], 'asset_manifest_sha256': manifest_digest(assets),
        'input_sha256': digest(args.input),
        'cadence_sha256': digest(args.cadence) if args.cadence else 'live',
        'configurations': config_hashes, 'configuration_sha256': manifest_digest(config_hashes),
        'stereo_slider_requested': 1.0,
        'run_requirements': ['Exclusive emulator/device run; no concurrent builds or timing tests.',
            'Restart process for cold and independent repeat runs; declare reset semantics.',
            'Use audible music and SFX, maximum slider, native speed; observe actual output.',
            'Before/after snapshots must bracket this run and use the same request.',
            'Do not install a frontend/capture/tour config in the clean gameplay run.',
            'Report startup latency and all route stalls. PCM runs are separate correctness evidence.'],
    }
    write_json(args.output / 'request.json', request)
    return request


def snapshot(args):
    request = json.loads(args.request.read_text())
    observed = {'binary_sha256': digest(args.installed / 'goldeneye-3ds.3dsx'),
                'assets': asset_inventory(args.installed), 'configurations': config_inventory(args.installed)}
    for key in observed:
        if observed[key] != request[key]:
            raise ValueError('installed ' + key + ' differs from request')
    result = args.installed / 'dam-input-probe.result'
    if args.phase == 'before' and result.exists():
        raise ValueError('move the existing result aside before the coordinator starts the run')
    if args.phase == 'after' and not result.is_file():
        raise ValueError('missing completed result')
    observed.update({'schema': SCHEMA, 'request_sha256': digest(args.request), 'phase': args.phase,
                     'timestamp_utc': datetime.now(timezone.utc).isoformat(),
                     'result_sha256': digest(result) if args.phase == 'after' else None})
    write_json(args.output, observed)
    return observed


def collect(args):
    request = json.loads(args.request.read_text())
    before, after = [json.loads(path.read_text()) for path in (args.before, args.after)]
    request_hash = digest(args.request)
    for phase, snap in [('before', before), ('after', after)]:
        if snap.get('schema') != SCHEMA or snap.get('phase') != phase or snap.get('request_sha256') != request_hash:
            raise ValueError('snapshot identity or phase mismatch')
        for key in ('binary_sha256', 'assets', 'configurations'):
            if snap.get(key) != request[key]:
                raise ValueError('snapshot differs from request: ' + key)
    if before.get('result_sha256') is not None or after.get('result_sha256') != digest(args.result):
        raise ValueError('result is stale or differs from the post-run inventory')
    if datetime.fromisoformat(before['timestamp_utc']) >= datetime.fromisoformat(after['timestamp_utc']):
        raise ValueError('snapshot times are out of order')
    sidecar = args.output.with_suffix('.provenance.json')
    if args.output.exists() or sidecar.exists():
        raise ValueError('output evidence already exists')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    provenance = dict(request)
    provenance.update({'result_sha256': digest(args.result), 'request_sha256': request_hash,
        'before_sha256': digest(args.before), 'after_sha256': digest(args.after),
        'inventory_verified': True, 'audible_confirmed': args.audible_confirmed,
        'run_kind': args.run_kind, 'operator_observations': args.observations,
        'before_utc': before['timestamp_utc'], 'after_utc': after['timestamp_utc']})
    shutil.copyfile(args.result, args.output)
    write_json(sidecar, provenance)
    return provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    prepare_parser = commands.add_parser('prepare')
    for name in ('output', 'input', 'binary', 'assets'):
        prepare_parser.add_argument('--' + name, type=Path, required=True)
    for name in ('label', 'stage', 'toolchain', 'environment-description'):
        prepare_parser.add_argument('--' + name, required=True)
    prepare_parser.add_argument('--environment', choices=('emulator', 'physical-new3dsxl'), required=True)
    prepare_parser.add_argument('--mode', choices=('clean', 'pcm'), default='clean')
    prepare_parser.add_argument('--warmup', type=int, default=120)
    prepare_parser.add_argument('--samples', type=int,
                                help='expected measured count; default measures every post-warmup frame except final GPU guard')
    prepare_parser.add_argument('--cadence', type=Path)
    prepare_parser.set_defaults(action=prepare)
    snapshot_parser = commands.add_parser('snapshot')
    for name in ('request', 'installed', 'output'):
        snapshot_parser.add_argument('--' + name, type=Path, required=True)
    snapshot_parser.add_argument('--phase', choices=('before', 'after'), required=True)
    snapshot_parser.set_defaults(action=snapshot)
    collect_parser = commands.add_parser('collect')
    for name in ('request', 'before', 'after', 'result', 'output'):
        collect_parser.add_argument('--' + name, type=Path, required=True)
    collect_parser.add_argument('--run-kind', choices=('cold', 'repeat'), required=True)
    collect_parser.add_argument('--audible-confirmed', action='store_true')
    collect_parser.add_argument('--observations', required=True,
                                help='actual device/emulator settings, reset behavior, audible output and issues')
    collect_parser.set_defaults(action=collect)
    args = parser.parse_args()
    try:
        value = args.action(args)
    except (ValueError, OSError, KeyError, StopIteration) as error:
        parser.error(str(error))
    print(json.dumps(value, indent=2))


if __name__ == '__main__':
    main()

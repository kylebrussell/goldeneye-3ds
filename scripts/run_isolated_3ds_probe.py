#!/usr/bin/env python3
"""Run an owned Azahar process using a new portable profile and private SD tree."""
from __future__ import annotations
import argparse
import configparser
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import signal
import subprocess
import time
from package_3ds_candidate import digest
import prepare_3ds_benchmark as benchmark

REQUEST_CONFIGS = frozenset((
    'stage.cfg', 'probe-detail.cfg', 'world-profile.cfg', 'stereo.cfg',
    'controls.cfg', 'probe-retraces.cfg', 'dam-input-probe.cfg',
    'probe-difficulty.cfg'))
REQUIRED_REQUEST_CONFIGS = REQUEST_CONFIGS - {'probe-retraces.cfg', 'probe-difficulty.cfg'}


def prepare_profile(source, destination, user, max_stereo=False):
    # Preserve caller-selected emulation/audio settings; rewrite filesystem
    # destinations so no shared SD, NAND, cache or capture receives writes.
    config = configparser.ConfigParser(interpolation=None, strict=False)
    config.optionxform = str
    config.read(source)
    for section in config.sections():
        if section in ('UI', 'Paths', 'WebService', 'Multiplayer'):
            config.remove_section(section)
    # QSettings percent-encodes spaces in group names. Collapse both spellings
    # in source order, preserving flags but never retaining a shared path in
    # a second alias that Qt may merge back into the same group.
    storage = {}
    for section in config.sections():
        if section in ('Data Storage', 'Data%20Storage'):
            storage.update(config.items(section))
            config.remove_section(section)
    storage_section = 'Data%20Storage'
    config.add_section(storage_section)
    for key, value in storage.items():
        config[storage_section][key] = value
    for key, path in [('sdmc_directory', user / 'sdmc'), ('nand_directory', user / 'nand')]:
        config[storage_section][key] = str(path) + '/'
        config[storage_section][key + '\\default'] = 'false'
    # Refuse unrecognized absolute path overrides, rather than silently reuse
    # a shared dump/mod/load directory from a custom profile.
    for section in config.sections():
        for key, value in config.items(section):
            if value.startswith('/') and not (section == storage_section
                    and key in ('sdmc_directory', 'nand_directory')):
                raise ValueError(f'profile has an unhandled absolute path: {section}/{key}')
    # Avoid an update modal changing the recorded emulator version mid-run.
    if not config.has_section('Miscellaneous'):
        config.add_section('Miscellaneous')
    config['Miscellaneous']['check_for_update_on_start'] = 'false'
    config['Miscellaneous']['check_for_update_on_start\\default'] = 'false'
    if max_stereo:
        # Azahar 2126.0 QtConfig::ReadLayoutValues and StereoRenderOption:
        # factor_3d/render_3d belong to Layout; SideBySide is enum value 1.
        for section, settings in {
                'Layout': {'factor_3d': '100', 'render_3d': '1'},
                'Renderer': {'disable_right_eye_render': 'false'}}.items():
            if not config.has_section(section):
                config.add_section(section)
            for key, value in settings.items():
                config[section][key] = value
                config[section][key + '\\default'] = 'false'
    destination.parent.mkdir(parents=True)
    with destination.open('w') as stream:
        config.write(stream, space_around_delimiters=False)


def regular_file(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f'input must be a regular non-symlink file: {path}')


def validate_difficulty(path):
    if path.read_bytes() not in (b'0\n', b'1\n', b'2\n', b'3\n'):
        raise ValueError('probe-difficulty.cfg requires exactly 0, 1, 2 or 3 followed by a newline')


def validate_request(path, binary, pack):
    """Validate all source bytes before creating the private run directory."""
    regular_file(path)
    request = json.loads(path.read_text())
    if not isinstance(request, dict) or request.get('schema') != benchmark.SCHEMA:
        raise ValueError('unsupported benchmark request schema')
    if request.get('environment') != 'emulator':
        raise ValueError('isolated emulator cannot fulfill a physical-device request')
    configurations = request.get('configurations')
    if not isinstance(configurations, dict) or not REQUIRED_REQUEST_CONFIGS <= configurations.keys():
        raise ValueError('benchmark request is missing required configurations')
    if not configurations.keys() <= REQUEST_CONFIGS:
        raise ValueError('benchmark request contains unknown or nested configurations')
    configs = path.parent / 'configs'
    if configs.is_symlink() or not configs.is_dir():
        raise ValueError('request configs must be a regular sibling directory')
    entries = list(configs.iterdir())
    for entry in entries:
        if entry.name not in REQUEST_CONFIGS:
            raise ValueError(f'unknown or nested request config: {entry.name}')
        regular_file(entry)
    observed = {entry.name: digest(entry) for entry in entries}
    if observed != configurations:
        raise ValueError('request configuration hashes or inventory differ')
    if benchmark.manifest_digest(observed) != request.get('configuration_sha256'):
        raise ValueError('request configuration manifest hash differs')
    if 'probe-difficulty.cfg' in observed:
        validate_difficulty(configs / 'probe-difficulty.cfg')
    if digest(binary) != request.get('binary_sha256'):
        raise ValueError('binary hash differs from benchmark request')
    if digest(pack) != request.get('asset_sha256'):
        raise ValueError('pack hash differs from benchmark request')
    if observed['dam-input-probe.cfg'] != request.get('input_sha256'):
        raise ValueError('input hash differs from benchmark request')
    if (configs / 'stage.cfg').read_text().strip() != request.get('stage'):
        raise ValueError('stage config differs from benchmark request')
    try:
        frame_limit = benchmark.input_frame_limit((configs / 'dam-input-probe.cfg').read_text())
    except (ValueError, StopIteration, IndexError) as error:
        raise ValueError('invalid benchmark input probe') from error
    if frame_limit != request.get('configured_frame_limit'):
        raise ValueError('probe frame limit differs from benchmark request')
    if request.get('cadence_sha256') != observed.get('probe-retraces.cfg', 'live'):
        raise ValueError('cadence hash differs from benchmark request')
    assets = request.get('assets')
    if not isinstance(assets, dict) or assets.get('goldeneye.u.gepack') != request.get('asset_sha256'):
        raise ValueError('request asset inventory does not match pack hash')
    if benchmark.manifest_digest(assets) != request.get('asset_manifest_sha256'):
        raise ValueError('request asset manifest hash differs')
    asset_sources = {'goldeneye.u.gepack': pack}
    for name, expected_hash in assets.items():
        if name == 'goldeneye.u.gepack':
            continue
        relative = PurePosixPath(name)
        if (relative.is_absolute() or str(relative) != name or '..' in relative.parts
                or len(relative.parts) < 2 or relative.parts[0] != 'converted' or '\\' in name):
            raise ValueError(f'unsafe or unknown request asset: {name}')
        if not isinstance(request.get('asset_path'), str) or not request['asset_path']:
            raise ValueError('request sidecars require an asset_path')
        root = Path(request['asset_path'])
        current = root
        for part in relative.parts:
            if current.is_symlink():
                raise ValueError(f'symlink in request asset path: {name}')
            current = current / part
        regular_file(current)
        if digest(current) != expected_hash:
            raise ValueError(f'request asset hash differs: {name}')
        asset_sources[name] = current
    return request, configs, asset_sources


def complete_result(path):
    if not path.is_file():
        return False
    return [line for line in path.read_text().splitlines()
            if line.startswith('status=')] == ['status=complete']


def result_has_status(path):
    return path.is_file() and any(line.startswith('status=')
                                 for line in path.read_text().splitlines())


def execute(args):
    request_path = getattr(args, 'request', None)
    max_stereo = getattr(args, 'max_stereo', False)
    if request_path is not None and (args.probe is not None or args.stage is not None or args.setting):
        raise ValueError('--request is mutually exclusive with --probe, --stage and --setting')
    if request_path is None and args.probe is None:
        raise ValueError('one of --probe or --request is required')
    if args.output.exists():
        raise ValueError('output exists; use a new private run directory')
    # Caller owns timing scheduling; refuse to compete with any running
    # emulator and never kill a process we did not launch.
    processes = subprocess.check_output(['ps', '-axo', 'comm='], text=True)
    if any(Path(line.strip()).name in ('azahar', 'citra') for line in processes.splitlines()):
        raise ValueError('an emulator is already running; preserving that process')
    for source in (args.emulator, args.profile, args.binary, args.pack):
        if not source.is_file():
            raise ValueError(f'missing input: {source}')
    request = None
    if request_path is not None:
        request, request_configs, asset_sources = validate_request(request_path, args.binary, args.pack)
        probe = request_configs / 'dam-input-probe.cfg'
    else:
        probe = args.probe
        regular_file(probe)
    probe_text = probe.read_text()
    gameplay_only = probe_text.startswith('GE_INPUT_PROBE ')
    if args.stage is not None and (not gameplay_only or not args.stage.isalnum()):
        raise ValueError('stage selection requires a gameplay probe and alphanumeric stage key')
    for setting in args.setting:
        if setting.name not in ('stereo.cfg', 'controls.cfg', 'probe-difficulty.cfg'):
            raise ValueError('only explicit stereo.cfg, controls.cfg and probe-difficulty.cfg settings supported')
        regular_file(setting)
        if setting.name == 'probe-difficulty.cfg':
            if not gameplay_only:
                raise ValueError('probe difficulty requires a gameplay GE_INPUT_PROBE')
            validate_difficulty(setting)
    if len({setting.name for setting in args.setting}) != len(args.setting):
        raise ValueError('duplicate setting names are not supported')
    args.output.mkdir(parents=True)
    # macOS Azahar changes cwd to the app's parent. Explicit existing XDG
    # roots survive that change and prevent fallback to the shared profile.
    user = args.output.resolve() / 'xdg-data/azahar-emu'
    config_root = args.output.resolve() / 'xdg-config/azahar-emu'
    cache_root = args.output.resolve() / 'xdg-cache/azahar-emu'
    cache_root.mkdir(parents=True)
    install = user / 'sdmc/3ds/goldeneye-3ds'
    install.mkdir(parents=True)
    profile = config_root / 'qt-config.ini'
    prepare_profile(args.profile, profile, user, max_stereo=max_stereo)
    shutil.copyfile(profile, args.output / 'profile-before.ini')
    shutil.copyfile(args.binary, install / 'goldeneye-3ds.3dsx')
    shutil.copyfile(args.pack, install / 'goldeneye.u.gepack')
    probe_name = 'dam-input-probe.cfg' if gameplay_only else 'frontend-probe.cfg'
    if request is not None:
        stored_request = args.output / 'request.json'
        shutil.copyfile(request_path, stored_request)
        for name in request['configurations']:
            shutil.copyfile(request_configs / name, install / name)
        for name, source in asset_sources.items():
            if name != 'goldeneye.u.gepack':
                destination = install / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, destination)
    else:
        shutil.copyfile(probe, install / probe_name)
        if args.stage:
            (install / 'stage.cfg').write_text(args.stage + '\n')
        for setting in args.setting:
            shutil.copyfile(setting, install / setting.name)
    provenance = {'environment': 'emulator', 'emulator_sha256': digest(args.emulator),
                  'binary_sha256': digest(args.binary), 'pack_sha256': digest(args.pack),
                  'probe_sha256': digest(probe),
                  'source_profile_sha256': digest(args.profile),
                  'profile_sha256': digest(profile), 'max_stereo': max_stereo,
                  'settings': {s.name: digest(s) for s in args.setting},
                  'timeout_seconds': args.timeout,
                  'stage': request['stage'] if request is not None else args.stage, 'status': 'started'}
    if request is not None:
        provenance['request_sha256'] = digest(stored_request)
        provenance['configurations'] = request['configurations']
        benchmark.snapshot(argparse.Namespace(request=stored_request, installed=install,
                                             output=args.output / 'before.json', phase='before'))
        provenance['before_sha256'] = digest(args.output / 'before.json')
    (args.output / 'run.json').write_text(json.dumps(provenance, indent=2) + '\n')
    expected_gameplay = gameplay_only or any(line.split()[:1] == ['gameplay_frames'] and int(line.split()[1]) > 0
                            for line in probe_text.splitlines())
    start = time.monotonic()
    process = None
    def interrupted(signum, _frame):
        raise SystemExit(128 + signum)
    previous_handler = signal.signal(signal.SIGTERM, interrupted)
    try:
        with (args.output / 'process.log').open('wb') as log:
            command = [str(args.emulator.resolve()), '-w', str(install / 'goldeneye-3ds.3dsx')]
            if args.protect:
                if sys.platform != 'darwin' or not Path('/usr/bin/sandbox-exec').is_file():
                    raise ValueError('profile write protection requires macOS sandbox-exec')
                policy = '(version 1)(allow default)' + ''.join(
                    '(deny file-write* (subpath ' + json.dumps(str(path.resolve())) + '))'
                    for path in args.protect)
                command = ['/usr/bin/sandbox-exec', '-p', policy] + command
            process = subprocess.Popen(command,
                                       cwd=args.output.resolve(), stdout=log, stderr=subprocess.STDOUT,
                                       env=dict(os.environ, XDG_DATA_HOME=str(user.parent),
                                                XDG_CONFIG_HOME=str(config_root.parent),
                                                XDG_CACHE_HOME=str(cache_root.parent)))
            while process.poll() is None and time.monotonic() - start < args.timeout:
                frontend = install / 'frontend-probe.result'
                gameplay = install / 'dam-input-probe.result'
                if (gameplay_only or result_has_status(frontend)) and (
                        not expected_gameplay or result_has_status(gameplay)):
                    # A pending final-frame capture is written after its draw.
                    time.sleep(1)
                    break
                time.sleep(.25)
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
        signal.signal(signal.SIGTERM, previous_handler)
    provenance['elapsed_seconds'] = time.monotonic() - start
    expected = ([] if gameplay_only else ['frontend-probe.result']) + (['dam-input-probe.result'] if expected_gameplay else [])
    passed = True
    for name in expected:
        result = install / name
        if not result.is_file():
            passed = False
        else:
            shutil.copyfile(result, args.output / name)
            passed &= complete_result(result)
    if request is not None and complete_result(install / 'dam-input-probe.result'):
        try:
            benchmark.snapshot(argparse.Namespace(request=stored_request, installed=install,
                                                 output=args.output / 'after.json', phase='after'))
            provenance['after_sha256'] = digest(args.output / 'after.json')
        except (ValueError, OSError) as error:
            provenance['inventory_error'] = str(error)
            passed = False
    private_log = user / 'log/azahar_log.txt'
    provenance['profile_isolation_verified'] = private_log.is_file() and private_log.stat().st_size > 0
    shutil.copyfile(profile, args.output / 'profile-after.ini')
    provenance['profile_after_sha256'] = digest(profile)
    passed &= provenance['profile_isolation_verified']
    provenance['status'] = 'complete' if passed else 'failed' 
    (args.output / 'run.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(json.dumps(provenance))
    return 0 if passed else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('emulator', 'profile', 'binary', 'pack', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument('--probe', type=Path)
    selection.add_argument('--request', type=Path)
    parser.add_argument('--setting', type=Path, action='append', default=[])
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--stage')
    parser.add_argument('--max-stereo', action='store_true',
                        help='set maximum slider and side-by-side output in the isolated Azahar profile')
    parser.add_argument('--protect', type=Path, action='append', default=[])
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('timeout must be positive')
    try:
        return execute(args)
    except (ValueError, OSError) as error:
        parser.exit(1, f'isolated probe rejected: {error}\n')

if __name__ == '__main__':
    raise SystemExit(main())

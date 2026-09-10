#!/usr/bin/env python3
"""Run an owned Azahar process using a new portable profile and private SD tree."""
from __future__ import annotations
import argparse
import configparser
import json
import os
from pathlib import Path
import shutil
import sys
import signal
import subprocess
import time
from package_3ds_candidate import digest


def prepare_profile(source, destination, user):
    # Preserve caller-selected emulation/audio settings; rewrite filesystem
    # destinations so no shared SD, NAND, cache or capture receives writes.
    config = configparser.ConfigParser(interpolation=None, strict=False)
    config.optionxform = str
    config.read(source)
    for section in config.sections():
        if section in ('UI', 'Paths', 'WebService', 'Multiplayer'):
            config.remove_section(section)
    if not config.has_section('Data Storage'):
        config.add_section('Data Storage')
    for key, path in [('sdmc_directory', user / 'sdmc'), ('nand_directory', user / 'nand')]:
        config['Data Storage'][key] = str(path) + '/'
        config['Data Storage'][key + '\\default'] = 'false'
    # Refuse unrecognized absolute path overrides, rather than silently reuse
    # a shared dump/mod/load directory from a custom profile.
    for section in config.sections():
        for key, value in config.items(section):
            if value.startswith('/') and key not in ('sdmc_directory', 'nand_directory'):
                raise ValueError(f'profile has an unhandled absolute path: {section}/{key}')
    # Avoid an update modal changing the recorded emulator version mid-run.
    if not config.has_section('Miscellaneous'):
        config.add_section('Miscellaneous')
    config['Miscellaneous']['check_for_update_on_start'] = 'false'
    config['Miscellaneous']['check_for_update_on_start\\default'] = 'false'
    destination.parent.mkdir(parents=True)
    with destination.open('w') as stream:
        config.write(stream, space_around_delimiters=False)


def execute(args):
    if args.output.exists():
        raise ValueError('output exists; use a new private run directory')
    # Caller owns timing scheduling; refuse to compete with any running
    # emulator and never kill a process we did not launch.
    processes = subprocess.check_output(['ps', '-axo', 'comm='], text=True)
    if any(Path(line.strip()).name in ('azahar', 'citra') for line in processes.splitlines()):
        raise ValueError('an emulator is already running; preserving that process')
    for source in (args.emulator, args.profile, args.binary, args.pack, args.probe):
        if not source.is_file():
            raise ValueError(f'missing input: {source}')
    args.output.mkdir(parents=True)
    # macOS Azahar changes cwd to the app's parent. Explicit existing XDG
    # roots survive that change and prevent fallback to the shared profile.
    user = args.output.resolve() / 'xdg-data/azahar-emu'
    config_root = args.output.resolve() / 'xdg-config/azahar-emu'
    cache_root = args.output.resolve() / 'xdg-cache/azahar-emu'
    cache_root.mkdir(parents=True)
    install = user / 'sdmc/3ds/goldeneye-3ds'
    install.mkdir(parents=True)
    prepare_profile(args.profile, config_root / 'qt-config.ini', user)
    shutil.copyfile(args.binary, install / 'goldeneye-3ds.3dsx')
    shutil.copyfile(args.pack, install / 'goldeneye.u.gepack')
    gameplay_only = args.probe.read_text().startswith('GE_INPUT_PROBE ')
    probe_name = 'dam-input-probe.cfg' if gameplay_only else 'frontend-probe.cfg'
    shutil.copyfile(args.probe, install / probe_name)
    if args.stage:
        if not gameplay_only or not args.stage.isalnum():
            raise ValueError('stage selection requires a gameplay probe and alphanumeric stage key')
        (install / 'stage.cfg').write_text(args.stage + '\n')
    for setting in args.setting:
        if setting.name not in ('stereo.cfg', 'controls.cfg'):
            raise ValueError('only explicit stereo.cfg and controls.cfg settings supported')
        shutil.copyfile(setting, install / setting.name)
    provenance = {'environment': 'emulator', 'emulator_sha256': digest(args.emulator),
                  'binary_sha256': digest(args.binary), 'pack_sha256': digest(args.pack),
                  'probe_sha256': digest(args.probe),
                  'profile_sha256': digest(config_root / 'qt-config.ini'),
                  'settings': {s.name: digest(s) for s in args.setting},
                  'timeout_seconds': args.timeout, 'stage': args.stage, 'status': 'started'}
    (args.output / 'run.json').write_text(json.dumps(provenance, indent=2) + '\n')
    expected_gameplay = gameplay_only or any(line.split()[:1] == ['gameplay_frames'] and int(line.split()[1]) > 0
                            for line in args.probe.read_text().splitlines())
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
                if (gameplay_only or frontend.is_file() and frontend.stat().st_size) and (
                        not expected_gameplay or gameplay.is_file() and gameplay.stat().st_size):
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
            passed &= 'status=complete' in result.read_text().splitlines()
    private_log = user / 'log/azahar_log.txt'
    provenance['profile_isolation_verified'] = private_log.is_file() and private_log.stat().st_size > 0
    provenance['profile_after_sha256'] = digest(config_root / 'qt-config.ini')
    passed &= provenance['profile_isolation_verified']
    provenance['status'] = 'complete' if passed else 'failed' 
    (args.output / 'run.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(json.dumps(provenance))
    return 0 if passed else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('emulator', 'profile', 'binary', 'pack', 'probe', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--setting', type=Path, action='append', default=[])
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--stage')
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

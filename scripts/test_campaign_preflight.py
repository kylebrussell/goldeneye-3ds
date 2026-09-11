#!/usr/bin/env python3
"""Run fresh campaign fixtures against an explicit existing private asset pack.

This is a host preflight, not proof of mission completion, hardware pixels or
frame rate. It never edits SD files, emulator settings or the supplied pack.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()


def source_files():
    # Include local native source inputs, even when excluded from Git. Generated
    # temporary test slices are reconstructed from these source files.
    result = {}
    for directory in ('port', 'platform/3ds', 'scripts', 'src', 'include'):
        for path in (ROOT / directory).rglob('*'):
            relative = path.relative_to(ROOT)
            if 'build' in relative.parts or '__pycache__' in relative.parts:
                continue
            if path.is_file() and (path.suffix in ('.c', '.h', '.py', '.sh', '.S', '.pica', '.inc')
                                   or path.name == 'Makefile'):
                result[relative.as_posix()] = digest(path)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path,
                        help='new evidence directory; existing runs are preserved')
    parser.add_argument('--jobs', type=int, default=3, choices=range(1, 5))
    args = parser.parse_args()
    pack = args.pack.resolve()
    if not pack.is_file():
        parser.error('supply an existing complete original asset pack')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    before = source_files()
    pack_hash = digest(pack)
    (output / 'source-inputs.json').write_text(json.dumps(before, indent=2, sort_keys=True) + '\n')
    plan = [
        ('campaign-objectives', ['bash', 'scripts/test_campaign_acceptance.sh', str(pack)]),
        ('campaign-setup-links', ['bash', 'scripts/test_campaign_setup_links.sh', str(pack)]),
        ('campaign-room-publication', ['bash', 'scripts/test_campaign_room_publication.sh', str(pack)]),
        ('switch-activation', ['bash', 'scripts/test_stage_switch_runtime.sh']),
        ('controls', ['bash', 'scripts/test_3ds_controls.sh']),
        ('sight-alignment', [sys.executable, 'scripts/tests/test_3ds_sight_alignment.py']),
        ('use-feedback', [sys.executable, 'scripts/tests/test_3ds_use_feedback.py']),
        ('input-history', ['bash', 'scripts/test_multiplayer_input_gates.sh']),
        ('apt-lifecycle', ['bash', 'scripts/test_3ds_lifecycle.sh']),
        ('watch-abort', ['bash', 'scripts/test_original_watch_mission_abort.sh']),
        ('watch-input', ['bash', 'scripts/test_multiplayer_watch.sh']),
        ('runtime-rebind', ['bash', 'scripts/test_multiplayer_main_state_lifecycle.sh']),
        ('prop-retirement', ['bash', 'scripts/test_original_prop_stage_end.sh']),
        ('renderer-submission', [sys.executable, 'scripts/tests/test_3ds_renderer_submission_cache.py']),
        ('world-samplers', [sys.executable, 'scripts/tests/test_world_hardware_regressions.py']),
    ]
    report = {
        'schema_version': 1,
        'started_utc': datetime.now(timezone.utc).isoformat(),
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_inputs_sha256': digest(output / 'source-inputs.json'),
        'pack': str(pack), 'pack_sha256': pack_hash,
        'scope': 'Host fixtures, native-source tests and actual-asset publication checks; not complete mission playthroughs or hardware acceptance.',
        'checks': [],
    }

    def run(item):
        name, command = item
        log = output / (name + '.log')
        env = dict(os.environ, GE_WATCH_TEST_DIR=str(output / 'watch-fixtures'),
                   GE_CAMPAIGN_RENDER_TEST_DIR=str(output / 'room-fixtures'))
        started = time.monotonic()
        with log.open('wb') as stream:
            try:
                result = subprocess.run(command, cwd=ROOT, env=env,
                                        stdout=stream, stderr=subprocess.STDOUT, timeout=600)
                code = result.returncode
            except subprocess.TimeoutExpired:
                stream.write(b'\nPRECHECK TIMEOUT after 600 seconds\n')
                code = 124
        return {'name': name, 'command': command, 'exit_code': code,
                'elapsed_seconds': round(time.monotonic() - started, 3),
                'log': log.name, 'log_sha256': digest(log)}

    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        for future in as_completed([executor.submit(run, item) for item in plan]):
            check = future.result()
            report['checks'].append(check)
            print(f"{check['name']}: {'PASS' if check['exit_code'] == 0 else 'FAIL'}", flush=True)
            (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    report['source_unchanged'] = before == source_files()
    report['pack_unchanged'] = pack_hash == digest(pack)
    report['complete'] = len(report['checks']) == len(plan)
    report['passed'] = report['complete'] and report['source_unchanged'] and report['pack_unchanged'] \
        and all(check['exit_code'] == 0 for check in report['checks'])
    report['finished_utc'] = datetime.now(timezone.utc).isoformat()
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(output / 'report.json')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

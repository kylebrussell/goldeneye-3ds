#!/usr/bin/env python3
"""Summarize buffered ARM11 input-probe timings without rounded-ms thresholds."""
import argparse
import json
import math
from pathlib import Path


def summarize(path, warmup):
    values = {}
    frames = []
    presents = []
    details = []
    if warmup < 0:
        raise ValueError("warmup must be nonnegative")
    for line in path.read_text().splitlines():
        key, separator, value = line.partition('=')
        if key == 'frame_timing':
            row = [int(item) for item in value.split(',')]
            if len(row) not in (13, 20):
                raise ValueError(f'{path}: invalid frame timing row')
            frames.append(row)
        elif key == 'frame_present_timing':
            presents.append([int(item) for item in value.split(',')])
        elif key == 'frame_detail':
            details.append([int(item) for item in value.split(',')])
        elif separator:
            values[key] = value
    hz = int(values['runtime_profile_tick_hz'])
    if len(frames) != int(values['frame_timing_count']) or not frames:
        raise ValueError(f'{path}: missing or incomplete timing records')
    if [row[0] for row in frames] != list(range(1, len(frames) + 1)):
        raise ValueError(f'{path}: nonsequential timing records')
    if hz <= 0 or any(len(row) != len(frames[0]) or min(row) < 0 for row in frames):
        raise ValueError(f'{path}: invalid clock or mixed/negative timing records')
    if presents and (len(presents) != len(frames) or any(
            len(row) != 4 or row[0] != i + 1 or min(row) < 0
            for i, row in enumerate(presents))):
        raise ValueError(f'{path}: incomplete presentation records')
    if details and (len(details) != len(frames) or any(
            len(row) != 25 or row[0] != i + 1 or min(row) < 0
            for i, row in enumerate(details))):
        raise ValueError(f'{path}: incomplete detail records')
    warm = frames[warmup:]
    if not warm:
        raise ValueError(f'{path}: no frames after warmup')

    def stats(rows, column):
        ticks = sorted(row[column] for row in rows)
        if not ticks:
            return None
        return {name: round(value * 1000 / hz, 4) for name, value in {
            'mean_ms': sum(ticks) / len(ticks),
            'p95_ms': ticks[math.ceil(len(ticks) * .95) - 1],
            'p99_ms': ticks[math.ceil(len(ticks) * .99) - 1],
            'peak_ms': ticks[-1],
        }.items()}

    fields = ['work', 'start_interval', 'frame_begin_wait', 'music',
              'canonical_tick', 'guard_scene', 'world', 'renderer',
              'props', 'actor_vertices']
    if len(frames[0]) == 20:
        fields += ['guard_gpu_upload', 'guard_replace', 'guard_import',
                   'guard_visibility', 'first_person', 'guard_commit', 'world_flush']
    result = {
        'path': str(path), 'status': values.get('status'),
        'level_id': values.get('level_id'), 'end': values.get('end'),
        'frames': len(frames), 'warmup': warmup, 'samples': len(warm),
        'music_timing_scope': values.get('music_timing_scope', 'main_synchronous'),
        'work_over_60hz_budget': sum(row[1] * 60 > hz for row in warm),
        'unsampled_work_over_budget': sum(row[1] * 60 > hz and not row[-1] for row in warm),
        'world_sampled_frames': sum(bool(row[-1]) for row in warm),
        # Version 1 accidentally recorded dispatches (always one), not retraces.
        'multiple_retrace_frames': sum(row[-2] > 1 for row in warm)
            if int(values.get('frame_timing_version', 1)) >= 2 else None,
        'submission_intervals': {
            'samples': len(presents[warmup + 1:]),
            'repeated_vblank': sum(b[3] == a[3] for a, b in zip(
                presents[warmup:], presents[warmup + 1:])),
            'skipped_vblank': sum(((b[3] - a[3]) & 0xffffffff) > 1 for a, b in zip(
                presents[warmup:], presents[warmup + 1:])),
            'previous_gpu_peak_us': max(row[2] for row in presents[warmup:]),
            'pacing_wait': stats(presents[warmup:], 1),
        } if presents else None,
        'phases': {name: stats(warm, column + 1) for column, name in enumerate(fields)},
        'unsampled_work': stats([row for row in warm if not row[-1]], 1),
        'slowest_frames': sorted(warm, key=lambda row: row[1], reverse=True)[:12],
    }
    # Canonical tick includes music and props; renderer includes world. Never
    # sum nested categories as independent CPU time.
    result['phase_nesting'] = {'canonical_tick': ['music', 'props'],
                               'renderer': ['world']}
    result['deep_profile_enabled'] = values.get('probe_detail', '1') == '1'
    if details:
        names = ['pre_ai', 'move', 'gun', 'combat_audit', 'chr_dispatch',
                 'obj_dispatch', 'other_dispatch']
        result['dispatch_phases'] = {name: (stats(details[warmup:], i+1) if result['deep_profile_enabled'] or i<4 else None)
                                     for i, name in enumerate(names)}
        result['over_budget_attribution'] = []
        for row, detail in zip(frames[warmup:], details[warmup:]):
            if row[1]*60 <= hz:
                continue
            result['over_budget_attribution'].append({
                'frame': row[0], 'work_ms': round(row[1]*1000/hz, 4),
                'music_ms': round(row[4]*1000/hz, 4),
                'canonical_ms': round(row[5]*1000/hz, 4),
                'props_ms': round(row[9]*1000/hz, 4),
                **{name+'_ms': round(detail[i+1]*1000/hz, 4) for i,name in enumerate(names)},
                'dispatch_calls': detail[8:11], 'topology_rebuilds': detail[11],
                'scene_generations': detail[12], 'full_overlay_rebuilds': detail[13],
                'allocator_requests': dict(zip(['malloc','calloc','realloc','free','bytes','failures'],detail[19:])),
                **{name+'_ms': round(detail[14+i]*1000/hz,4) for i,name in enumerate(['action','animation','matrices','firing','sfx_decode'])},
                'guard_upload_ms': round(row[11]*1000/hz,4),
                'import_ms': round(row[13]*1000/hz,4),
                'retrace_delta': row[-2],
            })
    if details and not result['deep_profile_enabled']:
        for row in result['over_budget_attribution']:
            for key in ('chr_dispatch_ms','obj_dispatch_ms','other_dispatch_ms',
                        'dispatch_calls','allocator_requests','action_ms','animation_ms',
                        'matrices_ms','firing_ms','sfx_decode_ms'):
                row[key] = None
    return result


def comparison_gate(before, after):
    """Matching recorded state is necessary, not proof of deterministic replay."""
    required = ('status', 'level_id', 'frames', 'simulation_frames', 'start', 'end',
                'start_look', 'end_look', 'room_trace', 'objective_status',
                'mission_result', 'player_death', 'door_interaction', 'door_runtime',
                'probe_initial_rng', 'probe_clock', 'route_targets')
    def identity(path):
        fields = {}
        segments = []
        checkpoints = []
        cadence = []
        for line in path.read_text().splitlines():
            key, sep, value = line.partition('=')
            if sep:
                if key == 'probe_segment':
                    segments.append(value)
                elif key == 'probe_checkpoint':
                    checkpoints.append(value)
                elif key == 'frame_timing':
                    cadence.append(value.split(',')[-2])
                elif key in required or key in ('probe_detail', 'music_pcm'):
                    fields[key] = value
        fields['probe_segments'] = segments
        fields['checkpoints'] = checkpoints
        fields['cadence'] = cadence
        return fields
    left, right = identity(before), identity(after)
    left.setdefault('probe_detail', '1'); right.setdefault('probe_detail', '1')
    reasons = []
    for key in (*required, 'probe_detail'):
        if key not in left or key not in right:
            reasons.append('missing ' + key)
        elif left[key] != right[key]:
            reasons.append('different ' + key)
    # When PCM verification was requested, both captures must include the
    # same generated byte count and rolling digest. Older captures omit it.
    if 'music_pcm' in left or 'music_pcm' in right:
        if not left.get('music_pcm') or not right.get('music_pcm'):
            reasons.append('missing music_pcm')
        elif left['music_pcm'] != right['music_pcm']:
            reasons.append('different music_pcm')
    if left.get('status') != 'complete' or right.get('status') != 'complete':
        reasons.append('incomplete probe')
    if not left['probe_segments'] and not right['probe_segments'] and left.get('route_targets', '0,0') != '0,0':
        # Target/aim routes are bound to the complete input-file hash below.
        # Their intermediate player/guard/RNG checkpoints must still match.
        pass
    elif not left['probe_segments'] or not right['probe_segments']:
        reasons.append('missing input script provenance')
    elif left['probe_segments'] != right['probe_segments']:
        reasons.append('different input script')
    for key in ('checkpoints', 'cadence'):
        if not left[key] or not right[key]:
            reasons.append('missing ' + key)
        elif left[key] != right[key]:
            reasons.append('different ' + key)
    # The runner verifies installed files before launch and after completion.
    # Different executables are the independent variable; assets must match.
    provenance = []
    for path in (before, after):
        sidecar = path.with_suffix('.provenance.json')
        data = json.loads(sidecar.read_text()) if sidecar.exists() else {}
        if not all(data.get(k) for k in ('binary_sha256', 'asset_sha256', 'input_sha256')):
            reasons.append('missing build/asset/input provenance')
        provenance.append(data)
    for key in ('asset_sha256', 'input_sha256'):
        if provenance[0].get(key) != provenance[1].get(key):
            reasons.append('different ' + key)
    return {'recorded_state_matches': not reasons, 'reasons': reasons,
            'limit': 'Matching recorded fields does not prove identical intermediate gameplay or hardware performance.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path, nargs='+')
    parser.add_argument('--warmup', type=int, default=120)
    parser.add_argument('--compare', action='store_true', help='gate a pair by recorded input and gameplay state')
    args = parser.parse_args()
    if args.warmup < 0:
        parser.error('--warmup must be nonnegative')
    if args.compare and len(args.results) != 2:
        parser.error('--compare requires exactly two results')
    summaries = [summarize(path, args.warmup) for path in args.results]
    output = {'comparison': comparison_gate(*args.results), 'results': summaries} if args.compare else summaries
    print(json.dumps(output, indent=2))

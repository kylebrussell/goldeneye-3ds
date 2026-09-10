#!/usr/bin/env python3
"""Summarize buffered ARM11 input-probe timings without rounded-ms thresholds."""
import argparse
import json
import math
import hashlib
import re
from pathlib import Path


def summarize(path, warmup, measure_frames=None):
    values = {}
    frames = []
    presents = []
    details = []
    pipeline = []
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
        elif key == 'frame_pipeline':
            items = value.split(',')
            if len(items) != 11:
                raise ValueError(f'{path}: invalid pipeline record')
            pipeline.append([float(item) if i in (2, 3, 4) else int(item)
                             for i, item in enumerate(items)])
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
    if pipeline and (len(pipeline) != len(frames) or any(
            row[0] != i + 1 or any(not math.isfinite(x) or x < 0 for x in row)
            or any(row[j] not in (0, 1) for j in (1, 6, 7, 8))
            or row[4] > 1 or row[5] not in (1, 2)
            for i, row in enumerate(pipeline))):
        raise ValueError(f'{path}: invalid or incomplete pipeline records')
    if pipeline and any(b[j] < a[j] for a, b in zip(pipeline, pipeline[1:]) for j in (9, 10)):
        raise ValueError(f'{path}: audio counters reversed')
    if measure_frames == 'all-after-warmup-except-final':
        measure_frames = len(frames) - warmup - 1
    if measure_frames is not None and (measure_frames < 1 or warmup + measure_frames > len(frames)):
        raise ValueError(f'{path}: measured window does not fit the captured frames')
    stop = len(frames) if measure_frames is None else warmup + measure_frames
    warm = frames[warmup:stop]
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
        'trailing_frames': len(frames) - stop,
        'clock_hz': hz,
        'budget': {'test': 'work_ticks * 60 > clock_hz', 'hz': 60},
        'stage_load_ms': int(values['stage_load_ticks']) * 1000 / hz
            if 'stage_load_ticks' in values else None,
        'music_timing_scope': values.get('music_timing_scope', 'main_synchronous'),
        'work_over_60hz_budget': sum(row[1] * 60 > hz for row in warm),
        'unsampled_work_over_budget': sum(row[1] * 60 > hz and not row[-1] for row in warm),
        'world_sampled_frames': sum(bool(row[-1]) for row in warm),
        # Version 1 accidentally recorded dispatches (always one), not retraces.
        'multiple_retrace_frames': sum(row[-2] > 1 for row in warm)
            if int(values.get('frame_timing_version', 1)) >= 2 else None,
        'submission_intervals': {
            'samples': len(presents[warmup + 1:stop]),
            'repeated_vblank': sum(b[3] == a[3] for a, b in zip(
                presents[warmup:stop], presents[warmup + 1:stop])),
            'skipped_vblank': sum(((b[3] - a[3]) & 0xffffffff) > 1 for a, b in zip(
                presents[warmup:stop], presents[warmup + 1:stop])),
            # Row N describes GPU work for frame N-1, not frame N.
            'previous_gpu_peak_us': max((row[2] for row in presents[warmup + 1:stop + 1]), default=None),
            'pacing_wait': stats(presents[warmup:stop], 1),
            'start_intervals': stats(frames[warmup + 1:stop], 2),
            'start_intervals_over_60hz': sum(row[2] * 60 > hz for row in frames[warmup + 1:stop]),
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
        result['dispatch_phases'] = {name: (stats(details[warmup:stop], i+1) if result['deep_profile_enabled'] or i<4 else None)
                                     for i, name in enumerate(names)}
        result['over_budget_attribution'] = []
        for row, detail in zip(frames[warmup:stop], details[warmup:stop]):
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
    result['pipeline'] = None
    if pipeline:
        measured = pipeline[warmup:stop]
        completed = [row for row in pipeline[warmup + 1:stop + 1] if row[1]]
        def microsecond_stats(column):
            samples = sorted(row[column] for row in completed)
            return None if not samples else {
                'mean_us': sum(samples) / len(samples),
                'p95_us': samples[math.ceil(len(samples) * .95) - 1],
                'p99_us': samples[math.ceil(len(samples) * .99) - 1],
                'peak_us': samples[-1]}
        result['pipeline'] = {
            'gpu_samples': len(completed),
            'gpu_missing_frames': [i + 1 for i in range(warmup, stop)
                                   if i + 1 >= len(pipeline) or not pipeline[i + 1][1]],
            'gpu_queue': microsecond_stats(2),
            'gpu_over_60hz_budget': sum(row[2] * 60 > 1000000 for row in completed),
            'citro3d_processing': microsecond_stats(3),
            'slider_min': min(row[4] for row in measured),
            'stereo_frames': sum(row[5] == 2 for row in measured),
            'ndsp_active_frames': sum(row[6] for row in measured),
            'music_rendered_frames': sum(row[7] for row in measured),
            'capture_io_frames': sum(row[8] for row in pipeline),
            'primary_audio_source_frames': measured[-1][9] - (pipeline[warmup - 1][9] if warmup else 0),
            'primary_audio_zero_fill_frames': measured[-1][10] - (pipeline[warmup - 1][10] if warmup else 0),
            'limits': 'GPU values are Citro3D float microseconds for the preceding completed queue, '
                      'including transfers and callback latency, not PICA occupancy. Citro3D processing '
                      'includes pacing and nests within frame work; do not add these values. '
                      'Primary audio zero-fill excludes the secondary music ring; it does not establish audible dropout.'}
    result['stereo_config'] = values.get('stereo_config')
    # Selecting a window must never hide late ordinary stalls. Report and
    # gate the full post-warmup CPU/retrace/presentation route, including its
    # final GPU guard frame (whose own GPU completion remains unavailable).
    result['route_after_warmup'] = {
        'frames': len(frames) - warmup,
        'work': stats(frames[warmup:], 1),
        'work_over_60hz_budget': sum(row[1] * 60 > hz for row in frames[warmup:]),
        'multiple_retrace_frames': sum(row[-2] > 1 for row in frames[warmup:])
            if int(values.get('frame_timing_version', 1)) >= 2 else None,
        'repeated_vblank': sum(b[3] == a[3] for a, b in zip(presents[warmup:], presents[warmup + 1:])),
        'skipped_vblank': sum(((b[3] - a[3]) & 0xffffffff) > 1 for a, b in zip(presents[warmup:], presents[warmup + 1:])),
        'trailing_work': stats(frames[stop:], 1),
        'trailing_work_over_60hz_budget': sum(row[1] * 60 > hz for row in frames[stop:]),
    }
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
        environment = []
        for line in path.read_text().splitlines():
            key, sep, value = line.partition('=')
            if sep:
                if key == 'probe_segment':
                    segments.append(value)
                elif key == 'probe_checkpoint':
                    checkpoints.append(value)
                elif key == 'frame_timing':
                    cadence.append(value.split(',')[-2])
                elif key == 'frame_pipeline':
                    environment.append(value.split(',')[4:9])
                elif key in required or key in ('probe_detail', 'music_pcm', 'stereo_config'):
                    fields[key] = value
        fields['probe_segments'] = segments
        fields['checkpoints'] = checkpoints
        fields['cadence'] = cadence
        fields['environment'] = environment
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
    for key in ('stereo_config', 'environment'):
        if left.get(key) or right.get(key):
            if not left.get(key) or not right.get(key):
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
    for key in ('asset_manifest_sha256', 'configuration_sha256', 'cadence_sha256',
                'environment', 'environment_description', 'mode', 'warmup', 'samples'):
        if key in provenance[0] or key in provenance[1]:
            if key not in provenance[0] or key not in provenance[1]:
                reasons.append('missing ' + key)
            elif provenance[0][key] != provenance[1][key]:
                reasons.append('different ' + key)
    return {'recorded_state_matches': not reasons, 'reasons': reasons,
            'limit': 'Matching recorded fields does not prove identical intermediate gameplay or hardware performance.'}


def sustained_60hz_gate(summary, min_samples=3600):
    """Require both frame work and uninterrupted presentation in this capture.

    Passing certifies only the recorded route on the device that produced it.
    In particular, an emulator trace cannot certify physical hardware FPS.
    """
    if min_samples < 2:
        raise ValueError('min_samples must be at least 2')
    reasons = []
    if summary['status'] != 'complete':
        reasons.append('incomplete probe')
    if summary['samples'] < min_samples:
        reasons.append(f'fewer than {min_samples} measured frames')
    if summary['work_over_60hz_budget']:
        reasons.append('frame work exceeded the exact 60 Hz budget')
    route = summary.get('route_after_warmup', {})
    if route.get('trailing_work_over_60hz_budget'):
        reasons.append('post-window route work exceeded the exact 60 Hz budget')
    if summary.get('pipeline') and summary['pipeline']['gpu_over_60hz_budget']:
        reasons.append('completed Citro3D queue elapsed time exceeded the 60 Hz budget')
    if summary['multiple_retrace_frames'] is None:
        reasons.append('missing trustworthy retrace counts')
    elif summary['multiple_retrace_frames']:
        reasons.append('simulation advanced across multiple retraces')
    elif route.get('multiple_retrace_frames'):
        reasons.append('post-window simulation advanced across multiple retraces')
    presents = summary['submission_intervals']
    if presents is None or presents['samples'] != summary['samples'] - 1:
        reasons.append('missing complete presentation intervals')
    else:
        if presents['skipped_vblank']:
            reasons.append('presentation skipped VBlanks')
        elif route.get('skipped_vblank'):
            reasons.append('post-window presentation skipped VBlanks')
        if presents['repeated_vblank']:
            reasons.append('multiple submissions in one VBlank')
        elif route.get('repeated_vblank'):
            reasons.append('post-window repeated submissions in one VBlank')
    return {'passed': not reasons, 'reasons': reasons,
            'minimum_samples': min_samples,
            'limit': 'Applies only to this captured route and environment; '
                     'does not certify other stages, stereo settings, fidelity, '
                     'or physical hardware from emulator measurements.'}


def benchmark_evidence_gate(path, summary):
    """Audit reproducibility and effective settings, independently of speed."""
    sidecar = path.with_suffix('.provenance.json')
    provenance = json.loads(sidecar.read_text()) if sidecar.exists() else {}
    reasons = []
    if provenance.get('schema') != 'ge-3ds-benchmark-v1' or provenance.get('inventory_verified') is not True:
        reasons.append('missing verified pre/post inventory provenance')
    for key in ('binary_sha256', 'asset_sha256', 'input_sha256', 'asset_manifest_sha256',
                'configuration_sha256', 'result_sha256', 'request_sha256', 'before_sha256', 'after_sha256'):
        if not re.fullmatch(r'[0-9a-f]{64}', str(provenance.get(key, ''))):
            reasons.append('missing valid ' + key)
    if provenance.get('result_sha256') != hashlib.sha256(path.read_bytes()).hexdigest():
        reasons.append('result differs from verified inventory')
    if provenance.get('mode') != 'clean' or summary['deep_profile_enabled'] or summary['world_sampled_frames']:
        reasons.append('clean timing requires deep/world/PCM diagnostics disabled')
    if summary['samples'] < 3600 or summary['warmup'] < 1:
        reasons.append('benchmark requires declared warmup and at least 3600 measured frames')
    if provenance.get('warmup') != summary['warmup'] or provenance.get('samples') not in (
            summary['samples'], 'all-after-warmup-except-final'):
        reasons.append('analyzed window differs from declared request')
    if summary['trailing_frames'] != 1:
        reasons.append('clean benchmark must cover the full route except exactly one trailing GPU guard frame')
    if provenance.get('environment') not in ('emulator', 'physical-new3dsxl'):
        reasons.append('missing measurement environment')
    for key in ('environment_description', 'operator_observations', 'toolchain'):
        if not provenance.get(key):
            reasons.append('missing ' + key)
    source = provenance.get('source', {})
    if not source.get('commit') or not source.get('source_tree_sha256'):
        reasons.append('missing source identity')
    if provenance.get('run_kind') not in ('cold', 'repeat'):
        reasons.append('missing cold/repeat declaration')
    if provenance.get('audible_confirmed') is not True:
        reasons.append('operator has not confirmed audible music and SFX')
    pipeline = summary['pipeline']
    if pipeline is None:
        reasons.append('missing effective stereo/audio and completed GPU telemetry')
    else:
        if pipeline['gpu_missing_frames']:
            reasons.append('missing completed GPU samples; include the declared trailing guard frame')
        if pipeline['slider_min'] < 1 or pipeline['stereo_frames'] != summary['samples']:
            reasons.append('maximum slider and both eyes were not active for every measured frame')
        if pipeline['ndsp_active_frames'] != summary['samples'] or pipeline['music_rendered_frames'] != summary['samples']:
            reasons.append('NDSP and music synthesis were not active for every measured frame')
        if pipeline['primary_audio_source_frames'] <= 0:
            reasons.append('no primary audio output consumed')
        if pipeline['capture_io_frames']:
            reasons.append('diagnostic capture I/O contaminated this route')
    try:
        stereo = [float(x) for x in summary['stereo_config'].split(',')]
        if len(stereo) != 7 or any(not math.isclose(a, b, rel_tol=1e-7) for a, b in
                zip(stereo, (1, 8, 180, 3, 45, 2, .00625))):
            reasons.append('effective stereo depth differs from the full default configuration')
    except (TypeError, AttributeError, ValueError):
        reasons.append('missing effective stereo configuration')
    if summary['stage_load_ms'] is None:
        reasons.append('missing separately measured stage startup')
    return {'passed': not reasons, 'reasons': reasons,
            'environment': provenance.get('environment'),
            'limit': 'An evidence gate cannot establish subjective quality or hardware performance from an emulator. '
                     'VBlank counters observe submission cadence, not optical scanout. GPU is queue elapsed time.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path, nargs='+')
    parser.add_argument('--warmup', type=int, default=120)
    parser.add_argument('--measure-frames', type=int,
                        help='declared measured window; retain at least one trailing frame for completed GPU timing')
    parser.add_argument('--compare', action='store_true', help='gate a pair by recorded input and gameplay state')
    parser.add_argument('--require-60hz', action='store_true',
                        help='exit unsuccessfully unless all captures sustain measured 60 Hz')
    parser.add_argument('--require-benchmark', action='store_true',
                        help='also require verified clean audio/stereo/GPU evidence and at least 3600 measured frames')
    parser.add_argument('--min-samples', type=int, default=3600,
                        help='minimum post-warmup frames for --require-60hz (default: 3600)')
    args = parser.parse_args()
    if args.warmup < 0:
        parser.error('--warmup must be nonnegative')
    if args.compare and len(args.results) != 2:
        parser.error('--compare requires exactly two results')
    if args.min_samples < 2:
        parser.error('--min-samples must be at least 2')
    window = args.measure_frames
    if args.require_benchmark and window is None:
        window = 'all-after-warmup-except-final'
    summaries = [summarize(path, args.warmup, window) for path in args.results]
    if args.require_benchmark:
        args.require_60hz = True
        for path, summary in zip(args.results, summaries):
            summary['benchmark_evidence'] = benchmark_evidence_gate(path, summary)
    if args.require_60hz:
        for summary in summaries:
            summary['sustained_60hz'] = sustained_60hz_gate(summary, args.min_samples)
    output = {'comparison': comparison_gate(*args.results), 'results': summaries} if args.compare else summaries
    print(json.dumps(output, indent=2))
    if args.require_60hz and (any(not item['sustained_60hz']['passed'] for item in summaries)
            or (args.require_benchmark and any(not item['benchmark_evidence']['passed'] for item in summaries))
            or (args.compare and not output['comparison']['recorded_state_matches'])):
        raise SystemExit(1)

#!/usr/bin/env python3
"""Check frame-budget boundaries and reject incomplete benchmark evidence."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import subprocess
import sys
import hashlib

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('timing', ROOT / 'scripts/analyze_3ds_frame_timing.py')
TIMING = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TIMING)


class TimingTests(unittest.TestCase):
    def analyze(self, version=2, rows=None, presents=None, count=3, warmup=1, metadata=''):
        if rows is None:
            rows = [[i, work] + [0] * 16 + [retraces, sampled]
                    for i, work, retraces, sampled in [(1, 99, 4, 0), (2, 1, 1, 0), (3, 2, 2, 1)]]
        text = f'runtime_profile_tick_hz=60\nframe_timing_version={version}\nframe_timing_count={count}\n'
        text += metadata
        text += ''.join('frame_timing=' + ','.join(map(str, row)) + '\n' for row in rows)
        text += ''.join('frame_present_timing=' + ','.join(map(str, row)) + '\n' for row in presents or [])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'probe.result'
            path.write_text(text)
            return TIMING.summarize(path, warmup)

    def test_exact_budget_warmup_and_sampling(self):
        result = self.analyze()
        self.assertEqual(result['samples'], 2)
        self.assertEqual(result['work_over_60hz_budget'], 1)
        self.assertEqual(result['unsampled_work_over_budget'], 0)
        self.assertEqual(result['multiple_retrace_frames'], 1)

    def test_legacy_retrace_count_is_unknown(self):
        self.assertIsNone(self.analyze(version=1)['multiple_retrace_frames'])

    def test_worker_timing_scope_is_explicit(self):
        self.assertEqual(self.analyze()['music_timing_scope'], 'main_synchronous')
        result = self.analyze(metadata='music_timing_scope=main_prepare_and_join\n')
        self.assertEqual(result['music_timing_scope'], 'main_prepare_and_join')
        self.assertEqual(result['work_over_60hz_budget'], 1)

    def test_submission_wrap_duplicate_and_skip(self):
        result = self.analyze(presents=[[1, 0, 10, 0xffffffff], [2, 1, 20, 0], [3, 2, 30, 1]], warmup=0)
        self.assertEqual(result['submission_intervals']['skipped_vblank'], 0)
        result = self.analyze(presents=[[1, 0, 10, 1], [2, 1, 20, 1], [3, 2, 30, 3]], warmup=0)
        self.assertEqual(result['submission_intervals']['repeated_vblank'], 1)
        self.assertEqual(result['submission_intervals']['skipped_vblank'], 1)

    def test_incomplete_records_rejected(self):
        for arguments in ({'count': 4}, {'rows': []}, {'presents': [[1, 0, 10, 1]]}, {'warmup': 3}, {'warmup': -1}):
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                self.analyze(**arguments)

    def test_mixed_width_and_nonsequential_rejected(self):
        for rows in ([[1] * 20, [2] * 13, [3] * 20], [[1] * 20, [3] * 20, [3] * 20]):
            with self.assertRaises(ValueError):
                self.analyze(rows=rows)

    def test_sustained_gate_requires_work_and_presentation(self):
        rows = [[i, 1] + [0] * 16 + [1, 0] for i in range(1, 4)]
        arguments = dict(rows=rows, warmup=0, metadata='status=complete\n',
                         presents=[[i, 0, 10, i] for i in range(1, 4)])
        result = self.analyze(**arguments)
        self.assertTrue(TIMING.sustained_60hz_gate(result, 3)['passed'])
        self.assertFalse(TIMING.sustained_60hz_gate(result, 4)['passed'])
        for changes in (
                {'metadata': 'status=running\n'}, {'version': 1},
                {'presents': None},
                {'presents': [[1, 0, 10, 1], [2, 0, 10, 1], [3, 0, 10, 2]]},
                {'presents': [[1, 0, 10, 1], [2, 0, 10, 3], [3, 0, 10, 4]]},
                {'rows': [[i, 2] + [0] * 16 + [1, 0] for i in range(1, 4)]},
                {'rows': [[i, 1] + [0] * 16 + [2, 0] for i in range(1, 4)]}):
            with self.subTest(changes=changes):
                result = self.analyze(**(arguments | changes))
                self.assertFalse(TIMING.sustained_60hz_gate(result, 3)['passed'])
        with self.assertRaises(ValueError):
            TIMING.sustained_60hz_gate(result, 1)

    def test_sustained_gate_cli_fails_incomplete_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'probe.result'
            lines = ['status=complete', 'runtime_profile_tick_hz=60',
                     'frame_timing_version=2', 'frame_timing_count=2']
            lines += ['frame_timing=' + ','.join(map(str,
                      [i, 1] + [0] * 16 + [1, 0])) for i in (1, 2)]
            path.write_text('\n'.join(lines))
            command = [sys.executable, str(ROOT / 'scripts/analyze_3ds_frame_timing.py'),
                       str(path), '--warmup', '0', '--require-60hz', '--min-samples', '2']
            run = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(run.returncode, 1, run.stderr)
            self.assertFalse(json.loads(run.stdout)[0]['sustained_60hz']['passed'])
            lines += [f'frame_present_timing={i},0,10,{i}' for i in (1, 2)]
            path.write_text('\n'.join(lines))
            run = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)


class ComparisonTests(unittest.TestCase):
    def test_gate_refuses_missing_or_different_state(self):
        fields = dict(status='complete', level_id='39', frames='750', simulation_frames='750',
                      start='1,2,3', end='4,5,6', start_look='1,0,0', end_look='1,0,0',
                      room_trace='62@0', objective_status='0', mission_result='0,0,0',
                      player_death='0', door_interaction='0', door_runtime='0',
                      probe_initial_rng='abc,def', probe_clock='recorded-retraces-v1',
                      route_targets='0,0', music_pcm='0123456789abcdef,6400',
                      probe_checkpoint='1,abc,def,012,1,2,3',
                      frame_timing='1,'+','.join(['0']*17+['1','0']),
                      probe_segment='750,0,0,0,0,0')
        with tempfile.TemporaryDirectory() as directory:
            before, after = Path(directory)/'before', Path(directory)/'after'
            for path in (before, after):
                path.with_suffix('.provenance.json').write_text(json.dumps({
                    'binary_sha256':'abc', 'asset_sha256':'def', 'input_sha256':'ghi'}))
            def write(path, values):
                path.write_text(''.join(f'{key}={value}\n' for key,value in values.items()))
            write(before, fields); write(after, fields)
            self.assertTrue(TIMING.comparison_gate(before, after)['recorded_state_matches'])
            for key in fields:
                changed = dict(fields)
                changed[key] = changed[key].replace(',1,0', ',2,0') if key=='frame_timing' else changed[key]+'x'
                write(after, changed)
                self.assertFalse(TIMING.comparison_gate(before, after)['recorded_state_matches'], key)
                del changed[key]; write(after, changed)
                self.assertFalse(TIMING.comparison_gate(before, after)['recorded_state_matches'], key)

            write(after, fields)
            sidecar=after.with_suffix('.provenance.json')
            sidecar.unlink()
            self.assertFalse(TIMING.comparison_gate(before, after)['recorded_state_matches'])


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / 'measured.result'

    def write(self, count=5, changes=None):
        self.pipeline = [[i, int(i > 1), 1000.25, 5000.5, 1, 2, 1, 1, 0, 256*i, 0]
                         for i in range(1, count + 1)]
        if changes:
            changes(self.pipeline)
        text = ('status=complete\nruntime_profile_tick_hz=60\nframe_timing_version=4\n'
                f'frame_timing_count={count}\nprobe_detail=0\nstage_load_ticks=120\n'
                'stereo_config=1,8,180,3,45,2,0.00625\n')
        for i in range(1, count + 1):
            text += 'frame_timing=' + ','.join(map(str, [i, 1]+[0]*16+[1, 0])) + '\n'
            text += f'frame_present_timing={i},0,1000,{i}\n'
        text += ''.join('frame_pipeline=' + ','.join(map(str, row)) + '\n' for row in self.pipeline)
        self.path.write_text(text)

    def provenance(self, **changes):
        data = {key: 'a'*64 for key in ('binary_sha256', 'asset_sha256', 'input_sha256',
                'asset_manifest_sha256', 'configuration_sha256', 'request_sha256', 'before_sha256', 'after_sha256')}
        data.update(schema='ge-3ds-benchmark-v1', inventory_verified=True, mode='clean',
                    samples=3600, warmup=1, environment='emulator', environment_description='isolated test',
                    operator_observations='test fixture, no real performance claim', toolchain='test',
                    source={'commit':'b'*40, 'source_tree_sha256':'c'*64}, run_kind='cold', audible_confirmed=True,
                    result_sha256=hashlib.sha256(self.path.read_bytes()).hexdigest())
        data.update(changes)
        self.path.with_suffix('.provenance.json').write_text(json.dumps(data))

    def test_gpu_is_previous_frame_and_guard_covers_window(self):
        def changes(rows):
            rows[1][2] = 999999  # GPU for warmup frame 1, never measured frame 2.
            rows[4][2] = 7000.75  # GPU for measured frame 4, in trailing row 5.
        self.write(changes=changes)
        summary = TIMING.summarize(self.path, 1, 3)
        self.assertEqual(summary['trailing_frames'], 1)
        self.assertEqual(summary['pipeline']['gpu_samples'], 3)
        self.assertEqual(summary['pipeline']['gpu_missing_frames'], [])
        self.assertEqual(summary['pipeline']['gpu_queue']['peak_us'], 7000.75)
        self.assertEqual(summary['pipeline']['gpu_over_60hz_budget'], 0)
        self.assertEqual(summary['stage_load_ms'], 2000)
        summary = TIMING.summarize(self.path, 1)
        self.assertEqual(summary['pipeline']['gpu_missing_frames'], [5])
        self.assertEqual(summary['pipeline']['primary_audio_source_frames'], 1024)

    def test_fractional_gpu_budget_and_invalid_completion(self):
        self.write(changes=lambda rows: rows[2].__setitem__(2, 16666.668))
        summary = TIMING.summarize(self.path, 1, 3)
        self.assertEqual(summary['pipeline']['gpu_over_60hz_budget'], 1)
        self.assertFalse(TIMING.sustained_60hz_gate(summary, 3)['passed'])
        self.write(changes=lambda rows: rows[2].__setitem__(1, 0))
        summary = TIMING.summarize(self.path, 1, 3)
        self.assertEqual(summary['pipeline']['gpu_missing_frames'], [2])

    def test_bad_pipeline_records_and_windows_are_rejected(self):
        for column, value in [(1, 2), (2, float('nan')), (3, -1), (4, 1.01),
                              (5, 0), (6, 2), (7, -1), (8, 2), (9, 0)]:
            with self.subTest(column=column, value=value):
                self.write(changes=lambda rows: rows[2].__setitem__(column, value))
                with self.assertRaises(ValueError):
                    TIMING.summarize(self.path, 1)
        self.write()
        for samples in (0, -1, 5):
            with self.assertRaises(ValueError):
                TIMING.summarize(self.path, 1, samples)

    def test_declared_full_route_policy_and_uncropped_late_stalls(self):
        self.write(3603)
        self.provenance(samples='all-after-warmup-except-final')
        summary = TIMING.summarize(self.path, 1, 'all-after-warmup-except-final')
        self.assertEqual(summary['samples'], 3601)
        self.assertTrue(TIMING.benchmark_evidence_gate(self.path, summary)['passed'])
        cropped = TIMING.summarize(self.path, 1, 3600)
        self.assertFalse(TIMING.benchmark_evidence_gate(self.path, cropped)['passed'])
        # Ordinary late work, including the final GPU guard, remains gated.
        self.path.write_text(self.path.read_text().replace('frame_timing=3603,1,', 'frame_timing=3603,2,'))
        summary = TIMING.summarize(self.path, 1, 3600)
        self.assertEqual(summary['work_over_60hz_budget'], 0)
        self.assertEqual(summary['route_after_warmup']['trailing_work_over_60hz_budget'], 1)
        self.assertFalse(TIMING.sustained_60hz_gate(summary, 3600)['passed'])

    def test_evidence_requires_actual_audio_stereo_clean_route_and_identity(self):
        self.write(3602)
        self.provenance()
        summary = TIMING.summarize(self.path, 1, 3600)
        self.assertTrue(TIMING.benchmark_evidence_gate(self.path, summary)['passed'])
        for change in ({'audible_confirmed': False}, {'inventory_verified': False},
                       {'result_sha256': 'a'*64}, {'warmup': 2}, {'samples': 3599},
                       {'mode': 'pcm'}, {'source': {}}, {'environment': 'unknown'}):
            self.provenance(**change)
            self.assertFalse(TIMING.benchmark_evidence_gate(self.path, summary)['passed'], change)
        for column, value, frame in [(1, 0, 2), (4, .99, 1), (5, 1, 1),
                                     (6, 0, 1), (7, 0, 1), (8, 1, 0)]:
            self.write(3602, lambda rows: rows[frame].__setitem__(column, value))
            self.provenance()
            summary = TIMING.summarize(self.path, 1, 3600)
            self.assertFalse(TIMING.benchmark_evidence_gate(self.path, summary)['passed'], column)


if __name__ == '__main__':
    unittest.main()

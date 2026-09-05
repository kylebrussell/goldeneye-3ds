#!/usr/bin/env python3
"""Check frame-budget boundaries and reject incomplete benchmark evidence."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

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


if __name__ == '__main__':
    unittest.main()

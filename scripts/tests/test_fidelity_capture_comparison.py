#!/usr/bin/env python3
"""Synthetic fixtures validate evidence gating; these are not N64 captures."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('comparison', ROOT / 'scripts/compare_fidelity_captures.py')
C = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(C)


class CaptureComparisonTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.serial = 0
        self.write('rom', b'original ROM fixture, not game data')
        self.write('port', b'port executable fixture')
        self.write('replay', b'0,1,0,0\n1,1,0,0\n')
        self.write('retraces', b'1,1\n2,1\n')
        for name in ('log', 'settings', 'recipe', 'assets'):
            self.write(name, ('synthetic ' + name).encode())
        self.pixels = bytes((10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120))
        self.ppm('reference.ppm', 2, 2, self.pixels)
        self.ppm('candidate.ppm', 2, 2, self.pixels)
        identity = [int(i % 5 == 0) for i in range(16)]
        self.state = {
            'schema': 1, 'scene': {'stage_id': 33, 'difficulty': 0, 'screen': 'mission'},
            'clock': {'tick': 2, 'hz': 60, 'phase': 'render-input',
                      'retrace_trace_sha256': C.digest(self.root / 'retraces')},
            'rng': [123, 456],
            'camera': {'convention': 'n64-authoring-row-major',
                       'view': identity, 'projection': identity},
            'channels': {key: [] for key in C.CHANNELS}}
        self.state['channels']['doors'] = [{'id': 5, 'fraction': 0}]
        self.write('state.json', self.state)
        initial = copy.deepcopy(self.state)
        initial['clock']['tick'] = 0
        self.write('initial.json', initial)

    def write(self, name, contents):
        path = self.root / name
        if isinstance(contents, dict):
            path.write_text(json.dumps(contents))
        else:
            path.write_bytes(contents)
        return path

    def ppm(self, name, width, height, pixels):
        return self.write(name, f'P6\n{width} {height}\n255\n'.encode() + pixels)

    def descriptor(self, port=False):
        artifacts = dict(source_rom='rom', binary='port' if port else 'rom', input='replay',
                         retrace_trace='retraces', initial_state='initial.json', state='state.json',
                         execution_log='log', settings='settings', recipe='recipe')
        if port:
            artifacts['assets'] = 'assets'
        return {'schema': 1,
                'execution': {'kind': 'port-3ds-emulator' if port else 'original-n64-emulator',
                              'producer': 'fixture', 'version': 'unit-test-1',
                              'video_backend': 'fixture', 'state_method': 'runtime-instrumented'},
                'stereo_slider': 0,
                'artifacts': {key: {'path': path} for key, path in artifacts.items()},
                'captures': [{'eye': 'left' if port else 'mono',
                              'path': 'candidate.ppm' if port else 'reference.ppm',
                              'crop': [0, 0, 2, 2], 'pixel_aspect': [1, 1], 'tick': 2}]}

    def seal(self, descriptor):
        self.serial += 1
        source = self.write(f'descriptor-{self.serial}.json', descriptor)
        target = self.root / f'manifest-{self.serial}.json'
        C.seal(source, target)
        return target

    def compare(self, reference=None, candidate=None, **kwargs):
        a = self.seal(reference or self.descriptor())
        b = self.seal(candidate or self.descriptor(True))
        output = self.root / f'report-{self.serial}'
        return C.compare(a, b, output, **kwargs), output

    def test_identical_mono_and_zero_slider_both_eyes(self):
        candidate = self.descriptor(True)
        candidate['captures'].append(dict(candidate['captures'][0], eye='right'))
        report, output = self.compare(candidate=candidate, require_matched=True, max_mae=0)
        self.assertTrue(report['matched_recorded_state'])
        self.assertTrue(report['acceptance']['passed'])
        self.assertEqual(len(report['pairs']), 2)
        self.assertEqual([p['metrics']['changed_pixels'] for p in report['pairs']], [0, 0])
        self.assertEqual(json.loads((output / 'report.json').read_text()), report)
        self.assertTrue((output / 'index.html').is_file())
        self.assertTrue((output / 'right-difference.png').is_file())
        self.assertIn('producer declarations', report['limit'])

    def test_known_rgb_error_and_strict_threshold(self):
        pixels = bytearray(self.pixels)
        pixels[0] += 12
        self.ppm('candidate.ppm', 2, 2, bytes(pixels))
        report, _ = self.compare(max_mae=1)
        metrics = report['pairs'][0]['metrics']
        self.assertEqual(metrics['mean_absolute_error_8bit'], 1)
        self.assertEqual(metrics['changed_pixels'], 1)
        self.assertEqual(metrics['max_channel_error_8bit'], 12)
        self.assertAlmostEqual(metrics['root_mean_square_error_8bit'], 12 ** .5)
        self.assertTrue(report['acceptance']['passed'])
        report, _ = self.compare(max_mae=.999)
        self.assertFalse(report['acceptance']['passed'])

    def test_explicit_pillarbox_crop_preserves_pixels(self):
        row0, row1 = self.pixels[:6], self.pixels[6:]
        self.ppm('candidate.ppm', 4, 2, b'\xff' * 3 + row0 + b'\xff' * 6 + row1 + b'\xff' * 3)
        candidate = self.descriptor(True)
        candidate['captures'][0]['crop'] = [1, 0, 2, 2]
        report, _ = self.compare(candidate=candidate)
        self.assertEqual(report['pairs'][0]['metrics']['changed_pixels'], 0)
        self.assertEqual(report['pairs'][0]['candidate_crop'], [1, 0, 2, 2])

    def test_no_implicit_resize_aspect_correction_or_out_of_bounds_crop(self):
        for update in ({'crop': [0, 0, 1, 2]}, {'pixel_aspect': [2, 1]}, {'crop': [1, 1, 2, 2]}):
            candidate = self.descriptor(True)
            candidate['captures'][0].update(update)
            with self.subTest(update=update), self.assertRaises(ValueError):
                self.compare(candidate=candidate)

    def test_all_provenance_and_state_artifacts_required(self):
        for key in C.REQUIRED + ('assets',):
            candidate = self.descriptor(True)
            del candidate['artifacts'][key]
            with self.subTest(key=key):
                report, _ = self.compare(candidate=candidate, require_matched=True)
                self.assertFalse(report['matched_recorded_state'])
                self.assertFalse(report['acceptance']['passed'])
                self.assertTrue(any(key in reason for reason in report['mismatch_reasons']))

    def test_port_only_synthetic_unknown_manual_and_stereo_cannot_pass(self):
        for kind in ('port-3ds-emulator', 'synthetic', 'unknown'):
            reference = self.descriptor()
            reference['execution']['kind'] = kind
            report, _ = self.compare(reference=reference, max_mae=255)
            self.assertFalse(report['acceptance']['passed'])
        for update in ({'state_method': 'manual'}, {'version': 'unknown'}, {'video_backend': 'unknown'}):
            candidate = self.descriptor(True)
            candidate['execution'].update(update)
            report, _ = self.compare(candidate=candidate, require_matched=True)
            self.assertFalse(report['matched_recorded_state'])
        candidate = self.descriptor(True)
        candidate['stereo_slider'] = .5
        report, _ = self.compare(candidate=candidate, max_mae=255)
        self.assertFalse(report['acceptance']['passed'])
        self.assertEqual(report['classification'], 'unmatched-exploratory')

    def test_archival_unknowns_are_preserved_without_inventing_provenance(self):
        candidate = self.descriptor(True)
        candidate['execution'].update(version='unknown', state_method='unknown')
        candidate['stereo_slider'] = None
        candidate['captures'][0]['tick'] = None
        candidate['artifacts'] = {'execution_log': {'path': 'log'}}
        report, _ = self.compare(reference=candidate, candidate=candidate,
                                 reference_eye='left', require_matched=True)
        self.assertFalse(report['matched_recorded_state'])
        self.assertFalse(report['acceptance']['passed'])
        self.assertEqual(report['pairs'][0]['metrics']['changed_pixels'], 0)
        self.assertEqual(report['pairs'][0]['reference_eye'], 'left')

    def test_invalid_snapshot_values_and_empty_artifacts_are_not_evidence(self):
        for changes in ({'schema': True}, {'rng': []}, {'rng': [True]},
                        {'camera': {}}, {'clock': {}}, {'channels': []}):
            state = copy.deepcopy(self.state)
            state.update(changes)
            self.write('state.json', state)
            report, _ = self.compare()
            self.assertFalse(report['matched_recorded_state'], changes)
        self.write('state.json', self.state)
        self.write('log', b'')
        report, _ = self.compare()
        self.assertFalse(report['matched_recorded_state'])

    def test_cameras_ticks_retraces_rng_gameplay_and_frontend_must_match(self):
        def camera(state):
            state['camera']['view'][12] += .000001
        def projection(state):
            state['camera']['projection'][8] += .000001
        def rng(state):
            state['rng'][0] += 1
        def clock(state):
            state['clock']['tick'] += 1
        def retraces(state):
            state['clock']['retrace_trace_sha256'] = 'a' * 64
        def doors(state):
            state['channels']['doors'][0]['fraction'] = .25
        def effects(state):
            state['channels']['effects'] = [{'smoke_age': 1}]
        def frontend(state):
            state['channels']['frontend'] = {'watch_page': 3}
        for edit in (camera, projection, rng, clock, retraces, doors, effects, frontend):
            changed = copy.deepcopy(self.state)
            edit(changed)
            self.write('changed.json', changed)
            candidate = self.descriptor(True)
            candidate['artifacts']['state']['path'] = 'changed.json'
            with self.subTest(edit=edit.__name__):
                report, _ = self.compare(candidate=candidate)
                self.assertFalse(report['matched_recorded_state'])
                self.assertTrue(report['mismatch_reasons'])

    def test_initial_state_and_source_content_must_match(self):
        for key in ('source_rom', 'input', 'retrace_trace', 'initial_state'):
            candidate = self.descriptor(True)
            if key == 'initial_state':
                changed = copy.deepcopy(self.state)
                changed['rng'][0] += 1
                self.write('changed-initial', changed)
            else:
                self.write('changed-initial', b'other artifact')
            candidate['artifacts'][key]['path'] = 'changed-initial'
            report, _ = self.compare(candidate=candidate)
            self.assertFalse(report['matched_recorded_state'], key)
        reference = self.descriptor()
        reference['artifacts']['binary']['path'] = 'port'
        report, _ = self.compare(reference=reference)
        self.assertIn('reference binary differs from declared original source ROM', report['mismatch_reasons'])

    def test_missing_channels_do_not_match_even_when_both_omit_them(self):
        for channel in C.CHANNELS:
            state = copy.deepcopy(self.state)
            del state['channels'][channel]
            self.write('state.json', state)
            report, _ = self.compare(require_matched=True)
            self.assertFalse(report['matched_recorded_state'], channel)
        self.write('state.json', self.state)

    def test_capture_tick_bound_to_instrumented_render_input(self):
        candidate = self.descriptor(True)
        candidate['captures'][0]['tick'] += 1
        report, _ = self.compare(candidate=candidate)
        self.assertIn('candidate.left: capture tick differs from state', report['mismatch_reasons'])

    def test_all_artifacts_and_capture_bytes_are_revalidated(self):
        for key in ('rom', 'port', 'replay', 'retraces', 'initial.json', 'state.json',
                    'log', 'settings', 'recipe', 'assets', 'candidate.ppm'):
            path = self.root / key
            original = path.read_bytes()
            sealed = self.seal(self.descriptor(True))
            # Equal-size corruption must be caught by the digest, not only length.
            path.write_bytes(bytes([original[0] ^ 1]) + original[1:])
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, 'hash/size mismatch'):
                C.verify(sealed)
            path.write_bytes(original)

    def test_no_evidence_or_report_overwrite(self):
        descriptor = self.write('descriptor.json', self.descriptor())
        with self.assertRaisesRegex(ValueError, 'refusing to replace'):
            C.seal(descriptor, descriptor)
        reference, candidate = self.seal(self.descriptor()), self.seal(self.descriptor(True))
        with self.assertRaisesRegex(ValueError, 'refusing to replace'):
            C.compare(reference, candidate, self.root)

    def test_malformed_json_ppm_and_duplicate_eyes_rejected(self):
        for data in (b'', b'P6', b'P6\n# comment', b'P6\n1 1\n255\n',
                     b'P6\n0 1\n255\n', b'P3\n1 1\n255\n123'):
            with self.subTest(data=data), self.assertRaises(ValueError):
                C.read_image(self.write('bad.ppm', data))
        for text in (b'{"x":1,"x":2}', b'{"x":NaN}', b'{"x":1e999}'):
            with self.assertRaises(ValueError):
                C.load_json(self.write('bad.json', text))
        candidate = self.descriptor(True)
        candidate['captures'].append(candidate['captures'][0])
        with self.assertRaises(ValueError):
            self.seal(candidate)
        for slider in (float('nan'), float('inf'), -1, 2, True):
            candidate = self.descriptor(True)
            candidate['stereo_slider'] = slider
            with self.assertRaises(ValueError):
                self.seal(candidate)

    def test_ppm_comments_crlf_and_whitespace_pixel_bytes(self):
        pixels = b'\n \r'
        for header in (b'P6\n# test\n1 1\n255\n', b'P6\r\n1 1\r\n255\r\n'):
            path = self.write('valid.ppm', header + pixels)
            self.assertEqual(C.read_image(path), (1, 1, pixels))

    def test_optional_png_input_roundtrip(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest('Pillow unavailable; PPM input remains dependency-free')
        path = self.root / 'test.png'
        C.write_png(path, 2, 2, self.pixels)
        self.assertEqual(C.read_image(path), (2, 2, self.pixels))

    def test_cli_statuses_and_reported_acceptance(self):
        a, b = self.seal(self.descriptor()), self.seal(self.descriptor(True))
        command = [sys.executable, str(ROOT / 'scripts/compare_fidelity_captures.py'),
                   'compare', str(a), str(b)]
        run = subprocess.run(command + ['--out-dir', str(self.root / 'success'),
                             '--require-matched', '--max-mae', '0'], capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertTrue(json.loads(run.stdout)['acceptance']['passed'])
        self.write('rom', b'changed')
        run = subprocess.run(command + ['--out-dir', str(self.root / 'invalid')], capture_output=True, text=True)
        self.assertEqual(run.returncode, 2)
        self.assertIn('hash/size mismatch', run.stderr)
        a = self.descriptor()
        a['execution']['kind'] = 'synthetic'
        command[3], command[4] = str(self.seal(a)), str(self.seal(self.descriptor(True)))
        run = subprocess.run(command + ['--out-dir', str(self.root / 'unmatched'),
                             '--max-mae', '255'], capture_output=True, text=True)
        self.assertEqual(run.returncode, 1, run.stderr)
        self.assertFalse(json.loads(run.stdout)['acceptance']['passed'])
        for limit in ('nan', 'inf', '-1', '256'):
            run = subprocess.run(command + ['--out-dir', str(self.root / 'bad-limit'),
                                 '--max-mae', limit], capture_output=True, text=True)
            self.assertEqual(run.returncode, 2, limit)


if __name__ == '__main__':
    unittest.main()

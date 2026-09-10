#!/usr/bin/env python3
"""Verify private requests, immutable evidence and pre/post launch inventories."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('benchmark', ROOT / 'scripts/prepare_3ds_benchmark.py')
BENCHMARK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCHMARK)


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.assets = self.root / 'assets'
        self.assets.mkdir()
        (self.assets / 'goldeneye.u.gepack').write_bytes(b'fixture asset pack')
        (self.assets / 'converted').mkdir()
        (self.assets / 'converted' / 'fixture.t3x').write_bytes(b'fixture sidecar')
        self.binary = self.root / 'fixture.3dsx'
        self.binary.write_bytes(b'fixture binary')
        self.input = self.root / 'input.cfg'
        self.input.write_text('GE_INPUT_PROBE 2\nsegments 2\nsegment 120 0 0 0 0 0\nsegment 3601 0 0 0 0 0\n')
        self.output = self.root / 'request'
        self.arguments = argparse.Namespace(output=self.output, input=self.input, binary=self.binary,
            assets=self.assets, stage='caverns', warmup=120, samples=3600, mode='clean', cadence=None,
            label='fixture', environment='emulator', environment_description='unit test', toolchain='fixture')

    def prepare(self):
        with patch.object(BENCHMARK, 'source_identity', return_value={'commit':'a'*40, 'source_tree_sha256':'b'*64}):
            return BENCHMARK.prepare(self.arguments)

    def install_fixture(self):
        self.installed = self.root / 'installed'
        shutil.copytree(self.assets, self.installed)
        shutil.copyfile(self.binary, self.installed / 'goldeneye-3ds.3dsx')
        for path in (self.output / 'configs').iterdir():
            shutil.copyfile(path, self.installed / path.name)

    def snapshot(self, phase):
        return BENCHMARK.snapshot(argparse.Namespace(request=self.output / 'request.json',
            installed=self.installed, output=self.root / (phase + '.json'), phase=phase))

    def collect_arguments(self):
        return argparse.Namespace(request=self.output / 'request.json', before=self.root / 'before.json',
            after=self.root / 'after.json', result=self.installed / 'dam-input-probe.result',
            output=self.root / 'capture' / 'run.result', audible_confirmed=True, run_kind='cold',
            observations='fixture only, no actual hardware measurement')

    def test_private_configs_preserve_input_and_disable_capture_diagnostics(self):
        result = self.prepare()
        configs = self.output / 'configs'
        self.assertEqual((configs / 'dam-input-probe.cfg').read_bytes(), self.input.read_bytes())
        self.assertEqual((configs / 'probe-detail.cfg').read_text(), '0\n')
        self.assertEqual((configs / 'world-profile.cfg').read_text(), '0\n')
        self.assertFalse((configs / 'frontend-probe.cfg').exists())
        self.assertEqual(result['cadence_sha256'], 'live')
        self.assertEqual(result['configured_frame_limit'], 3721)
        self.assertIn('converted/fixture.t3x', result['assets'])
        with self.assertRaises(FileExistsError):
            self.prepare()

    def test_small_window_or_missing_guard_is_rejected_before_writing(self):
        for samples, warmup in [(3599, 120), (3601, 120), (3600, 0)]:
            self.arguments.samples = samples
            self.arguments.warmup = warmup
            with self.assertRaises(ValueError):
                self.prepare()
            self.assertFalse(self.output.exists())

    def test_pcm_request_is_explicit_and_keeps_recorded_cadence(self):
        self.arguments.mode = 'pcm'
        self.arguments.cadence = self.root / 'cadence.cfg'
        self.arguments.cadence.write_text('GE_RETRACE_REPLAY 1\nframes 1\n1\n')
        result = self.prepare()
        self.assertEqual((self.output / 'configs' / 'probe-detail.cfg').read_text(), '1\n')
        self.assertEqual(result['cadence_sha256'], BENCHMARK.digest(self.arguments.cadence))

    def test_default_request_declares_all_actual_frames_including_late_combat(self):
        self.arguments.samples = None
        self.input.write_text('GE_INPUT_PROBE 1\nframes 4500\nactive_frames 4500\n')
        result = self.prepare()
        self.assertEqual(result['samples'], 'all-after-warmup-except-final')
        self.assertEqual(result['configured_frame_limit'], 4500)

    def test_roundtrip_snapshots_bind_result_to_exact_files_and_observations(self):
        request = self.prepare()
        self.install_fixture()
        before = self.snapshot('before')
        (self.installed / 'dam-input-probe.result').write_text('fixture result\n')
        after = self.snapshot('after')
        self.assertLess(before['timestamp_utc'], after['timestamp_utc'])
        arguments = self.collect_arguments()
        collected = BENCHMARK.collect(arguments)
        self.assertEqual(collected['binary_sha256'], request['binary_sha256'])
        self.assertTrue(collected['inventory_verified'])
        self.assertTrue(collected['audible_confirmed'])
        self.assertEqual(arguments.output.read_bytes(), arguments.result.read_bytes())
        self.assertEqual(json.loads(arguments.output.with_suffix('.provenance.json').read_text()), collected)
        with self.assertRaises(ValueError):
            BENCHMARK.collect(arguments)

    def test_inventory_rejects_same_size_asset_change_unknown_config_and_stale_result(self):
        self.prepare()
        self.install_fixture()
        sidecar = self.installed / 'converted' / 'fixture.t3x'
        original = sidecar.read_bytes()
        sidecar.write_bytes(b'x' * len(original))
        with self.assertRaisesRegex(ValueError, 'assets'):
            self.snapshot('before')
        sidecar.write_bytes(original)
        config = self.installed / 'frontend-probe.cfg'
        config.write_text('capture active')
        with self.assertRaisesRegex(ValueError, 'configurations'):
            self.snapshot('before')
        config.unlink()
        (self.installed / 'dam-input-probe.result').write_text('stale')
        with self.assertRaisesRegex(ValueError, 'existing result'):
            self.snapshot('before')

    def test_collection_rejects_result_tampering_or_snapshots_from_another_request(self):
        self.prepare()
        self.install_fixture()
        self.snapshot('before')
        result = self.installed / 'dam-input-probe.result'
        result.write_text('original')
        self.snapshot('after')
        arguments = self.collect_arguments()
        result.write_text('changed')
        with self.assertRaisesRegex(ValueError, 'result'):
            BENCHMARK.collect(arguments)
        result.write_text('original')
        before = json.loads(arguments.before.read_text())
        before['request_sha256'] = 'f'*64
        arguments.before.write_text(json.dumps(before))
        with self.assertRaisesRegex(ValueError, 'identity'):
            BENCHMARK.collect(arguments)
        self.assertFalse(arguments.output.exists())


if __name__ == '__main__':
    unittest.main()

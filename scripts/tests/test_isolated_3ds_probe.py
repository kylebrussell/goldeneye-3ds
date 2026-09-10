#!/usr/bin/env python3
import argparse
import configparser
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import run_isolated_3ds_probe as runner

class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ('azahar', 'binary', 'pack'):
            (self.root / name).write_text('fixture\n')
        self.profile = self.root / 'qt-config.ini'
        self.profile.write_text('[Audio]\nenable_audio_stretching=true\n'
                                '[Data Storage]\nsdmc_directory=/shared/sd\n'
                                'nand_directory=/shared/nand\n'
                                '[UI]\nPaths\\recentFiles=/shared/game\n')
        self.probe = self.root / 'probe.cfg'
        self.probe.write_text('GE_FRONTEND_PROBE 1\ngameplay_frames 1\n')
        self.args = argparse.Namespace(emulator=self.root/'azahar', profile=self.profile,
                                      binary=self.root/'binary',pack=self.root/'pack',
                                      probe=self.probe, request=None, max_stereo=False,
                                      output=self.root/'run',setting=[], timeout=1, stage=None, protect=[])

    def request_fixture(self):
        self.probe.write_text('GE_INPUT_PROBE 1\nframes 3721\nactive_frames 3721\n')
        assets = self.root / 'assets'
        assets.mkdir()
        (assets / 'goldeneye.u.gepack').write_bytes(self.args.pack.read_bytes())
        (assets / 'converted').mkdir()
        (assets / 'converted/model.t3x').write_bytes(b'private sidecar fixture')
        cadence = self.root / 'cadence.cfg'
        cadence.write_text('GE_RETRACE_REPLAY 1\nframes 1\n1\n')
        arguments = argparse.Namespace(
            output=self.root / 'request', input=self.probe, binary=self.args.binary,
            assets=assets, stage='dam', warmup=120, samples=3600, mode='clean',
            cadence=cadence, label='fixture', environment='emulator',
            environment_description='host fixture only', toolchain='fixture')
        with patch.object(runner.benchmark, 'source_identity', return_value={'commit': 'fixture'}):
            self.request = runner.benchmark.prepare(arguments)
        self.args.probe = None
        self.args.request = arguments.output / 'request.json'
        self.configs = arguments.output / 'configs'
        # Accept the imminent difficulty setting without rewriting or guessing it.
        (self.configs / 'probe-difficulty.cfg').write_text('2\n')
        self.request['configurations']['probe-difficulty.cfg'] = runner.digest(self.configs / 'probe-difficulty.cfg')
        self.request['configuration_sha256'] = runner.benchmark.manifest_digest(self.request['configurations'])
        self.save_request()

    def save_request(self):
        self.args.request.write_text(json.dumps(self.request))

    def run_mock_request(self, status='complete', mutate_config=False, mutate_profile=False,
                         mutate_dsp_source=False, mutate_dsp_staged=False):
        class Process:
            def poll(self): return 0
        def start(command, **kwargs):
            install = Path(command[-1]).parent
            self.assertTrue((self.args.output / 'before.json').is_file())
            self.assertFalse((self.args.output / 'after.json').exists())
            (install / 'dam-input-probe.result').write_text('status=' + status + '\n')
            log = Path(kwargs['env']['XDG_DATA_HOME']) / 'azahar-emu/log/azahar_log.txt'
            log.parent.mkdir(parents=True)
            log.write_text('fixture isolated log')
            if mutate_config:
                (install / 'stereo.cfg').write_text('enabled 0\n')
            if mutate_profile:
                profile = Path(kwargs['env']['XDG_CONFIG_HOME']) / 'azahar-emu/qt-config.ini'
                with profile.open('a') as stream:
                    stream.write('\n[Fixture]\nrewritten=true\n')
            if mutate_dsp_source:
                self.args.dsp_component.write_bytes(b'changed source after launch')
            if mutate_dsp_staged:
                (install.parent / 'dspfirm.cdc').write_bytes(b'changed staged component')
            return Process()
        with patch.object(runner.subprocess, 'check_output', return_value='shell\n'), \
                patch.object(runner.subprocess, 'Popen', side_effect=start), \
                patch('sys.stdout', new_callable=io.StringIO):
            return runner.execute(self.args)

    def test_profile_preserves_audio_and_rehomes_storage(self):
        user = self.root / 'user'
        before = self.profile.read_bytes()
        runner.prepare_profile(self.profile, user/'config/qt-config.ini', user)
        c = configparser.ConfigParser(interpolation=None)
        c.read(user/'config/qt-config.ini')
        self.assertEqual(c['Audio']['enable_audio_stretching'], 'true')
        self.assertEqual(c['Data%20Storage']['sdmc_directory'], str(user/'sdmc')+'/')
        self.assertFalse(c.has_section('UI'))
        self.assertEqual(self.profile.read_bytes(), before)

    def test_percent_encoded_qsettings_storage_has_one_private_alias(self):
        with self.profile.open('a') as stream:
            stream.write('[Data%20Storage]\nsdmc_directory=/shared/encoded-sd\n'
                         'nand_directory=/shared/encoded-nand\nuse_custom_storage=true\n'
                         'async_fs_operations=false\n[VideoDumping]\noutput_format=webm\n')
        original = self.profile.read_bytes()
        user = self.root / 'user'
        destination = user / 'config/qt-config.ini'
        runner.prepare_profile(self.profile, destination, user)
        c = configparser.ConfigParser(interpolation=None)
        c.read(destination)
        self.assertNotIn('Data Storage', c)
        self.assertEqual(c['Data%20Storage']['sdmc_directory'], str(user/'sdmc')+'/')
        self.assertEqual(c['Data%20Storage']['nand_directory'], str(user/'nand')+'/')
        self.assertEqual(c['Data%20Storage']['use_custom_storage'], 'true')
        self.assertEqual(c['Data%20Storage']['async_fs_operations'], 'false')
        self.assertNotIn('/shared/', destination.read_text())
        self.assertEqual(self.profile.read_bytes(), original)

    def test_max_stereo_uses_verified_azahar_layout_and_keeps_audio(self):
        with self.profile.open('a') as stream:
            stream.write('[Layout]\nfactor_3d=0\nfactor_3d\\default=true\n'
                         'render_3d=0\nrender_3d\\default=true\n'
                         '[Renderer]\ndisable_right_eye_render=true\n')
        original = self.profile.read_bytes()
        user = self.root / 'user'
        runner.prepare_profile(self.profile, user/'config/qt-config.ini', user, max_stereo=True)
        c = configparser.ConfigParser(interpolation=None)
        c.read(user/'config/qt-config.ini')
        self.assertEqual(c['Layout']['factor_3d'], '100')
        self.assertEqual(c['Layout']['render_3d'], '1')
        self.assertEqual(c['Layout']['factor_3d\\default'], 'false')
        self.assertEqual(c['Layout']['render_3d\\default'], 'false')
        self.assertEqual(c['Renderer']['disable_right_eye_render'], 'false')
        self.assertEqual(c['Audio']['enable_audio_stretching'], 'true')
        self.assertNotIn('factor_3d', c['Renderer'])
        self.assertEqual(self.profile.read_bytes(), original)

    def test_request_stages_exact_configs_assets_and_bracketing_snapshots(self):
        self.request_fixture()
        request_before = self.args.request.read_bytes()
        profile_before = self.profile.read_bytes()
        self.args.max_stereo = True
        self.assertEqual(self.run_mock_request(mutate_profile=True), 0)
        install = self.args.output / 'xdg-data/azahar-emu/sdmc/3ds/goldeneye-3ds'
        self.assertEqual(runner.benchmark.config_inventory(install), self.request['configurations'])
        self.assertEqual(runner.benchmark.asset_inventory(install), self.request['assets'])
        for name in self.request['configurations']:
            self.assertFalse((install / name).samefile(self.configs / name))
        before = json.loads((self.args.output / 'before.json').read_text())
        after = json.loads((self.args.output / 'after.json').read_text())
        report = json.loads((self.args.output / 'run.json').read_text())
        self.assertEqual(before['phase'], 'before')
        self.assertIsNone(before['result_sha256'])
        self.assertEqual(after['phase'], 'after')
        self.assertEqual(after['result_sha256'], runner.digest(self.args.output / 'dam-input-probe.result'))
        self.assertEqual(after['request_sha256'], before['request_sha256'])
        self.assertEqual(report['before_sha256'], runner.digest(self.args.output / 'before.json'))
        self.assertEqual(report['after_sha256'], runner.digest(self.args.output / 'after.json'))
        self.assertEqual(report['profile_sha256'], runner.digest(self.args.output / 'profile-before.ini'))
        self.assertEqual(report['profile_after_sha256'], runner.digest(self.args.output / 'profile-after.ini'))
        self.assertNotEqual(report['profile_sha256'], report['profile_after_sha256'])
        self.assertEqual((self.args.output / 'request.json').read_bytes(), request_before)
        self.assertEqual(self.args.request.read_bytes(), request_before)
        self.assertEqual(self.profile.read_bytes(), profile_before)
        self.assertNotIn('audible_confirmed', report)

    def test_request_overrides_are_rejected(self):
        self.request_fixture()
        for field, value in [('probe', self.probe), ('stage', 'dam'), ('setting', [self.profile])]:
            with self.subTest(field=field):
                old = getattr(self.args, field)
                setattr(self.args, field, value)
                with self.assertRaisesRegex(ValueError, 'mutually exclusive'), \
                        patch.object(runner.subprocess, 'Popen') as start:
                    runner.execute(self.args)
                start.assert_not_called()
                setattr(self.args, field, old)
        self.assertFalse(self.args.output.exists())

    def test_request_hashes_and_inventory_fail_before_launch(self):
        self.request_fixture()
        cases = [('binary_sha256', '0'*64), ('asset_sha256', '0'*64),
                 ('configuration_sha256', '0'*64), ('asset_manifest_sha256', '0'*64),
                 ('input_sha256', '0'*64), ('cadence_sha256', '0'*64),
                 ('stage', 'facility'), ('configured_frame_limit', 1),
                 ('environment', 'physical-new3dsxl'), ('schema', 'other')]
        for key, value in cases:
            with self.subTest(key=key):
                old = self.request[key]
                self.request[key] = value
                self.save_request()
                with patch.object(runner.subprocess, 'check_output', return_value='shell\n'), \
                        patch.object(runner.subprocess, 'Popen') as start, self.assertRaises(ValueError):
                    runner.execute(self.args)
                start.assert_not_called()
                self.assertFalse(self.args.output.exists())
                self.request[key] = old
        self.save_request()
        (self.configs / 'stereo.cfg').write_text('enabled 0\n')
        with self.assertRaisesRegex(ValueError, 'configuration hashes'):
            runner.validate_request(self.args.request, self.args.binary, self.args.pack)

    def test_request_rejects_nested_unknown_missing_and_symlink_configs(self):
        self.request_fixture()
        for name in ('other.cfg', 'nested', 'readme.txt'):
            extra = self.configs / name
            if name == 'nested': extra.mkdir()
            else: extra.write_text('extra')
            with self.assertRaisesRegex(ValueError, 'unknown or nested'):
                runner.validate_request(self.args.request, self.args.binary, self.args.pack)
            if extra.is_dir(): extra.rmdir()
            else: extra.unlink()
        config = self.configs / 'stereo.cfg'
        original = config.read_bytes()
        config.unlink()
        with self.assertRaisesRegex(ValueError, 'inventory differ'):
            runner.validate_request(self.args.request, self.args.binary, self.args.pack)
        outside = self.root / 'outside.cfg'
        outside.write_bytes(original)
        config.symlink_to(outside)
        with self.assertRaisesRegex(ValueError, 'non-symlink'):
            runner.validate_request(self.args.request, self.args.binary, self.args.pack)
        config.unlink()
        config.write_bytes(original)
        self.request['configurations']['../extra.cfg'] = '0'*64
        self.save_request()
        with self.assertRaisesRegex(ValueError, 'unknown or nested'):
            runner.validate_request(self.args.request, self.args.binary, self.args.pack)

    def test_request_rejects_unsafe_sidecars_and_source_mutation(self):
        self.request_fixture()
        sidecar = Path(self.request['asset_path']) / 'converted/model.t3x'
        original = sidecar.read_bytes()
        sidecar.write_bytes(b'x' * len(original))
        with self.assertRaisesRegex(ValueError, 'asset hash differs'):
            runner.validate_request(self.args.request, self.args.binary, self.args.pack)
        sidecar.unlink()
        sidecar.symlink_to(self.args.pack)
        with self.assertRaisesRegex(ValueError, 'non-symlink'):
            runner.validate_request(self.args.request, self.args.binary, self.args.pack)
        sidecar.unlink()
        sidecar.write_bytes(original)
        self.request['assets']['converted/../../outside'] = '0'*64
        self.request['asset_manifest_sha256'] = runner.benchmark.manifest_digest(self.request['assets'])
        self.save_request()
        with self.assertRaisesRegex(ValueError, 'unsafe or unknown'):
            runner.validate_request(self.args.request, self.args.binary, self.args.pack)

    def test_post_run_inventory_mismatch_is_failure_with_retained_result(self):
        self.request_fixture()
        self.assertEqual(self.run_mock_request(mutate_config=True), 1)
        report = json.loads((self.args.output / 'run.json').read_text())
        self.assertEqual(report['status'], 'failed')
        self.assertIn('configurations', report['inventory_error'])
        self.assertTrue((self.args.output / 'dam-input-probe.result').is_file())
        self.assertFalse((self.args.output / 'after.json').exists())

    def test_failed_or_partial_result_does_not_get_completed_snapshot(self):
        self.request_fixture()
        self.assertEqual(self.run_mock_request(status='failed'), 1)
        self.assertTrue((self.args.output / 'before.json').is_file())
        self.assertFalse((self.args.output / 'after.json').exists())
        result = self.root / 'partial.result'
        result.write_text('frames=5\n')
        self.assertFalse(runner.result_has_status(result))
        self.assertFalse(runner.complete_result(result))
        result.write_text('status=complete\nstatus=failed\n')
        self.assertFalse(runner.complete_result(result))

    def test_explicit_gameplay_difficulty_without_benchmark_request(self):
        self.probe.write_text('GE_INPUT_PROBE 1\nframes 55000\n')
        difficulty = self.root / 'probe-difficulty.cfg'
        difficulty.write_text('2\n')
        self.args.setting = [difficulty]
        self.args.stage = 'dam'
        class Process:
            def poll(self): return 0
        with patch.object(runner.subprocess, 'check_output', return_value='shell\n'), \
                patch.object(runner.subprocess, 'Popen', return_value=Process()), \
                patch('sys.stdout', new_callable=io.StringIO):
            self.assertEqual(runner.execute(self.args), 1) # no completed fixture result
        install = self.args.output / 'xdg-data/azahar-emu/sdmc/3ds/goldeneye-3ds'
        self.assertEqual((install / 'probe-difficulty.cfg').read_bytes(), b'2\n')
        report = json.loads((self.args.output / 'run.json').read_text())
        self.assertEqual(report['settings']['probe-difficulty.cfg'], runner.digest(difficulty))
        self.assertFalse((self.args.output / 'request.json').exists())

    def test_explicit_difficulty_rejects_frontend_and_noncanonical_values(self):
        difficulty = self.root / 'probe-difficulty.cfg'
        difficulty.write_bytes(b'2\n')
        self.args.setting = [difficulty]
        with patch.object(runner.subprocess, 'check_output', return_value='shell\n'), \
                self.assertRaisesRegex(ValueError, 'gameplay GE_INPUT_PROBE'):
            runner.execute(self.args)
        self.probe.write_text('GE_INPUT_PROBE 1\nframes 55000\n')
        for value in (b'4\n', b'-1\n', b'2', b'2\n0\n', b'2 # comment\n', b'\xff\n'):
            difficulty.write_bytes(value)
            with self.subTest(value=value), \
                    patch.object(runner.subprocess, 'check_output', return_value='shell\n'), \
                    self.assertRaisesRegex(ValueError, 'exactly 0, 1, 2 or 3'):
                runner.execute(self.args)
        self.assertFalse(self.args.output.exists())

    def test_dsp_component_private_bytes_and_environment_receipt_are_stable(self):
        self.request_fixture()
        self.args.dsp_component = self.root / 'component.bin'
        original = b'non-firmware fixture bytes' * 16
        self.args.dsp_component.write_bytes(original)
        original_hash = runner.digest(self.args.dsp_component)
        original_prepare = runner.prepare_profile
        def mutate_before_staging(*args, **kwargs):
            self.args.dsp_component.write_bytes(b'changed after validation before staging')
            return original_prepare(*args, **kwargs)
        with patch.object(runner, 'prepare_profile', side_effect=mutate_before_staging):
            self.assertEqual(self.run_mock_request(mutate_dsp_source=True), 0)
        report = json.loads((self.args.output / 'run.json').read_text())
        component = report['environment_components']['dsp']
        staged = self.args.output / component['staged_path']
        self.assertEqual(component['kind'], 'supplied-dsp-component')
        self.assertEqual(component['source_sha256'], original_hash)
        self.assertEqual(component['staged_sha256'], original_hash)
        self.assertEqual(component['staged_after_sha256'], original_hash)
        self.assertTrue(component['staged_unchanged'])
        self.assertEqual(staged.read_bytes(), original)
        self.assertFalse(staged.samefile(self.args.dsp_component))
        self.assertNotEqual(runner.digest(self.args.dsp_component), original_hash)
        for phase in ('before', 'after'):
            environment = json.loads((self.args.output / ('environment-' + phase + '.json')).read_text())
            self.assertEqual(environment['request_sha256'], runner.digest(self.args.request))
            self.assertEqual(environment['components']['dsp']['staged_sha256'], original_hash)
            self.assertEqual(report['environment_' + phase + '_sha256'],
                             runner.digest(self.args.output / ('environment-' + phase + '.json')))
            inventory = json.loads((self.args.output / (phase + '.json')).read_text())
            self.assertEqual(inventory['configurations'], self.request['configurations'])
            self.assertEqual(inventory['assets'], self.request['assets'])

    def test_staged_component_mutation_fails_environment_verification(self):
        self.request_fixture()
        self.args.dsp_component = self.root / 'component.bin'
        self.args.dsp_component.write_bytes(b'bounded fixture')
        self.assertEqual(self.run_mock_request(mutate_dsp_staged=True), 1)
        report = json.loads((self.args.output / 'run.json').read_text())
        self.assertFalse(report['environment_components']['dsp']['staged_unchanged'])
        self.assertEqual(report['status'], 'failed')
        self.assertTrue((self.args.output / 'environment-after.json').is_file())

    def test_component_rejects_empty_oversize_nonregular_and_symlink_files(self):
        source = self.root / 'component.bin'
        for payload in (b'', b'x' * (runner.MAX_DSP_COMPONENT_BYTES + 1)):
            source.write_bytes(payload)
            with self.assertRaisesRegex(ValueError, 'nonempty regular file'):
                runner.read_dsp_component(source)
        source.unlink()
        source.symlink_to(self.args.pack)
        with self.assertRaisesRegex(ValueError, 'non-symlink'):
            runner.read_dsp_component(source)
        source.unlink()
        source.mkdir()
        with self.assertRaisesRegex(ValueError, 'non-symlink'):
            runner.read_dsp_component(source)

    def test_hle_marker_requires_verified_mode_and_is_labeled_not_firmware(self):
        self.request_fixture()
        self.args.azahar_hle_dsp_marker = True
        self.profile.write_text('[Audio]\naudio_emulation=0\naudio_emulation\\default=true\n')
        original_profile = self.profile.read_bytes()
        self.assertEqual(self.run_mock_request(), 0)
        report = json.loads((self.args.output / 'run.json').read_text())
        component = report['environment_components']['dsp']
        self.assertEqual(component['kind'], 'azahar-hle-dsp-marker')
        self.assertTrue(component['emulator_only'] and component['not_firmware'])
        self.assertIsNone(component['source_sha256'])
        self.assertEqual(component['size_bytes'], 0)
        self.assertEqual((self.args.output / component['staged_path']).read_bytes(), b'')
        self.assertEqual(component['staged_after_sha256'], runner.hashlib.sha256(b'').hexdigest())
        self.assertEqual(self.profile.read_bytes(), original_profile)
        self.assertNotIn('audible_confirmed', report)

    def test_hle_marker_never_changes_lle_or_missing_settings(self):
        self.args.azahar_hle_dsp_marker = True
        for audio in ('', 'audio_emulation=1\n', 'audio_emulation=2\n',
                      'enable_dsp_lle=false\n', 'audio_emulation=0\nenable_dsp_lle=true\n'):
            self.profile.write_text('[Audio]\n' + audio)
            with patch.object(runner.subprocess, 'check_output', return_value='shell\n'), \
                    patch.object(runner.subprocess, 'Popen') as start, \
                    self.assertRaisesRegex(ValueError, 'HLE DSP marker'):
                runner.execute(self.args)
            start.assert_not_called()
            self.assertFalse(self.args.output.exists())
            self.assertEqual(self.profile.read_text(), '[Audio]\n' + audio)
        self.args.dsp_component = self.args.pack
        with self.assertRaisesRegex(ValueError, 'mutually exclusive'):
            runner.execute(self.args)

    def test_unknown_absolute_destination_rejected(self):
        self.profile.write_text('[Video Dumping]\nvideo_file=/shared/capture.webm\n')
        with self.assertRaisesRegex(ValueError, 'unhandled absolute path'):
            runner.prepare_profile(self.profile,self.root/'user/config/qt-config.ini',self.root/'user')

    def test_preexisting_emulator_untouched(self):
        with patch.object(runner.subprocess,'check_output',return_value='/Applications/Azahar.app/Contents/MacOS/azahar\n'), \
                patch.object(runner.subprocess,'Popen') as start:
            with self.assertRaisesRegex(ValueError, 'already running'):
                runner.execute(self.args)
            start.assert_not_called()
        self.assertFalse(self.args.output.exists())

    def test_timeout_terminates_only_owned_process(self):
        class Process:
            terminated = 0
            def poll(self): return None
            def terminate(self): self.terminated += 1
            def wait(self, timeout): return 0
        owned = Process()
        with patch.object(runner.subprocess,'check_output',return_value='shell\n'), \
                patch.object(runner.subprocess,'Popen',return_value=owned) as start, \
                patch.object(runner.time,'monotonic',side_effect=[0,2,3]):
            self.assertEqual(runner.execute(self.args),1)
        self.assertEqual(owned.terminated,1)
        self.assertEqual(start.call_args.kwargs['cwd'],self.args.output.resolve())
        self.assertEqual(self.profile.read_text().count('/shared/'),3)

if __name__ == '__main__': unittest.main()

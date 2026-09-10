#!/usr/bin/env python3
"""Release acceptance and staged-content tamper regression tests."""
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import package_3ds_candidate as package
import build_3ds_candidate as builder
from verify_3ds_asset_pack import fnv1a64


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data = json.loads((ROOT / 'docs/acceptance/candidate-template.json').read_text())
        (self.root / 'run.log').write_text('measured bounded fixture\n')

    def evidence(self, environment='host'):
        return {'artifact': 'run.log', 'environment': environment,
                'result': 'passed', 'scope': 'fixture only'}

    def test_pending_candidate_allowed_but_completion_rejected(self):
        package.validate_acceptance(self.data, self.root)
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            package.validate_acceptance(self.data, self.root, True)

    def test_every_roadmap_row_required(self):
        for area in package.AREAS:
            bad = copy.deepcopy(self.data)
            del bad['areas'][area]
            with self.assertRaises(ValueError):
                package.validate_acceptance(bad, self.root)

    def test_software_cannot_satisfy_physical_acceptance(self):
        for area in package.PHYSICAL:
            for environment in ('host', 'emulator'):
                bad = copy.deepcopy(self.data)
                bad['areas'][area].update(status='passed', remaining='', evidence=[self.evidence(environment)])
                with self.assertRaisesRegex(ValueError, 'physical acceptance'):
                    package.validate_acceptance(bad, self.root)

    def test_physical_requires_device_identity(self):
        row = self.data['areas']['release']
        row.update(status='passed', remaining='', evidence=[self.evidence('physical')])
        with self.assertRaisesRegex(ValueError, 'device identity'):
            package.validate_acceptance(self.data, self.root)
        row['evidence'][0]['device'] = 'New 3DS XL test-unit-A'
        package.validate_acceptance(self.data, self.root)

    def test_fidelity_requires_reference(self):
        row = self.data['areas']['original_fidelity']
        row.update(status='passed', remaining='', evidence=[self.evidence()])
        with self.assertRaisesRegex(ValueError, 'reference'):
            package.validate_acceptance(self.data, self.root)

    def test_missing_empty_and_escaping_evidence_rejected(self):
        row = self.data['areas']['release']
        row['evidence'] = [self.evidence()]
        for name in ('missing.log', '../run.log', str(self.root / 'run.log')):
            row['evidence'][0]['artifact'] = name
            with self.assertRaises(ValueError):
                package.validate_acceptance(self.data, self.root)
        (self.root / 'empty.log').touch()
        row['evidence'][0]['artifact'] = 'empty.log'
        with self.assertRaises(ValueError):
            package.validate_acceptance(self.data, self.root)

    def test_symlink_evidence_rejected(self):
        (self.root / 'alias.log').symlink_to(self.root / 'run.log')
        self.data['areas']['release']['evidence'] = [dict(self.evidence(), artifact='alias.log')]
        with self.assertRaises(ValueError):
            package.validate_acceptance(self.data, self.root)
        with self.assertRaises(ValueError):
            package.files(self.root)

    def test_inventory_detects_same_size_mutation_and_addition(self):
        before = package.files(self.root)
        (self.root / 'run.log').write_text('MEASURED bounded fixture\n')
        self.assertNotEqual(before, package.files(self.root))
        (self.root / 'stage.cfg').write_text('level 1\n')
        self.assertIn('stage.cfg', package.files(self.root))

    def test_failure_cannot_be_marked_passed(self):
        row = self.data['areas']['wireless_game_state']
        row.update(status='passed', remaining='', evidence=[dict(self.evidence(), result='failed')])
        with self.assertRaises(ValueError):
            package.validate_acceptance(self.data, self.root)

    def test_pinned_evidence_hash_rejects_stale_run(self):
        item = dict(self.evidence(), sha256=package.digest(self.root / 'run.log'))
        self.data['areas']['release']['evidence'] = [item]
        package.validate_acceptance(self.data, self.root)
        (self.root / 'run.log').write_text('a different run\n')
        with self.assertRaisesRegex(ValueError, 'evidence hash mismatch'):
            package.validate_acceptance(self.data, self.root)

    def test_passed_row_cannot_keep_unfinished_acceptance(self):
        self.data['areas']['wireless_game_state'].update(status='passed', evidence=[self.evidence()])
        with self.assertRaisesRegex(ValueError, 'unfinished acceptance'):
            package.validate_acceptance(self.data, self.root)


class CandidateTests(unittest.TestCase):
    """Exercise staging and offline verification with real Git and pack fixtures.

    Only the ARM build subprocess is mocked. The tiny fixture binaries provide
    parser magic, not evidence of an executable game or hardware acceptance.
    """
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.private_name = 'assets/obseg/gun/test/Model.c'
        tracked = {
            '.gitignore': 'build/\nplatform/3ds/build/\nplatform/3ds/*.3dsx\n'
                          'platform/3ds/*.smdh\nassets/obseg/**/Model.c\n',
            'scripts/build_3ds.sh': '# fixture only\n',
            'scripts/build_3ds_assets.sh': '# fixture only\n',
            'Dockerfile.3ds-assets': '# fixture only\n',
            'platform/3ds/source/main.c': 'int main(void) { return 0; }\n',
        }
        for name, content in tracked.items():
            self.write(name, content)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', '-C', str(self.root), 'add', '.'], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Test',
                        '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'fixture'], check=True)
        self.write(self.private_name, 'private model fixture\n')
        self.write(package.PRIVATE_REQUIRED[0], 'private AI object fixture\n')
        self.build_args = SimpleNamespace(root=self.root, output=self.root / 'build/receipt',
            toolchain='devkitpro/devkitarm@sha256:' + 'a' * 64, jobs=4)
        evidence = self.root / 'build/evidence'
        evidence.mkdir()
        self.write('build/evidence/run.log', 'bounded software fixture\n')
        acceptance = json.loads((ROOT / 'docs/acceptance/candidate-template.json').read_text())
        acceptance['areas']['release']['evidence'] = [{
            'artifact': 'run.log', 'environment': 'host', 'result': 'passed',
            'scope': 'fixture only', 'sha256': package.digest(evidence / 'run.log')}]
        self.write('build/acceptance.json', json.dumps(acceptance))
        self.pack = self.root / 'build/fixture.gepack'
        self.make_pack()
        self.stage_args = SimpleNamespace(root=self.root, output=self.root / 'build/candidate',
            acceptance=self.root / 'build/acceptance.json', evidence=evidence, pack=self.pack,
            require_complete=False, toolchain=self.build_args.toolchain, settings=None,
            binaries=self.build_args.output / 'binaries',
            build_receipt=self.build_args.output / 'build-receipt.json')

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def make_pack(self):
        names = sorted((b'converted/textures/t3x/fixture.t3x', b'converted/levels/fixture',
                        b'converted/runtime/fixture'), key=fnv1a64)
        paths = b''.join(names)
        paths_offset = 80 + len(names) * 32
        data_offset = (paths_offset + len(paths) + 15) & ~15
        entries = bytearray()
        offset = 0
        for index, name in enumerate(names):
            entries.extend(struct.pack('<QIIQQ', fnv1a64(name), offset, len(name),
                                       data_offset + 16 * index, 16))
            offset += len(name)
        header = struct.pack('<8sIIIIQQQ20s12x', b'GEPACK\0\0', 1, 0, len(names), 80,
                             80, paths_offset, data_offset, b'\0' * 20)
        self.pack.write_bytes(header + entries + paths + b'\0' * (data_offset - paths_offset - len(paths))
                              + b'\0' * (16 * len(names)))

    def fake_build(self, command, **kwargs):
        self.assertEqual(command, ['bash', 'scripts/build_3ds.sh', '-j4'])
        self.assertEqual(kwargs['env']['DEVKITPRO_IMAGE'], self.build_args.toolchain)
        kwargs['stdout'].write(b'mocked ARM build; software fixture only\n')
        for name, magic in package.BINARIES.items():
            (self.root / 'platform/3ds' / name).write_bytes(magic + b'fixture')
        self.write('platform/3ds/build/generated.c', 'generated fixture\n')
        self.write('platform/3ds/build/main.d', 'main.o: /workspace/platform/3ds/source/main.c '
            '/workspace/' + self.private_name + ' generated.c\n')
        return SimpleNamespace(returncode=0)

    def build(self, side_effect=None):
        process = SimpleNamespace(run=mock.Mock(side_effect=side_effect or self.fake_build),
            STDOUT=subprocess.STDOUT, CalledProcessError=subprocess.CalledProcessError)
        with mock.patch.object(builder, 'subprocess', process):
            return builder.build(self.build_args)

    def stage(self):
        receipt = self.build()
        self.assertEqual(receipt['exit_code'], 0)
        return package.stage(self.stage_args)

    def reindex(self):
        path = self.stage_args.output / 'manifest.json'
        manifest = json.loads(path.read_text())
        inventory = package.files(self.stage_args.output)
        inventory.pop('manifest.json')
        manifest['files'] = inventory
        path.write_text(json.dumps(manifest))

    def test_private_candidate_round_trip_preserves_bounds(self):
        manifest = self.stage()
        self.assertTrue(package.verify(self.stage_args.output)['candidate_only'])
        receipt = json.loads(self.stage_args.build_receipt.read_text())
        self.assertIn(self.private_name, receipt['private_inputs'])
        self.assertIn(self.private_name, receipt['consumed_private_inputs'])
        self.assertIn('platform/3ds/build/generated.c', receipt['generated_dependencies'])
        self.assertEqual(manifest['source_tree'], package.git(self.root, 'rev-parse', 'HEAD^{tree}'))
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            package.verify(self.stage_args.output, True)

    def test_untracked_source_rejected_before_build(self):
        self.write('platform/3ds/source/untracked.c', 'new compile input\n')
        run = mock.Mock(side_effect=AssertionError('build must not run'))
        with self.assertRaisesRegex(ValueError, 'untracked source'):
            self.build(run)
        run.assert_not_called()

    def test_preserved_original_file_is_not_treated_as_source(self):
        self.write('platform/3ds/source/main.c.orig', 'user backup\n')
        self.assertEqual(self.build()['exit_code'], 0)

    def test_private_source_change_during_build_rejected(self):
        def changed(*args, **kwargs):
            result = self.fake_build(*args, **kwargs)
            self.write(self.private_name, 'changed ignored source\n')
            return result
        receipt = self.build(changed)
        self.assertEqual(receipt['exit_code'], 1)
        self.assertEqual(receipt['binaries'], {})
        self.assertIn('private inputs changed', receipt['error'])

    def test_tracked_source_change_during_build_rejected(self):
        def changed(*args, **kwargs):
            result = self.fake_build(*args, **kwargs)
            self.write('platform/3ds/source/main.c', 'changed source\n')
            return result
        receipt = self.build(changed)
        self.assertEqual(receipt['exit_code'], 1)
        self.assertEqual(receipt['binaries'], {})

    def test_success_without_dependency_reports_rejected(self):
        def missing(*args, **kwargs):
            result = self.fake_build(*args, **kwargs)
            (self.root / 'platform/3ds/build/main.d').unlink()
            return result
        receipt = self.build(missing)
        self.assertEqual(receipt['exit_code'], 1)
        self.assertIn('lacks compiler dependency reports', receipt['error'])

    def test_failed_build_retains_failure_receipt(self):
        def failed(*args, **kwargs):
            kwargs['stdout'].write(b'compile failed\n')
            return SimpleNamespace(returncode=2)
        receipt = self.build(failed)
        self.assertEqual(receipt['exit_code'], 2)
        self.assertEqual(receipt['binaries'], {})
        self.assertTrue(self.stage_args.build_receipt.is_file())

    def test_unrecorded_private_compiler_dependency_rejected(self):
        def extra(*args, **kwargs):
            result = self.fake_build(*args, **kwargs)
            self.write('build/hidden.h', 'unrecorded input\n')
            self.write('platform/3ds/build/main.d', 'main.o: /workspace/build/hidden.h\n')
            return result
        receipt = self.build(extra)
        self.assertEqual(receipt['exit_code'], 1)
        self.assertIn('unrecorded private compiler input', receipt['error'])

    def test_stale_private_inputs_rejected_at_stage(self):
        self.build()
        self.write(package.PRIVATE_REQUIRED[0], 'different private AI object\n')
        with self.assertRaisesRegex(ValueError, 'private build inputs changed'):
            package.stage(self.stage_args)

    def test_malformed_image_digest_rejected(self):
        for value in ('devkitpro/devkitarm:latest', 'image@sha256:x', 'image@sha256:' + 'z' * 64):
            self.build_args.toolchain = value
            with self.assertRaisesRegex(ValueError, 'immutable image digest'):
                self.build()

    def test_manifest_source_toolchain_and_asset_crosschecks(self):
        self.stage()
        path = self.stage_args.output / 'manifest.json'
        original = json.loads(path.read_text())
        edits = [('source_commit', 'b' * 40), ('source_tree', 'b' * 40),
                 ('toolchain_image', 'image@sha256:' + 'b' * 64),
                 ('build_command', ['make']),
                 ('asset_source', dict(original['asset_source'], pack_sha256='b' * 64))]
        for key, value in edits:
            with self.subTest(key=key):
                changed = copy.deepcopy(original)
                changed[key] = value
                path.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):
                    package.verify(self.stage_args.output)
        path.write_text(json.dumps(original))

    def test_updated_inventory_cannot_hide_receipt_disagreement(self):
        self.stage()
        root = self.stage_args.output
        cases = [('build.log', b'changed log\n'),
                 ('build-dependencies/main.d', b'main.o: different.c\n'),
                 ('sd/3ds/goldeneye-3ds/goldeneye-3ds.3dsx', b'3DSXchanged')]
        for name, content in cases:
            with self.subTest(name=name):
                path = root / name
                original = path.read_bytes()
                path.write_bytes(content)
                self.reindex()
                with self.assertRaises(ValueError):
                    package.verify(root)
                path.write_bytes(original)
                self.reindex()

    def test_other_sd_paths_and_nested_settings_rejected(self):
        self.stage()
        for name in ('sd/other-app.cfg', 'sd/3ds/goldeneye-3ds/controls.cfg/hidden.cfg'):
            with self.subTest(name=name):
                path = self.stage_args.output / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('unexpected file\n')
                self.reindex()
                with self.assertRaisesRegex(ValueError, 'clean SD'):
                    package.verify(self.stage_args.output)
                path.unlink()
                if path.parent.name == 'controls.cfg':
                    path.parent.rmdir()
                self.reindex()


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3
"""Release acceptance and staged-content tamper regression tests."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import package_3ds_candidate as package


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
                bad['areas'][area].update(status='passed', evidence=[self.evidence(environment)])
                with self.assertRaisesRegex(ValueError, 'physical acceptance'):
                    package.validate_acceptance(bad, self.root)

    def test_physical_requires_device_identity(self):
        row = self.data['areas']['release']
        row.update(status='passed', evidence=[self.evidence('physical')])
        with self.assertRaisesRegex(ValueError, 'device identity'):
            package.validate_acceptance(self.data, self.root)
        row['evidence'][0]['device'] = 'New 3DS XL test-unit-A'
        package.validate_acceptance(self.data, self.root)

    def test_fidelity_requires_reference(self):
        row = self.data['areas']['original_fidelity']
        row.update(status='passed', evidence=[self.evidence()])
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
        row.update(status='passed', evidence=[dict(self.evidence(), result='failed')])
        with self.assertRaises(ValueError):
            package.validate_acceptance(self.data, self.root)


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "stage_character_models", ROOT / "scripts/stage_3ds_character_models.py")
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CharacterModelStagingTests(unittest.TestCase):
    def test_canonical_dependency_identity(self):
        manifest = MODULE.checked_manifest(ROOT)
        self.assertEqual(manifest["body_count"], 39)
        self.assertEqual(manifest["head_count"], 35)
        self.assertEqual(manifest["model_count"], 74)
        self.assertEqual(manifest["total_size"], 1155216)
        self.assertEqual(
            manifest["identity_sha256"],
            "fbb8414ba7fd88ce9c1b38eb854e6a47f461502e3c545ea158bc358cea038b8a")
        self.assertEqual(
            manifest["body_ids"],
            [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
             16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 27, 28, 29, 32, 33,
             34, 35, 36, 37, 38, 39, 40, 79])
        self.assertEqual(
            manifest["head_ids"],
            [42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
             55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
             70, 71, 72, 73, 74, 75, 78])
        by_id = {model["model_id"]: model for model in manifest["models"]}
        self.assertEqual(by_id[37]["name"], "greatguard2")
        self.assertEqual(by_id[37]["sha256"],
                         "25b149db172d11a26231a76b3dfffae611f356db814e5d7bbbaefc73753d5a3f")
        self.assertEqual(by_id[69]["name"], "headmishkin")
        self.assertEqual(by_id[69]["roles"], ["head"])
        self.assertEqual(by_id[78]["name"], "headbrosnan")
        self.assertEqual(by_id[78]["resource"], "CheadbrosnanZ")
        self.assertEqual(by_id[78]["roles"], ["head"])
        self.assertEqual(by_id[22]["name"], "boilerbond")
        self.assertEqual(by_id[22]["roles"], ["body"])
        self.assertEqual(by_id[74]["name"], "headbrosnanboiler")
        self.assertEqual(by_id[74]["roles"], ["head"])

    def test_atomic_stage_and_check(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "characters"
            manifest = MODULE.stage(ROOT, output)
            MODULE.verify(output, manifest)
            on_disk = json.loads((output / "manifest.json").read_text())
            self.assertEqual(on_disk["identity_sha256"],
                             manifest["identity_sha256"])
            self.assertEqual(len(list(output.glob("C*Z.bin"))), 74)


if __name__ == "__main__":
    unittest.main()

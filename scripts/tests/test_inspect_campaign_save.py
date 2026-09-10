"""Exercise real CRC/provider reload and forbid writes on every failure path."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.tests.test_verify_dam_end_to_end_result import GOOD


class SaveInspectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix="ge-save-inspector-test-")
        directory = Path(cls.build.name)
        cls.inspector = directory / "inspect"
        cls.generator = directory / "generate"
        flags = ["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                 "-Wno-comment", "-Wno-unused-parameter", "-Wno-pointer-sign",
                 "-Wno-missing-braces", "-D_LANGUAGE_C",
                 "-DGE_PORT_USE_ORIGINAL_TYPES", "-DVERSION_US",
                 "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                 "-I" + str(ROOT), "-I" + str(ROOT / "port/include"),
                 "-I" + str(ROOT / "src/game"),
                 "-idirafter", str(ROOT / "include"),
                 "-idirafter", str(ROOT / "src")]
        random = str(ROOT / "port/src/random_port.c")
        subprocess.run(flags + [str(ROOT / "scripts/inspect_campaign_save.c"),
                                random, "-o", str(cls.inspector)], check=True)
        source = directory / "generate.c"
        source.write_text(r'''
#include "ge_3ds_save_provider.h"
#include "ge_original_mission_result.h"
#include <assert.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
s32 selected_folder_num = FOLDER1;
int main(int argc, char **argv) {
    assert(argc == 4);
    Ge3dsSaveProvider state;
    GeOriginalMissionResultProviders providers;
    int difficulty = atoi(argv[2]);
    selected_folder_num = atoi(argv[3]);
    assert(ge_3ds_save_provider_init(&state, argv[1], selected_folder_num)
        == GE_3DS_SAVE_PROVIDER_OK);
    if (difficulty >= 0) {
        ge_original_mission_result_reset();
        ge_3ds_save_provider_make_mission_result_providers(&state, &providers);
        ge_original_mission_result_bind(&providers);
        assert(ge_original_mission_result_apply_exact(difficulty, 123 * 60));
        assert(state.status == GE_3DS_SAVE_PROVIDER_OK);
    }
    ge_3ds_save_provider_close(&state);
    return 0;
}
''')
        subprocess.run(flags + [str(source), random,
                                str(ROOT / "port/src/ge_3ds_save_provider.c"),
                                str(ROOT / "port/src/ge_original_mission_result.c"),
                                "-o", str(cls.generator)], check=True)
        cls.environment = {**os.environ,
                           "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                           "UBSAN_OPTIONS": "halt_on_error=1"}

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="ge-save-inspector-case-")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "goldeneye.sav"
        self.temporary = self.path.with_suffix(".sav.tmp")
        self.temporary.write_bytes(b"preexisting temporary must survive")

    def generate(self, difficulty=2, folder=0):
        subprocess.run([str(self.generator), str(self.path), str(difficulty),
                        str(folder)], check=True, env=self.environment)
        # The fixture writer legitimately uses its own temporary filename.
        self.temporary.write_bytes(b"preexisting temporary must survive")

    def inspect(self, expected, folder="0"):
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in self.path.parent.iterdir() if p.is_file()}
        result = subprocess.run([str(self.inspector), str(self.path), folder],
                                capture_output=True, text=True, env=self.environment)
        self.assertEqual(result.returncode, expected, result.stderr)
        after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in self.path.parent.iterdir() if p.is_file()}
        self.assertEqual(before, after)
        return json.loads(result.stdout) if result.stdout else None

    def verify_combined(self, expected, route=GOOD, folder="0"):
        result_path = self.path.parent / "controller.result"
        result_path.write_text(route)
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in self.path.parent.iterdir() if p.is_file()}
        result = subprocess.run(
            [sys.executable, str(ROOT / "scripts/verify_dam_end_to_end_result.py"),
             str(result_path), "--save", str(self.path), "--folder", folder],
            capture_output=True, text=True, env=self.environment)
        self.assertEqual(result.returncode, expected, result.stderr)
        after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in self.path.parent.iterdir() if p.is_file()}
        self.assertEqual(before, after)
        if expected != 0:
            # Even complete callback/route telemetry must not print a success
            # label before a missing, incomplete or corrupt save is rejected.
            self.assertEqual(result.stdout, "")
            self.assertNotIn("Retained save verified", result.stderr)
            self.assertNotIn("durable Dam", result.stderr)
        return result

    def test_complete_route_with_00_save_verifies_durable_progress(self):
        self.generate()
        report = self.verify_combined(0).stdout
        self.assertIn("Dam controller route verified", report)
        self.assertIn("Retained save verified: durable Dam 00 Agent completion "
                      "(123 s) and Facility 00 Agent access in folder 0", report)
        self.assertIn("CRC valid, 1 reload, 0 writes", report)
        self.assertIn(str(self.path.resolve()), report)
        self.assertIn(f"{self.path.stat().st_size} bytes", report)
        self.assertIn(hashlib.sha256(self.path.read_bytes()).hexdigest(), report)

    def test_complete_route_with_incomplete_save_rejects_durable_progress(self):
        for difficulty in (-1, 0, 1, 3):
            with self.subTest(difficulty=difficulty):
                if self.path.exists():
                    self.path.unlink()
                self.generate(difficulty)
                self.assertIn("retained save did not verify",
                              self.verify_combined(1).stderr)

    def test_complete_route_with_missing_save_rejects_durable_progress(self):
        self.assertIn("retained save could not be read",
                      self.verify_combined(1).stderr)
        self.assertFalse(self.path.exists())

    def test_complete_route_with_corrupt_save_rejects_durable_progress(self):
        self.generate()
        original = self.path.read_bytes()
        corrupt = bytearray(original)
        corrupt[19] ^= 1
        for contents in (bytes(corrupt), original[:-1], original + b"extra"):
            with self.subTest(length=len(contents)):
                self.path.write_bytes(contents)
                self.assertIn("retained save did not verify",
                              self.verify_combined(1).stderr)

    def test_complete_route_requires_the_retained_save_folder(self):
        self.generate(folder=2)
        self.assertIn("retained save did not verify",
                      self.verify_combined(1).stderr)
        self.assertIn("Facility 00 Agent access in folder 2",
                      self.verify_combined(0, folder="2").stdout)

    def test_completed_save_cannot_bypass_route_requirements(self):
        self.generate()
        for old, new, error in (
                ("status=complete", "status=failed", "route did not complete"),
                ("route_targets=124,124", "route_targets=123,124", "every stop"),
                ("2:1:0:0", "2:0:0:0", "not all four"),
                ("setup_difficulty=2", "setup_difficulty=0", "requires 00 Agent"),
                ("mission_result=1,0,1", "mission_result=1,1,0", "provider callbacks")):
            with self.subTest(field=old):
                self.assertIn(error, self.verify_combined(
                    1, route=GOOD.replace(old, new)).stderr)

    def test_reload_00_completion_and_unlock(self):
        self.generate()
        report = self.inspect(0)
        self.assertTrue(report["crc_valid"] and report["acceptance_passed"])
        self.assertEqual(report["dam_00_time_seconds"], 123)
        self.assertEqual((report["loads"], report["writes"]), (1, 0))
        self.assertEqual(report["dam_00_status"], 3)
        self.assertEqual(report["facility_00_status"], 1)

    def test_other_difficulties_and_blank_do_not_certify_00(self):
        for difficulty in (-1, 0, 1, 3):
            with self.subTest(difficulty=difficulty):
                if self.path.exists():
                    self.path.unlink()
                self.generate(difficulty)
                self.assertFalse(self.inspect(1)["acceptance_passed"])

    def test_missing_save_never_creates_or_replaces_files(self):
        self.inspect(2)

    def test_crc_truncation_and_trailing_data_fail_without_writes(self):
        self.generate()
        original = self.path.read_bytes()
        corrupt = bytearray(original)
        corrupt[19] ^= 1
        for contents in (bytes(corrupt), original[:-1], original + b"extra"):
            with self.subTest(length=len(contents)):
                self.path.write_bytes(contents)
                self.inspect(2)

    def test_folder_identity_and_strict_argument(self):
        self.generate(folder=2)
        self.inspect(2)
        self.assertEqual(self.inspect(0, "2")["folder"], 2)
        for folder in ("-1", "4", "02", "", "1x"):
            self.inspect(2, folder)


if __name__ == "__main__":
    unittest.main()

"""Exercise real CRC/provider reload and forbid writes on every failure path."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


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

"""Hermetic executable checks of frontend probe backup/cleanup, without Azahar."""

import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time
import unittest


REPO = Path(__file__).resolve().parents[2]


class FrontendProbeRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ge-probe-runner-")
        self.root = Path(self.temp.name)
        self.scripts = self.root / "scripts"
        self.scripts.mkdir()
        shutil.copy2(REPO / "scripts/run_frontend_probe.sh", self.scripts)
        self.stage = self.root / "build/3ds-sd/3ds/goldeneye-3ds"
        self.stage.mkdir(parents=True)
        (self.stage / "goldeneye-3ds.3dsx").write_bytes(b"test executable")
        (self.stage / "goldeneye.u.gepack").write_bytes(b"test pack")
        self.install = self.root / "sd/3ds/goldeneye-3ds"
        self.install.mkdir(parents=True)
        self.output = self.root / "build/frontend-probe/test"
        self.pid_file = self.root / "mock.pid"
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.config = self.root / "test.cfg"
        self.config.write_text("gameplay_frames 10\n")
        self.old = {}
        for name in (
            "dam-input-probe.cfg", "stage.cfg", "frontend-visual-probe.cfg",
            "probe-detail.cfg", "probe-retraces.cfg", "world-profile.cfg",
            "dam-visual-tour.geview", "frontend-probe.cfg",
            "frontend-probe.result", "dam-input-probe.result",
            "captures/old.ppm", "captures/nested/other.bin",
        ):
            path = self.install / name
            path.parent.mkdir(parents=True, exist_ok=True)
            self.old[name] = ("original " + name).encode()
            path.write_bytes(self.old[name])
        self.executable(self.bin / "pgrep", '''import os, pathlib, sys
p = pathlib.Path(os.environ["MOCK_PID"])
if not p.exists(): sys.exit(1)
try: os.kill(int(p.read_text()), 0)
except ProcessLookupError: sys.exit(1)
print(p.read_text())
''')
        self.executable(self.bin / "open", '''import os, pathlib, signal, sys, time
root = pathlib.Path(os.environ["MOCK_INSTALL"])
def stopped(signum, frame):
    if (root / "captures").is_dir():
        (root / "captures/last-frame.ppm").write_bytes(b"final capture")
    sys.exit(0)
signal.signal(signal.SIGTERM, stopped)
pathlib.Path(os.environ["MOCK_PID"]).write_text(str(os.getpid()))
mode = os.environ.get("MOCK_MODE", "success")
if mode != "interrupted":
    (root / "frontend-probe.result").write_text("status=complete\\n")
    if mode != "missing_gameplay":
        (root / "dam-input-probe.result").write_text("status=complete\\n")
    (root / "captures").mkdir(exist_ok=True)
    (root / "captures/new.ppm").write_bytes(b"new capture")
time.sleep(60)
''')
        self.executable(self.bin / "cp", '''import os, sys
if os.environ.get("MOCK_FAIL_COPY") and sys.argv[-1].endswith("/build/frontend-probe/test/frontend-probe.result"):
    print("mock output disk copy failure", file=sys.stderr)
    sys.exit(9)
os.execv(os.environ["REAL_CP"], ["cp"] + sys.argv[1:])
''')
        self.executable(self.bin / "mv", '''import os, pathlib, sys
src = pathlib.Path(sys.argv[1])
if os.environ.get("MOCK_FAIL_BACKUP") and src.name == "frontend-visual-probe.cfg" and ".frontend-probe-backup." not in str(src):
    print("mock backup failure", file=sys.stderr)
    sys.exit(8)
if os.environ.get("MOCK_FAIL_RESTORE") and src.name == "stage.cfg" and ".frontend-probe-backup." in str(src):
    print("mock restoration failure", file=sys.stderr)
    sys.exit(8)
os.execv(os.environ["REAL_MV"], ["mv"] + sys.argv[1:])
''')
        (self.scripts / "analyze_3ds_frontend_probe.py").write_text(
            'import os, sys\nsys.exit(int(os.environ.get("MOCK_ANALYZER_EXIT", "0")))\n'
        )
        # The runner checks this path; the mocked `open` owns the fake process.
        azahar = self.root / "Azahar.app/Contents/MacOS/azahar"
        azahar.parent.mkdir(parents=True)
        self.executable(azahar, "pass\n")
        self.env = dict(os.environ,
            PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
            AZAHAR_BIN=str(azahar), AZAHAR_SD=str(self.root / "sd"),
            MOCK_INSTALL=str(self.install), MOCK_PID=str(self.pid_file),
            REAL_CP=shutil.which("cp"), REAL_MV=shutil.which("mv"),
            GE_FRONTEND_PROBE_SKIP_BUILD="1", GE_FRONTEND_PROBE_LABEL="test",
            GE_FRONTEND_PROBE_TIMEOUT_SECONDS="0")

    @staticmethod
    def executable(path, body):
        path.write_text("#!/usr/bin/env python3\n" + body)
        path.chmod(0o755)

    def tearDown(self):
        if self.pid_file.exists():
            try:
                os.kill(int(self.pid_file.read_text()), signal.SIGKILL)
            except ProcessLookupError:
                pass
        self.temp.cleanup()

    def run_probe(self, **overrides):
        result = subprocess.run(
            ["/bin/bash", str(self.scripts / "run_frontend_probe.sh"), str(self.config)],
            env=dict(self.env, **overrides), capture_output=True, text=True, timeout=12)
        self.assert_emulator_stopped()
        return result

    def assert_emulator_stopped(self):
        if self.pid_file.exists():
            with self.assertRaises(ProcessLookupError):
                os.kill(int(self.pid_file.read_text()), 0)

    def assert_restored(self, excluded=()):
        for name, contents in self.old.items():
            if name not in excluded:
                self.assertEqual((self.install / name).read_bytes(), contents, name)
        self.assertFalse((self.install / "captures/new.ppm").exists())
        if not excluded:
            self.assertEqual(list(self.install.glob(".frontend-probe-backup.*")), [])

    def test_success_restores_configs_results_and_nested_captures(self):
        target = self.root / "original-detail.cfg"
        target.write_bytes(self.old["probe-detail.cfg"])
        (self.install / "probe-detail.cfg").unlink()
        (self.install / "probe-detail.cfg").symlink_to(target)
        result = self.run_probe()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assert_restored()
        self.assertEqual((self.install / "probe-detail.cfg").readlink(), target)
        self.assertEqual((self.output / "gameplay.result").read_text(), "status=complete\n")
        self.assertEqual((self.output / "captures/new.ppm").read_bytes(), b"new capture")
        self.assertEqual((self.output / "captures/last-frame.ppm").read_bytes(), b"final capture")
        self.assertFalse((self.output / "captures/old.ppm").exists())

    def test_requested_gameplay_missing_is_failure_and_restores(self):
        result = self.run_probe(MOCK_MODE="missing_gameplay")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requested gameplay probe did not complete", result.stderr)
        self.assert_restored()

    def test_output_copy_failure_cannot_interrupt_cleanup(self):
        result = self.run_probe(MOCK_FAIL_COPY="1")
        self.assertEqual(result.returncode, 9, result.stderr)
        self.assertIn("cleanup encountered errors", result.stderr)
        self.assert_restored()

    def test_partial_backup_failure_restores_already_moved_files(self):
        result = self.run_probe(MOCK_FAIL_BACKUP="1")
        self.assertEqual(result.returncode, 8, result.stderr)
        self.assert_restored()
        self.assertFalse((self.output / "frontend-probe.result").exists())

    def test_failed_restoration_retains_backup_and_restores_other_paths(self):
        result = self.run_probe(MOCK_FAIL_RESTORE="1")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("Probe backup retained for recovery:", result.stderr)
        self.assert_restored(excluded=("stage.cfg",))
        backups = list(self.install.glob(".frontend-probe-backup.*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / "stage.cfg").read_bytes(), self.old["stage.cfg"])

    def test_original_analyzer_failure_code_survives_cleanup(self):
        result = self.run_probe(MOCK_ANALYZER_EXIT="7")
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assert_restored()

    def test_empty_backup_works_with_macos_bash_nounset(self):
        for name in self.old:
            (self.install / name).unlink()
        shutil.rmtree(self.install / "captures")
        self.old.clear()
        result = self.run_probe()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assert_restored()
        self.assertFalse((self.install / "frontend-probe.cfg").exists())

    def test_sigterm_restores_original_data_and_preserves_signal_status(self):
        process = subprocess.Popen(
            ["/bin/bash", str(self.scripts / "run_frontend_probe.sh"), str(self.config)],
            env=dict(self.env, MOCK_MODE="interrupted", GE_FRONTEND_PROBE_TIMEOUT_SECONDS="30"),
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 5
            while not self.pid_file.exists() and time.monotonic() < deadline:
                time.sleep(0.025)
            self.assertTrue(self.pid_file.exists())
            process.terminate()
            _, error = process.communicate(timeout=8)
            self.assertEqual(process.returncode, 143, error)
            self.assert_emulator_stopped()
            self.assert_restored()
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()


if __name__ == "__main__":
    unittest.main()

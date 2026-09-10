#!/usr/bin/env python3
import argparse
import configparser
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
                                      probe=self.probe,output=self.root/'run',setting=[], timeout=1)

    def test_profile_preserves_audio_and_rehomes_storage(self):
        user = self.root / 'user'
        before = self.profile.read_bytes()
        runner.prepare_profile(self.profile, user/'config/qt-config.ini', user)
        c = configparser.ConfigParser(interpolation=None)
        c.read(user/'config/qt-config.ini')
        self.assertEqual(c['Audio']['enable_audio_stretching'], 'true')
        self.assertEqual(c['Data Storage']['sdmc_directory'], str(user/'sdmc')+'/')
        self.assertFalse(c.has_section('UI'))
        self.assertEqual(self.profile.read_bytes(), before)

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
        self.assertEqual(start.call_args.kwargs['cwd'],self.args.output)
        self.assertEqual(self.profile.read_text().count('/shared/'),3)

if __name__ == '__main__': unittest.main()

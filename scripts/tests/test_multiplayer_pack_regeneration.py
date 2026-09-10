#!/usr/bin/env python3
"""Synthetic provenance, isolation and failure tests; no private ROM required."""
import csv
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))
import regenerate_3ds_multiplayer_pack as tool
from gepack_update import read_pack


STAGER = r'''
import argparse,json,hashlib,pathlib
p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path);p.add_argument('--check',action='store_true');a=p.parse_args()
models=[]
for source in sorted((a.root/'build/u/assets/obseg/chr').glob('*.bin')):
 data=source.read_bytes();models.append(dict(path=source.name,size=len(data),sha256=hashlib.sha256(data).hexdigest()))
expected=dict(model_count=len(models),models=models,identity_sha256='fixture-independent-identity')
if a.check:
 assert json.loads((a.output/'manifest.json').read_text())==expected
else:
 a.output.mkdir(parents=True)
 for model in models:(a.output/model['path']).write_bytes((a.root/'build/u/assets/obseg/chr'/model['path']).read_bytes())
 (a.output/'manifest.json').write_text(json.dumps(expected,sort_keys=True))
print('fixture staged',len(models))
'''

ARENAS = r'''
import argparse,json,hashlib,pathlib
p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path);p.add_argument('--rom',type=pathlib.Path);p.add_argument('--registry',type=pathlib.Path);p.add_argument('--check',action='store_true');a=p.parse_args()
rom=a.rom.read_bytes()
for i in range(11):
 key='arena'+str(i);out=a.output/key;out.mkdir(parents=True)
 files={}
 for name,value in [('background.bin',b'background'),('setup.bin',b'setup'),('collision.gestan',b'collision'),('room_bounds.gebounds',b'bounds')]:
  (out/name).write_bytes(value);files[name]=dict(size=len(value),sha256=hashlib.sha256(value).hexdigest())
 (out/'manifest.json').write_text(json.dumps(dict(key=key,source_rom_sha1=hashlib.sha1(rom).hexdigest(),background=key,setup='Ump_'+key,files=files),sort_keys=True))
print('fixture converted 11 arenas; registry check requested',a.check)
'''


class RegenerationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="ge-regeneration-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name).resolve()

    def fixture(self):
        root = self.directory / "source"
        files = {
            "scripts/stage_3ds_character_models.py": STAGER,
            "scripts/extract_3ds_multiplayer_stages.py": ARENAS,
            "scripts/helper.py": "# converter dependency\n",
            "docs/generated/solo_stage_asset_inventory.json": "{}",
            "port/include/ge_multiplayer_stage_registry.inc": "fixture registry",
            "src/bondconstants.h": "fixture", "src/game/front.c": "fixture",
            "src/game/chr.c": "fixture", "src/game/bg.c": "fixture", "src/game/chraidata.c": "fixture",
            "assets/obseg/setup/setup.c": "fixture", "assets/obseg/stan/stan.c": "fixture",
            "assets/obseg/chr/chrModelFileRecords.inc.c": "#include <assets/obseg/chr/test/chrModelFileRecord.inc.c>",
            "assets/obseg/chr/test/chrModelFileRecord.inc.c": "fixture",
            "assets/obseg/chr/test/modelFileHeader.inc.c": "fixture",
        }
        rom = bytearray(); rows = []
        def payload(name, data):
            rows.append([len(rom), len(data), name, 0, 0]); rom.extend(data)
        for i in range(74):
            payload(f"assets/obseg/chr/Ctest{i:02}Z.bin", b"character" + bytes([i]))
        for i in range(11):
            payload(f"assets/obseg/bg/bg_arena{i}_all_p.bin", b"background")
            setup_directory = "setup/u" if i % 2 else "setup"
            payload(f"assets/obseg/{setup_directory}/Ump_arena{i}.bin", b"setup")
        filelist = io.StringIO(); csv.writer(filelist).writerows(rows)
        files["scripts/filelist.u.csv"] = filelist.getvalue()
        for name, data in files.items():
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(data)
        self.rom = self.directory / "fixture.rom"; self.rom.write_bytes(rom)
        self.sha1 = hashlib.sha1(rom).hexdigest()
        assets = self.directory / "assets"; assets.mkdir()
        (assets / "preserved.bin").write_bytes(b"do not replace this payload")
        for name in ("converted/models/characters/stale.bin", "converted/multiplayer/obsolete/data.bin"):
            path = assets / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b"stale")
        self.base = self.directory / "base.gepack"
        subprocess.run([sys.executable, str(REPO / "scripts/pack_3ds_assets.py"), "--assets", str(assets),
                        "--source-sha1", self.sha1, "--output", str(self.base)], check=True, stdout=subprocess.DEVNULL)
        return root

    def lock(self, root, name="lock.json"):
        path = self.directory / name
        check_output = subprocess.check_output
        def fixture_git(command, *args, **kwargs):
            if command[0] == "git":
                return "fixture-commit\n"
            return check_output(command, *args, **kwargs)
        with mock.patch.object(tool, "ROM_SHA1", self.sha1), mock.patch.object(tool.subprocess, "check_output", side_effect=fixture_git):
            value = tool.make_lock(root, self.rom, self.base, path)
        return path, value

    def test_full_update_repeats_and_binds_inputs_logs_and_inherited_payloads(self):
        root = self.fixture(); lock_path, lock = self.lock(root)
        original_base = self.base.read_bytes()
        original_sources = tool.source_inventory(root)
        receipts = []
        with mock.patch.object(tool, "ROM_SHA1", self.sha1):
            for name in ("first", "repeat"):
                result = tool.build(lock_path, self.directory / name)
                receipts.append(result)
                self.assertEqual(result["verified"]["character_models"], 74)
                self.assertEqual(len(result["verified"]["arena_keys"]), 11)
                self.assertEqual([r["exit_status"] for r in result["commands"]], [0, 0, 0])
                for command in result["commands"]:
                    path = self.directory / name / command["log"]
                    self.assertEqual(tool.file_identity(path), {k: command[k] for k in ("size", "sha256")})
                    self.assertIn(str(self.directory / name / "canonical-source"), command["cwd"])
                pack = read_pack(self.directory / name / "goldeneye.u.gepack")
                self.assertEqual(pack.entries["preserved.bin"], b"do not replace this payload")
                self.assertNotIn("converted/models/characters/stale.bin", pack.entries)
                self.assertNotIn("converted/multiplayer/obsolete/data.bin", pack.entries)
                self.assertIn("not a fresh full conversion", result["claim"])
            with self.assertRaisesRegex(ValueError, "already exists"):
                tool.build(lock_path, self.directory / "first")
        self.assertEqual(receipts[0]["pack_update"]["output"]["sha256"], receipts[1]["pack_update"]["output"]["sha256"])
        self.assertEqual(receipts[0]["canonical_payload_inventory_sha256"], receipts[1]["canonical_payload_inventory_sha256"])
        self.assertEqual(self.base.read_bytes(), original_base)
        self.assertEqual(tool.source_inventory(root), original_sources)

    def test_changed_converter_and_runtime_rejected_before_output(self):
        root = self.fixture(); lock_path, lock = self.lock(root)
        with mock.patch.object(tool, "ROM_SHA1", self.sha1):
            (root / "scripts/helper.py").write_text("# altered dependency\n")
            with self.assertRaisesRegex(ValueError, "converter bytes differ"):
                tool.build(lock_path, self.directory / "bad")
            self.assertFalse((self.directory / "bad").exists())
            (root / "scripts/helper.py").write_text("# converter dependency\n")
            with mock.patch.object(tool, "runtime_identity", return_value={}):
                with self.assertRaisesRegex(ValueError, "runtime"):
                    tool.build(lock_path, self.directory / "bad-runtime")

    def test_publication_interrupt_removes_unreceipted_candidate(self):
        from gepack_update import update_pack
        root = self.fixture(); lock_path, _ = self.lock(root)
        def interrupted_publication(*args, **kwargs):
            update_pack(*args, **kwargs)
            raise KeyboardInterrupt("after publication before helper return")
        with mock.patch.object(tool, "ROM_SHA1", self.sha1), mock.patch(
                "gepack_update.update_pack", side_effect=interrupted_publication):
            with self.assertRaises(KeyboardInterrupt):
                tool.build(lock_path, self.directory / "interrupted")
        output = self.directory / "interrupted"
        self.assertTrue((output / "failure.json").is_file())
        self.assertFalse((output / "goldeneye.u.gepack").exists())
        self.assertFalse((output / "receipt.json").exists())

    def test_payload_change_after_validation_cannot_receive_success_receipt(self):
        root = self.fixture(); lock_path, _ = self.lock(root)
        check_outputs = tool.check_outputs
        def changed_after_check(generated, payloads):
            validated = check_outputs(generated, payloads)
            (generated / "characters/Ctest00Z.bin").write_bytes(b"changed after canonical validation")
            return validated
        with mock.patch.object(tool, "ROM_SHA1", self.sha1), mock.patch.object(
                tool, "check_outputs", side_effect=changed_after_check):
            with self.assertRaisesRegex(ValueError, "changed after canonical validation"):
                tool.build(lock_path, self.directory / "changed-output")
        output = self.directory / "changed-output"
        self.assertTrue((output / "failure.json").is_file())
        self.assertFalse((output / "goldeneye.u.gepack").exists())
        self.assertFalse((output / "receipt.json").exists())

    def test_failed_conversion_keeps_logs_without_publish_or_input_mutation(self):
        root = self.fixture()
        (root / "scripts/stage_3ds_character_models.py").write_text("print('deliberate fixture failure');raise SystemExit(9)\n")
        lock_path, _ = self.lock(root)
        baseline = self.base.read_bytes()
        with mock.patch.object(tool, "ROM_SHA1", self.sha1):
            with self.assertRaisesRegex(ValueError, "converter characters failed"):
                tool.build(lock_path, self.directory / "failed")
        output = self.directory / "failed"
        self.assertFalse((output / "goldeneye.u.gepack").exists())
        self.assertFalse((output / "receipt.json").exists())
        failure = json.loads((output / "failure.json").read_text())
        self.assertEqual(failure["commands"][0]["exit_status"], 9)
        self.assertIn("deliberate fixture failure", (output / "logs/characters.log").read_text())
        self.assertEqual(self.base.read_bytes(), baseline)

    def test_wrong_rom_lineage_tamper_and_scope_contract_rejected(self):
        root = self.fixture()
        with self.assertRaisesRegex(ValueError, "unsupported source ROM"):
            tool.make_lock(root, self.rom, self.base, self.directory / "unsupported.json")
        _, lock = self.lock(root)
        with mock.patch.object(tool, "ROM_SHA1", self.sha1):
            bad = dict(lock, replacement_prefixes=["converted"])
            with self.assertRaisesRegex(ValueError, "lock contract"):
                tool.validate_lock(bad)
            self.rom.write_bytes(self.rom.read_bytes() + b"changed")
            with self.assertRaisesRegex(ValueError, "ROM differs"):
                tool.validate_lock(lock)

    def test_safe_sources_and_symlinks(self):
        for name in ("/absolute", "../escape", "a/../b", "a//b", "a\\b", "a/./b", "a\nb", "C:/b"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                tool.safe_name(name)
        target = self.directory / "real"; target.mkdir()
        (target / "file").write_text("data")
        alias = self.directory / "alias"; alias.symlink_to(target, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            tool.file_identity(alias / "file")
        root = self.fixture()
        (root / "scripts/linked").symlink_to(target, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            tool.source_inventory(root)

    def test_compressed_payload_hashes_and_invalid_streams(self):
        data = b"bounded canonical payload"
        compressor = zlib.compressobj(wbits=-15)
        raw = b"\x11\x72" + compressor.compress(data) + compressor.flush()
        row = f"0,{len(raw)},assets/obseg/chr/CvalidZ.bin,1,0\n"
        output = self.directory / "payloads"; output.mkdir()
        result = tool.extract_payloads(raw, row, output)
        self.assertEqual(result["assets/obseg/chr/CvalidZ.bin"]["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual((output / "build/u/assets/obseg/chr/CvalidZ.bin").read_bytes(), data)
        for stream, rows in ((raw, row + row), (raw, row.replace(",1,0", ",2,0")),
                             (raw[:-1], row), (b"bad" + raw[3:], row),
                             (raw, row.replace("CvalidZ.bin", "../escape"))):
            with self.assertRaises((ValueError, OSError, zlib.error)):
                tool.extract_payloads(stream, rows, self.directory / "bad-payload")
        with mock.patch.object(tool, "MAX_PAYLOAD", 8), self.assertRaisesRegex(ValueError, "oversized"):
            tool.extract_payloads(raw, row, self.directory / "oversized")


if __name__ == "__main__":
    unittest.main()

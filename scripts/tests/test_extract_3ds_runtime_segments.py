#!/usr/bin/env python3
"""Tests for verified, atomic private runtime-segment extraction."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "extract_3ds_runtime_segments.py"
SPEC = importlib.util.spec_from_file_location("extract_3ds_runtime_segments", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
SegmentDefinition = MODULE.SegmentDefinition


class RuntimeSegmentExtractionTests(unittest.TestCase):
    @staticmethod
    def fixture() -> tuple[bytes, tuple[object, ...]]:
        rom = b"header--FONT--gap--RAREWARE--tail"
        font_start = rom.index(b"FONT")
        rare_start = rom.index(b"RAREWARE")
        segments = (
            SegmentDefinition("fontdl", 1, font_start, font_start + 4, 0x01000000,
                              hashlib.sha256(b"FONT").hexdigest()),
            SegmentDefinition("rarewarelogo", 2, rare_start, rare_start + 8, 0x02000000,
                              hashlib.sha256(b"RAREWARE").hexdigest()),
        )
        return rom, segments

    def test_exact_outputs_manifests_stale_pruning_and_reproducibility(self) -> None:
        rom, segments = self.fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            rom_path = root / "game.z64"
            output = root / "runtime"
            rom_path.write_bytes(rom)
            output.mkdir()
            (output / "stale.bin").write_bytes(b"stale")

            manifest = MODULE.extract_runtime_segments(
                rom_path, output, segments, hashlib.sha1(rom).hexdigest())
            self.assertEqual(manifest["segment_count"], 2)
            self.assertEqual((output / "segments/fontdl.bin").read_bytes(), b"FONT")
            self.assertEqual((output / "segments/rarewarelogo.bin").read_bytes(), b"RAREWARE")
            self.assertFalse((output / "stale.bin").exists())
            first_json = (output / "manifest.json").read_bytes()
            first_binary = (output / "manifest.geseg").read_bytes()
            parsed = json.loads(first_json)
            self.assertEqual(parsed["rom_sha1"], hashlib.sha1(rom).hexdigest())
            self.assertNotIn(str(root), first_json.decode())

            header = struct.unpack_from("<8sIIIIQ", first_binary)
            self.assertEqual(header, (b"GEROMSEG", 1, 2, 32, 80, 192))
            first_entry = struct.unpack_from("<IIQQQQII32s", first_binary, 32)
            self.assertEqual(first_entry[0], 1)
            self.assertEqual(first_entry[2:6], (rom.index(b"FONT"), rom.index(b"FONT") + 4,
                                               0x01000000, 4))
            self.assertEqual(first_entry[8].hex(), hashlib.sha256(b"FONT").hexdigest())

            MODULE.extract_runtime_segments(
                rom_path, output, reversed(segments), hashlib.sha1(rom).hexdigest())
            self.assertEqual(first_json, (output / "manifest.json").read_bytes())
            self.assertEqual(first_binary, (output / "manifest.geseg").read_bytes())

    def test_rejects_wrong_rom_hash_and_out_of_bounds_segment(self) -> None:
        rom, segments = self.fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            rom_path = root / "game.z64"
            output = root / "runtime"
            rom_path.write_bytes(rom)
            with self.assertRaisesRegex(ValueError, "unsupported ROM SHA-1"):
                MODULE.extract_runtime_segments(rom_path, output, segments, "0" * 40)
            outside = SegmentDefinition("outside", 3, len(rom) - 1, len(rom) + 1, 0,
                                        hashlib.sha256(b"x").hexdigest())
            with self.assertRaisesRegex(ValueError, "beyond ROM size"):
                MODULE.extract_runtime_segments(
                    rom_path, output, (outside,), hashlib.sha1(rom).hexdigest())
            self.assertFalse(output.exists())

    def test_hash_failure_is_atomic_and_preserves_previous_output(self) -> None:
        rom, segments = self.fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            rom_path = root / "game.z64"
            output = root / "runtime"
            rom_path.write_bytes(rom)
            MODULE.extract_runtime_segments(
                rom_path, output, segments, hashlib.sha1(rom).hexdigest())
            before = {path.relative_to(output): path.read_bytes()
                      for path in output.rglob("*") if path.is_file()}
            invalid = SegmentDefinition("fontdl", 1, segments[0].rom_start,
                                        segments[0].rom_end, 0x01000000, "0" * 64)
            with self.assertRaisesRegex(ValueError, "failed verification"):
                MODULE.extract_runtime_segments(
                    rom_path, output, (invalid,), hashlib.sha1(rom).hexdigest())
            after = {path.relative_to(output): path.read_bytes()
                     for path in output.rglob("*") if path.is_file()}
            self.assertEqual(before, after)

    def test_refuses_symlink_output(self) -> None:
        rom, segments = self.fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            rom_path = root / "game.z64"
            target = root / "target"
            output = root / "runtime"
            rom_path.write_bytes(rom)
            target.mkdir()
            (target / "keep.bin").write_bytes(b"keep")
            output.symlink_to(target, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "symlink output"):
                MODULE.extract_runtime_segments(
                    rom_path, output, segments, hashlib.sha1(rom).hexdigest())
            self.assertEqual((target / "keep.bin").read_bytes(), b"keep")


if __name__ == "__main__":
    unittest.main()

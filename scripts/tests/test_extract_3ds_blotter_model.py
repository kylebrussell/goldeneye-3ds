#!/usr/bin/env python3
"""Tests for verified, atomic extraction of the blotter prop model."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

SCRIPT = Path(__file__).resolve().parents[1] / "extract_3ds_blotter_model.py"
SPEC = importlib.util.spec_from_file_location("extract_3ds_blotter_model", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
ModelDefinition = MODULE.ModelDefinition


def make_model() -> bytes:
    model = bytearray(400)
    struct.pack_into(">IBBBBBBBB", model, 0, 182, 64, 32, 7, 0, 1, 2, 2, 0)
    vertices = (
        (360, 0, -240, 0, 0, 0, 0xFE, 0xFE, 0xFE, 0xFF),
        (-360, 0, -240, 0, 2048, 0, 0xFE, 0xFE, 0xFE, 0xFF),
        (-360, 0, 240, 0, 2048, 1024, 0xFE, 0xFE, 0xFE, 0xFF),
        (360, 0, 240, 0, 0, 1024, 0xFE, 0xFE, 0xFE, 0xFF),
    )
    for index, vertex in enumerate(vertices):
        struct.pack_into(">hhhHhhBBBB", model, 0x90 + index * 16, *vertex)
    commands = (
        (0xE7000000, 0x00000000),
        (0xBA001001, 0x00010000),
        (0xBB003001, 0xFFFFFFFF),
        (0xC0580002, 0x000000B6),
        (0xBA001102, 0x00000000),
        (0xBA000C02, 0x00002000),
        (0x01020040, 0x03000000),
        (0x04300040, 0x04000000),
        (0xB1000032, 0x00002010),
        (0xB8000000, 0x00000000),
    )
    for index, command in enumerate(commands):
        struct.pack_into(">II", model, 0x138 + index * 8, *command)
    return bytes(model)


def fixture() -> tuple[bytes, ModelDefinition]:
    model = make_model()
    compressor = zlib.compressobj(level=9, wbits=-15)
    compressed = b"\x11\x72" + compressor.compress(model) + compressor.flush() + b"\0" * 5
    rom = b"fixture-prefix" + compressed + b"fixture-tail"
    start = len(b"fixture-prefix")
    definition = ModelDefinition(
        name="blotter1",
        description="Synthetic blotter fixture",
        rom_start=start,
        compressed_size=len(compressed),
        compressed_sha256=hashlib.sha256(compressed).hexdigest(),
        model_size=len(model),
        model_sha256=hashlib.sha256(model).hexdigest(),
        texture_table_offset=0,
        texture_count=1,
        texture_id=182,
        texture_source="BLOTTER.bin",
        texture_width=64,
        texture_height=32,
        vertex_offset=0x90,
        vertex_count=4,
        display_list_offset=0x138,
        display_list_commands=10,
    )
    return rom, definition


class BlotterModelExtractionTests(unittest.TestCase):
    def test_exact_outputs_stale_pruning_and_reproducibility(self) -> None:
        rom, definition = fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            rom_path = root / "game.z64"
            output = root / "model"
            rom_path.write_bytes(rom)
            output.mkdir()
            (output / "stale.bin").write_bytes(b"stale")

            manifest = MODULE.extract_model(
                rom_path, output, definition, hashlib.sha1(rom).hexdigest(), None)
            self.assertEqual(manifest["expected_pipeline"]["commands"], 10)
            self.assertEqual(manifest["expected_pipeline"]["triangles"], 2)
            self.assertEqual((output / "model.bin").read_bytes(), make_model())
            self.assertEqual(len((output / "texture_table.bin").read_bytes()), 12)
            self.assertEqual(len((output / "vertices.bin").read_bytes()), 64)
            self.assertEqual(len((output / "display_list.bin").read_bytes()), 80)
            self.assertEqual((output / "matrix_identity.bin").read_bytes(),
                             MODULE.IDENTITY_MATRIX_BE)
            self.assertFalse((output / "stale.bin").exists())
            first = {path.name: path.read_bytes() for path in output.iterdir()}
            parsed = json.loads(first["manifest.json"])
            self.assertEqual(parsed["textures"][0]["id"], 182)
            self.assertEqual(parsed["runtime_bindings"]["vertices"]["segment"], 4)
            self.assertNotIn(str(root), first["manifest.json"].decode())

            MODULE.extract_model(
                rom_path, output, definition, hashlib.sha1(rom).hexdigest(), None)
            self.assertEqual(first,
                             {path.name: path.read_bytes() for path in output.iterdir()})

    def test_rejects_rom_compressed_and_model_hash_mismatches(self) -> None:
        rom, definition = fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            rom_path = root / "game.z64"
            rom_path.write_bytes(rom)
            with self.assertRaisesRegex(ValueError, "unsupported ROM SHA-1"):
                MODULE.extract_model(rom_path, root / "out", definition, "0" * 40, None)

            bad_compressed = ModelDefinition(
                **{**definition.__dict__, "compressed_sha256": "0" * 64})
            with self.assertRaisesRegex(ValueError, "compressed model SHA-256 mismatch"):
                MODULE.extract_model(rom_path, root / "out", bad_compressed,
                                     hashlib.sha1(rom).hexdigest(), None)

            bad_model = ModelDefinition(
                **{**definition.__dict__, "model_sha256": "0" * 64})
            with self.assertRaisesRegex(ValueError, "model SHA-256 mismatch"):
                MODULE.extract_model(rom_path, root / "out", bad_model,
                                     hashlib.sha1(rom).hexdigest(), None)
            self.assertFalse((root / "out").exists())

    def test_invalid_layout_is_atomic_and_preserves_previous_output(self) -> None:
        rom, definition = fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            rom_path = root / "game.z64"
            output = root / "model"
            rom_path.write_bytes(rom)
            MODULE.extract_model(rom_path, output, definition,
                                 hashlib.sha1(rom).hexdigest(), None)
            before = {path.name: path.read_bytes() for path in output.iterdir()}
            bad_layout = ModelDefinition(
                **{**definition.__dict__, "display_list_offset": 0x139})
            with self.assertRaisesRegex(ValueError, "opcode sequence"):
                MODULE.extract_model(rom_path, output, bad_layout,
                                     hashlib.sha1(rom).hexdigest(), None)
            self.assertEqual(before,
                             {path.name: path.read_bytes() for path in output.iterdir()})

    def test_rejects_bad_stream_and_symlink_output(self) -> None:
        with self.assertRaisesRegex(ValueError, "1172 header"):
            MODULE.inflate_1172(b"not-a-stream")
        rom, definition = fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            rom_path = root / "game.z64"
            target = root / "target"
            output = root / "model"
            rom_path.write_bytes(rom)
            target.mkdir()
            (target / "keep.bin").write_bytes(b"keep")
            output.symlink_to(target, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "symlink output"):
                MODULE.extract_model(rom_path, output, definition,
                                     hashlib.sha1(rom).hexdigest(), None)
            self.assertEqual((target / "keep.bin").read_bytes(), b"keep")


if __name__ == "__main__":
    unittest.main()

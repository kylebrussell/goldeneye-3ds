#!/usr/bin/env python3
"""Tests for deterministic, validated Dam room 1 extraction."""

from __future__ import annotations

from dataclasses import replace
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib


SCRIPT = Path(__file__).resolve().parents[1] / "extract_3ds_dam_room1.py"
SPEC = importlib.util.spec_from_file_location("extract_3ds_dam_room1", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
RoomDefinition = MODULE.RoomDefinition


def compress_1172(data: bytes) -> bytes:
    compressor = zlib.compressobj(level=9, wbits=-15)
    return b"\x11\x72" + compressor.compress(data) + compressor.flush()


def make_point_table() -> bytes:
    vertices = (
        (-10, -20, -30, 0, 0, 0, 0x10, 0x20, 0x30, 0xFF),
        (10, -20, -30, 0, 32, 0, 0x40, 0x50, 0x60, 0xFF),
        (10, 20, 30, 0, 32, 32, 0x70, 0x80, 0x90, 0xFF),
        (-10, 20, 30, 0, 0, 32, 0xA0, 0xB0, 0xC0, 0xFF),
    )
    return b"".join(struct.pack(">hhhHhhBBBB", *vertex) for vertex in vertices)


def make_gdl(vertex_address: int = 0x0E000000) -> bytes:
    commands = ((0x04300040, vertex_address), (0xB8000000, 0))
    return b"".join(struct.pack(">II", *command) for command in commands)


def fixture() -> tuple[bytes, RoomDefinition]:
    point = make_point_table()
    gdl = make_gdl()
    point_stream = compress_1172(point)
    gdl_stream = compress_1172(gdl)
    point_offset, point_end = 0x100, 0x180
    primary_offset, primary_end = 0x200, 0x280
    source = bytearray(0x300)
    header = (0, MODULE.N64_BG_BASE + 0x14, MODULE.N64_BG_BASE + 0x80,
              MODULE.N64_BG_BASE + 0x90, 0)
    struct.pack_into(">5I", source, 0, *header)
    struct.pack_into(">IIIfff", source, 0x14 + 24,
                     MODULE.N64_BG_BASE + point_offset,
                     MODULE.N64_BG_BASE + primary_offset, 0, 1.0, 2.0, 3.0)
    struct.pack_into(">IIIfff", source, 0x14 + 48,
                     MODULE.N64_BG_BASE + point_end,
                     MODULE.N64_BG_BASE + primary_end, 0, 0.0, 0.0, 0.0)
    source[point_offset:point_offset + len(point_stream)] = point_stream
    source[primary_offset:primary_offset + len(gdl_stream)] = gdl_stream
    data = bytes(source)
    definition = RoomDefinition(
        source_size=len(data), source_sha256=hashlib.sha256(data).hexdigest(),
        header_words=header, room_index=1,
        point_offset=point_offset, point_span_end=point_end,
        point_compressed_size=len(point_stream),
        point_compressed_sha256=hashlib.sha256(point_stream).hexdigest(),
        point_span_sha256=hashlib.sha256(data[point_offset:point_end]).hexdigest(),
        point_size=len(point), point_sha256=hashlib.sha256(point).hexdigest(),
        primary_offset=primary_offset, primary_span_end=primary_end,
        primary_compressed_size=len(gdl_stream),
        primary_compressed_sha256=hashlib.sha256(gdl_stream).hexdigest(),
        primary_span_sha256=hashlib.sha256(data[primary_offset:primary_end]).hexdigest(),
        primary_size=len(gdl), primary_sha256=hashlib.sha256(gdl).hexdigest(),
        origin=(1.0, 2.0, 3.0), opcodes=(0x04, 0xB8),
        vertex_references=((0, 0x0E000000),),
    )
    return data, definition


class DamRoom1ExtractionTests(unittest.TestCase):
    def test_exact_atomic_outputs_and_reproducibility(self) -> None:
        source, definition = fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            source_path = root / "bg.bin"
            output = root / "dam-room1"
            source_path.write_bytes(source)
            output.mkdir()
            (output / "stale.bin").write_bytes(b"stale")

            manifest = MODULE.extract_room(source_path, output, definition)
            self.assertEqual((output / "point_table.bin").read_bytes(), make_point_table())
            self.assertEqual((output / "primary_gdl.bin").read_bytes(), make_gdl())
            self.assertFalse((output / "stale.bin").exists())
            parsed = json.loads((output / "manifest.json").read_text())
            self.assertEqual(parsed["runtime_bindings"]["point_table"]["segment"], 14)
            self.assertEqual(parsed["runtime_bindings"]["point_table"]["vertex_count"], 4)
            self.assertEqual(parsed["streams"]["point_table"]["compressed_size"],
                             definition.point_compressed_size)
            self.assertEqual(parsed["streams"]["primary_gdl"]["compression"],
                             "rare-1172-raw-deflate")
            self.assertEqual(manifest["expected_pipeline"]["commands"], 2)
            self.assertNotIn(str(root), (output / "manifest.json").read_text())
            first = {path.name: path.read_bytes() for path in output.iterdir()}
            MODULE.extract_room(source_path, output, definition)
            self.assertEqual(first, {path.name: path.read_bytes() for path in output.iterdir()})

    def test_rejects_source_header_pointer_and_alignment_changes(self) -> None:
        source, definition = fixture()
        with self.assertRaisesRegex(ValueError, "background SHA-256 mismatch"):
            MODULE.validate_source(source, replace(definition, source_sha256="0" * 64))

        changed = bytearray(source)
        struct.pack_into(">I", changed, 4, MODULE.N64_BG_BASE + 0x18)
        changed_bytes = bytes(changed)
        with self.assertRaisesRegex(ValueError, "header changed"):
            MODULE.validate_source(
                changed_bytes,
                replace(definition, source_sha256=hashlib.sha256(changed_bytes).hexdigest()),
            )

        changed = bytearray(source)
        struct.pack_into(">I", changed, 0x14 + 24, MODULE.N64_BG_BASE + 0x104)
        changed_bytes = bytes(changed)
        with self.assertRaisesRegex(ValueError, "stream pointers changed"):
            MODULE.validate_source(
                changed_bytes,
                replace(definition, source_sha256=hashlib.sha256(changed_bytes).hexdigest()),
            )

        point_span = source[definition.point_offset:definition.point_span_end]
        with self.assertRaisesRegex(ValueError, "nonzero alignment bytes"):
            MODULE.inflate_1172(point_span[:-1] + b"x", definition.point_compressed_size,
                                "point table")

    def test_rejects_opcode_and_segment_reference_changes(self) -> None:
        _source, definition = fixture()
        point = make_point_table()
        bad_opcode = bytearray(make_gdl())
        bad_opcode[0] = 0xB7
        with self.assertRaisesRegex(ValueError, "opcode sequence"):
            MODULE.validate_decoded(
                point, bytes(bad_opcode),
                replace(definition, primary_sha256=hashlib.sha256(bad_opcode).hexdigest()),
            )

        bad_reference = make_gdl(0x0D000000)
        with self.assertRaisesRegex(ValueError, "segment references"):
            MODULE.validate_decoded(
                point, bad_reference,
                replace(definition, primary_sha256=hashlib.sha256(bad_reference).hexdigest()),
            )

    def test_failure_preserves_output_and_refuses_symlink(self) -> None:
        source, definition = fixture()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            source_path = root / "bg.bin"
            output = root / "dam-room1"
            source_path.write_bytes(source)
            MODULE.extract_room(source_path, output, definition)
            before = {path.name: path.read_bytes() for path in output.iterdir()}
            with self.assertRaisesRegex(ValueError, "background SHA-256 mismatch"):
                MODULE.extract_room(source_path, output,
                                    replace(definition, source_sha256="0" * 64))
            self.assertEqual(before, {path.name: path.read_bytes() for path in output.iterdir()})

            target = root / "target"
            link = root / "link"
            target.mkdir()
            (target / "keep.bin").write_bytes(b"keep")
            link.symlink_to(target, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "symlink output"):
                MODULE.extract_room(source_path, link, definition)
            self.assertEqual((target / "keep.bin").read_bytes(), b"keep")

    @unittest.skipUnless(
        (SCRIPT.parents[1] / "build/u/assets/obseg/bg/bg_dam_all_p.bin").is_file(),
        "private US build asset is not present",
    )
    def test_checked_in_definition_matches_private_us_build(self) -> None:
        source_path = SCRIPT.parents[1] / "build/u/assets/obseg/bg/bg_dam_all_p.bin"
        point, gdl = MODULE.validate_source(source_path.read_bytes(), MODULE.DAM_ROOM_1)
        self.assertEqual(len(point), 576)
        self.assertEqual(len(gdl), 176)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Tests for deterministic multi-room Dam extraction."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib


SCRIPT = Path(__file__).resolve().parents[1] / "extract_3ds_dam_rooms.py"
SPEC = importlib.util.spec_from_file_location("extract_3ds_dam_rooms", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def compress_1172(decoded: bytes) -> bytes:
    compressor = zlib.compressobj(level=9, wbits=-15)
    return b"\x11\x72" + compressor.compress(decoded) + compressor.flush()


def points(seed: int) -> bytes:
    return b"".join(struct.pack(">hhhHhhBBBB", seed + index, index, -index,
                                0, index * 32, 0, 1, 2, 3, 0xFF)
                    for index in range(3))


def gdl(texture_id: int | None = None) -> bytes:
    commands: list[tuple[int, int]] = []
    if texture_id is not None:
        commands.append((0xC0080002, texture_id))
    commands.append((0xB8000000, 0))
    return b"".join(struct.pack(">II", *command) for command in commands)


def synthetic_background() -> tuple[bytes, dict[str, bytes]]:
    source = bytearray(0x300)
    table_start = 0x14
    table_end = table_start + 5 * MODULE.ROOM_ENTRY_SIZE
    struct.pack_into(">5I", source, 0, 0, MODULE.N64_BG_BASE + table_start,
                     MODULE.N64_BG_BASE + 0xA0,
                     MODULE.N64_BG_BASE + table_end, 0)
    entries = (
        (0, 0, 0, 0.0, 0.0, 0.0),
        (MODULE.N64_BG_BASE + 0x100, MODULE.N64_BG_BASE + 0x200,
         MODULE.N64_BG_BASE + 0x240, 1.0, 2.0, 3.0),
        (MODULE.N64_BG_BASE + 0x140, MODULE.N64_BG_BASE + 0x280,
         0, -4.0, 5.0, -6.0),
        (MODULE.N64_BG_BASE + 0x180, MODULE.N64_BG_BASE + 0x2C0,
         MODULE.N64_BG_BASE + 0x2C0, 0.0, 0.0, 0.0),
        (0, 0, 0, 0.0, 0.0, 0.0),
    )
    for index, entry in enumerate(entries):
        struct.pack_into(">IIIfff", source,
                         table_start + index * MODULE.ROOM_ENTRY_SIZE, *entry)
    decoded = {
        "room001/point_table.bin": points(10),
        "room001/primary_gdl.bin": gdl(0x123),
        "room001/secondary_gdl.bin": gdl(0x456),
        "room002/point_table.bin": points(20),
        "room002/primary_gdl.bin": gdl(0x789),
    }
    spans = (
        (0x100, 0x140, decoded["room001/point_table.bin"]),
        (0x140, 0x180, decoded["room002/point_table.bin"]),
        (0x200, 0x240, decoded["room001/primary_gdl.bin"]),
        (0x240, 0x280, decoded["room001/secondary_gdl.bin"]),
        (0x280, 0x2C0, decoded["room002/primary_gdl.bin"]),
    )
    for start, end, data in spans:
        compressed = compress_1172(data)
        assert len(compressed) <= end - start
        source[start:start + len(compressed)] = compressed
    return bytes(source), decoded


class DamRoomsExtractionTests(unittest.TestCase):
    def test_all_rooms_exact_metadata_and_reproducibility(self) -> None:
        source, decoded = synthetic_background()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            source_path = root / "dam.bin"
            output = root / "rooms"
            source_path.write_bytes(source)
            output.mkdir()
            (output / "stale.bin").write_bytes(b"stale")
            manifest = MODULE.extract_rooms(
                source_path, output, expected_size=None, expected_sha256=None)

            self.assertEqual(manifest["room_count"], 3)
            self.assertEqual(manifest["selected_rooms"], [0, 1, 2])
            self.assertFalse((output / "stale.bin").exists())
            self.assertEqual({path.relative_to(output).as_posix(): path.read_bytes()
                              for path in output.rglob("*.bin")}, decoded)
            room1 = manifest["rooms"][1]
            self.assertEqual(room1["origin"], [1.0, 2.0, 3.0])
            self.assertEqual(room1["streams"]["point_table"]["vertex_count"], 3)
            self.assertEqual(room1["streams"]["primary_gdl"]["command_count"], 2)
            self.assertEqual(room1["streams"]["primary_gdl"]["texture_ids"], [0x123])
            self.assertTrue(room1["streams"]["primary_gdl"]
                            ["ends_with_display_list_end"])
            self.assertNotIn(str(root), (output / "manifest.json").read_text())

            before = {path.relative_to(output).as_posix(): path.read_bytes()
                      for path in output.rglob("*") if path.is_file()}
            MODULE.extract_rooms(source_path, output,
                                 expected_size=None, expected_sha256=None)
            after = {path.relative_to(output).as_posix(): path.read_bytes()
                     for path in output.rglob("*") if path.is_file()}
            self.assertEqual(before, after)

    def test_selection_and_atomic_replacement(self) -> None:
        source, decoded = synthetic_background()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            source_path = root / "dam.bin"
            output = root / "rooms"
            source_path.write_bytes(source)
            manifest = MODULE.extract_rooms(
                source_path, output, ["2"], None, None)
            self.assertEqual(manifest["selected_rooms"], [2])
            self.assertEqual([room["index"] for room in manifest["rooms"]], [2])
            self.assertFalse((output / "room001").exists())
            self.assertEqual((output / "room002/point_table.bin").read_bytes(),
                             decoded["room002/point_table.bin"])

            before = (output / "manifest.json").read_bytes()
            with self.assertRaisesRegex(ValueError, "background SHA-256 mismatch"):
                MODULE.extract_rooms(source_path, output, ["1"], None, "0" * 64)
            self.assertEqual((output / "manifest.json").read_bytes(), before)

    def test_selection_validation(self) -> None:
        self.assertEqual(MODULE.parse_room_selection(["2,0-1", "2"], 3),
                         [0, 1, 2])
        for selection, message in ((["2-1"], "descending"),
                                   (["3"], "within"),
                                   (["1,"], "empty")):
            with self.assertRaisesRegex(ValueError, message):
                MODULE.parse_room_selection(selection, 3)

    def test_rejects_bad_table_and_nonzero_stream_padding(self) -> None:
        source, _decoded = synthetic_background()
        changed = bytearray(source)
        struct.pack_into(">I", changed, 12, MODULE.N64_BG_BASE + 0x8D)
        with self.assertRaisesRegex(ValueError, "bounded array"):
            MODULE.parse_room_table(bytes(changed))

        changed = bytearray(source)
        changed[0x13F] = 1
        rooms, sentinel = MODULE.parse_room_table(bytes(changed))
        boundaries = MODULE.stream_boundaries(rooms, sentinel)
        with self.assertRaisesRegex(ValueError, "nonzero bytes"):
            MODULE.inflate_1172(bytes(changed), rooms[1].point_address,
                                boundaries[rooms[1].point_address], "point")

    def test_refuses_symlink_output(self) -> None:
        source, _decoded = synthetic_background()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            source_path = root / "dam.bin"
            target = root / "target"
            link = root / "link"
            source_path.write_bytes(source)
            target.mkdir()
            link.symlink_to(target, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "symlink output"):
                MODULE.extract_rooms(source_path, link, [], None, None)

    @unittest.skipUnless(
        (SCRIPT.parents[1] / "build/u/assets/obseg/bg/bg_dam_all_p.bin").is_file(),
        "private US build asset is not present",
    )
    def test_private_us_bundle_summary(self) -> None:
        source_path = SCRIPT.parents[1] / "build/u/assets/obseg/bg/bg_dam_all_p.bin"
        with tempfile.TemporaryDirectory() as temporary_name:
            manifest = MODULE.extract_rooms(source_path, Path(temporary_name) / "rooms")
        self.assertEqual(manifest["room_count"], 137)
        self.assertEqual(len(manifest["rooms"]), 137)
        self.assertEqual(sum(len(room["streams"]) for room in manifest["rooms"]), 337)
        self.assertEqual(manifest["rooms"][1]["streams"]["point_table"]
                         ["vertex_count"], 36)
        self.assertEqual(manifest["rooms"][1]["streams"]["primary_gdl"]
                         ["command_count"], 22)
        self.assertEqual(manifest["rooms"][1]["streams"]["primary_gdl"]
                         ["texture_ids"], [949])


if __name__ == "__main__":
    unittest.main()

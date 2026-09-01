#!/usr/bin/env python3
"""Extract Dam room 1 point/GDL streams for the private 3DS asset pack."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import zlib


N64_BG_BASE = 0x0F000000


@dataclass(frozen=True)
class RoomDefinition:
    source_size: int
    source_sha256: str
    header_words: tuple[int, int, int, int, int]
    room_index: int
    point_offset: int
    point_span_end: int
    point_compressed_size: int
    point_compressed_sha256: str
    point_span_sha256: str
    point_size: int
    point_sha256: str
    primary_offset: int
    primary_span_end: int
    primary_compressed_size: int
    primary_compressed_sha256: str
    primary_span_sha256: str
    primary_size: int
    primary_sha256: str
    origin: tuple[float, float, float]
    opcodes: tuple[int, ...]
    vertex_references: tuple[tuple[int, int], ...]


DAM_ROOM_1 = RoomDefinition(
    source_size=197024,
    source_sha256="40f74a2a087fffa6f1d3519d796d2a2989377ee85fd352deb72d774ea76430c0",
    header_words=(0, 0x0F000014, 0x0F001944, 0x0F000D1C, 0),
    room_index=1,
    point_offset=0x4084,
    point_span_end=0x4204,
    point_compressed_size=381,
    point_compressed_sha256="890d52d6ca38fe8120fd0558bcd9ceba06177dc0ae51e6d32e42958ddbc71696",
    point_span_sha256="fa1e62f5ab3d0ad66499d49e05df6e1c3b00b0e5cba2726f6b913fef2d605ac0",
    point_size=576,
    point_sha256="97ac694b70d6dfdcbddbd5eb53b00108201565f1c92b71ceff8cfb6bc9354edd",
    primary_offset=0x25920,
    primary_span_end=0x259B8,
    primary_compressed_size=152,
    primary_compressed_sha256="1c79914553b197400d6831bb910d27beb00ccfe73bf1cb3fda000dc2d0df00e2",
    primary_span_sha256="1c79914553b197400d6831bb910d27beb00ccfe73bf1cb3fda000dc2d0df00e2",
    primary_size=176,
    primary_sha256="19614465bcfe0e50cbb65dce76d45e58b43b94cb26f7165b2bf0086f1bc89955",
    origin=(3536.0, 850.0, -693.0),
    opcodes=(
        0xE7, 0xBA, 0xB9, 0xFC, 0xBA, 0xBB, 0xC0, 0xBA, 0xBA, 0xB7, 0xFB,
        0x04, 0xB1, 0xB1, 0x04, 0xB1, 0xB1, 0xB1, 0xB1, 0x04, 0xB1, 0xB8,
    ),
    vertex_references=((11, 0x0E000000), (14, 0x0E000100), (19, 0x0E000200)),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def inflate_1172(span: bytes, expected_compressed_size: int, label: str) -> bytes:
    if len(span) < 3 or span[:2] != b"\x11\x72":
        raise ValueError(f"{label} does not have a Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    decoded = inflater.decompress(span[2:]) + inflater.flush()
    if not inflater.eof or inflater.unconsumed_tail:
        raise ValueError(f"{label} has an incomplete raw-deflate stream")
    consumed = len(span) - len(inflater.unused_data)
    if consumed != expected_compressed_size:
        raise ValueError(
            f"unexpected {label} compressed size: {consumed} (expected {expected_compressed_size})"
        )
    if any(inflater.unused_data):
        raise ValueError(f"{label} has nonzero alignment bytes after its deflate stream")
    return decoded


def validate_source(source: bytes, definition: RoomDefinition) -> tuple[bytes, bytes]:
    if len(source) != definition.source_size:
        raise ValueError(f"unexpected background size: {len(source)}")
    if sha256(source) != definition.source_sha256:
        raise ValueError(f"background SHA-256 mismatch: {sha256(source)}")
    if len(source) < 20 or struct.unpack_from(">5I", source) != definition.header_words:
        raise ValueError("Dam background header changed")

    room_table_offset = definition.header_words[1] - N64_BG_BASE
    entry_offset = room_table_offset + definition.room_index * 24
    next_entry_offset = entry_offset + 24
    if entry_offset < 0 or next_entry_offset + 12 > len(source):
        raise ValueError("room table entry is outside the background")
    point_address, primary_address, secondary_address, x, y, z = struct.unpack_from(
        ">IIIfff", source, entry_offset
    )
    if (point_address - N64_BG_BASE, primary_address - N64_BG_BASE) != (
        definition.point_offset,
        definition.primary_offset,
    ):
        raise ValueError("room 1 stream pointers changed")
    if secondary_address != 0 or (x, y, z) != definition.origin:
        raise ValueError("room 1 secondary pointer or origin changed")
    next_point, next_primary, _next_secondary = struct.unpack_from(">III", source, next_entry_offset)
    if (next_point - N64_BG_BASE, next_primary - N64_BG_BASE) != (
        definition.point_span_end,
        definition.primary_span_end,
    ):
        raise ValueError("room 2 no longer bounds the room 1 stream spans")

    point_span = source[definition.point_offset:definition.point_span_end]
    primary_span = source[definition.primary_offset:definition.primary_span_end]
    if sha256(point_span) != definition.point_span_sha256:
        raise ValueError(f"point-table span SHA-256 mismatch: {sha256(point_span)}")
    if sha256(primary_span) != definition.primary_span_sha256:
        raise ValueError(f"primary-GDL span SHA-256 mismatch: {sha256(primary_span)}")
    if sha256(point_span[:definition.point_compressed_size]) != definition.point_compressed_sha256:
        raise ValueError("point-table compressed stream SHA-256 mismatch")
    if sha256(primary_span[:definition.primary_compressed_size]) != definition.primary_compressed_sha256:
        raise ValueError("primary-GDL compressed stream SHA-256 mismatch")

    point_table = inflate_1172(point_span, definition.point_compressed_size, "point table")
    primary_gdl = inflate_1172(primary_span, definition.primary_compressed_size, "primary GDL")
    validate_decoded(point_table, primary_gdl, definition)
    return point_table, primary_gdl


def validate_decoded(point_table: bytes, primary_gdl: bytes, definition: RoomDefinition) -> None:
    if len(point_table) != definition.point_size or sha256(point_table) != definition.point_sha256:
        raise ValueError(f"decoded point-table size/hash mismatch: {len(point_table)} {sha256(point_table)}")
    if len(point_table) % 16 != 0:
        raise ValueError("decoded point table is not an array of 16-byte vertices")
    for offset in range(0, len(point_table), 16):
        _x, _y, _z, flag, _s, _t, _r, _g, _b, alpha = struct.unpack_from(
            ">hhhHhhBBBB", point_table, offset
        )
        if flag != 0 or alpha != 0xFF:
            raise ValueError(f"unexpected vertex flag/alpha at index {offset // 16}")

    if len(primary_gdl) != definition.primary_size or sha256(primary_gdl) != definition.primary_sha256:
        raise ValueError(f"decoded primary-GDL size/hash mismatch: {len(primary_gdl)} {sha256(primary_gdl)}")
    if len(primary_gdl) % 8 != 0:
        raise ValueError("decoded primary GDL is not an array of 8-byte commands")
    commands = [struct.unpack_from(">II", primary_gdl, offset)
                for offset in range(0, len(primary_gdl), 8)]
    opcodes = tuple(word0 >> 24 for word0, _word1 in commands)
    if opcodes != definition.opcodes:
        raise ValueError(f"unexpected primary-GDL opcode sequence: {opcodes}")
    actual_vertex_refs = tuple((index, word1) for index, (word0, word1) in enumerate(commands)
                               if word0 >> 24 == 0x04)
    if actual_vertex_refs != definition.vertex_references:
        raise ValueError(f"unexpected primary-GDL vertex segment references: {actual_vertex_refs}")
    for _index, address in actual_vertex_refs:
        if address >> 24 != 0x0E or (address & 0x00FFFFFF) >= len(point_table):
            raise ValueError(f"primary GDL has an out-of-range point-table reference: 0x{address:08x}")
    if commands[-1] != (0xB8000000, 0):
        raise ValueError("primary GDL is missing its terminating command")


def replace_directory(staging: Path, output: Path) -> None:
    def remove(path: Path) -> None:
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.exists():
            shutil.rmtree(path)

    if output.is_symlink():
        raise ValueError(f"refusing to replace symlink output: {output}")
    old = output.with_name(output.name + ".old")
    remove(old)
    moved_old = False
    if output.exists():
        os.replace(output, old)
        moved_old = True
    try:
        os.replace(staging, output)
    except BaseException:
        if moved_old and old.exists() and not output.exists():
            os.replace(old, output)
        raise
    if moved_old:
        remove(old)


def extract_room(source_path: Path, output: Path,
                 definition: RoomDefinition = DAM_ROOM_1) -> dict[str, object]:
    point_table, primary_gdl = validate_source(source_path.read_bytes(), definition)
    sections = {"point_table.bin": point_table, "primary_gdl.bin": primary_gdl}
    manifest: dict[str, object] = {
        "schema": 1,
        "name": "dam-room1",
        "description": "Dam background room 1 point table and primary display list",
        "source": {
            "path": "build/u/assets/obseg/bg/bg_dam_all_p.bin",
            "size": definition.source_size,
            "sha256": definition.source_sha256,
        },
        "streams": {
            "point_table": {
                "source_offset": definition.point_offset,
                "span_size": definition.point_span_end - definition.point_offset,
                "compressed_size": definition.point_compressed_size,
                "compressed_sha256": definition.point_compressed_sha256,
                "compression": "rare-1172-raw-deflate",
            },
            "primary_gdl": {
                "source_offset": definition.primary_offset,
                "span_size": definition.primary_span_end - definition.primary_offset,
                "compressed_size": definition.primary_compressed_size,
                "compressed_sha256": definition.primary_compressed_sha256,
                "compression": "rare-1172-raw-deflate",
            },
        },
        "room": {"index": definition.room_index, "origin": list(definition.origin)},
        "runtime_bindings": {
            "point_table": {"path": "point_table.bin", "segment": 14, "offset": 0,
                            "vertex_count": len(point_table) // 16, "stride": 16},
            "primary_gdl": {"path": "primary_gdl.bin", "command_count": len(primary_gdl) // 8},
        },
        "sections": [
            {"path": name, "size": len(data), "sha256": sha256(data)}
            for name, data in sections.items()
        ],
        "expected_pipeline": {"commands": len(primary_gdl) // 8,
                              "vertex_batches": len(definition.vertex_references)},
    }
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()

    output = output.expanduser().absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-dam-room1-", dir=output.parent) as temporary_name:
        staging = Path(temporary_name) / "output"
        staging.mkdir()
        for name, data in sections.items():
            (staging / name).write_bytes(data)
        (staging / "manifest.json").write_bytes(manifest_bytes)
        for name, data in sections.items():
            written = (staging / name).read_bytes()
            if written != data:
                raise OSError(f"written section failed verification: {name}")
        replace_directory(staging, output)
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        manifest = extract_room(args.input, args.output)
    except (OSError, ValueError, zlib.error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"extracted Dam room 1 -> {args.output}")
    for section in manifest["sections"]:
        print(f"  {section['path']}: {section['size']} bytes sha256={section['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

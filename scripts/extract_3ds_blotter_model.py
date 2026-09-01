#!/usr/bin/env python3
"""Extract the GoldenEye desk-blotter prop into portable GBI runtime sections."""

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

SUPPORTED_US_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"

IDENTITY_MATRIX_BE = bytes.fromhex(
    "00010000000000000000000100000000"
    "00000000000100000000000000000001"
    "00000000000000000000000000000000"
    "00000000000000000000000000000000"
)


@dataclass(frozen=True)
class ModelDefinition:
    name: str
    description: str
    rom_start: int
    compressed_size: int
    compressed_sha256: str
    model_size: int
    model_sha256: str
    texture_table_offset: int
    texture_count: int
    texture_id: int
    texture_source: str
    texture_width: int
    texture_height: int
    vertex_offset: int
    vertex_count: int
    display_list_offset: int
    display_list_commands: int


# filelist.u.csv and build/u/ge007.u.map both place Pblotter1Z at 0x7b2790.
BLOTTER = ModelDefinition(
    name="blotter1",
    description="Desk Blotter prop (PROP_BLOTTER1)",
    rom_start=0x7B2790,
    compressed_size=224,
    compressed_sha256="5980a683a8a46ffad779d342b9ad420738cad636bd85e8fb6eecf3b04bf4cd41",
    model_size=400,
    model_sha256="ef7456e5edde110e8b2632e1ee1feb059ceffab4401fb165c8fcdf375d69e668",
    texture_table_offset=0x000,
    texture_count=1,
    texture_id=182,
    texture_source="BLOTTER.bin",
    texture_width=64,
    texture_height=32,
    vertex_offset=0x090,
    vertex_count=4,
    display_list_offset=0x138,
    display_list_commands=10,
)

EXPECTED_SECTION_SHA256 = {
    "texture_table.bin": "fe08de7c6378c409fc73fb16f3c844ccd8460e125f8363340dcc628f87092155",
    "vertices.bin": "ec38236f0927cba752ea9bdf2b74f8792c32dc906cbdeb4b7171c8c5047c5229",
    "display_list.bin": "8b49b83f8170b9964902e506d4c40b3e8cb98f69aa1f404a6166ffd1440caf46",
    "matrix_identity.bin": "3cc03f81f5d9a6c8cca5b9b3db46efaa284d60c34d7d5d5083ff809e30bbc52e",
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def inflate_1172(compressed: bytes) -> bytes:
    if len(compressed) < 3 or compressed[:2] != b"\x11\x72":
        raise ValueError("model resource does not have a Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    model = inflater.decompress(compressed[2:]) + inflater.flush()
    if not inflater.eof or inflater.unconsumed_tail:
        raise ValueError("model resource has an incomplete deflate stream")
    if any(inflater.unused_data):
        raise ValueError("model resource has nonzero bytes after the deflate stream")
    return model


def validate_model(model: bytes, definition: ModelDefinition) -> dict[str, bytes]:
    texture_size = definition.texture_count * 12
    vertex_size = definition.vertex_count * 16
    display_list_size = definition.display_list_commands * 8
    required_end = max(
        definition.texture_table_offset + texture_size,
        definition.vertex_offset + vertex_size,
        definition.display_list_offset + display_list_size,
    )
    if len(model) != definition.model_size or required_end > len(model):
        raise ValueError(f"unexpected model size/layout: {len(model)} bytes")
    if sha256(model) != definition.model_sha256:
        raise ValueError(f"model SHA-256 mismatch: {sha256(model)}")

    texture_table = model[definition.texture_table_offset:
                          definition.texture_table_offset + texture_size]
    vertices = model[definition.vertex_offset:definition.vertex_offset + vertex_size]
    display_list = model[definition.display_list_offset:
                         definition.display_list_offset + display_list_size]
    texture = struct.unpack_from(">IBBBBBBBB", texture_table)
    if (texture[0] != definition.texture_id or texture[1] != definition.texture_width or
            texture[2] != definition.texture_height):
        raise ValueError(f"unexpected texture table record: {texture}")

    commands = [struct.unpack_from(">II", display_list, index * 8)
                for index in range(definition.display_list_commands)]
    opcodes = [word0 >> 24 for word0, _word1 in commands]
    if opcodes != [0xE7, 0xBA, 0xBB, 0xC0, 0xBA, 0xBA, 0x01, 0x04, 0xB1, 0xB8]:
        raise ValueError(f"unexpected display-list opcode sequence: {opcodes}")
    if commands[3][1] & 0xFFF != definition.texture_id:
        raise ValueError("display-list texture reference disagrees with texture table")
    if commands[6][1] != 0x03000000 or commands[7][1] != 0x04000000:
        raise ValueError("display-list matrix/vertex segment references changed")
    if commands[-1] != (0xB8000000, 0):
        raise ValueError("display list is missing its terminating command")

    return {
        "model.bin": model,
        "texture_table.bin": texture_table,
        "vertices.bin": vertices,
        "display_list.bin": display_list,
        # GoldenEye supplies segment 3 matrices at draw time. This identity is
        # an explicit portable preview binding, not model-authored geometry.
        "matrix_identity.bin": IDENTITY_MATRIX_BE,
    }


def replace_directory(staging: Path, output: Path) -> None:
    def remove_path(path: Path) -> None:
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.exists():
            shutil.rmtree(path)

    if output.is_symlink():
        raise ValueError(f"refusing to replace symlink output: {output}")
    old = output.with_name(output.name + ".old")
    remove_path(old)
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
        remove_path(old)


def extract_model(rom_path: Path, output: Path, definition: ModelDefinition = BLOTTER,
                  expected_rom_sha1: str = SUPPORTED_US_SHA1,
                  expected_section_hashes: dict[str, str] | None = EXPECTED_SECTION_SHA256
                  ) -> dict[str, object]:
    rom = rom_path.read_bytes()
    rom_sha1 = hashlib.sha1(rom).hexdigest()
    if rom_sha1 != expected_rom_sha1:
        raise ValueError(f"unsupported ROM SHA-1: {rom_sha1}")
    rom_end = definition.rom_start + definition.compressed_size
    if definition.rom_start < 0 or rom_end > len(rom):
        raise ValueError("compressed model range is outside the ROM")
    compressed = rom[definition.rom_start:rom_end]
    if sha256(compressed) != definition.compressed_sha256:
        raise ValueError(f"compressed model SHA-256 mismatch: {sha256(compressed)}")
    sections = validate_model(inflate_1172(compressed), definition)

    section_records = []
    for name, data in sections.items():
        digest = sha256(data)
        if expected_section_hashes is not None and name != "model.bin":
            expected = expected_section_hashes.get(name)
            if expected is None or digest != expected:
                raise ValueError(f"section {name} SHA-256 mismatch: {digest}")
        section_records.append({"path": name, "size": len(data), "sha256": digest})

    manifest = {
        "schema": 1,
        "name": definition.name,
        "description": definition.description,
        "rom": {
            "start": definition.rom_start,
            "end": rom_end,
            "compressed_size": definition.compressed_size,
            "compressed_sha256": definition.compressed_sha256,
            "source_sha1": rom_sha1,
            "compression": "rare-1172-raw-deflate",
        },
        "model": {
            "base_address": 0x05000000,
            "size": definition.model_size,
            "sha256": definition.model_sha256,
        },
        "runtime_bindings": {
            "display_list": {"path": "display_list.bin", "segment": 5, "offset": 0,
                             "original_model_offset": definition.display_list_offset,
                             "command_count": definition.display_list_commands},
            "vertices": {"path": "vertices.bin", "segment": 4, "offset": 0,
                         "original_model_offset": definition.vertex_offset,
                         "count": definition.vertex_count, "stride": 16},
            "matrices": {"path": "matrix_identity.bin", "segment": 3, "offset": 0,
                         "count": 1, "stride": 64, "policy": "portable-preview-identity",
                         "original_reference": "0x03000000"},
        },
        "textures": [{
            "id": definition.texture_id,
            "source": definition.texture_source,
            "width": definition.texture_width,
            "height": definition.texture_height,
            "table_offset": definition.texture_table_offset,
            "display_list_command": 3,
        }],
        "expected_pipeline": {
            "commands": definition.display_list_commands,
            "vertex_batches": 1,
            "vertices": definition.vertex_count,
            "matrix_loads": 1,
            "draw_calls": 1,
            "triangles": 2,
        },
        "sections": section_records,
    }
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")

    output = output.expanduser().absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-model-", dir=output.parent) as temporary_name:
        staging = Path(temporary_name) / "output"
        staging.mkdir()
        for name, data in sections.items():
            (staging / name).write_bytes(data)
        (staging / "manifest.json").write_bytes(manifest_bytes)
        for record in section_records:
            written = (staging / str(record["path"])).read_bytes()
            if len(written) != record["size"] or sha256(written) != record["sha256"]:
                raise OSError(f"written section failed verification: {record['path']}")
        replace_directory(staging, output)
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        manifest = extract_model(args.rom, args.output)
    except (OSError, ValueError, zlib.error) as error:
        raise SystemExit(f"cannot extract blotter model: {error}") from error
    print(f"extracted {manifest['description']} -> {args.output.absolute()}")
    for section in manifest["sections"]:
        print(f"  {section['path']}: {section['size']} bytes sha256={section['sha256']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

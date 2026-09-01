#!/usr/bin/env python3
"""Compile the deterministic JSON texture catalog into a runtime binary."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import struct
import sys
import re

MAGIC = b"GETEXCAT"
VERSION = 2
HEADER_SIZE = 40
ENTRY_SIZE = 48
RESOURCE_PREFIX = "converted/textures/t3x/"
MAX_ENTRIES = 1_000_000


def fnv1a64(value: bytes) -> int:
    result = 14695981039346656037
    for byte in value:
        result ^= byte
        result = (result * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return result


def checked_string(value: object, field: str) -> str:
    if not isinstance(value, str) or not value or "\0" in value:
        raise ValueError(f"{field} must be a non-empty string without NUL bytes")
    encoded = value.encode("utf-8")
    if len(encoded) > 0xFFFFFFFF:
        raise ValueError(f"{field} is too long")
    return value


def checked_u32(value: object, field: str, *, minimum: int = 0) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum or value > 0xFFFFFFFF:
        raise ValueError(f"{field} is outside the uint32 range")
    return value


def checked_u64(value: object, field: str, *, minimum: int = 0) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum or value > 0xFFFFFFFFFFFFFFFF:
        raise ValueError(f"{field} is outside the uint64 range")
    return value


def compile_catalog(document: object, image_ids: dict[str, int] | None = None) -> bytes:
    if not isinstance(document, dict) or document.get("schema") != 1:
        raise ValueError("unsupported JSON texture catalog schema")
    textures = document.get("textures")
    if not isinstance(textures, list):
        raise ValueError("textures must be an array")

    logical_entries: list[tuple[int, bytes, bytes, int, int, int, int, int]] = []
    for texture_index, texture in enumerate(textures):
        if not isinstance(texture, dict):
            raise ValueError(f"textures[{texture_index}] must be an object")
        source = checked_string(texture.get("source"), f"textures[{texture_index}].source")
        source_path = Path(source)
        if source_path.is_absolute() or ".." in source_path.parts or source_path.suffix != ".bin" or "\\" in source:
            raise ValueError(f"textures[{texture_index}].source is not a safe relative .bin path")
        source_bytes = source.encode("utf-8")
        image_id_value = texture.get("image_id")
        if image_id_value is None and image_ids is not None:
            image_id_value = image_ids.get(source_path.stem)
        image_key = 0
        if image_id_value is not None:
            image_id = checked_u32(image_id_value, f"textures[{texture_index}].image_id")
            if image_id > 0xFFF:
                raise ValueError(f"textures[{texture_index}].image_id exceeds the GBI 12-bit range")
            image_key = image_id + 1
        lods = texture.get("lods")
        if not isinstance(lods, list) or not lods:
            raise ValueError(f"textures[{texture_index}].lods must be a non-empty array")
        for lod_index, lod_record in enumerate(lods):
            field = f"textures[{texture_index}].lods[{lod_index}]"
            if not isinstance(lod_record, dict):
                raise ValueError(f"{field} must be an object")
            lod = checked_u32(lod_record.get("lod"), f"{field}.lod")
            if lod > 31:
                raise ValueError(f"{field}.lod exceeds the runtime limit")
            width = checked_u32(lod_record.get("width"), f"{field}.width", minimum=1)
            height = checked_u32(lod_record.get("height"), f"{field}.height", minimum=1)
            data_size = checked_u64(lod_record.get("t3x_size"), f"{field}.t3x_size", minimum=1)
            t3x = checked_string(lod_record.get("t3x"), f"{field}.t3x")
            t3x_path = Path(t3x)
            if (t3x_path.is_absolute() or ".." in t3x_path.parts or
                    t3x_path.suffix != ".t3x" or "\\" in t3x):
                raise ValueError(f"{field}.t3x is not a safe relative T3X path")
            resource = (RESOURCE_PREFIX + t3x_path.as_posix()).encode("utf-8")
            logical_entries.append((fnv1a64(source_bytes), source_bytes, resource,
                                    lod, width, height, image_key, data_size))

    if len(logical_entries) > MAX_ENTRIES:
        raise ValueError("texture catalog has too many entries")
    if document.get("texture_count") != len(textures):
        raise ValueError("texture_count does not match textures array")
    if document.get("lod_count") != len(logical_entries):
        raise ValueError("lod_count does not match texture LOD records")
    logical_entries.sort(key=lambda item: (item[0], item[1], item[3]))
    keys = [(item[1], item[3]) for item in logical_entries]
    if len(keys) != len(set(keys)):
        raise ValueError("duplicate source/LOD texture entry")

    strings = bytearray()
    string_offsets: dict[bytes, int] = {}

    def intern(value: bytes) -> int:
        if value not in string_offsets:
            string_offsets[value] = len(strings)
            strings.extend(value)
            strings.append(0)
        return string_offsets[value]

    encoded_entries = bytearray()
    for path_hash, source, resource, lod, width, height, image_key, data_size in logical_entries:
        source_offset = intern(source)
        resource_offset = intern(resource)
        encoded_entries.extend(struct.pack(
            "<QIIIIIIIIQ", path_hash, source_offset, len(source), resource_offset,
            len(resource), lod, width, height, image_key, data_size,
        ))

    entries_offset = HEADER_SIZE
    strings_offset = entries_offset + len(encoded_entries)
    header = struct.pack("<8sIIIIQQ", MAGIC, VERSION, 0, len(logical_entries),
                         HEADER_SIZE, entries_offset, strings_offset)
    return header + encoded_entries + strings


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="input catalog.json")
    parser.add_argument("--output", type=Path, required=True, help="output catalog.gecat")
    parser.add_argument("--images-def", type=Path,
                        help="images.def used to retain original numeric image IDs")
    return parser.parse_args()


def load_image_ids(path: Path | None) -> dict[str, int] | None:
    if path is None:
        return None
    result: dict[str, int] = {}
    pattern = re.compile(r"^\s*IMAGE\(([^,]+),")
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match is not None:
            name = match.group(1).strip()
            if not name or name in result:
                raise ValueError(f"duplicate or empty image name: {name!r}")
            result[name] = len(result)
    if not result:
        raise ValueError("images.def contains no IMAGE records")
    return result


def main() -> int:
    args = parse_args()
    try:
        document = json.loads(args.input.read_text(encoding="utf-8"))
        encoded = compile_catalog(document, load_image_ids(args.images_def))
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        raise SystemExit(f"cannot compile texture catalog: {error}") from error
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_bytes(encoded)
    os.replace(temporary, args.output)
    print(f"compiled {len(encoded)} byte runtime texture catalog -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

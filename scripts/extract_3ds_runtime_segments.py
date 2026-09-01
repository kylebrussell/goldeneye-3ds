#!/usr/bin/env python3
"""Extract verified private runtime segments from a supported GoldenEye ROM."""

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
from typing import Iterable

SUPPORTED_US_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"
MANIFEST_MAGIC = b"GEROMSEG"
MANIFEST_VERSION = 1
MANIFEST_HEADER_SIZE = 32
MANIFEST_ENTRY_SIZE = 80


@dataclass(frozen=True)
class SegmentDefinition:
    name: str
    segment_id: int
    rom_start: int
    rom_end: int
    virtual_address: int
    sha256: str

    @property
    def size(self) -> int:
        return self.rom_end - self.rom_start

    @property
    def output_path(self) -> str:
        return f"segments/{self.name}.bin"


# Confirmed by ge007.ld and build/u/ge007.u.map symbols:
# _fontdlSegmentRomStart/End and _rarewarelogoSegmentRomStart/End.
SEGMENTS = (
    SegmentDefinition(
        name="fontdl",
        segment_id=1,
        rom_start=0x117880,
        rom_end=0x117940,
        virtual_address=0x01000000,
        sha256="311de626fd1f90a2ec2d169c4dd4d7c6d745b86123e9b11541506721e4c8f6f7",
    ),
    SegmentDefinition(
        name="rarewarelogo",
        segment_id=2,
        rom_start=0x29E560,
        rom_end=0x2A4D50,
        virtual_address=0x02000000,
        sha256="90a5c04ed8461d21e8a766fb01673574b0484604a0284c65cab5527172b2decd",
    ),
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validated_definitions(definitions: Iterable[SegmentDefinition]) -> list[SegmentDefinition]:
    segments = sorted(definitions, key=lambda item: (item.segment_id, item.name.encode("utf-8")))
    ids: set[int] = set()
    names: set[str] = set()
    for segment in segments:
        if (not segment.name or not segment.name.isascii() or not segment.name.replace("_", "").isalnum() or
                segment.name in names or segment.segment_id in ids):
            raise ValueError(f"invalid or duplicate segment definition: {segment.name}")
        if not 0 < segment.segment_id <= 0xFFFFFFFF:
            raise ValueError(f"invalid segment id for {segment.name}")
        if not 0 <= segment.rom_start < segment.rom_end <= 0xFFFFFFFFFFFFFFFF:
            raise ValueError(f"invalid ROM range for {segment.name}")
        if not 0 <= segment.virtual_address <= 0xFFFFFFFFFFFFFFFF:
            raise ValueError(f"invalid virtual address for {segment.name}")
        if len(segment.sha256) != 64 or any(char not in "0123456789abcdef" for char in segment.sha256):
            raise ValueError(f"invalid expected SHA-256 for {segment.name}")
        ids.add(segment.segment_id)
        names.add(segment.name)
    return segments


def encode_binary_manifest(records: list[dict[str, object]]) -> bytes:
    strings = bytearray()
    encoded_entries = bytearray()
    for record in records:
        path = str(record["path"]).encode("utf-8")
        path_offset = len(strings)
        strings.extend(path)
        strings.append(0)
        encoded_entries.extend(struct.pack(
            "<IIQQQQII32s",
            int(record["segment_id"]), 0, int(record["rom_start"]), int(record["rom_end"]),
            int(record["virtual_address"]), int(record["size"]), path_offset, len(path),
            bytes.fromhex(str(record["sha256"])),
        ))
    strings_offset = MANIFEST_HEADER_SIZE + len(encoded_entries)
    header = struct.pack(
        "<8sIIIIQ", MANIFEST_MAGIC, MANIFEST_VERSION, len(records),
        MANIFEST_HEADER_SIZE, MANIFEST_ENTRY_SIZE, strings_offset,
    )
    return header + encoded_entries + strings


def replace_directory(staging: Path, destination: Path) -> None:
    if destination.is_symlink():
        raise ValueError(f"refusing to replace symlink output directory: {destination}")
    old = destination.with_name(destination.name + ".old")
    if old.is_symlink():
        old.unlink()
    elif old.exists():
        shutil.rmtree(old)
    moved_old = False
    if destination.exists():
        os.replace(destination, old)
        moved_old = True
    try:
        os.replace(staging, destination)
    except BaseException:
        if moved_old and old.exists() and not destination.exists():
            os.replace(old, destination)
        raise
    if moved_old:
        shutil.rmtree(old)


def extract_runtime_segments(rom_path: Path, output: Path,
                             definitions: Iterable[SegmentDefinition] = SEGMENTS,
                             expected_rom_sha1: str = SUPPORTED_US_SHA1) -> dict[str, object]:
    segments = validated_definitions(definitions)
    rom_data = rom_path.read_bytes()
    rom_sha1 = hashlib.sha1(rom_data).hexdigest()
    if rom_sha1 != expected_rom_sha1:
        raise ValueError(f"unsupported ROM SHA-1: {rom_sha1}")

    records: list[dict[str, object]] = []
    payloads: list[tuple[SegmentDefinition, bytes]] = []
    for segment in segments:
        if segment.rom_end > len(rom_data):
            raise ValueError(
                f"segment {segment.name} ends at 0x{segment.rom_end:x}, beyond ROM size 0x{len(rom_data):x}"
            )
        payload = rom_data[segment.rom_start:segment.rom_end]
        digest = sha256_bytes(payload)
        if len(payload) != segment.size or digest != segment.sha256:
            raise ValueError(
                f"segment {segment.name} failed verification: size={len(payload)} sha256={digest}"
            )
        payloads.append((segment, payload))
        records.append({
            "name": segment.name,
            "segment_id": segment.segment_id,
            "rom_start": segment.rom_start,
            "rom_end": segment.rom_end,
            "virtual_address": segment.virtual_address,
            "size": segment.size,
            "sha256": digest,
            "path": segment.output_path,
        })

    manifest = {
        "schema": 1,
        "rom_sha1": rom_sha1,
        "segment_count": len(records),
        "segments": records,
    }
    json_manifest = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")
    binary_manifest = encode_binary_manifest(records)

    output = output.expanduser().absolute()
    if output.is_symlink():
        raise ValueError(f"refusing to replace symlink output directory: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-runtime-segments-", dir=output.parent) as temporary_name:
        staging = Path(temporary_name) / "output"
        (staging / "segments").mkdir(parents=True)
        for segment, payload in payloads:
            destination = staging / segment.output_path
            destination.write_bytes(payload)
            if destination.stat().st_size != segment.size or sha256_bytes(destination.read_bytes()) != segment.sha256:
                raise OSError(f"written segment failed verification: {segment.name}")
        (staging / "manifest.json").write_bytes(json_manifest)
        (staging / "manifest.geseg").write_bytes(binary_manifest)
        replace_directory(staging, output)

    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True, help="verified US .z64 ROM")
    parser.add_argument("--output", type=Path, required=True, help="private generated output directory")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        manifest = extract_runtime_segments(args.rom, args.output)
    except (OSError, ValueError) as error:
        raise SystemExit(f"cannot extract runtime segments: {error}") from error
    print(f"extracted {manifest['segment_count']} verified runtime segments -> {args.output.resolve()}")
    for segment in manifest["segments"]:
        print(f"  {segment['name']}: {segment['size']} bytes sha256={segment['sha256']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Build a deterministic, local-only GoldenEye runtime asset pack."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import struct
import sys

MAGIC = b"GEPACK\0\0"
VERSION = 1
HEADER_SIZE = 80
ENTRY_SIZE = 32
ALIGNMENT = 16
SUPPORTED_US_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"


def fnv1a64(value: bytes) -> int:
    result = 14695981039346656037
    for byte in value:
        result ^= byte
        result = (result * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return result


def align(value: int) -> int:
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True, help="extracted assets directory")
    parser.add_argument("--output", type=Path, required=True, help="output .gepack")
    parser.add_argument("--extra", action="append", default=[], metavar="NAME=FILE",
                        help="add a converted runtime resource under NAME")
    parser.add_argument("--extra-dir", action="append", default=[], metavar="PREFIX=DIR",
                        help="recursively add regular files from DIR under PREFIX")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--rom", type=Path, help="source ROM used to validate the asset region")
    source.add_argument("--source-sha1", help="40-digit SHA-1, intended for tests")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    asset_root = args.assets.resolve()
    output = args.output.resolve()
    if not asset_root.is_dir():
        raise SystemExit(f"asset directory not found: {asset_root}")

    if args.rom is not None:
        digest = hashlib.sha1(args.rom.read_bytes()).hexdigest()
        if digest != SUPPORTED_US_SHA1:
            raise SystemExit(f"unsupported ROM SHA-1: {digest}")
    else:
        digest = args.source_sha1.lower()
        if len(digest) != 40 or any(char not in "0123456789abcdef" for char in digest):
            raise SystemExit("--source-sha1 must contain exactly 40 hexadecimal digits")

    files: list[tuple[int, bytes, Path, int]] = []
    for candidate in asset_root.rglob("*"):
        if candidate.is_file() and not candidate.is_symlink() and candidate.resolve() != output:
            relative = candidate.relative_to(asset_root).as_posix().encode("utf-8")
            files.append((fnv1a64(relative), relative, candidate, candidate.stat().st_size))
    existing_names = {item[1] for item in files}

    def add_extra(name: str, source_path: Path) -> None:
        encoded_name = name.encode("utf-8")
        if not name or name.startswith("/") or ".." in Path(name).parts:
            raise SystemExit(f"invalid asset name: {name}")
        if encoded_name in existing_names:
            raise SystemExit(f"duplicate asset name: {name}")
        if not source_path.is_file() or source_path.is_symlink():
            raise SystemExit(f"extra asset not found or is a symlink: {source_path}")
        existing_names.add(encoded_name)
        files.append((fnv1a64(encoded_name), encoded_name, source_path, source_path.stat().st_size))

    for specification in args.extra:
        if "=" not in specification:
            raise SystemExit(f"invalid --extra (expected NAME=FILE): {specification}")
        name, filename = specification.split("=", 1)
        source_path = Path(filename).resolve()
        add_extra(name, source_path)
    for specification in args.extra_dir:
        if "=" not in specification:
            raise SystemExit(f"invalid --extra-dir (expected PREFIX=DIR): {specification}")
        prefix, dirname = specification.split("=", 1)
        directory = Path(dirname).resolve()
        if not prefix or prefix.startswith("/") or ".." in Path(prefix).parts:
            raise SystemExit(f"invalid asset prefix: {prefix}")
        if not directory.is_dir():
            raise SystemExit(f"extra asset directory not found: {directory}")
        candidates = sorted(
            (path for path in directory.rglob("*") if path.is_file() and not path.is_symlink()),
            key=lambda path: path.relative_to(directory).as_posix().encode("utf-8"),
        )
        for source_path in candidates:
            name = f"{prefix.rstrip('/')}/{source_path.relative_to(directory).as_posix()}"
            add_extra(name, source_path)
    files.sort(key=lambda item: (item[0], item[1]))

    paths = bytearray()
    records: list[tuple[int, int, int, Path, int]] = []
    for path_hash, relative, source_path, size in files:
        path_offset = len(paths)
        paths.extend(relative)
        records.append((path_hash, path_offset, len(relative), source_path, size))

    index_offset = HEADER_SIZE
    paths_offset = index_offset + len(records) * ENTRY_SIZE
    data_offset = align(paths_offset + len(paths))
    current_data_offset = data_offset
    encoded_entries = bytearray()
    for path_hash, path_offset, path_length, _source_path, size in records:
        encoded_entries.extend(struct.pack("<QIIQQ", path_hash, path_offset, path_length,
                                           current_data_offset, size))
        current_data_offset = align(current_data_offset + size)

    header = struct.pack("<8sIIIIQQQ20s12x", MAGIC, VERSION, 0, len(records), HEADER_SIZE,
                         index_offset, paths_offset, data_offset, bytes.fromhex(digest))
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    with temporary.open("wb") as stream:
        stream.write(header)
        stream.write(encoded_entries)
        stream.write(paths)
        stream.write(b"\0" * (data_offset - stream.tell()))
        for _path_hash, _path_offset, _path_length, source_path, _size in records:
            with source_path.open("rb") as source_stream:
                while block := source_stream.read(1024 * 1024):
                    stream.write(block)
            stream.write(b"\0" * (align(stream.tell()) - stream.tell()))
    os.replace(temporary, output)
    print(f"packed {len(records)} assets ({output.stat().st_size} bytes) -> {output}")
    print(f"source ROM SHA-1: {digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Verify required runtime resources in a deterministic GoldenEye asset pack."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct


MAGIC = b"GEPACK\0\0"
VERSION = 1
HEADER = struct.Struct("<8sIIIIQQQ20s12x")
ENTRY = struct.Struct("<QIIQQ")
ALIGNMENT = 16


class PackIntegrityError(ValueError):
    pass


def fnv1a64(value: bytes) -> int:
    result = 14695981039346656037
    for byte in value:
        result ^= byte
        result = (result * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return result


def sha256_region(stream, offset: int, size: int) -> str:
    digest = hashlib.sha256()
    stream.seek(offset)
    remaining = size
    while remaining:
        block = stream.read(min(remaining, 1024 * 1024))
        if not block:
            raise PackIntegrityError("asset data is truncated")
        digest.update(block)
        remaining -= len(block)
    return digest.hexdigest()


def pack_entries(path: Path) -> dict[str, tuple[int, int]]:
    file_size = path.stat().st_size
    with path.open("rb") as stream:
        raw_header = stream.read(HEADER.size)
        if len(raw_header) != HEADER.size:
            raise PackIntegrityError("asset pack header is truncated")
        (magic, version, flags, count, header_size, index_offset,
         paths_offset, data_offset, _source_sha1) = HEADER.unpack(raw_header)
        if magic != MAGIC or version != VERSION or flags != 0:
            raise PackIntegrityError("asset pack header is invalid")
        if header_size != HEADER.size or index_offset != HEADER.size:
            raise PackIntegrityError("asset pack layout header is invalid")
        index_end = index_offset + count * ENTRY.size
        if not index_offset <= index_end <= paths_offset <= data_offset \
                or data_offset > file_size or data_offset % ALIGNMENT != 0:
            raise PackIntegrityError("asset pack section bounds are invalid")
        stream.seek(index_offset)
        raw_entries = stream.read(count * ENTRY.size)
        if len(raw_entries) != count * ENTRY.size:
            raise PackIntegrityError("asset pack index is truncated")
        stream.seek(paths_offset)
        path_table = stream.read(data_offset - paths_offset)

    entries: dict[str, tuple[int, int]] = {}
    previous_key: tuple[int, bytes] | None = None
    for index in range(count):
        path_hash, path_offset, path_length, asset_offset, asset_size = \
            ENTRY.unpack_from(raw_entries, index * ENTRY.size)
        path_end = path_offset + path_length
        if path_length == 0 or path_end > len(path_table):
            raise PackIntegrityError(f"asset path {index} is out of bounds")
        encoded = path_table[path_offset:path_end]
        try:
            name = encoded.decode("utf-8")
        except UnicodeDecodeError as error:
            raise PackIntegrityError(f"asset path {index} is not UTF-8") from error
        if name.startswith("/") or ".." in Path(name).parts:
            raise PackIntegrityError(f"asset path is unsafe: {name}")
        if fnv1a64(encoded) != path_hash:
            raise PackIntegrityError(f"asset path hash mismatch: {name}")
        key = path_hash, encoded
        if previous_key is not None and key <= previous_key:
            raise PackIntegrityError("asset index is not uniquely sorted")
        previous_key = key
        if name in entries:
            raise PackIntegrityError(f"duplicate asset path: {name}")
        if asset_offset < data_offset or asset_offset % ALIGNMENT != 0 \
                or asset_size > file_size - asset_offset:
            raise PackIntegrityError(f"asset data is out of bounds: {name}")
        entries[name] = asset_offset, asset_size
    return entries


def parse_required(specifications: list[str]) -> list[tuple[str, Path]]:
    result: list[tuple[str, Path]] = []
    names: set[str] = set()
    for specification in specifications:
        if "=" not in specification:
            raise PackIntegrityError(
                f"invalid --required (expected NAME=FILE): {specification}")
        name, filename = specification.split("=", 1)
        source = Path(filename).resolve()
        if not name or name.startswith("/") or ".." in Path(name).parts:
            raise PackIntegrityError(f"invalid required asset name: {name}")
        if name in names:
            raise PackIntegrityError(f"duplicate required asset: {name}")
        if not source.is_file() or source.is_symlink():
            raise PackIntegrityError(f"required source file is unavailable: {source}")
        names.add(name)
        result.append((name, source))
    if not result:
        raise PackIntegrityError("at least one --required asset is needed")
    return result


def verify(pack: Path, required: list[tuple[str, Path]]) -> None:
    entries = pack_entries(pack)
    with pack.open("rb") as stream:
        for name, source in required:
            entry = entries.get(name)
            if entry is None:
                raise PackIntegrityError(f"missing required asset: {name}")
            offset, size = entry
            source_size = source.stat().st_size
            if size != source_size:
                raise PackIntegrityError(
                    f"required asset size mismatch: {name} "
                    f"(pack {size}, source {source_size})")
            source_sha256 = hashlib.sha256(source.read_bytes()).hexdigest()
            packed_sha256 = sha256_region(stream, offset, size)
            if packed_sha256 != source_sha256:
                raise PackIntegrityError(
                    f"required asset content mismatch: {name}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--required", action="append", default=[],
                        metavar="NAME=FILE")
    args = parser.parse_args()
    try:
        if not args.pack.is_file():
            raise PackIntegrityError(f"asset pack is unavailable: {args.pack}")
        required = parse_required(args.required)
        verify(args.pack, required)
    except (OSError, PackIntegrityError) as error:
        raise SystemExit(str(error)) from error
    print(f"verified {len(required)} required runtime assets in {args.pack}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

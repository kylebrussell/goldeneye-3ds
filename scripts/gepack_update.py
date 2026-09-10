#!/usr/bin/env python3
"""Strict incremental GEPACK updates; this is not a fresh asset conversion.

The format has path hashes, but no stored payload hashes. ``read_pack`` checks
the canonical layout and computes a payload inventory; callers must bind that
inventory to independently recorded inputs/conversion evidence. No helper here
claims that preserved entries were regenerated or that a ROM was inspected.

All filesystem traversal uses directory descriptors and O_NOFOLLOW. Symlinks,
including ancestors, are rejected. The destination parent must exist; successful
publication atomically creates a new file, without replacing any existing name.
"""

from __future__ import annotations

from contextlib import contextmanager
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import secrets
import stat
import struct
from types import MappingProxyType
from typing import Iterator, Mapping


MAGIC = b"GEPACK\0\0"
VERSION = 1
HEADER = struct.Struct("<8sIIIIQQQ20s12x")
ENTRY = struct.Struct("<QIIQQ")
ALIGNMENT = 16


class GEPackError(ValueError):
    """An unsafe input, noncanonical pack, or failed incremental update."""


@dataclass(frozen=True)
class EntryInfo:
    size: int
    sha256: str
    path_hash: int


@dataclass(frozen=True)
class Pack:
    source_sha1: str
    sha256: str
    size: int
    entries: Mapping[str, bytes]
    inventory: Mapping[str, EntryInfo]


def fnv1a64(value: bytes) -> int:
    result = 14695981039346656037
    for byte in value:
        result = ((result ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return result


def _align(value: int) -> int:
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1)


def _safe_name(name: str) -> bytes:
    if not isinstance(name, str) or not name or "\\" in name or ":" in name \
            or any(part in ("", ".", "..") for part in name.split("/")) \
            or any(ord(char) < 32 or ord(char) == 127 for char in name):
        raise GEPackError(f"unsafe asset name or prefix: {name!r}")
    try:
        return name.encode("utf-8", errors="strict")
    except UnicodeEncodeError as error:
        raise GEPackError(f"asset name is not UTF-8: {name!r}") from error


def _absolute(path: os.PathLike[str] | str) -> str:
    value = os.fspath(path)
    if not isinstance(value, str) or not value or "\0" in value \
            or ".." in value.split("/"):
        raise GEPackError(f"unsafe filesystem path: {value!r}")
    # abspath only normalizes '.' and separators; it must never resolve links.
    return os.path.abspath(value)


@contextmanager
def _directory(path: os.PathLike[str] | str) -> Iterator[int]:
    absolute = _absolute(path)
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    descriptor = os.open("/", flags)
    try:
        for component in absolute.split("/")[1:]:
            if not component:
                continue
            child = os.open(component, flags, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
        yield descriptor
    finally:
        os.close(descriptor)


def _read_regular(parent: int, name: str) -> bytes:
    # O_NONBLOCK prevents a malicious FIFO from blocking before fstat rejects it.
    descriptor = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                         dir_fd=parent)
    try:
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise GEPackError(f"input is not a regular file: {name!r}")
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            payload = stream.read()
        after = os.fstat(descriptor)
        identity = lambda value: (value.st_dev, value.st_ino, value.st_size,
                                  value.st_mtime_ns, value.st_ctime_ns)
        if identity(before) != identity(after) or len(payload) != before.st_size:
            raise GEPackError(f"input changed while reading: {name!r}")
        return payload
    finally:
        os.close(descriptor)


def _read_file(path: os.PathLike[str] | str) -> bytes:
    absolute = _absolute(path)
    with _directory(os.path.dirname(absolute)) as descriptor:
        return _read_regular(descriptor, os.path.basename(absolute))


def _decode(raw: bytes) -> Pack:
    if len(raw) < HEADER.size:
        raise GEPackError("pack header is truncated")
    (magic, version, flags, count, header_size, index_offset, paths_offset,
     data_offset, source_sha1) = HEADER.unpack_from(raw)
    if magic != MAGIC or version != VERSION or flags != 0 \
            or header_size != HEADER.size or index_offset != HEADER.size \
            or any(raw[68:80]):
        raise GEPackError("pack header is invalid or has nonzero reserved bytes")
    if paths_offset != HEADER.size + count * ENTRY.size \
            or not paths_offset <= data_offset <= len(raw) \
            or data_offset % ALIGNMENT:
        raise GEPackError("pack section bounds are invalid")
    path_cursor = 0
    data_cursor = data_offset
    previous_key: tuple[int, bytes] | None = None
    hashes: set[int] = set()
    entries: dict[str, bytes] = {}
    inventory: dict[str, EntryInfo] = {}
    for index in range(count):
        path_hash, path_offset, path_length, asset_offset, asset_size = \
            ENTRY.unpack_from(raw, index_offset + index * ENTRY.size)
        path_end = path_offset + path_length
        if path_offset != path_cursor or not path_length \
                or path_end > data_offset - paths_offset:
            raise GEPackError(f"noncanonical or overlapping path bounds at entry {index}")
        encoded = raw[paths_offset + path_offset:paths_offset + path_end]
        try:
            name = encoded.decode("utf-8", errors="strict")
        except UnicodeDecodeError as error:
            raise GEPackError(f"asset path {index} is not UTF-8") from error
        _safe_name(name)
        if fnv1a64(encoded) != path_hash:
            raise GEPackError(f"asset path hash mismatch: {name!r}")
        if name in entries:
            raise GEPackError(f"duplicate asset path: {name!r}")
        if path_hash in hashes:
            raise GEPackError(f"FNV path hash collision: {name!r}")
        key = path_hash, encoded
        if previous_key is not None and key <= previous_key:
            raise GEPackError("asset index is not uniquely sorted")
        if asset_offset != data_cursor or asset_size > len(raw) - asset_offset:
            raise GEPackError(f"noncanonical or overlapping asset bounds: {name!r}")
        asset_end = asset_offset + asset_size
        aligned_end = _align(asset_end)
        if aligned_end > len(raw) or any(raw[asset_end:aligned_end]):
            raise GEPackError(f"invalid asset padding: {name!r}")
        payload = raw[asset_offset:asset_end]
        entries[name] = payload
        inventory[name] = EntryInfo(asset_size, hashlib.sha256(payload).hexdigest(),
                                    path_hash)
        hashes.add(path_hash)
        previous_key = key
        path_cursor = path_end
        data_cursor = aligned_end
    if data_offset != _align(paths_offset + path_cursor) \
            or any(raw[paths_offset + path_cursor:data_offset]):
        raise GEPackError("noncanonical path table padding")
    if data_cursor != len(raw):
        raise GEPackError("pack has trailing data")
    return Pack(source_sha1.hex(), hashlib.sha256(raw).hexdigest(), len(raw),
                MappingProxyType(entries), MappingProxyType(inventory))


def read_pack(path: os.PathLike[str] | str) -> Pack:
    """Read and validate an immutable snapshot of a canonical version-1 pack."""
    try:
        return _decode(_read_file(path))
    except OSError as error:
        raise GEPackError(f"cannot safely read pack {os.fspath(path)!r}: {error}") from error


def _encode(source_sha1: str, entries: Mapping[str, bytes]) -> bytes:
    records = []
    hashes: set[int] = set()
    for name, payload in entries.items():
        encoded = _safe_name(name)
        path_hash = fnv1a64(encoded)
        if path_hash in hashes:
            raise GEPackError(f"FNV path hash collision: {name!r}")
        hashes.add(path_hash)
        records.append((path_hash, encoded, payload))
    records.sort(key=lambda item: (item[0], item[1]))
    paths = b"".join(item[1] for item in records)
    paths_offset = HEADER.size + len(records) * ENTRY.size
    data_offset = _align(paths_offset + len(paths))
    result = bytearray(HEADER.pack(MAGIC, VERSION, 0, len(records), HEADER.size,
                                  HEADER.size, paths_offset, data_offset,
                                  bytes.fromhex(source_sha1)))
    path_cursor, data_cursor = 0, data_offset
    for path_hash, encoded, payload in records:
        result.extend(ENTRY.pack(path_hash, path_cursor, len(encoded),
                                 data_cursor, len(payload)))
        path_cursor += len(encoded)
        data_cursor = _align(data_cursor + len(payload))
    result.extend(paths)
    result.extend(b"\0" * (data_offset - len(result)))
    for _path_hash, _encoded, payload in records:
        result.extend(payload)
        result.extend(b"\0" * (_align(len(result)) - len(result)))
    return bytes(result)


def _replacement_files(directory: os.PathLike[str] | str, prefix: str) -> dict[str, bytes]:
    files: dict[str, bytes] = {}

    def visit(descriptor: int, relative: str) -> None:
        # scandir's duplicate descriptor is closed before recursion.
        with os.scandir(descriptor) as scan:
            children = sorted((item.name for item in scan), key=lambda name: name.encode("utf-8"))
        for child in children:
            name = f"{relative}/{child}" if relative else child
            _safe_name(name)
            mode = os.stat(child, dir_fd=descriptor, follow_symlinks=False).st_mode
            if stat.S_ISLNK(mode):
                raise GEPackError(f"replacement tree contains a symlink: {name!r}")
            if stat.S_ISDIR(mode):
                nested = os.open(child, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                                 dir_fd=descriptor)
                try:
                    visit(nested, name)
                finally:
                    os.close(nested)
            elif stat.S_ISREG(mode):
                files[f"{prefix}/{name}"] = _read_regular(descriptor, child)
            else:
                raise GEPackError(f"replacement tree contains a nonregular entry: {name!r}")

    with _directory(directory) as descriptor:
        visit(descriptor, "")
    return files


def _inventory(pack: Pack, path: str) -> dict:
    return {
        "path": path, "sha256": pack.sha256, "size": pack.size,
        "entry_count": len(pack.entries),
        "entries": [{"name": name, "size": info.size, "sha256": info.sha256,
                     "path_hash": f"{info.path_hash:016x}"}
                    for name, info in sorted(pack.inventory.items())],
    }


def _absent(descriptor: int, name: str) -> None:
    try:
        os.stat(name, dir_fd=descriptor, follow_symlinks=False)
    except FileNotFoundError:
        return
    raise GEPackError(f"output already exists: {name!r}")


def _publish(descriptor: int, name: str, payload: bytes) -> None:
    temporary = f".gepack-update-{secrets.token_hex(16)}.tmp"
    stream_fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                        0o600, dir_fd=descriptor)
    try:
        with os.fdopen(stream_fd, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        # A hard-link publication is atomic and fails if another process won the
        # destination name, unlike replace/rename. Both names are in one directory.
        os.link(temporary, name, src_dir_fd=descriptor, dst_dir_fd=descriptor,
                follow_symlinks=False)
    finally:
        os.unlink(temporary, dir_fd=descriptor)


def update_pack(base_path: os.PathLike[str] | str,
                replacements: Mapping[str, os.PathLike[str] | str],
                output_path: os.PathLike[str] | str) -> dict:
    """Replace complete prefix subtrees and return an incremental provenance receipt.

    Empty replacement directories intentionally delete a complete subtree. Prefix
    matching always includes '/', so replacing 'characters' preserves 'characters2'.
    All input validation/encoding precedes temporary-file creation. Failure never
    publishes a partial pack or overwrites an existing output.
    """
    try:
        base_name, output_name = _absolute(base_path), _absolute(output_path)
        if not replacements:
            raise GEPackError("at least one replacement prefix is required")
        prefixes = sorted(replacements)
        for prefix in prefixes:
            _safe_name(prefix)
        for index, prefix in enumerate(prefixes):
            if any(other.startswith(prefix + "/") for other in prefixes[index + 1:]):
                raise GEPackError(f"overlapping replacement prefixes: {prefix!r}")
        with _directory(os.path.dirname(output_name)) as output_directory:
            basename = os.path.basename(output_name)
            _absent(output_directory, basename)
            base = read_pack(base_name)
            for prefix in prefixes:
                if any(prefix == name or prefix.startswith(name + "/") for name in base.entries):
                    raise GEPackError(f"replacement prefix conflicts with existing asset: {prefix!r}")
            replaced = {name for name in base.entries
                        if any(name.startswith(prefix + "/") for prefix in prefixes)}
            entries = {name: payload for name, payload in base.entries.items()
                       if name not in replaced}
            preserved = sorted(entries)
            regenerated: set[str] = set()
            replacements_receipt = []
            for prefix in prefixes:
                directory_name = _absolute(replacements[prefix])
                new_entries = _replacement_files(directory_name, prefix)
                if entries.keys() & new_entries.keys():
                    raise GEPackError(f"replacement collides with an existing entry: {prefix!r}")
                entries.update(new_entries)
                regenerated.update(new_entries)
                replacements_receipt.append({
                    "prefix": prefix, "source_directory": directory_name,
                    "previous_entry_count": sum(name.startswith(prefix + "/") for name in replaced),
                    "replacement_entry_count": len(new_entries),
                })
            encoded = _encode(base.source_sha1, entries)
            output = _decode(encoded)
            if any(output.entries[name] != base.entries[name] for name in preserved):
                raise GEPackError("untouched entry preservation failed")
            receipt = {
                "schema_version": 1, "operation": "incremental_pack_update",
                "source_rom_sha1": base.source_sha1,
                "base": _inventory(base, base_name),
                "output": _inventory(output, output_name),
                "replacements": replacements_receipt,
                "preserved_entries": preserved,
                "removed_entries": sorted(replaced - regenerated),
                "regenerated_entries": sorted(regenerated),
            }
            _publish(output_directory, basename, encoded)
            return receipt
    except (OSError, UnicodeError) as error:
        raise GEPackError(f"incremental pack update failed: {error}") from error

#!/usr/bin/env python3
"""Extract a deterministic private bundle for every selectable Dam room."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import tempfile
import zlib


N64_BG_BASE = 0x0F000000
ROOM_ENTRY_SIZE = 24
EXPECTED_SOURCE_SIZE = 197024
EXPECTED_SOURCE_SHA256 = "40f74a2a087fffa6f1d3519d796d2a2989377ee85fd352deb72d774ea76430c0"


@dataclass(frozen=True)
class RoomEntry:
    point_address: int
    primary_address: int
    secondary_address: int
    origin: tuple[float, float, float]


@dataclass(frozen=True)
class InflatedStream:
    decoded: bytes
    compressed_size: int
    span_size: int
    source_offset: int
    compressed_sha256: str
    span_sha256: str
    decoded_sha256: str


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def bg_offset(address: int, source_size: int, label: str) -> int:
    if address < N64_BG_BASE or address >= N64_BG_BASE + source_size:
        raise ValueError(f"{label} address is outside segment 0x0f: 0x{address:08x}")
    return address - N64_BG_BASE


def parse_room_table(source: bytes) -> tuple[list[RoomEntry], RoomEntry]:
    if len(source) < 20:
        raise ValueError("background is shorter than its five-word header")
    header = struct.unpack_from(">5I", source)
    table_start = bg_offset(header[1], len(source), "room-table start")
    table_end = bg_offset(header[3], len(source), "room-table end")
    if table_start != 20 or table_end <= table_start:
        raise ValueError("unexpected room-table bounds")
    table_size = table_end - table_start
    if table_size % ROOM_ENTRY_SIZE != 0 or table_size < ROOM_ENTRY_SIZE * 3:
        raise ValueError("room table is not a bounded array of 24-byte entries")

    table: list[RoomEntry] = []
    for offset in range(table_start, table_end, ROOM_ENTRY_SIZE):
        point, primary, secondary, x, y, z = struct.unpack_from(">IIIfff", source, offset)
        table.append(RoomEntry(point, primary, secondary, (x, y, z)))
    zero = RoomEntry(0, 0, 0, (0.0, 0.0, 0.0))
    if table[0] != zero or table[-1] != zero:
        raise ValueError("room table is missing its dummy or terminating zero entry")
    sentinel = table[-2]
    if (sentinel.point_address == 0 or sentinel.primary_address == 0
            or sentinel.secondary_address == 0
            or sentinel.origin != (0.0, 0.0, 0.0)):
        raise ValueError("room table is missing its final stream-bounds sentinel")
    rooms = table[:-2]
    if len(rooms) < 2:
        raise ValueError("room table contains no playable rooms")
    for room_index, room in enumerate(rooms[1:], 1):
        if room.primary_address == 0:
            raise ValueError(f"room {room_index} is missing its primary display list")
        if room.point_address == 0 and room.origin != (0.0, 0.0, 0.0):
            raise ValueError(
                f"room {room_index} has no point stream but has a nonzero origin"
            )
        for stream_name, address in (
                ("point", room.point_address),
                ("primary", room.primary_address),
                ("secondary", room.secondary_address)):
            if address != 0:
                bg_offset(address, len(source), f"room {room_index} {stream_name}")
    for stream_name, address in (
            ("point", sentinel.point_address),
            ("primary", sentinel.primary_address),
            ("secondary", sentinel.secondary_address)):
        bg_offset(address, len(source), f"final {stream_name} bound")
    return rooms, sentinel


def stream_boundaries(rooms: list[RoomEntry], sentinel: RoomEntry) -> dict[int, int]:
    starts = {
        address
        for room in rooms[1:]
        for address in (room.point_address, room.primary_address, room.secondary_address)
        if address != 0
    }
    bounds = starts | {
        sentinel.point_address,
        sentinel.primary_address,
        sentinel.secondary_address,
    }
    ordered = sorted(bounds)
    result: dict[int, int] = {}
    for start in starts:
        following = next((candidate for candidate in ordered if candidate > start), None)
        if following is None:
            raise ValueError(f"stream 0x{start:08x} has no upper bound")
        result[start] = following
    return result


def inflate_1172(source: bytes, start_address: int, end_address: int,
                 label: str) -> InflatedStream:
    start = bg_offset(start_address, len(source), f"{label} start")
    end = end_address - N64_BG_BASE
    if end <= start or end > len(source):
        raise ValueError(f"{label} has invalid stream bounds")
    span = source[start:end]
    if len(span) < 3 or span[:2] != b"\x11\x72":
        raise ValueError(f"{label} does not have a Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    decoded = inflater.decompress(span[2:]) + inflater.flush()
    if not inflater.eof or inflater.unconsumed_tail:
        raise ValueError(f"{label} has an incomplete raw-deflate stream")
    compressed_size = len(span) - len(inflater.unused_data)
    if compressed_size <= 2:
        raise ValueError(f"{label} has an empty compressed stream")
    if any(inflater.unused_data):
        raise ValueError(f"{label} has nonzero bytes after its deflate stream")
    return InflatedStream(
        decoded=decoded,
        compressed_size=compressed_size,
        span_size=len(span),
        source_offset=start,
        compressed_sha256=sha256(span[:compressed_size]),
        span_sha256=sha256(span),
        decoded_sha256=sha256(decoded),
    )


def analyze_points(decoded: bytes, room_index: int) -> dict[str, object]:
    if not decoded or len(decoded) % 16 != 0:
        raise ValueError(f"room {room_index} point table is not 16-byte vertices")
    return {"vertex_count": len(decoded) // 16, "stride": 16, "segment": 14}


def analyze_gdl(decoded: bytes, room_index: int, stream_name: str) -> dict[str, object]:
    if not decoded or len(decoded) % 8 != 0:
        raise ValueError(f"room {room_index} {stream_name} is not 8-byte commands")
    commands = [struct.unpack_from(">II", decoded, offset)
                for offset in range(0, len(decoded), 8)]
    texture_ids = sorted({word1 & 0xFFF for word0, word1 in commands
                          if word0 >> 24 == 0xC0})
    vertex_segments = sorted({word1 >> 24 for word0, word1 in commands
                              if word0 >> 24 == 0x04})
    return {
        "command_count": len(commands),
        "ends_with_display_list_end": commands[-1] == (0xB8000000, 0),
        "texture_ids": texture_ids,
        "vertex_segments": vertex_segments,
    }


def stream_manifest(path: str, stream: InflatedStream,
                    analysis: dict[str, object]) -> dict[str, object]:
    return {
        "path": path,
        "source_offset": stream.source_offset,
        "compressed_size": stream.compressed_size,
        "span_size": stream.span_size,
        "decoded_size": len(stream.decoded),
        "compressed_sha256": stream.compressed_sha256,
        "span_sha256": stream.span_sha256,
        "decoded_sha256": stream.decoded_sha256,
        **analysis,
    }


def parse_room_selection(specifications: list[str], room_count: int) -> list[int]:
    if not specifications:
        return list(range(room_count))
    selected: set[int] = set()
    for specification in specifications:
        for item in specification.split(","):
            item = item.strip()
            if not item:
                raise ValueError("empty room selection")
            if "-" in item:
                first_text, last_text = item.split("-", 1)
                first, last = int(first_text), int(last_text)
                if first > last:
                    raise ValueError(f"descending room range: {item}")
                selected.update(range(first, last + 1))
            else:
                selected.add(int(item))
    if not selected or min(selected) < 0 or max(selected) >= room_count:
        raise ValueError(f"room selection must be within 0..{room_count - 1}")
    return sorted(selected)


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


def extract_rooms(source_path: Path, output: Path,
                  room_specs: list[str] | None = None,
                  expected_size: int | None = EXPECTED_SOURCE_SIZE,
                  expected_sha256: str | None = EXPECTED_SOURCE_SHA256,
                  stage_key: str = "dam",
                  source_manifest_path: str =
                  "build/u/assets/obseg/bg/bg_dam_all_p.bin") -> dict[str, object]:
    source = source_path.read_bytes()
    if expected_size is not None and len(source) != expected_size:
        raise ValueError(f"unexpected background size: {len(source)}")
    source_digest = sha256(source)
    if expected_sha256 is not None and source_digest != expected_sha256:
        raise ValueError(f"background SHA-256 mismatch: {source_digest}")
    rooms, sentinel = parse_room_table(source)
    selected = parse_room_selection(room_specs or [], len(rooms))
    boundaries = stream_boundaries(rooms, sentinel)

    output = output.expanduser().absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest: dict[str, object] = {
        "schema": 1,
        "name": f"{stage_key}-rooms",
        "source": {
            "path": source_manifest_path,
            "size": len(source),
            "sha256": source_digest,
            "compression": "rare-1172-raw-deflate",
        },
        "room_count": len(rooms),
        "selected_rooms": selected,
        "rooms": [],
    }
    with tempfile.TemporaryDirectory(prefix=f"ge-{stage_key}-rooms-",
                                     dir=output.parent) as temporary_name:
        staging = Path(temporary_name) / "output"
        staging.mkdir()
        room_manifests: list[dict[str, object]] = []
        for room_index in selected:
            room = rooms[room_index]
            room_manifest: dict[str, object] = {
                "index": room_index,
                "origin": list(room.origin),
                "streams": {},
            }
            if room_index != 0:
                room_directory = staging / f"room{room_index:03d}"
                room_directory.mkdir()
                stream_definitions = [
                    ("point_table", room.point_address, "point_table.bin"),
                    ("primary_gdl", room.primary_address, "primary_gdl.bin"),
                    ("secondary_gdl", room.secondary_address, "secondary_gdl.bin"),
                ]
                for stream_name, start, filename in stream_definitions:
                    if start == 0:
                        continue
                    inflated = inflate_1172(source, start, boundaries[start],
                                            f"room {room_index} {stream_name}")
                    analysis = analyze_points(inflated.decoded, room_index) \
                        if stream_name == "point_table" \
                        else analyze_gdl(inflated.decoded, room_index, stream_name)
                    (room_directory / filename).write_bytes(inflated.decoded)
                    room_manifest["streams"][stream_name] = stream_manifest(
                        f"room{room_index:03d}/{filename}", inflated, analysis)
            room_manifests.append(room_manifest)
        manifest["rooms"] = room_manifests
        manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
        (staging / "manifest.json").write_bytes(manifest_bytes)
        replace_directory(staging, output)
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rooms", action="append", default=[], metavar="LIST",
                        help="room indexes/ranges such as 1,3-5; defaults to all")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        manifest = extract_rooms(args.input, args.output, args.rooms)
    except (OSError, ValueError, zlib.error) as error:
        print(f"error: {error}", file=__import__("sys").stderr)
        return 1
    stream_count = sum(len(room["streams"]) for room in manifest["rooms"])
    print(f"extracted {len(manifest['selected_rooms'])} of {manifest['room_count']} Dam rooms "
          f"({stream_count} streams) -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

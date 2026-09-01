#!/usr/bin/env python3
"""Build bounded authored Dam room AABBs for original portal visibility."""

from __future__ import annotations

import argparse
import hashlib
import math
import os
from pathlib import Path
import struct
import tempfile


BG_BASE = 0x0F000000
MAGIC = b"GEDMBND\0"
VERSION = 1
HEADER_SIZE = 80
RECORD_SIZE = 24
EXPECTED_SOURCE_SIZE = 197024
EXPECTED_SOURCE_SHA256 = "40f74a2a087fffa6f1d3519d796d2a2989377ee85fd352deb72d774ea76430c0"
EXPECTED_ROOM_COUNT = 137
EXPECTED_PORTAL_COUNT = 194


def fnv1a64(data: bytes) -> int:
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = value * 0x100000001B3 & 0xFFFFFFFFFFFFFFFF
    return value


def bg_offset(address: int, source_size: int, label: str) -> int:
    if address < BG_BASE or address >= BG_BASE + source_size:
        raise ValueError(f"{label} address outside segment 0x0f: 0x{address:08x}")
    return address - BG_BASE


def include_point(bounds: list[list[float]], point: tuple[float, float, float]) -> None:
    for axis in range(3):
        bounds[0][axis] = min(bounds[0][axis], point[axis])
        bounds[1][axis] = max(bounds[1][axis], point[axis])


def build_bounds(background: bytes, rooms_directory: Path,
                 expected_room_count: int | None = EXPECTED_ROOM_COUNT,
                 expected_portal_count: int | None = EXPECTED_PORTAL_COUNT
                 ) -> tuple[list[tuple[float, ...]], int]:
    if len(background) < 20:
        raise ValueError("background shorter than header")
    _root, rooms_address, portals_address, visibility_address, _environment = \
        struct.unpack_from(">5I", background)
    room_start = bg_offset(rooms_address, len(background), "room table")
    room_end = bg_offset(visibility_address, len(background), "visibility table")
    portal_start = bg_offset(portals_address, len(background), "portal table")
    if room_end <= room_start or (room_end - room_start) % 24:
        raise ValueError("invalid room table span")
    records = (room_end - room_start) // 24
    if records < 3:
        raise ValueError("room table lacks sentinels")
    room_count = records - 2
    if expected_room_count is not None and room_count != expected_room_count:
        raise ValueError(f"unexpected room count: {room_count}")

    bounds: list[list[list[float]]] = [
        [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]
        for _ in range(room_count)
    ]
    for room in range(1, room_count):
        record = room_start + room * 24
        point_address = struct.unpack_from(">I", background, record)[0]
        origin = struct.unpack_from(">fff", background, record + 12)
        if point_address == 0:
            # Streets contains authored empty room slots with a zero origin and
            # no point stream.  Their zero AABB follows directly from that
            # room-table record; any live portal geometry is still included by
            # the portal pass below.
            if origin != (0.0, 0.0, 0.0):
                raise ValueError(f"room {room} has no points but a nonzero origin")
            continue
        point_path = rooms_directory / f"room{room:03d}" / "point_table.bin"
        points = point_path.read_bytes()
        if not points or len(points) % 16:
            raise ValueError(f"room {room} point table is not 16-byte vertices")
        current = [[math.inf, math.inf, math.inf],
                   [-math.inf, -math.inf, -math.inf]]
        for offset in range(0, len(points), 16):
            local = struct.unpack_from(">hhh", points, offset)
            include_point(current, tuple(origin[axis] + local[axis]
                                         for axis in range(3)))
        bounds[room] = current

    portal_count = 0
    while portal_count < 200:
        record = portal_start + portal_count * 8
        if record + 8 > len(background):
            raise ValueError("unterminated portal table")
        geometry_address, room1, room2, _control1, _control2 = \
            struct.unpack_from(">IBBBB", background, record)
        if geometry_address == 0:
            break
        geometry = bg_offset(geometry_address, len(background), "portal geometry")
        point_count = background[geometry]
        # The authored solo-stage backgrounds contain 3- through 7-point
        # portal polygons (Depot owns the two 7-point records).  Do not
        # impose the Dam-only six-point ceiling on the generic builder.
        if point_count < 3 or point_count > 7 \
                or geometry + 4 + point_count * 12 > len(background):
            raise ValueError(f"portal {portal_count} has invalid polygon")
        if room1 >= room_count or room2 >= room_count:
            raise ValueError(f"portal {portal_count} has invalid room ownership")
        for point_index in range(point_count):
            point = struct.unpack_from(">fff", background,
                                       geometry + 4 + point_index * 12)
            if room1:
                include_point(bounds[room1], point)
            if room2:
                include_point(bounds[room2], point)
        portal_count += 1
    if portal_count == 200:
        raise ValueError("portal capacity exceeded")
    if expected_portal_count is not None and portal_count != expected_portal_count:
        raise ValueError(f"unexpected portal count: {portal_count}")

    records_out: list[tuple[float, ...]] = []
    for room, current in enumerate(bounds):
        values = (*current[0], *current[1])
        if not all(math.isfinite(value) for value in values):
            raise ValueError(f"room {room} has non-finite bounds")
        if any(current[0][axis] > current[1][axis] for axis in range(3)):
            raise ValueError(f"room {room} has inverted bounds")
        records_out.append(values)
    return records_out, portal_count


def encode_bounds(background: bytes, bounds: list[tuple[float, ...]]) -> bytes:
    payload = b"".join(struct.pack("<6f", *record) for record in bounds)
    header = struct.pack(
        "<8s6I32sQQ",
        MAGIC, VERSION, HEADER_SIZE, len(bounds), RECORD_SIZE, len(payload), 0,
        hashlib.sha256(background).digest(), fnv1a64(background), fnv1a64(payload),
    )
    if len(header) != HEADER_SIZE:
        raise AssertionError(f"internal header size mismatch: {len(header)}")
    return header + payload


def build_asset(background_path: Path, rooms_directory: Path,
                output_path: Path,
                expected_source_size: int | None = EXPECTED_SOURCE_SIZE,
                expected_source_sha256: str | None = EXPECTED_SOURCE_SHA256,
                expected_room_count: int | None = EXPECTED_ROOM_COUNT,
                expected_portal_count: int | None = EXPECTED_PORTAL_COUNT,
                stage_label: str = "Dam") -> bytes:
    background = background_path.read_bytes()
    if expected_source_size is not None and len(background) != expected_source_size:
        raise ValueError(f"unexpected background size: {len(background)}")
    digest = hashlib.sha256(background).hexdigest()
    if expected_source_sha256 is not None and digest != expected_source_sha256:
        raise ValueError(f"background SHA-256 mismatch: {digest}")
    bounds, portals = build_bounds(background, rooms_directory,
                                   expected_room_count, expected_portal_count)
    encoded = encode_bounds(background, bounds)
    output_path = output_path.expanduser().absolute()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix=output_path.name + ".",
                                     dir=output_path.parent, delete=False) as temporary:
        temporary.write(encoded)
        temporary_path = Path(temporary.name)
    try:
        os.replace(temporary_path, output_path)
    except BaseException:
        temporary_path.unlink(missing_ok=True)
        raise
    print(f"{stage_label} room bounds: {len(bounds)} room, {portals} portal, "
          f"{len(encoded)} bytes, sha256={hashlib.sha256(encoded).hexdigest()}")
    return encoded


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--background", type=Path, required=True)
    parser.add_argument("--rooms", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build_asset(args.background, args.rooms, args.output)


if __name__ == "__main__":
    main()
